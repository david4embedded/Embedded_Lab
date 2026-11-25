/**
 * @file app_shell.cpp
 * @brief Implements the IMU and microphone shell commands.
 * @date 2026-10-28
 * @author Sungsu Kim
 */

#include "app_shell.hpp"

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

static constexpr uint32_t IMU_MIN_INTERVAL_MS = 10;
static constexpr uint32_t IMU_MAX_INTERVAL_MS = 60000;
static constexpr uint32_t MIC_DEFAULT_INTERVAL_MS = Mic::BLOCK_MS;
static constexpr uint32_t MIC_MAX_INTERVAL_MS = 10000;

static constexpr int32_t METER_MIN_DB10 = -900;
static constexpr int32_t METER_MAX_DB10 = -200;
static constexpr int32_t METER_DB10_PER_CHAR = 20;
static constexpr int32_t METER_WIDTH = (METER_MAX_DB10 - METER_MIN_DB10) / METER_DB10_PER_CHAR;
static constexpr int32_t FULL_SCALE = 32768;
static constexpr int32_t CLIP_LEVEL = 32000;
static constexpr int32_t CENTI_UNITS_PER_UNIT = 100;
static constexpr size_t DB_TEXT_BUFFER_SIZE = 16;

static_assert(METER_WIDTH > 0);

static constexpr size_t STREAM_STACK_SIZE = 4096;
static constexpr int32_t STREAM_THREAD_PRIORITY = 5;
static constexpr int32_t STOP_TIMEOUT_MS = 2000;

static constexpr uint8_t KEY_ESC = 0x1B;

enum class StreamType
{
	Imu,
	Mic,
};

struct StreamRequest
{
	const struct shell* m_shell;	//!< Shell associated with the active stream.
	StreamType m_type{};			//!< Kind of stream to run.
	uint32_t m_interval{};			//!< Interval between reports in milliseconds.
};

static Imu* s_imu = nullptr;
static Mic* s_mic = nullptr;
static bool s_imuReady = false;
static bool s_micReady = false;

K_THREAD_STACK_DEFINE(s_streamStack, STREAM_STACK_SIZE);
static struct k_thread s_streamThread;
static bool s_streamThreadCreated = false;
static StreamRequest s_request;

K_SEM_DEFINE(s_stopSem, 0, 1);		//!< Given by the bypass callback when ESC is received.
K_SEM_DEFINE(s_doneSem, 0, 1);		//!< Given by the stream thread when it finishes.
static atomic_t s_stopRequested = ATOMIC_INIT(0);

static bool waitForStop(const int64_t deadline);
static bool isStopRequested();
static bool hasEscKey(const uint8_t* data, const size_t length);
static void streamBypass(const struct shell* shell, uint8_t* data, const size_t length);
static bool parseInterval(const struct shell* shell, const char* argument, const uint32_t minimum, const uint32_t maximum, uint32_t& interval);
static bool checkImu(const struct shell* shell);
static bool checkMic(const struct shell* shell);
static void printImuHeader(const struct shell* shell);
static void printImuSample(const struct shell* shell, const uint32_t timeMs, const Imu::Sample& sample);
static int32_t toDb10(const int32_t amplitude);
static bool runImuStream(const struct shell* shell, const uint32_t interval);
static bool runMicMonitor(const struct shell* shell, const uint32_t interval);
static void streamThread(void* parameter1, void* parameter2, void* parameter3);
static int startStream(const struct shell* shell, const StreamType type, const uint32_t interval);
static int showImuInfo(const struct shell* shell, const size_t argumentCount, char** arguments);
static int readImu(const struct shell* shell, const size_t argumentCount, char** arguments);
static int streamImu(const struct shell* shell, const size_t argumentCount, char** arguments);
static int monitorMic(const struct shell* shell, const size_t argumentCount, char** arguments);

/**
 * @brief Registers the IMU and microphone objects used by shell commands.
 * @retval None
 */
void AppShell::Initialize(Imu& imu, const bool imuReady, Mic& mic, const bool micReady)
{
	s_imu = &imu;
	s_imuReady = imuReady;
	s_mic = &mic;
	s_micReady = micReady;
}

/**
 * @brief Waits until the deadline or until ESC is pressed.
 * @retval true if ESC was pressed; otherwise false.
 */
static bool waitForStop(const int64_t deadline)
{
	const int64_t remaining = deadline - k_uptime_get();
	const k_timeout_t timeout = (remaining > 0) ? K_MSEC(remaining) : K_NO_WAIT;

	return k_sem_take(&s_stopSem, timeout) == 0;
}

