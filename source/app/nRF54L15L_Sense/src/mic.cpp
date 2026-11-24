/**
 * @file mic.cpp
 * @brief Implements PDM capture and sound-level calculation.
 * @date 2026-10-28
 * @author Sungsu Kim
 */

#include "mic.hpp"

#include <zephyr/audio/dmic.h>
#include <zephyr/kernel.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <span>

static constexpr uint32_t SAMPLE_RATE = 16000;
static constexpr uint8_t SAMPLE_BIT_WIDTH = 16;
static constexpr uint32_t MILLISECONDS_PER_SECOND = 1000;
static constexpr uint32_t BLOCK_SIZE =
	sizeof(int16_t) * (SAMPLE_RATE * Mic::BLOCK_MS / MILLISECONDS_PER_SECOND);
static constexpr uint32_t BLOCK_COUNT = 4;
static constexpr int32_t READ_TIMEOUT_MS = 1000;
static constexpr int32_t DISCARD_BLOCKS = 2;
static constexpr int32_t STOP_WAIT_MS = 20;
static constexpr enum pdm_lr MIC_CHANNEL = PDM_CHAN_LEFT;
static constexpr uint32_t MIN_PDM_CLOCK_FREQUENCY_HZ = 1000000;
static constexpr uint32_t MAX_PDM_CLOCK_FREQUENCY_HZ = 3500000;
static constexpr uint32_t MIN_PDM_CLOCK_DUTY_PERCENT = 40;
static constexpr uint32_t MAX_PDM_CLOCK_DUTY_PERCENT = 60;
static constexpr uint32_t DMIC_STREAM_COUNT = 1;
static constexpr uint32_t DMIC_CHANNEL_COUNT = 1;
static constexpr uint32_t SAMPLE_BLOCK_ALIGNMENT_BYTES = 4;

static_assert(BLOCK_SIZE == 3200U);

K_MEM_SLAB_DEFINE_STATIC(s_micSlab, BLOCK_SIZE, BLOCK_COUNT, SAMPLE_BLOCK_ALIGNMENT_BYTES);

/**
 * @brief Configures the PDM peripheral without starting capture.
 * @retval 0 on success; a negative errno value otherwise.
 */
int Mic::initialize()
{
	if (!device_is_ready(&m_dmic))
	{
		return -ENODEV;
	}

	struct pcm_stream_cfg stream;
	std::memset(&stream, 0, sizeof(stream));
	stream.pcm_rate = SAMPLE_RATE;
	stream.pcm_width = SAMPLE_BIT_WIDTH;
	stream.block_size = BLOCK_SIZE;
	stream.mem_slab = &s_micSlab;

	struct dmic_cfg config;
	std::memset(&config, 0, sizeof(config));
	config.io.min_pdm_clk_freq = MIN_PDM_CLOCK_FREQUENCY_HZ;
	config.io.max_pdm_clk_freq = MAX_PDM_CLOCK_FREQUENCY_HZ;
	config.io.min_pdm_clk_dc = MIN_PDM_CLOCK_DUTY_PERCENT;
	config.io.max_pdm_clk_dc = MAX_PDM_CLOCK_DUTY_PERCENT;
	config.streams = &stream;
	config.channel.req_num_streams = DMIC_STREAM_COUNT;
	config.channel.req_num_chan = DMIC_CHANNEL_COUNT;
	config.channel.req_chan_map_lo = dmic_build_channel_map(0, 0, MIC_CHANNEL);

	auto result = dmic_configure(&m_dmic, &config);
	m_configured = (result == 0);
	return result;
}

/**
 * @brief Starts capture and discards the PDM filter start-up transient.
 * @retval 0 on success; a negative errno value otherwise.
 */
int Mic::start()
{
	if (!m_configured)
	{
		return -EIO;
	}
	if (m_running)
	{
		return 0;
	}

	auto result = dmic_trigger(&m_dmic, DMIC_TRIGGER_START);
	if (result != 0)
	{
		return result;
	}
	m_running = true;

	for (int32_t index = 0; index < DISCARD_BLOCKS; ++index)
	{
		void* buffer = nullptr;
		size_t size = 0;

		result = dmic_read(&m_dmic, 0, &buffer, &size, READ_TIMEOUT_MS);
		if (result != 0)
		{
			stop();
			return result;
		}
		k_mem_slab_free(&s_micSlab, buffer);
	}

	return 0;
}

/**
 * @brief Stops capture and releases queued blocks.
 * @retval None
 */
void Mic::stop()
{
	if (!m_running)
	{
		return;
	}

	(void)dmic_trigger(&m_dmic, DMIC_TRIGGER_STOP);
	k_msleep(STOP_WAIT_MS);
	drain();
	m_running = false;
}

/**
 * @brief Reads the next PCM block and computes its sound level.
 * @retval 0 on success; a negative errno value otherwise.
 */
int Mic::read(Level& level)
{
	if (!m_running)
	{
		return -EIO;
	}

	void* buffer = nullptr;
	size_t size = 0;

	auto result = dmic_read(&m_dmic, 0, &buffer, &size, READ_TIMEOUT_MS);
	if (result != 0)
	{
		return result;
	}

	const auto* samples = static_cast<const int16_t*>(buffer);
	const size_t count = size / sizeof(int16_t);
	const std::span<const int16_t> sampleRange(samples, count);
	const int16_t firstSample = (count > 0) ? samples[0] : 0;

	int64_t sum = 0;
	bool flat = true;
	for (const int16_t sample : sampleRange)
	{
		sum += sample;
		if (sample != firstSample)
		{
			flat = false;
		}
	}

	const int32_t dc = (count > 0) ? static_cast<int32_t>(sum / static_cast<int64_t>(count)) : 0;

	int32_t peak = 0;
	double sumSquare = 0.0;
	for (const int16_t sample : sampleRange)
	{
		const int32_t ac = sample - dc;
		peak = std::max(peak, std::abs(ac));
		sumSquare += static_cast<double>(ac) * ac;
	}

	k_mem_slab_free(&s_micSlab, buffer);

	level.m_peak = peak;
	level.m_rms = (count > 0) ? static_cast<int32_t>(std::sqrt(sumSquare / count)) : 0;
	level.m_dc = dc;
	level.m_flat = flat;

	return 0;
}

/**
 * @brief Releases blocks remaining in the driver queue.
 * @retval None
 */
void Mic::drain()
{
	void* buffer = nullptr;
	size_t size = 0;

	while (dmic_read(&m_dmic, 0, &buffer, &size, 0) == 0)
	{
		k_mem_slab_free(&s_micSlab, buffer);
	}
}
