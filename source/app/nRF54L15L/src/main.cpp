/**
 * @file main.cpp
 * @brief Runs the button, LED and UART demonstration.
 * @date 2026-04-19
 * @author Sungsu Kim
 */

/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "button.hpp"
#include "led.hpp"
#include "serial.hpp"

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include <cstdint>
#include <span>

constexpr int32_t SLEEP_TIME_MS = 100;

static const struct gpio_dt_spec s_ledGpio = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec s_buttonGpio = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct device* s_uart = DEVICE_DT_GET(DT_NODELABEL(uart20));

constexpr struct uart_config UART_CONFIG =
{
	.baudrate = 115200,	// TODO: The overlay currently determines the effective baud rate.
	.parity = UART_CFG_PARITY_NONE,
	.stop_bits = UART_CFG_STOP_BITS_1,
	.data_bits = UART_CFG_DATA_BITS_8,
	.flow_ctrl = UART_CFG_FLOW_CTRL_NONE
};

static void MAIN_uartCallback(const struct device* dev, struct uart_event* event, void* userData);

static uint8_t s_rxBuffer[10] = {};
static std::span<uint8_t> s_rxBufferSpan(s_rxBuffer);
static Serial s_serial(UART_CONFIG, *s_uart, s_rxBufferSpan, MAIN_uartCallback);

/**
 * @brief Initializes the demo and repeatedly mirrors the button state to the LED.
 * @retval 0 if the demonstration exits normally.
 */
int main(void)
{
	Led led(s_ledGpio);
	Button button(s_buttonGpio);

	auto result = s_serial.initialize();
	if (result != 0)
	{
		printk("Failed to initialize UART: %d\n", result);
	}

	result = s_serial.send("Hello!\n\r");
	if (result != 0)
	{
		printk("Failed to send UART greeting: %d\n", result);
	}

	bool buttonWasPressed = button.isPressed();

	for (;;)
	{
		const auto buttonIsPressed = button.isPressed();
		if (buttonIsPressed)
		{
			led.turnOn();
		}
		else 
		{
			led.turnOff();
		}

		if (buttonWasPressed != buttonIsPressed)
		{
			buttonWasPressed = buttonIsPressed;
			printk("Button changed! LED is %s.\n", buttonIsPressed ? "ON" : "OFF");
		}

		k_msleep(SLEEP_TIME_MS);
	}

	return 0;
}

/**
 * @brief Handles UART events and resumes reception after it is disabled.
 * @retval None
 */
static void MAIN_uartCallback(const struct device* dev, struct uart_event* event, void* userData)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(userData);

	switch (event->type)
	{
		case UART_RX_RDY:
			printk("\n");
			for (size_t i = event->data.rx.offset; i < event->data.rx.offset + event->data.rx.len; ++i)
			{
				printk("%c", event->data.rx.buf[i]);
			}
			printk(" (echo)\n");
			break;

		case UART_RX_DISABLED:
			if (const auto result = s_serial.enableRx(); result != 0)
			{
				printk("Failed to re-enable UART RX: %d\n", result);
			}
			break;

		case UART_TX_DONE:
		case UART_TX_ABORTED:
		case UART_RX_BUF_REQUEST:
		case UART_RX_BUF_RELEASED:
		case UART_RX_STOPPED:
		default:
			break;
	}
}