/**
 * @brief Checks whether the stop semaphore has been signaled.
 * @retval true if a stop request was pending; otherwise false.
 */
static bool isStopRequested()
{
	return k_sem_take(&s_stopSem, K_NO_WAIT) == 0;
}

/**
 * @brief Checks for a single ESC key press.
 * @details Arrow and function keys also start with ESC (e.g. ESC [ A), so ESC followed
 *          by '[' or 'O' in the same chunk is treated as an escape sequence and ignored.
 * @retval true if a standalone ESC key was found; otherwise false.
 */
static bool hasEscKey(const uint8_t* data, const size_t length)
{
	for (size_t index = 0; index < length; ++index)
	{
		if (data[index] != KEY_ESC)
		{
			continue;
		}

		const bool isSequence = ((index + 1) < length) &&
			((data[index + 1] == '[') || (data[index + 1] == 'O'));
		if (!isSequence)
		{
			return true;
		}
	}
	return false;
}

/**
 * @brief Handles input bytes while a stream is running.
 * @retval None
 */
static void streamBypass(const struct shell* shell, uint8_t* data, const size_t length)
{
	if (!hasEscKey(data, length))
	{
		return;
	}

	atomic_set(&s_stopRequested, 1);
	k_sem_give(&s_stopSem);

	//! Wait for the last stream output so the prompt is printed afterward.
	(void)k_sem_take(&s_doneSem, K_MSEC(STOP_TIMEOUT_MS));
	shell_set_bypass(shell, NULL);
}

/**
 * @brief Parses and validates a millisecond interval.
 * @retval true if the interval is valid; otherwise false.
 */
static bool parseInterval(const struct shell* shell, const char* argument, const uint32_t minimum, const uint32_t maximum, uint32_t& interval)
{
	int error = 0;
	const auto result = shell_strtoul(argument, 10, &error);

	if ((error != 0) || (result < minimum) || (result > maximum))
	{
		shell_error(shell, "Invalid interval '%s' (%u ~ %u ms)", argument, minimum, maximum);
		return false;
	}

	interval = static_cast<uint32_t>(result);
	return true;
}

/**
 * @brief Verifies that the IMU is available to a shell command.
 * @retval true if available; otherwise false.
 */
static bool checkImu(const struct shell* shell)
{
	if ((s_imu == nullptr) || !s_imuReady)
	{
		shell_error(shell, "IMU is not available");
		return false;
	}
	return true;
}

/**
 * @brief Verifies that the microphone is available to a shell command.
 * @retval true if available; otherwise false.
 */
static bool checkMic(const struct shell* shell)
{
	if ((s_mic == nullptr) || !s_micReady)
	{
		shell_error(shell, "Mic is not available");
		return false;
	}
	return true;
}

/**
 * @brief Prints the IMU sample column headings.
 * @retval None
 */
static void printImuHeader(const struct shell* shell)
{
	shell_print(shell, "  time[ms] |  ax[mg]  ay[mg]  az[mg] | gx[mdps] gy[mdps] gz[mdps] | temp[C]");
}

/**
 * @brief Prints a timestamped IMU sample.
 * @retval None
 */
static void printImuSample(const struct shell* shell, const uint32_t timeMs, const Imu::Sample& sample)
{
	shell_print(shell, "%10u | %7d %7d %7d | %8d %8d %8d | %3d.%02d",
		timeMs,
		sample.m_accelMg[0], sample.m_accelMg[1], sample.m_accelMg[2],
		sample.m_gyroMdps[0], sample.m_gyroMdps[1], sample.m_gyroMdps[2],
		sample.m_tempCentiC / CENTI_UNITS_PER_UNIT,
		std::abs(sample.m_tempCentiC % CENTI_UNITS_PER_UNIT));
}

/**
 * @brief Converts an amplitude to dBFS in 0.1 dB units.
 * @retval The amplitude in 0.1 dBFS units, clamped to the meter's lower bound.
 */
static int32_t toDb10(const int32_t amplitude)
{
	if (amplitude <= 0)
	{
		return METER_MIN_DB10;
	}

	const double db = 20.0 * std::log10(static_cast<double>(amplitude) / FULL_SCALE);
	return std::max(static_cast<int32_t>(std::lround(db * 10.0)), METER_MIN_DB10);
}

/**
 * @brief Streams IMU samples until ESC is pressed.
 * @retval true if stopped by ESC; false if a read failed.
 */
