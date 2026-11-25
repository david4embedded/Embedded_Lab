#pragma once

/**
 * @file led.hpp
 * @brief GPIO LED wrapper.
 * @date 2026-11-16
 * @author Sungsu Kim
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/gpio.h>

/**
 * @brief A simple LED class that abstracts away the low-level details.
 */
class Led
{
public:
    /**
     * @brief Construct an LED and configure it as an inactive output.
     * @retval None
     */
    explicit Led(const struct gpio_dt_spec& ledSpec)
    : m_led(ledSpec)
    {
        if (gpio_is_ready_dt(&m_led))
        {
            const auto result = gpio_pin_configure_dt(&m_led, GPIO_OUTPUT_INACTIVE);
            m_configured = (result == 0);
        }
    }

    /**
     * @brief Report whether the LED GPIO is configured.
     * @retval true when configured, false otherwise
     */
    bool isConfigured() const
    {
        return m_configured;
    }

    /**
     * @brief Toggle the LED output.
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
     * @brief Turn the LED on.
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
     * @brief Turn the LED off.
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
    const struct gpio_dt_spec& m_led; //!< LED GPIO specification.
    bool m_configured{}; //!< True if the GPIO output was configured successfully.
};
