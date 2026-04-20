
#pragma once

#include <zephyr/drivers/gpio.h>

/**
 * @brief A simple button class that abstracts away the GPIO details
 * @details This class accepts a gpio_dt_spec in the contstructor
 */
class Button
{
public:
    Button(const struct gpio_dt_spec& button_spec) 
    : m_button(button_spec) 
    {
        if (gpio_is_ready_dt(&m_button)) 
        {
            auto ret = gpio_pin_configure_dt(&m_button, GPIO_INPUT);
            m_configured = (ret == 0);
        }
    }

    bool IsPressed() 
    {
        if(m_configured)
            return gpio_pin_get_dt(&m_button) > 0;
        return false;
    }

private:
    const struct gpio_dt_spec& m_button;
    bool m_configured{false};
};