static bool runImuStream(const struct shell* shell, const uint32_t interval)
{
	printImuHeader(shell);

	const int64_t start = k_uptime_get();
	int64_t next = start;
	uint32_t count = 0;

	for (;;)
	{
		Imu::Sample sample;
		auto result = s_imu->read(sample);
		if (result != 0)
		{
			shell_error(shell, "IMU read failed (%d)", result);
			return false;
		}

		printImuSample(shell, static_cast<uint32_t>(k_uptime_get() - start), sample);
		++count;

		//! Skip missed slots rather than burst after a delayed print.
		next += interval;
		const int64_t now = k_uptime_get();
		if (next < now)
		{
			next = now;
		}

		if (waitForStop(next))
		{
			break;
		}
	}

	shell_print(shell, "Stopped by ESC (%u samples)", count);
	return true;
}

/**
 * @brief Prints microphone levels until ESC is pressed.
 * @retval true if stopped by ESC; false if a microphone operation failed.
 */
static bool runMicMonitor(const struct shell* shell, const uint32_t interval)
{
	//! The level is computed per 100 ms block.
	const uint32_t blocksPerReport = interval / Mic::BLOCK_MS;

	auto result = s_mic->start();
	if (result != 0)
	{
		shell_error(shell, "Mic start failed (%d)", result);
		return false;
	}

	shell_print(shell, "Meter range: %d ~ %d dBFS, one '#' per %d dB", METER_MIN_DB10 / 10, METER_MAX_DB10 / 10, METER_DB10_PER_CHAR / 10);
	shell_print(shell, "  time[ms] |  peak |   rms | rms[dBFS] | level");

	const int64_t start = k_uptime_get();
	char meter[METER_WIDTH + 1];
	std::fill_n(meter, METER_WIDTH + 1, '\0');
	bool stopped = false;

	while (!stopped)
	{
		int32_t peak = 0;
		double sumSquare = 0.0;
		bool flat = true;

		for (uint32_t i = 0; i < blocksPerReport; ++i)
		{
			Mic::Level level;
			result = s_mic->read(level);
			if (result != 0)
			{
				s_mic->stop();
				shell_error(shell, "Mic read failed (%d)", result);
				return false;
			}

			peak = std::max(peak, level.m_peak);
			sumSquare += static_cast<double>(level.m_rms) * level.m_rms;
			flat = flat && level.m_flat;

			if (isStopRequested())
			{
				stopped = true;
				break;
			}
		}

		if (stopped)
		{
			break;
		}

		const auto rms = static_cast<int32_t>(std::sqrt(sumSquare / blocksPerReport));
		const int32_t db10 = toDb10(rms);
		const int32_t bars = std::clamp(
			(db10 - METER_MIN_DB10) / METER_DB10_PER_CHAR, 0, METER_WIDTH);

		for (int32_t i = 0; i < METER_WIDTH; ++i)
		{
			meter[i] = (i < bars) ? '#' : '.';
		}

		//! Format without float printf support and preserve the sign for -0.x.
		char dbText[DB_TEXT_BUFFER_SIZE];
		snprintk(dbText, sizeof(dbText), "%s%d.%d", (db10 < 0) ? "-" : "", std::abs(db10) / 10, std::abs(db10) % 10);

		shell_print(shell, "%10u | %5d | %5d | %9s | %s%s%s",
			static_cast<uint32_t>(k_uptime_get() - start),
			peak, rms,
			dbText,
			meter,
			(peak >= CLIP_LEVEL) ? " CLIP" : "",
			flat ? " FLAT (no data?)" : "");
	}

	s_mic->stop();
	shell_print(shell, "Stopped by ESC");
	return true;
}

/**
 * @brief Runs the selected stream in the dedicated thread.
 * @retval None
 */
static void streamThread(void* parameter1, void* parameter2, void* parameter3)
{
	ARG_UNUSED(parameter2);
	ARG_UNUSED(parameter3);

	const auto* request = static_cast<const StreamRequest*>(parameter1);

	if (request->m_type == StreamType::Imu)
	{
		(void)runImuStream(request->m_shell, request->m_interval);
	}
	else
	{
		(void)runMicMonitor(request->m_shell, request->m_interval);
	}

	k_sem_give(&s_doneSem);

	//! Leave bypass mode after an error; the next key input restores the prompt.
	if (!atomic_get(&s_stopRequested))
	{
		shell_print(request->m_shell, "Stream ended. Press Enter to continue.");
		shell_set_bypass(request->m_shell, NULL);
	}
}

/**
 * @brief Starts the stream thread and passes every key input to the bypass handler.
 * @details The command returns immediately while a separate thread runs the stream.
 * @retval 0 when the stream thread is started.
 */
