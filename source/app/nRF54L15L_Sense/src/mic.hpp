/**
 * @file mic.hpp
 * @brief Declares a PDM microphone wrapper that computes sound levels.
 * @date 2026-10-28
 * @author Sungsu Kim
 */

#pragma once

#include <zephyr/device.h>

#include <cstdint>

/**
 * @brief Wraps PDM capture and reports the sound level of each captured block.
 * @details Captures 16 kHz / 16 bit / mono PCM through the Zephyr DMIC API.
 *          One block holds 100 ms of audio, so read() blocks for about 100 ms.
 *          Capturing runs only between start() and stop(); keep reading while started.
 */
class Mic
{
public:
	static constexpr uint32_t BLOCK_MS = 100;

	struct Level
	{
		int32_t m_peak{};	//!< Peak absolute amplitude after DC removal (0 to 32767).
		int32_t m_rms{};	//!< RMS amplitude after DC removal.
		int32_t m_dc{};		//!< DC offset of the block.
		bool m_flat{};		//!< Whether all samples in the block have the same value.
	};

	/**
	 * @brief Stores the device used by the microphone.
	 * @retval None
	 */
	explicit Mic(const struct device& dmic)
	: m_dmic(dmic)
	{
	}

	/**
	 * @brief Configures the PDM peripheral without starting capture.
	 * @retval 0 on success; a negative errno value otherwise.
	 */
	int initialize();

	/**
	 * @brief Starts capture and discards the start-up transient.
	 * @retval 0 on success; a negative errno value otherwise.
	 */
	int start();

	/**
	 * @brief Stops capture and releases queued blocks.
	 * @retval None
	 */
	void stop();

	/**
	 * @brief Waits for the next block and computes its sound level.
	 * @retval 0 on success; a negative errno value otherwise.
	 */
	int read(Level& level);

private:
	/**
	 * @brief Releases blocks remaining in the driver queue.
	 * @retval None
	 */
	void drain();

	const struct device& m_dmic;	//!< PDM microphone device.
	bool m_configured{};			//!< Whether the PDM configuration succeeded.
	bool m_running{};				//!< Whether capture is currently running.
};
