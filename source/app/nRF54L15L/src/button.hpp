/**
 * @file button.hpp
 * @brief Provides a wrapper for the user button GPIO.
 * @date 2026-04-19
 * @author Sungsu Kim
 */

#pragma once

#include <zephyr/drivers/gpio.h>

/**
 * @brief Wraps the GPIO details of a button.
 */
class Button
{
public:
	/**
	 * @brief Configures the button GPIO as an input.
	 * @retval None
	 */
	explicit Button(const struct gpio_dt_spec& buttonSpec)
	: m_button(buttonSpec)
	{
		if (gpio_is_ready_dt(&m_button))
		{
			const auto result = gpio_pin_configure_dt(&m_button, GPIO_INPUT);
			m_configured = result == 0;
		}
	}

	/**
	 * @brief Reports whether the button is currently pressed.
	 * @retval true if pressed; otherwise false.
	 */
	bool isPressed() const
	{
		if (m_configured)
		{
			return gpio_pin_get_dt(&m_button) > 0;
		}

		return false;
	}

private:
	const struct gpio_dt_spec& m_button;	//!< Button GPIO specification.
	bool m_configured{};					//!< Whether GPIO configuration succeeded.
};
