/**
 * @file app_shell.cpp
 * @brief Shell command implementation for the IMU and microphone.
 * @date 2026-11-16
 * @author Sungsu Kim
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app_shell.hpp"
#include "gesture_classifier.hpp"

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>

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
constexpr size_t STREAM_STACK_SIZE		= 8192;
constexpr int STREAM_THREAD_PRIORITY	= 5;		// Lower than the shell thread (0)
constexpr int32_t STOP_TIMEOUT_MS		= 2000;		// Longest mic read (1000 ms) + margin

constexpr uint8_t KEY_ESC				= 0x1B;

enum class StreamType
{
	Imu,
	Gesture,
	Mic,
};

struct StreamRequest
{
    const struct shell* m_sh; //!< Shell associated with this stream.
    StreamType m_type; //!< Requested stream type.
    uint32_t m_interval; //!< Stream interval in milliseconds.
};

static Imu* s_imu = nullptr;
static Mic* s_mic = nullptr;
static bool s_imuReady = false;
static bool s_micReady = false;

K_THREAD_STACK_DEFINE(s_streamStack, STREAM_STACK_SIZE);
static struct k_thread s_streamThread;
static bool s_streamThreadCreated = false;
static StreamRequest s_request = {};

K_SEM_DEFINE(s_stopSem, 0, 1);		// Given by the bypass callback when ESC is received
K_SEM_DEFINE(s_doneSem, 0, 1);		// Given by the stream thread when it finishes
static atomic_t s_stopRequested = ATOMIC_INIT(0);

/**
 * @brief Wait until the deadline or until ESC is pressed
 * @retval true if ESC was pressed
 */
static bool waitForStop(int64_t deadline)
{
	const int64_t remaining = deadline - k_uptime_get();
	const k_timeout_t timeout = (remaining > 0) ? K_MSEC(remaining) : K_NO_WAIT;

	return k_sem_take(&s_stopSem, timeout) == 0;
}

/**
 * @brief Check whether a stop request has been raised.
 * @retval true if stop was requested, false otherwise
 */
static bool isStopRequested()
{
	return k_sem_take(&s_stopSem, K_NO_WAIT) == 0;
}

/**
 * @brief Check for a single ESC key press
 * @details Arrow and function keys also start with ESC (e.g. ESC [ A), so ESC followed
 *          by '[' or 'O' in the same chunk is treated as an escape sequence and ignored
 * @retval true when a standalone ESC key is present, false otherwise
 */
static bool hasEscKey(const uint8_t* data, size_t len)
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
 * @brief Receive every input byte while a stream is running (shell thread context).
 * @retval None
 */
static void streamBypass(const struct shell* sh, uint8_t* data, size_t len)
{
	if (!hasEscKey(data, len))
	{
		return;
	}

	atomic_set(&s_stopRequested, 1);
	k_sem_give(&s_stopSem);

	// Wait for the last output of the stream so that the prompt is printed after it
	(void)k_sem_take(&s_doneSem, K_MSEC(STOP_TIMEOUT_MS));
	shell_set_bypass(sh, NULL);
}

/**
 * @brief Parse and validate a stream interval.
 * @retval true when valid, false otherwise
 */
static bool parseInterval(const struct shell* sh, const char* arg, uint32_t min, uint32_t max, uint32_t& interval)
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

/**
 * @brief Check whether the IMU shell context is available.
 * @retval true when available, false otherwise
 */
static bool checkImu(const struct shell* sh)
{
	if ((s_imu == nullptr) || !s_imuReady)
	{
		shell_error(sh, "IMU is not available");
		return false;
	}
	return true;
}

/**
 * @brief Check whether the microphone shell context is available.
 * @retval true when available, false otherwise
 */
static bool checkMic(const struct shell* sh)
{
	if ((s_mic == nullptr) || !s_micReady)
	{
		shell_error(sh, "Mic is not available");
		return false;
	}
	return true;
}

/**
 * @brief Print the IMU stream column header.
 * @retval None
 */
static void printImuHeader(const struct shell* sh)
{
	shell_print(sh, "  time[ms] |  ax[mg]  ay[mg]  az[mg] | gx[mdps] gy[mdps] gz[mdps] | temp[C]");
}

/**
 * @brief Print one IMU sample.
 * @retval None
 */
