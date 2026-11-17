#include "Mic.hpp"

#include <zephyr/audio/dmic.h>
#include <zephyr/kernel.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>

namespace
{
	constexpr uint32_t SAMPLE_RATE		= 16000;
	constexpr uint8_t SAMPLE_BIT_WIDTH	= 16;
	constexpr uint32_t BLOCK_SIZE		= sizeof(int16_t) * (SAMPLE_RATE * Mic::BLOCK_MS / 1000);	// mono
	constexpr uint32_t BLOCK_COUNT		= 4;
	constexpr int32_t READ_TIMEOUT_MS	= 1000;
	constexpr int DISCARD_BLOCKS		= 2;	// Skip the start-up transient of the PDM filter
	constexpr int32_t STOP_WAIT_MS		= 20;	// Time for the driver to handle the STOPPED event

	// Change to PDM_CHAN_RIGHT if the level stays flat while talking near the mic
	constexpr enum pdm_lr MIC_CHANNEL	= PDM_CHAN_LEFT;
}

K_MEM_SLAB_DEFINE_STATIC(s_micSlab, BLOCK_SIZE, BLOCK_COUNT, 4);

int Mic::Initialize()
{
	if (!device_is_ready(&m_dmic))
	{
		return -ENODEV;
	}

	struct pcm_stream_cfg stream = {};
	stream.pcm_rate = SAMPLE_RATE;
	stream.pcm_width = SAMPLE_BIT_WIDTH;
	stream.block_size = BLOCK_SIZE;
	stream.mem_slab = &s_micSlab;

	struct dmic_cfg cfg = {};
	cfg.io.min_pdm_clk_freq = 1000000;
	cfg.io.max_pdm_clk_freq = 3500000;
	cfg.io.min_pdm_clk_dc = 40;
	cfg.io.max_pdm_clk_dc = 60;
	cfg.streams = &stream;
	cfg.channel.req_num_streams = 1;
	cfg.channel.req_num_chan = 1;
	cfg.channel.req_chan_map_lo = dmic_build_channel_map(0, 0, MIC_CHANNEL);

	auto ret = dmic_configure(&m_dmic, &cfg);
	m_configured = (ret == 0);
	return ret;
}

int Mic::Start()
{
	if (!m_configured)
	{
		return -EIO;
	}
	if (m_running)
	{
		return 0;
	}

	auto ret = dmic_trigger(&m_dmic, DMIC_TRIGGER_START);
	if (ret != 0)
	{
		return ret;
	}
	m_running = true;

	for (int i = 0; i < DISCARD_BLOCKS; ++i)
	{
		void* buffer = nullptr;
		size_t size = 0;

		ret = dmic_read(&m_dmic, 0, &buffer, &size, READ_TIMEOUT_MS);
		if (ret != 0)
		{
			Stop();
			return ret;
		}
		k_mem_slab_free(&s_micSlab, buffer);
	}

	return 0;
}

void Mic::Stop()
{
	if (!m_running)
	{
		return;
	}

	(void)dmic_trigger(&m_dmic, DMIC_TRIGGER_STOP);
	k_msleep(STOP_WAIT_MS);
	Drain();
	m_running = false;
}

void Mic::Drain()
{
	// Blocks completed before STOP stay in the driver queue; free them so the next Start() gets fresh audio
	void* buffer = nullptr;
	size_t size = 0;

	while (dmic_read(&m_dmic, 0, &buffer, &size, 0) == 0)
	{
		k_mem_slab_free(&s_micSlab, buffer);
	}
}

int Mic::Read(Level& level)
{
	if (!m_running)
	{
		return -EIO;
	}

	void* buffer = nullptr;
	size_t size = 0;

	auto ret = dmic_read(&m_dmic, 0, &buffer, &size, READ_TIMEOUT_MS);
	if (ret != 0)
	{
		return ret;
	}

	const auto* samples = static_cast<const int16_t*>(buffer);
	const size_t count = size / sizeof(int16_t);

	int64_t sum = 0;
	bool flat = true;
	for (size_t i = 0; i < count; ++i)
	{
		sum += samples[i];
		if (samples[i] != samples[0])
		{
			flat = false;
		}
	}

	const int32_t dc = (count > 0) ? static_cast<int32_t>(sum / static_cast<int64_t>(count)) : 0;

	int32_t peak = 0;
	double sumSquare = 0.0;
	for (size_t i = 0; i < count; ++i)
	{
		const int32_t ac = samples[i] - dc;
		peak = std::max(peak, std::abs(ac));
		sumSquare += static_cast<double>(ac) * ac;
	}

	k_mem_slab_free(&s_micSlab, buffer);

	level.peak = peak;
	level.rms = (count > 0) ? static_cast<int32_t>(std::sqrt(sumSquare / count)) : 0;
	level.dc = dc;
	level.flat = flat;

	return 0;
}
