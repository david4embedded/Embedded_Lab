/**
 * @file spi.hpp
 * @brief Declares the SPI driver wrapper used by the loopback example.
 * @date 2026-04-22
 * @author Sungsu Kim
 */

#pragma once

#include <span>

#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/device_runtime.h>

/**
 * @brief Provides synchronous and asynchronous access to a Zephyr SPI device.
 */
class Spi
{
public:
    using Buffer = std::span<uint8_t>;

    explicit Spi(spi_dt_spec &spec, k_poll_event *asyncEvent = nullptr);

    ~Spi();

    Spi(const Spi &) = delete;
    Spi &operator=(const Spi &) = delete;

    int initialize();
    int transceive(const Buffer &tx, const Buffer &rx);
    int transceiveAsync(const Buffer &tx, const Buffer &rx);
    int waitComplete(uint32_t timeoutMs = 2000U);

private:
    spi_buf_set populateSpiBufSingle(spi_buf &spiBuffer);
    spi_buf toSpiBuffer(const Buffer &buffer);

    spi_dt_spec &m_spec;
    k_poll_event *m_asyncEvent{nullptr};
    spi_buf m_asyncSpiBufferTx{};
    spi_buf m_asyncSpiBufferRx{};
    spi_buf_set m_asyncSpiBuffersTx{};
    spi_buf_set m_asyncSpiBuffersRx{};
    bool m_initialized{false};
};
