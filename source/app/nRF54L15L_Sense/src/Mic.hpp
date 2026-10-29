#pragma once

#include <zephyr/device.h>

#include <cstdint>

/**
 * @brief PDM microphone wrapper that reports the sound level of each captured block
 * @details Captures 16 kHz / 16 bit / mono PCM through the Zephyr DMIC API.
 *          One block holds 100 ms of audio, so Read() blocks for about 100 ms.
 *          Capturing runs only between Start() and Stop(); the driver stops by itself
 *          when blocks are not read, so keep calling Read() while started.
 */
class Mic
{
public:
	static constexpr uint32_t BLOCK_MS = 100;

	struct Level
	{
		int32_t peak;		// Peak absolute amplitude after DC removal (0 ~ 32767)
		int32_t rms;		// RMS amplitude after DC removal
		int32_t dc;			// DC offset of the block
		bool flat;			// true if every sample in the block has the same value
	};

	explicit Mic(const struct device& dmic)
	: m_dmic(dmic)
	{
	}

	/**
	 * @brief Configure the PDM peripheral (does not start capturing)
	 * @return 0 on success, negative errno otherwise
	 */
	int Initialize();

	/**
	 * @brief Start capturing and drop the start-up transient
	 * @return 0 on success, negative errno otherwise
	 */
	int Start();

	/**
	 * @brief Stop capturing and release the blocks left in the driver queue
	 */
	void Stop();

	/**
	 * @brief Wait for the next block and compute its level
	 * @return 0 on success, negative errno otherwise
	 */
	int Read(Level& level);

private:
	void Drain();

	const struct device& m_dmic;
	bool m_configured{false};
	bool m_running{false};
};
