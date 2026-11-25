/**
 * @file serial.cpp
 * @brief Implements the UART wrapper.
 * @date 2026-04-19
 * @author Sungsu Kim
 */

#include "serial.hpp"

#include <cstring>

/**
 * @brief Stores the UART configuration, device, receive buffer and callback.
 * @retval None
 */
Serial::Serial(const struct uart_config& config, const struct device& uartDev, std::span<uint8_t> rxBuffer, UartCallbackFunc callback)
: m_config(config),
  m_uartDev(uartDev),
  m_rxBuffer(rxBuffer),
  m_callback(callback)
{
}

/**
 * @brief Configures the UART and enables asynchronous reception.
 * @retval 0 on success; a negative errno value otherwise.
 */
int Serial::initialize()
{
	if (!device_is_ready(&m_uartDev))
	{
		return -1;
	}

	auto result = uart_configure(&m_uartDev, &m_config);
	if (result != 0)
	{
		return result;
	}

	result = uart_callback_set(&m_uartDev, m_callback, nullptr);
	if (result != 0)
	{
		return result;
	}

	m_initialized = true;
	result = enableRx();
	if (result != 0)
	{
		m_initialized = false;
	}

	return result;
}

/**
 * @brief Transmits a null-terminated string over UART.
 * @retval 0 on success; a negative errno value otherwise.
 */
int Serial::send(const char* data)
{
	if (!m_initialized)
	{
		return -1;
	}

	return uart_tx(&m_uartDev, reinterpret_cast<const uint8_t*>(data), std::strlen(data), 100);
}

/**
 * @brief Provides the UART receive-operation interface.
 * @retval 0.
 */
int Serial::receive()
{
	return 0;
}

/**
 * @brief Enables asynchronous UART reception using the configured buffer.
 * @retval 0 on success; a negative errno value otherwise.
 */
int Serial::enableRx()
{
	if (!m_initialized)
	{
		return -1;
	}

	return uart_rx_enable(&m_uartDev, m_rxBuffer.data(), m_rxBuffer.size(), 100);
}
