#pragma once

/**
 * @file button.hpp
 * @brief GPIO button wrapper.
 * @date 2026-11-16
 * @author Sungsu Kim
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/gpio.h>

/**
 * @brief A simple button class that abstracts away the low-level details.
 */
class Button
{
public:
    /**
     * @brief Construct a button and configure its GPIO input.
     * @retval None
     */
    explicit Button(const struct gpio_dt_spec& buttonSpec)
    : m_button(buttonSpec)
    {
        if (gpio_is_ready_dt(&m_button))
        {
            const auto result = gpio_pin_configure_dt(&m_button, GPIO_INPUT);
            m_configured = (result == 0);
        }
    }

    /**
     * @brief Report whether the button GPIO is configured.
     * @retval true when configured, false otherwise
     */
    bool isConfigured() const
    {
        return m_configured;
    }

    /**
     * @brief Report whether the button is currently pressed.
     * @retval true when pressed, false otherwise
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
    const struct gpio_dt_spec& m_button; //!< Button GPIO specification.
    bool m_configured{}; //!< True if the GPIO input was configured successfully.
};
