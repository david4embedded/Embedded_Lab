/**
 * @file spi.cpp
 * @brief Implements the Zephyr SPI driver wrapper.
 * @date 2026-04-22
 * @author Sungsu Kim
 */

#include "spi.hpp"

#include <cerrno>
#include <cstdio>

/**
 * @brief Creates an SPI wrapper for the specified device.
 * @retval None
 */
Spi::Spi(spi_dt_spec &spec, k_poll_event *asyncEvent)
    : m_spec(spec), m_asyncEvent(asyncEvent)
{
}

/**
 * @brief Releases resources associated with the SPI device.
 * @retval None
 */
Spi::~Spi()
{
    spi_release_dt(&m_spec);
    m_spec.config.operation &= ~SPI_LOCK_ON;
}

/**
 * @brief Checks and initializes the SPI device.
 * @retval 0 on success; a negative error code otherwise.
 */
int Spi::initialize()
{
    if (!spi_is_ready_dt(&m_spec))
    {
        return -ENODEV;
    }

    m_initialized = true;
    return 0;
}

/**
 * @brief Performs a blocking SPI transfer.
 * @retval 0 on success; a negative error code otherwise.
 */
int Spi::transceive(const Buffer &tx, const Buffer &rx)
{
    if (!m_initialized)
    {
        std::printf("SPI not initialized. Call initialize() before transceiving.\n");
        return -EINVAL;
    }

    auto result = pm_device_runtime_get(m_spec.bus);
    if (result != 0)
    {
        std::printf("Failed to get runtime PM for device %s\n", m_spec.bus->name);
        return result;
    }

    auto spiBufferTx = toSpiBuffer(tx);
    auto spiBufferRx = toSpiBuffer(rx);
    auto spiBuffersTx = populateSpiBufSingle(spiBufferTx);
    auto spiBuffersRx = populateSpiBufSingle(spiBufferRx);

    std::printf("Transceive, Tx: %.*s\n", static_cast<int>(tx.size()), reinterpret_cast<const char *>(tx.data()));
    result = spi_transceive_dt(&m_spec, &spiBuffersTx, &spiBuffersRx);
    if (result == -EINVAL || result == -ENOTSUP)
    {
        std::printf("SPI config invalid for this controller\n");
    }
    else if (result == 0)
    {
        std::printf("Transceive, Rx: %.*s\n", static_cast<int>(rx.size()), reinterpret_cast<const char *>(rx.data()));
    }

    const auto pmResult = pm_device_runtime_put(m_spec.bus);
    if (result == 0 && pmResult != 0)
    {
        result = pmResult;
    }

    return result;
}

/**
 * @brief Starts an asynchronous SPI transfer.
 * @retval 0 on success; a negative error code otherwise.
 */
int Spi::transceiveAsync(const Buffer &tx, const Buffer &rx)
{
    if (!m_initialized || m_asyncEvent == nullptr || m_asyncEvent->signal == nullptr)
    {
        return -EINVAL;
    }

    m_asyncSpiBufferTx = toSpiBuffer(tx);
    m_asyncSpiBufferRx = toSpiBuffer(rx);
    m_asyncSpiBuffersTx = populateSpiBufSingle(m_asyncSpiBufferTx);
    m_asyncSpiBuffersRx = populateSpiBufSingle(m_asyncSpiBufferRx);

    std::printf("TransceiveAsync, Tx: %.*s\n", static_cast<int>(tx.size()), reinterpret_cast<const char *>(tx.data()));

    const auto result = spi_transceive_signal(
        m_spec.bus, &m_spec.config, &m_asyncSpiBuffersTx, &m_asyncSpiBuffersRx, m_asyncEvent->signal);
    if (result != 0)
    {
        std::printf("SPI async transceive failed, code %d\n", result);
    }

    return result;
}

/**
 * @brief Waits for the asynchronous SPI transfer to complete.
 * @retval 0 on success; a negative error code otherwise.
 */
int Spi::waitComplete(uint32_t timeoutMs)
{
    if (m_asyncEvent == nullptr || m_asyncEvent->signal == nullptr)
    {
        return -EINVAL;
    }

    const auto result = k_poll(m_asyncEvent, 1U, K_MSEC(timeoutMs));
    if (result != 0)
    {
        std::printf("Failed to wait for async transfer completion, code %d\n", result);
    }

    return result;
}

/**
 * @brief Creates a one-buffer SPI buffer set.
 * @retval The SPI buffer set.
 */
spi_buf_set Spi::populateSpiBufSingle(spi_buf &spiBuffer)
{
    spi_buf_set result;
    result.buffers = &spiBuffer;
    result.count = 1U;
    return result;
}

/**
 * @brief Converts a span to a Zephyr SPI buffer.
 * @retval The SPI buffer.
 */
spi_buf Spi::toSpiBuffer(const Buffer &buffer)
{
    spi_buf result;
    result.buf = buffer.data();
    result.len = buffer.size();
    return result;
}
