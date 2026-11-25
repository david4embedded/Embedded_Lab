/**
 * @file serial.hpp
 * @brief Declares a UART wrapper with asynchronous receive support.
 * @date 2026-04-19
 * @author Sungsu Kim
 */

#pragma once

#include <zephyr/drivers/uart.h>

#include <span>

/**
 * @brief Provides UART configuration, transmission and asynchronous reception.
 */
class Serial
{
public:
	using UartCallbackFunc = void (*)(const struct device* dev, struct uart_event* event, void* userData);

	Serial(const struct uart_config& config, const struct device& uartDev, std::span<uint8_t> rxBuffer, UartCallbackFunc callback);

	int initialize();
	int send(const char* data);
	int receive();
	int enableRx();

private:
	const struct uart_config& m_config;	//!< UART configuration.
	const struct device& m_uartDev;		//!< UART device.
	std::span<uint8_t> m_rxBuffer;		//!< Buffer used for asynchronous UART reception.
	UartCallbackFunc m_callback;		//!< UART event callback.
	bool m_initialized{};				//!< Whether UART initialization succeeded.
};
