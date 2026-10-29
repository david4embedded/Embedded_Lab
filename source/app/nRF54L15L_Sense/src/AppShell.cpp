#include "AppShell.hpp"

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace
{
constexpr uint32_t IMU_MIN_INTERVAL_MS	= 10;		// IMU ODR is 104 Hz
constexpr uint32_t IMU_MAX_INTERVAL_MS	= 60000;
constexpr uint32_t MIC_DEFAULT_INTERVAL_MS	= Mic::BLOCK_MS;
constexpr uint32_t MIC_MAX_INTERVAL_MS	= 10000;

// Level meter: one character per 2 dB from -90 dBFS to -20 dBFS
constexpr int32_t METER_MIN_DB10		= -900;		// 0.1 dB unit
constexpr int32_t METER_MAX_DB10		= -200;
constexpr int32_t METER_DB10_PER_CHAR	= 20;
constexpr int32_t METER_WIDTH			= (METER_MAX_DB10 - METER_MIN_DB10) / METER_DB10_PER_CHAR;
constexpr int32_t FULL_SCALE			= 32768;
constexpr int32_t CLIP_LEVEL			= 32000;

// Stream thread
constexpr size_t STREAM_STACK_SIZE		= 4096;
constexpr int STREAM_THREAD_PRIORITY	= 5;		// Lower than the shell thread (0)
constexpr int32_t STOP_TIMEOUT_MS		= 2000;		// Longest mic read (1000 ms) + margin

constexpr uint8_t KEY_ESC				= 0x1B;

enum class StreamType
{
	Imu,
	Mic,
};

struct StreamRequest
{
	const struct shell* sh;
	StreamType type;
	uint32_t interval;
};

Imu* s_imu = nullptr;
Mic* s_mic = nullptr;
bool s_imuReady = false;
bool s_micReady = false;

K_THREAD_STACK_DEFINE(s_streamStack, STREAM_STACK_SIZE);
struct k_thread s_streamThread;
bool s_streamThreadCreated = false;
StreamRequest s_request = {};

K_SEM_DEFINE(s_stopSem, 0, 1);		// Given by the bypass callback when ESC is received
K_SEM_DEFINE(s_doneSem, 0, 1);		// Given by the stream thread when it finishes
atomic_t s_stopRequested = ATOMIC_INIT(0);

/**
 * @brief Wait until the deadline or until ESC is pressed
 * @return true if ESC was pressed
 */
bool WaitForStop(int64_t deadline)
{
	const int64_t remaining = deadline - k_uptime_get();
	const k_timeout_t timeout = (remaining > 0) ? K_MSEC(remaining) : K_NO_WAIT;

	return k_sem_take(&s_stopSem, timeout) == 0;
}

bool IsStopRequested()
{
	return k_sem_take(&s_stopSem, K_NO_WAIT) == 0;
}

/**
 * @brief Check for a single ESC key press
 * @details Arrow and function keys also start with ESC (e.g. ESC [ A), so ESC followed
 *          by '[' or 'O' in the same chunk is treated as an escape sequence and ignored
 */
bool HasEscKey(const uint8_t* data, size_t len)
{
	for (size_t i = 0; i < len; ++i)
	{
		if (data[i] != KEY_ESC)
		{
			continue;
		}

		const bool isSequence = ((i + 1) < len) && ((data[i + 1] == '[') || (data[i + 1] == 'O'));
		if (!isSequence)
		{
			return true;
		}
	}
	return false;
}

/**
 * @brief Receives every input byte while a stream is running (shell thread context)
 */
void StreamBypass(const struct shell* sh, uint8_t* data, size_t len)
{
	if (!HasEscKey(data, len))
	{
		return;
	}

	atomic_set(&s_stopRequested, 1);
	k_sem_give(&s_stopSem);

	// Wait for the last output of the stream so that the prompt is printed after it
	(void)k_sem_take(&s_doneSem, K_MSEC(STOP_TIMEOUT_MS));
	shell_set_bypass(sh, NULL);
}

bool ParseInterval(const struct shell* sh, const char* arg, uint32_t min, uint32_t max, uint32_t& interval)
{
	int err = 0;
	auto value = shell_strtoul(arg, 10, &err);

	if ((err != 0) || (value < min) || (value > max))
	{
		shell_error(sh, "Invalid interval '%s' (%u ~ %u ms)", arg, min, max);
		return false;
	}

	interval = static_cast<uint32_t>(value);
	return true;
}

bool CheckImu(const struct shell* sh)
{
	if ((s_imu == nullptr) || !s_imuReady)
	{
		shell_error(sh, "IMU is not available");
		return false;
	}
	return true;
}

bool CheckMic(const struct shell* sh)
{
	if ((s_mic == nullptr) || !s_micReady)
	{
		shell_error(sh, "Mic is not available");
		return false;
	}
	return true;
}

void PrintImuHeader(const struct shell* sh)
{
	shell_print(sh, "  time[ms] |  ax[mg]  ay[mg]  az[mg] | gx[mdps] gy[mdps] gz[mdps] | temp[C]");
}

void PrintImuSample(const struct shell* sh, uint32_t timeMs, const Imu::Sample& sample)
{
	shell_print(sh, "%10u | %7d %7d %7d | %8d %8d %8d | %3d.%02d",
		timeMs,
		sample.accel_mg[0], sample.accel_mg[1], sample.accel_mg[2],
		sample.gyro_mdps[0], sample.gyro_mdps[1], sample.gyro_mdps[2],
		sample.temp_centi_c / 100, std::abs(sample.temp_centi_c % 100));
}

/**
 * @brief Convert an amplitude to dBFS in 0.1 dB unit (0 dBFS = full scale)
 */
int32_t ToDb10(int32_t amplitude)
{
	if (amplitude <= 0)
	{
		return METER_MIN_DB10;
	}

	const double db = 20.0 * std::log10(static_cast<double>(amplitude) / FULL_SCALE);
	return std::max(static_cast<int32_t>(std::lround(db * 10.0)), METER_MIN_DB10);
}

/**
 * @brief Stream IMU samples until ESC is pressed
 * @return true if stopped by ESC, false on error
 */
bool RunImuStream(const struct shell* sh, uint32_t interval)
{
	PrintImuHeader(sh);

	const int64_t start = k_uptime_get();
	int64_t next = start;
	uint32_t count = 0;

	for (;;)
	{
		Imu::Sample sample = {};
		auto ret = s_imu->Read(sample);
		if (ret != 0)
		{
			shell_error(sh, "IMU read failed (%d)", ret);
			return false;
		}

		PrintImuSample(sh, static_cast<uint32_t>(k_uptime_get() - start), sample);
		++count;

		// Skip missed slots instead of bursting when printing takes longer than the interval
		next += interval;
		const int64_t now = k_uptime_get();
		if (next < now)
		{
			next = now;
		}

		if (WaitForStop(next))
		{
			break;
		}
	}

	shell_print(sh, "Stopped by ESC (%u samples)", count);
	return true;
}

/**
 * @brief Print the mic level until ESC is pressed
 * @return true if stopped by ESC, false on error
 */
bool RunMicMonitor(const struct shell* sh, uint32_t interval)
{
	// The level is computed per 100 ms block
	const uint32_t blocksPerReport = interval / Mic::BLOCK_MS;

	auto ret = s_mic->Start();
	if (ret != 0)
	{
		shell_error(sh, "Mic start failed (%d)", ret);
		return false;
	}

	shell_print(sh, "Meter range: %d ~ %d dBFS, one '#' per %d dB", METER_MIN_DB10 / 10, METER_MAX_DB10 / 10, METER_DB10_PER_CHAR / 10);
	shell_print(sh, "  time[ms] |  peak |   rms | rms[dBFS] | level");

	const int64_t start = k_uptime_get();
	char meter[METER_WIDTH + 1] = {0};
	bool stopped = false;

	while (!stopped)
	{
		int32_t peak = 0;
		double sumSquare = 0.0;
		bool flat = true;

		for (uint32_t i = 0; i < blocksPerReport; ++i)
		{
			Mic::Level level = {};
			ret = s_mic->Read(level);
			if (ret != 0)
			{
				s_mic->Stop();
				shell_error(sh, "Mic read failed (%d)", ret);
				return false;
			}

			peak = std::max(peak, level.peak);
			sumSquare += static_cast<double>(level.rms) * level.rms;
			flat = flat && level.flat;

			if (IsStopRequested())
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
		const int32_t db10 = ToDb10(rms);
		const int32_t bars = std::clamp((db10 - METER_MIN_DB10) / METER_DB10_PER_CHAR, int32_t{0}, METER_WIDTH);

		for (int32_t i = 0; i < METER_WIDTH; ++i)
		{
			meter[i] = (i < bars) ? '#' : '.';
		}

		// Format with one decimal place without float printf support; keep the sign for -0.x
		char dbText[16] = {0};
		snprintk(dbText, sizeof(dbText), "%s%d.%d", (db10 < 0) ? "-" : "", std::abs(db10) / 10, std::abs(db10) % 10);

		shell_print(sh, "%10u | %5d | %5d | %9s | %s%s%s",
			static_cast<uint32_t>(k_uptime_get() - start),
			peak, rms,
			dbText,
			meter,
			(peak >= CLIP_LEVEL) ? " CLIP" : "",
			flat ? " FLAT (no data?)" : "");
	}

	s_mic->Stop();
	shell_print(sh, "Stopped by ESC");
	return true;
}

void StreamThread(void* p1, void* p2, void* p3)
{
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	const auto* request = static_cast<const StreamRequest*>(p1);

	if (request->type == StreamType::Imu)
	{
		(void)RunImuStream(request->sh, request->interval);
	}
	else
	{
		(void)RunMicMonitor(request->sh, request->interval);
	}

	k_sem_give(&s_doneSem);

	// Ended by an error: nobody waits in the bypass callback, so leave the bypass mode here.
	// The prompt shows up again with the next key input.
	if (!atomic_get(&s_stopRequested))
	{
		shell_print(request->sh, "Stream ended. Press Enter to continue.");
		shell_set_bypass(request->sh, NULL);
	}
}

/**
 * @brief Start the stream thread and pass every key input to StreamBypass()
 * @details The command returns right away. The shell thread has to keep running to
 *          receive ESC, so the stream itself runs on a separate thread.
 */
int StartStream(const struct shell* sh, StreamType type, uint32_t interval)
{
	if (s_streamThreadCreated)
	{
		// The previous stream has already signaled done; wait until its thread has exited
		(void)k_thread_join(&s_streamThread, K_FOREVER);
	}

	k_sem_reset(&s_stopSem);
	k_sem_reset(&s_doneSem);
	atomic_set(&s_stopRequested, 0);

	s_request.sh = sh;
	s_request.type = type;
	s_request.interval = interval;

	shell_set_bypass(sh, StreamBypass);

	k_thread_create(&s_streamThread, s_streamStack, K_THREAD_STACK_SIZEOF(s_streamStack),
		StreamThread, &s_request, NULL, NULL,
		STREAM_THREAD_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(&s_streamThread, "stream");
	s_streamThreadCreated = true;

	return 0;
}

int CmdImuInfo(const struct shell* sh, size_t argc, char** argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (!CheckImu(sh))
	{
		return -ENODEV;
	}

	shell_print(sh, "WHO_AM_I=0x%02X (%s), ODR 104 Hz, accel +-2 g, gyro 250 dps", s_imu->WhoAmI(), s_imu->ChipName());
	return 0;
}

int CmdImuRead(const struct shell* sh, size_t argc, char** argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (!CheckImu(sh))
	{
		return -ENODEV;
	}

	Imu::Sample sample = {};
	auto ret = s_imu->Read(sample);
	if (ret != 0)
	{
		shell_error(sh, "IMU read failed (%d)", ret);
		return ret;
	}

	PrintImuHeader(sh);
	PrintImuSample(sh, k_uptime_get_32(), sample);
	return 0;
}

int CmdImuStream(const struct shell* sh, size_t argc, char** argv)
{
	ARG_UNUSED(argc);

	if (!CheckImu(sh))
	{
		return -ENODEV;
	}

	uint32_t interval = 0;
	if (!ParseInterval(sh, argv[1], IMU_MIN_INTERVAL_MS, IMU_MAX_INTERVAL_MS, interval))
	{
		return -EINVAL;
	}

	shell_print(sh, "Streaming IMU every %u ms. Press ESC to stop.", interval);
	return StartStream(sh, StreamType::Imu, interval);
}

int CmdMicMonitor(const struct shell* sh, size_t argc, char** argv)
{
	if (!CheckMic(sh))
	{
		return -ENODEV;
	}

	uint32_t interval = MIC_DEFAULT_INTERVAL_MS;
	if ((argc > 1) && !ParseInterval(sh, argv[1], Mic::BLOCK_MS, MIC_MAX_INTERVAL_MS, interval))
	{
		return -EINVAL;
	}

	// Round down to whole 100 ms blocks
	interval = (interval / Mic::BLOCK_MS) * Mic::BLOCK_MS;

	shell_print(sh, "Monitoring mic every %u ms. Press ESC to stop.", interval);
	return StartStream(sh, StreamType::Mic, interval);
}
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_imu,
	SHELL_CMD(info, NULL, "Show IMU chip information", CmdImuInfo),
	SHELL_CMD(read, NULL, "Read one accel/gyro/temperature sample", CmdImuRead),
	SHELL_CMD_ARG(stream, NULL,
		"Stream samples until ESC is pressed\n"
		"Usage: imu stream <interval_ms> (10 ~ 60000)",
		CmdImuStream, 2, 0),
	SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(imu, &sub_imu, "IMU (LSM6DS3TR-C) commands", NULL);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_mic,
	SHELL_CMD_ARG(monitor, NULL,
		"Print the sound level until ESC is pressed\n"
		"Usage: mic monitor [interval_ms] (100 ~ 10000, multiple of 100, default 100)",
		CmdMicMonitor, 1, 1),
	SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(mic, &sub_mic, "PDM microphone commands", NULL);

namespace AppShell
{
	void Initialize(Imu& imu, bool imuReady, Mic& mic, bool micReady)
	{
		s_imu = &imu;
		s_imuReady = imuReady;
		s_mic = &mic;
		s_micReady = micReady;
	}
}
