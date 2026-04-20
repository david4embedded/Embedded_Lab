
#pragma once

#include <zephyr/drivers/uart.h>
#include <span>
class Serial
{
public:
    using UartCallBackFunc = void (*)(const struct device *dev, struct uart_event *evt, void *user_data);

    Serial(const struct uart_config& config, const struct device& uart_dev, 
           std::span<uint8_t> rxBuffer, UartCallBackFunc callback);

    int Initialize();
    int Send(const char* data);
    int Receive();
    void EnableRx();

private:

    const struct uart_config& m_config;
    const struct device& m_uart_dev;
    std::span<uint8_t> m_rxBuffer;
    UartCallBackFunc m_callback;

    bool m_initialized{false};
};