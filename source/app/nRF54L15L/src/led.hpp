/**
 * @file led.hpp
 * @brief Provides a wrapper for the status LED GPIO.
 * @date 2026-04-19
 * @author Sungsu Kim
 */

#pragma once

#include <zephyr/drivers/gpio.h>

/**
 * @brief Wraps the low-level details of an LED GPIO.
 */
class Led
{
public:
	/**
	 * @brief Configures the LED GPIO as an active output.
	 * @retval None
	 */
	explicit Led(const struct gpio_dt_spec& ledSpec)
	: m_led(ledSpec)
	{
		if (gpio_is_ready_dt(&m_led))
		{
			const auto result = gpio_pin_configure_dt(&m_led, GPIO_OUTPUT_ACTIVE);
			m_configured = result == 0;
		}
	}

	/**
	 * @brief Toggles the LED when it is configured.
	 * @retval None
	 */
	void toggle() const
	{
		if (m_configured)
		{
			gpio_pin_toggle_dt(&m_led);
		}
	}

	/**
	 * @brief Turns on the LED when it is configured.
	 * @retval None
	 */
	void turnOn() const
	{
		if (m_configured)
		{
			gpio_pin_set_dt(&m_led, 1);
		}
	}

	/**
	 * @brief Turns off the LED when it is configured.
	 * @retval None
	 */
	void turnOff() const
	{
		if (m_configured)
		{
			gpio_pin_set_dt(&m_led, 0);
		}
	}

private:
	const struct gpio_dt_spec& m_led;	//!< LED GPIO specification.
	bool m_configured{};				//!< Whether GPIO configuration succeeded.
};
