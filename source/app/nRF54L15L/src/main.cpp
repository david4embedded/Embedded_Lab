/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Led.hpp"
#include "Button.hpp"
#include "Serial.hpp"

#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#define SLEEP_TIME_MS  	(100)

// Device tree nodes
static const struct gpio_dt_spec s_gpioLed 		= GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec s_gpioButton 	= GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct device *s_uart 				= DEVICE_DT_GET(DT_NODELABEL(uart20));	

// UART configuration
const struct uart_config uart_cfg = 
{
	.baudrate = 115200,	// Todo: This doesn't work; the one in the overlay does. Why?
	.parity = UART_CFG_PARITY_NONE,
	.stop_bits = UART_CFG_STOP_BITS_1,
	.data_bits = UART_CFG_DATA_BITS_8,
	.flow_ctrl = UART_CFG_FLOW_CTRL_NONE
};

static void uartCallback(const struct device *dev, struct uart_event *evt, void *user_data);

static uint8_t s_rxBuffer[10] = {0};
static std::span<uint8_t> s_rxBufferSpan(s_rxBuffer, sizeof(s_rxBuffer));
static Serial s_serial(uart_cfg, *s_uart, s_rxBufferSpan, uartCallback);

int main(void)
{
	Led led1(s_gpioLed);
	Button btn1(s_gpioButton);

	s_serial.Initialize();
	s_serial.Send("Hello!\n\r");

	bool btn1State = btn1.IsPressed();

	for(;;)
	{	
		auto state = btn1.IsPressed();
		if(state) 
		{
			led1.TurnOn();
		} 
		else 
		{
			led1.TurnOff();
		}

		if(btn1State != state) 
		{
			btn1State = state;
			printk("Button changed! LED is %s.\n", state ? "ON" : "OFF");
		}
	
		k_msleep(SLEEP_TIME_MS);
	}
	return 0;
}

static void uartCallback(const struct device *dev, struct uart_event *evt, void *user_data)
{
	switch (evt->type) 
	{	
	case UART_RX_RDY:
		printk("\n");
		for (size_t i = evt->data.rx.offset; i < evt->data.rx.offset + evt->data.rx.len; ++i) 
		{
			printk("%c", evt->data.rx.buf[i]);
		}
    	printk(" (echo)\n");
		break;

	case UART_RX_DISABLED:
		s_serial.EnableRx();
		break;

	// Intentional fallthrough
	case UART_TX_DONE:
	case UART_TX_ABORTED:
	case UART_RX_BUF_REQUEST:
	case UART_RX_BUF_RELEASED:
	case UART_RX_STOPPED:
	default:
		break;
	}
}