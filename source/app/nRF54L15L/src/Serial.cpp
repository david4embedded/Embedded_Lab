
#include "Serial.hpp"

Serial::Serial(const struct uart_config& config, const struct device& uart_dev, 
               std::span<uint8_t> rxBuffer, UartCallBackFunc callback)
 : m_config(config), 
   m_uart_dev(uart_dev),
   m_rxBuffer(rxBuffer),
   m_callback(callback)
{  

}

int Serial::Initialize()
{
    if (!device_is_ready(&m_uart_dev)) 
	{
		return -1;
	}

	auto err = uart_configure(&m_uart_dev, &m_config);
	if (err == -ENOSYS) 
	{
		return -ENOSYS;
	}

    err = uart_callback_set(&m_uart_dev, m_callback, nullptr);
	if (err) 
	{
		return err;
	}

    m_initialized = true;
    EnableRx();
    
    return 0;
}

int Serial::Send(const char* data)
{
    if (!m_initialized) 
    {
        return -1;
    }

    return uart_tx(&m_uart_dev, reinterpret_cast<const uint8_t*>(data), strlen(data), 100);
}

int Serial::Receive()
{
    return 0;
}

void Serial::EnableRx()
{
    if(!m_initialized) 
    {
        return;
    }

    uart_rx_enable(&m_uart_dev, m_rxBuffer.data(), m_rxBuffer.size(), 100);    
}