static void printImuSample(const struct shell* sh, uint32_t timeMs, const Imu::Sample& sample)
{
	shell_print(sh, "%10u | %7d %7d %7d | %8d %8d %8d | %3d.%02d",
		timeMs,
		sample.m_accelMg[0], sample.m_accelMg[1], sample.m_accelMg[2],
		sample.m_gyroMdps[0], sample.m_gyroMdps[1], sample.m_gyroMdps[2],
		sample.m_tempCentiC / 100, std::abs(sample.m_tempCentiC % 100));
}

/**
 * @brief Convert an amplitude to dBFS in 0.1 dB unit (0 dBFS = full scale).
 * @retval The amplitude in 0.1 dBFS
 */
static int32_t toDb10(int32_t amplitude)
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
 * @retval true if stopped by ESC, false on error
 */
static bool runImuStream(const struct shell* sh, uint32_t interval)
{
	printImuHeader(sh);

	const int64_t start = k_uptime_get();
	int64_t next = start;
	uint32_t count = 0;

	for (;;)
	{
		Imu::Sample sample = {};
		auto ret = s_imu->read(sample);
		if (ret != 0)
		{
			shell_error(sh, "IMU read failed (%d)", ret);
			return false;
		}

		printImuSample(sh, static_cast<uint32_t>(k_uptime_get() - start), sample);
		++count;

		// Skip missed slots instead of bursting when printing takes longer than the interval
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

	shell_print(sh, "Stopped by ESC (%u samples)", count);
	return true;
}

/**
 * @brief Run live gesture inference until stopped.
 * @retval true if stopped by ESC, false on error
 */
static bool runGestureStream(const struct shell* sh)
{
	static_assert(GestureClassifier::AXIS_COUNT == 3, "Gesture model requires three IMU axes");
	static_assert(GestureClassifier::SAMPLE_COUNT > 0, "Gesture model window must not be empty");

	shell_print(sh, "Gesture inference: %u Hz, %u samples/window. Press ESC to stop.",
		EI_CLASSIFIER_FREQUENCY, static_cast<uint32_t>(GestureClassifier::SAMPLE_COUNT));

	std::array<float, GestureClassifier::VALUE_COUNT> ringBuffer = {};
	std::array<float, GestureClassifier::VALUE_COUNT> orderedWindow = {};
	size_t nextSample = 0;
	size_t samplesCollected = 0;
	size_t samplesSinceInference = 0;
	uint32_t inferenceCount = 0;
	int64_t next = k_uptime_get();

	for (;;)
	{
		Imu::Sample sample = {};
		const int ret = s_imu->read(sample);
		if (ret != 0)
		{
			shell_error(sh, "IMU read failed (%d)", ret);
			return false;
		}

		const size_t sampleOffset = nextSample * GestureClassifier::AXIS_COUNT;
		for (size_t axis = 0; axis < GestureClassifier::AXIS_COUNT; ++axis)
		{
			ringBuffer[sampleOffset + axis] = static_cast<float>(sample.m_accelMg[axis]);
		}
		nextSample = (nextSample + 1) % GestureClassifier::SAMPLE_COUNT;
		if (samplesCollected < GestureClassifier::SAMPLE_COUNT)
		{
			++samplesCollected;
		}

		if (samplesCollected == GestureClassifier::SAMPLE_COUNT &&
			(inferenceCount == 0 || ++samplesSinceInference >= GestureClassifier::INFERENCE_STEP))
		{
			for (size_t sampleIndex = 0; sampleIndex < GestureClassifier::SAMPLE_COUNT; ++sampleIndex)
			{
				const size_t sourceSample = (nextSample + sampleIndex) % GestureClassifier::SAMPLE_COUNT;
				for (size_t axis = 0; axis < GestureClassifier::AXIS_COUNT; ++axis)
				{
					orderedWindow[sampleIndex * GestureClassifier::AXIS_COUNT + axis] =
						ringBuffer[sourceSample * GestureClassifier::AXIS_COUNT + axis];
				}
			}

			GestureClassifier::Result result = {};
			const int classifyRet = GestureClassifier::Classify(
				orderedWindow.data(), orderedWindow.size(), result);
			if (classifyRet != 0)
			{
				shell_error(sh, "Gesture inference failed (%d)", classifyRet);
				return false;
			}

			const int confidence = static_cast<int>(result.m_confidence * 1000.0f + 0.5f);
			char output[192] = {};
			int outputLength = snprintk(output, sizeof(output),
				"window %u prediction=%s confidence=%d.%03d",
				++inferenceCount, result.m_prediction, confidence / 1000, confidence % 1000);
			if (outputLength < 0 || static_cast<size_t>(outputLength) >= sizeof(output))
			{
				shell_error(sh, "Gesture output does not fit on one line");
				return false;
			}

			for (size_t index = 0; index < GestureClassifier::CLASS_COUNT; ++index)
			{
				const int score = static_cast<int>(result.m_scores[index] * 1000.0f + 0.5f);
				const size_t remaining = sizeof(output) - static_cast<size_t>(outputLength);
				const int appended = snprintk(output + outputLength, remaining,
					" | %s=%d.%03d",
					result.m_labels[index], score / 1000, score % 1000);
				if (appended < 0 || static_cast<size_t>(appended) >= remaining)
				{
					shell_error(sh, "Gesture output does not fit on one line");
					return false;
				}
				outputLength += appended;
			}
			shell_print(sh, "%s", output);
			samplesSinceInference = 0;
		}

		next += EI_CLASSIFIER_INTERVAL_MS;
		const int64_t now = k_uptime_get();
		if (next < now)
		{
			next = now + EI_CLASSIFIER_INTERVAL_MS;
		}
		if (waitForStop(next))
		{
			break;
		}
	}

	shell_print(sh, "Gesture inference stopped after %u window(s)", inferenceCount);
	return true;
}

/**
 * @brief Print the mic level until ESC is pressed
 * @retval true if stopped by ESC, false on error
 */
static bool runMicMonitor(const struct shell* sh, uint32_t interval)
{
	// The level is computed per 100 ms block
	const uint32_t blocksPerReport = interval / Mic::BLOCK_MS;

	auto ret = s_mic->start();
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
			ret = s_mic->read(level);
			if (ret != 0)
			{
				s_mic->stop();
				shell_error(sh, "Mic read failed (%d)", ret);
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

	s_mic->stop();
	shell_print(sh, "Stopped by ESC");
	return true;
}

/**
 * @brief Execute a requested stream in a worker thread.
 * @retval None
 */
static void runStreamThread(void* p1, void* p2, void* p3)
{
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	const auto* request = static_cast<const StreamRequest*>(p1);

	if (request->m_type == StreamType::Imu)
	{
		(void)runImuStream(request->m_sh, request->m_interval);
	}
	else if (request->m_type == StreamType::Gesture)
	{
		(void)runGestureStream(request->m_sh);
	}
	else
	{
		(void)runMicMonitor(request->m_sh, request->m_interval);
	}

	k_sem_give(&s_doneSem);

	// Ended by an error: nobody waits in the bypass callback, so leave the bypass mode here.
	// The prompt shows up again with the next key input.
	if (!atomic_get(&s_stopRequested))
	{
		shell_print(request->m_sh, "Stream ended. Press Enter to continue.");
		shell_set_bypass(request->m_sh, NULL);
	}
}

/**
 * @brief Start the stream thread and pass every key input to streamBypass().
 * @details The command returns right away. The shell thread has to keep running to
 *          receive ESC, so the stream itself runs on a separate thread.
 * @retval 0 on success
 */
static int startStream(const struct shell* sh, StreamType type, uint32_t interval)
{
	if (s_streamThreadCreated)
	{
		// The previous stream has already signaled done; wait until its thread has exited
		(void)k_thread_join(&s_streamThread, K_FOREVER);
	}

	k_sem_reset(&s_stopSem);
	k_sem_reset(&s_doneSem);
	atomic_set(&s_stopRequested, 0);

	s_request.m_sh = sh;
	s_request.m_type = type;
	s_request.m_interval = interval;

	shell_set_bypass(sh, streamBypass);

	k_thread_create(&s_streamThread, s_streamStack, K_THREAD_STACK_SIZEOF(s_streamStack),
		runStreamThread, &s_request, NULL, NULL,
		STREAM_THREAD_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(&s_streamThread, "stream");
	s_streamThreadCreated = true;

	return 0;
}

/**
 * @brief Handle the imu info shell command.
 * @retval 0 on success, negative errno otherwise
 */
static int cmdImuInfo(const struct shell* sh, size_t argc, char** argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (!checkImu(sh))
	{
		return -ENODEV;
	}

	shell_print(sh, "WHO_AM_I=0x%02X (%s), ODR 104 Hz, accel +-2 g, gyro 250 dps", s_imu->getWhoAmI(), s_imu->getChipName());
	return 0;
}

/**
 * @brief Handle the imu read shell command.
 * @retval 0 on success, negative errno otherwise
 */
static int cmdImuRead(const struct shell* sh, size_t argc, char** argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (!checkImu(sh))
	{
		return -ENODEV;
	}

	Imu::Sample sample = {};
	auto ret = s_imu->read(sample);
	if (ret != 0)
	{
		shell_error(sh, "IMU read failed (%d)", ret);
		return ret;
	}

	printImuHeader(sh);
	printImuSample(sh, k_uptime_get_32(), sample);
	return 0;
}

/**
 * @brief Handle the imu stream shell command.
 * @retval 0 on success, negative errno otherwise
 */
static int cmdImuStream(const struct shell* sh, size_t argc, char** argv)
{
	ARG_UNUSED(argc);

	if (!checkImu(sh))
	{
		return -ENODEV;
	}

	uint32_t interval = 0;
	if (!parseInterval(sh, argv[1], IMU_MIN_INTERVAL_MS, IMU_MAX_INTERVAL_MS, interval))
	{
		return -EINVAL;
	}

	shell_print(sh, "Streaming IMU every %u ms. Press ESC to stop.", interval);
	return startStream(sh, StreamType::Imu, interval);
}

/**
 * @brief Handle the imu classify shell command.
 * @retval 0 on success, negative errno otherwise
 */
static int cmdImuClassify(const struct shell* sh, size_t argc, char** argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (!checkImu(sh))
	{
		return -ENODEV;
	}

	shell_print(sh, "Starting live gesture inference. Press ESC to stop.");
	return startStream(sh, StreamType::Gesture, EI_CLASSIFIER_INTERVAL_MS);
}

/**
 * @brief Handle the mic monitor shell command.
 * @retval 0 on success, negative errno otherwise
 */
static int cmdMicMonitor(const struct shell* sh, size_t argc, char** argv)
{
	if (!checkMic(sh))
	{
		return -ENODEV;
	}

	uint32_t interval = MIC_DEFAULT_INTERVAL_MS;
	if ((argc > 1) && !parseInterval(sh, argv[1], Mic::BLOCK_MS, MIC_MAX_INTERVAL_MS, interval))
	{
		return -EINVAL;
	}

	// Round down to whole 100 ms blocks
	interval = (interval / Mic::BLOCK_MS) * Mic::BLOCK_MS;

	shell_print(sh, "Monitoring mic every %u ms. Press ESC to stop.", interval);
	return startStream(sh, StreamType::Mic, interval);
}

SHELL_STATIC_SUBCMD_SET_CREATE(s_subImu,
	SHELL_CMD(info, NULL, "Show IMU chip information", cmdImuInfo),
	SHELL_CMD(read, NULL, "Read one accel/gyro/temperature sample", cmdImuRead),
	SHELL_CMD(classify, NULL, "Classify live gestures until ESC is pressed", cmdImuClassify),
	SHELL_CMD_ARG(stream, NULL,
		"Stream samples until ESC is pressed\n"
		"Usage: imu stream <interval_ms> (10 ~ 60000)",
		cmdImuStream, 2, 0),
	SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(imu, &s_subImu, "IMU (LSM6DS3TR-C) commands", NULL);

SHELL_STATIC_SUBCMD_SET_CREATE(s_subMic,
	SHELL_CMD_ARG(monitor, NULL,
		"Print the sound level until ESC is pressed\n"
		"Usage: mic monitor [interval_ms] (100 ~ 10000, multiple of 100, default 100)",
		cmdMicMonitor, 1, 1),
	SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(mic, &s_subMic, "PDM microphone commands", NULL);

namespace AppShell
{
	/**
	 * @brief Register the objects used by the shell commands.
	 * @retval None
	 */
	void Initialize(Imu& imu, bool imuReady, Mic& mic, bool micReady)
	{
		s_imu = &imu;
		s_imuReady = imuReady;
		s_mic = &mic;
		s_micReady = micReady;
	}
}
