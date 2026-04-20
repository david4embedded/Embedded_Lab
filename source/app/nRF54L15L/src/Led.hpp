
#pragma once

#include <zephyr/drivers/gpio.h>

/**
 * @brief A simple LED class that abstracts away the low level details
 * @details This class accepts a gpio_dt_spec in the contstructor
 */
class Led
{
public:
	Led(const struct gpio_dt_spec& led_spec) 
	: m_led(led_spec) 
	{
		if (gpio_is_ready_dt(&m_led)) 
		{
			auto ret = gpio_pin_configure_dt(&m_led, GPIO_OUTPUT_ACTIVE);
			m_configured = (ret == 0);
		}
	}

	void Toggle() 
	{
		if(m_configured)
			gpio_pin_toggle_dt(&m_led);
	}

	void TurnOn()
	{
		if(m_configured)
			gpio_pin_set_dt(&m_led, 1);
	}

	void TurnOff()
	{
		if(m_configured)
			gpio_pin_set_dt(&m_led, 0);
	}

private:
	const struct gpio_dt_spec& m_led;
	bool m_configured{false};
};