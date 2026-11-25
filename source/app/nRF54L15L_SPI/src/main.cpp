/**
 * @file main.cpp
 * @brief Runs synchronous and asynchronous SPI loopback transfers.
 * @date 2026-04-22
 * @author Sungsu Kim
 */

#include "mem_defines.h"
#include "spi.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>

#define FRAME_SIZE COND_CODE_1(CONFIG_SPI_LOOPBACK_16BITS_FRAMES, (16), (8))
#define MODE_LOOP COND_CODE_1(CONFIG_SPI_LOOPBACK_MODE_LOOP, (SPI_MODE_LOOP), (0))

#define SPI_OP(frame_size) (SPI_OP_MODE_MASTER | SPI_MODE_CPOL | MODE_LOOP | SPI_MODE_CPHA | SPI_WORD_SET(frame_size) | SPI_LINES_SINGLE)

#define SPI_FAST_DEV DT_COMPAT_GET_ANY_STATUS_OKAY(test_spi_loopback_fast)
static spi_dt_spec s_spiFast = SPI_DT_SPEC_GET(SPI_FAST_DEV, SPI_OP(FRAME_SIZE), 0);

static constexpr std::size_t BUFFER_SIZE = 32U;

static k_poll_signal s_asyncSignal = K_POLL_SIGNAL_INITIALIZER(s_asyncSignal);
static k_poll_event s_asyncEvent = {
    .poller = nullptr,
    .type = K_POLL_TYPE_SIGNAL,
    .state = K_POLL_STATE_NOT_READY,
    .mode = K_POLL_MODE_NOTIFY_ONLY,
    .unused = 0,
    .signal = &s_asyncSignal
};

/**
 * @brief Runs blocking and asynchronous SPI loopback transfers.
 * @retval 0 on success; a negative error code otherwise.
 */
int main()
{
    std::printf("Sungsu's SPI Test\n");

    SPI_BUFFER_ALIGN uint8_t txBuffer[BUFFER_SIZE];
    SPI_BUFFER_ALIGN uint8_t rxBuffer[BUFFER_SIZE];
    std::fill_n(txBuffer, BUFFER_SIZE, 0U);
    std::fill_n(rxBuffer, BUFFER_SIZE, 0U);

    Spi::Buffer tx(txBuffer, sizeof(txBuffer));
    Spi::Buffer rx(rxBuffer, sizeof(rxBuffer));

    Spi spi(s_spiFast, &s_asyncEvent);
    auto result = spi.initialize();
    if (result != 0)
    {
        std::printf("Failed to initialize SPI, code %d\n", result);
        return result;
    }

    result = std::snprintf(reinterpret_cast<char *>(tx.data()), tx.size(), "Hello");
    if (result < 0 || static_cast<std::size_t>(result) >= tx.size())
    {
        std::printf("Failed to prepare blocking SPI test message\n");
        return -EINVAL;
    }

    result = spi.transceive(tx, rx);
    if (result != 0)
    {
        std::printf("Blocking SPI transfer failed, code %d\n", result);
        return result;
    }

    result = std::snprintf(reinterpret_cast<char *>(tx.data()), tx.size(), "Haloo");
    if (result < 0 || static_cast<std::size_t>(result) >= tx.size())
    {
        std::printf("Failed to prepare asynchronous SPI test message\n");
        return -EINVAL;
    }

    result = spi.transceiveAsync(tx, rx);
    if (result != 0)
    {
        return result;
    }

    result = spi.waitComplete(10U);
    if (result != 0)
    {
        return result;
    }

    std::printf("Main, Rx: %.*s\n", static_cast<int>(rx.size()), reinterpret_cast<const char *>(rx.data()));
    return 0;
}