static int startStream(const struct shell* shell, const StreamType type, const uint32_t interval)
{
	if (s_streamThreadCreated)
	{
		//! The previous stream has signaled completion; wait for its thread to exit.
		(void)k_thread_join(&s_streamThread, K_FOREVER);
	}

	k_sem_reset(&s_stopSem);
	k_sem_reset(&s_doneSem);
	atomic_set(&s_stopRequested, 0);

	s_request.m_shell = shell;
	s_request.m_type = type;
	s_request.m_interval = interval;

	shell_set_bypass(shell, streamBypass);

	k_thread_create(&s_streamThread, s_streamStack, K_THREAD_STACK_SIZEOF(s_streamStack),
		streamThread, &s_request, NULL, NULL,
		STREAM_THREAD_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(&s_streamThread, "stream");
	s_streamThreadCreated = true;

	return 0;
}

/**
 * @brief Displays IMU identification and configuration information.
 * @retval 0 on success; a negative errno value if the IMU is unavailable.
 */
static int showImuInfo(const struct shell* shell, const size_t argumentCount, char** arguments)
{
	ARG_UNUSED(argumentCount);
	ARG_UNUSED(arguments);

	if (!checkImu(shell))
	{
		return -ENODEV;
	}

	shell_print(shell, "WHO_AM_I=0x%02X (%s), ODR 104 Hz, accel +-2 g, gyro 250 dps", s_imu->getWhoAmI(), s_imu->getChipName());
	return 0;
}

/**
 * @brief Reads and prints one IMU sample.
 * @retval 0 on success; a negative errno value if the IMU is unavailable or a read fails.
 */
static int readImu(const struct shell* shell, const size_t argumentCount, char** arguments)
{
	ARG_UNUSED(argumentCount);
	ARG_UNUSED(arguments);

	if (!checkImu(shell))
	{
		return -ENODEV;
	}

	Imu::Sample sample;
	auto result = s_imu->read(sample);
	if (result != 0)
	{
		shell_error(shell, "IMU read failed (%d)", result);
		return result;
	}

	printImuHeader(shell);
	printImuSample(shell, k_uptime_get_32(), sample);
	return 0;
}

/**
 * @brief Starts periodic IMU streaming.
 * @retval 0 on success; a negative errno value if validation fails.
 */
static int streamImu(const struct shell* shell, const size_t argumentCount, char** arguments)
{
	ARG_UNUSED(argumentCount);

	if (!checkImu(shell))
	{
		return -ENODEV;
	}

	uint32_t interval = 0;
	if (!parseInterval(shell, arguments[1], IMU_MIN_INTERVAL_MS, IMU_MAX_INTERVAL_MS, interval))
	{
		return -EINVAL;
	}

	shell_print(shell, "Streaming IMU every %u ms. Press ESC to stop.", interval);
	return startStream(shell, StreamType::Imu, interval);
}

/**
 * @brief Starts periodic microphone-level monitoring.
 * @retval 0 on success; a negative errno value if validation fails.
 */
static int monitorMic(const struct shell* shell, const size_t argumentCount, char** arguments)
{
	if (!checkMic(shell))
	{
		return -ENODEV;
	}

	uint32_t interval = MIC_DEFAULT_INTERVAL_MS;
	if ((argumentCount > 1) &&
		!parseInterval(shell, arguments[1], Mic::BLOCK_MS, MIC_MAX_INTERVAL_MS, interval))
	{
		return -EINVAL;
	}

	//! Round down to whole 100 ms blocks.
	interval = (interval / Mic::BLOCK_MS) * Mic::BLOCK_MS;

	shell_print(shell, "Monitoring mic every %u ms. Press ESC to stop.", interval);
	return startStream(shell, StreamType::Mic, interval);
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_imu,
	SHELL_CMD(info, NULL, "Show IMU chip information", showImuInfo),
	SHELL_CMD(read, NULL, "Read one accel/gyro/temperature sample", readImu),
	SHELL_CMD_ARG(stream, NULL,
		"Stream samples until ESC is pressed\n"
		"Usage: imu stream <interval_ms> (10 ~ 60000)",
		streamImu, 2, 0),
	SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(imu, &sub_imu, "IMU (LSM6DS3TR-C) commands", NULL);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_mic,
	SHELL_CMD_ARG(monitor, NULL,
		"Print the sound level until ESC is pressed\n"
		"Usage: mic monitor [interval_ms] (100 ~ 10000, multiple of 100, default 100)",
		monitorMic, 1, 1),
	SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(mic, &sub_mic, "PDM microphone commands", NULL);
