#pragma once

/**
 * @file mic.hpp
 * @brief PDM microphone wrapper for captured sound levels.
 * @date 2026-11-16
 * @author Sungsu Kim
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>

#include <cstdint>

/**
 * @brief PDM microphone wrapper that reports the sound level of each captured block.
 * @details Captures 16 kHz / 16 bit / mono PCM through the Zephyr DMIC API.
 *          One block holds 100 ms of audio, so read() blocks for about 100 ms.
 *          Capturing runs only between start() and stop(); the driver stops by itself
 *          when blocks are not read, so keep calling read() while started.
 */
class Mic
{
public:
    static constexpr uint32_t BLOCK_MS = 100;

    struct Level
    {
        int32_t m_peak; //!< Peak absolute amplitude after DC removal (0 ~ 32767).
        int32_t m_rms; //!< RMS amplitude after DC removal.
        int32_t m_dc; //!< DC offset of the block.
        bool m_flat; //!< True if every sample in the block has the same value.
    };

    /**
     * @brief Construct a microphone wrapper for a DMIC device.
     * @retval None
     */
    explicit Mic(const struct device& dmic)
    : m_dmic(dmic)
    {
    }

    int initialize();
    int start();
    void stop();
    int read(Level& level);

private:
    void drain();

    const struct device& m_dmic; //!< DMIC device reference.
    bool m_configured{}; //!< True after PDM configuration succeeds.
    bool m_running{}; //!< True while capture is running.
};
