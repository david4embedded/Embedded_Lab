/**
 * @file main.cpp
 * @brief Hardware check application entry point for Seeed Studio XIAO nRF54L15 Sense.
 * @date 2026-11-16
 * @author Sungsu Kim
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app_shell.hpp"
#include "button.hpp"
#include "imu.hpp"
#include "led.hpp"
#include "mic.hpp"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <cstdint>

constexpr uint32_t TICK_MS = 100;
constexpr uint32_t LED_TOGGLE_TICKS = 5;

static const struct gpio_dt_spec s_gpioLed = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec s_gpioButton = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct i2c_dt_spec s_i2cImu = I2C_DT_SPEC_GET(DT_ALIAS(imu0));
static const struct device* s_dmic = DEVICE_DT_GET(DT_NODELABEL(dmic_dev));

static Led s_led(s_gpioLed);
static Button s_button(s_gpioButton);
static Imu s_imu(s_i2cImu);
static Mic s_mic(*s_dmic);

static const char* getResultString(bool pass);
static void printChipInfo();
static bool checkLed(Led& led);
static int checkMic(Mic& mic, Mic::Level& level);

/**
 * @brief Hardware check application entry point.
 * @retval 0 if the application exits normally
 */
int main(void)
{
    printk("\n==============================================\n");
    printk(" XIAO nRF54L15 Sense - HW check (%s)\n", CONFIG_BOARD_TARGET);
    printk("==============================================\n");

    printChipInfo();

    const bool ledOk = checkLed(s_led);
    const bool buttonOk = s_button.isConfigured();

    const auto imuRet = s_imu.initialize();
    const bool imuOk = (imuRet == 0);

    Mic::Level micLevel = {};
    const auto micRet = checkMic(s_mic, micLevel);
    const bool micOk = (micRet == 0);

    printk("\n---------------- Self test -------------------\n");
    printk("[%s] LED    (P2.00)\n", getResultString(ledOk));
    printk("[%s] Button (P0.00)\n", getResultString(buttonOk));
    printk("[%s] IMU    WHO_AM_I=0x%02X (%s), ret=%d\n",
        getResultString(imuOk), s_imu.getWhoAmI(), s_imu.getChipName(), imuRet);
    printk("[%s] Mic    PDM 16 kHz mono, ret=%d, peak=%d%s\n",
        getResultString(micOk), micRet, micLevel.m_peak, micLevel.m_flat ? " (FLAT)" : "");
    printk("----------------------------------------------\n");
    printk("Shell commands: imu info | imu read | imu stream <ms> | imu classify | mic monitor [ms]\n");
    printk("Press ESC to stop streaming.\n\n");

    AppShell::Initialize(s_imu, imuOk, s_mic, micOk);

    for (uint32_t tick = 1; ; ++tick)
    {
        if (s_button.isPressed())
        {
            s_led.turnOn();
        }
        else if ((tick % LED_TOGGLE_TICKS) == 0)
        {
            s_led.toggle();
        }

        k_msleep(TICK_MS);
    }

    return 0;
}

/**
 * @brief Convert a test result to PASS or FAIL text.
 * @retval PASS for a passing result, FAIL otherwise
 */
static const char* getResultString(bool pass)
{
    return pass ? "PASS" : "FAIL";
}

/**
 * @brief Print the device identity and reset cause.
 * @retval None
 */
static void printChipInfo()
{
    uint8_t id[16] = {0};
    const auto len = hwinfo_get_device_id(id, sizeof(id));

    printk("Device ID   : ");
    if (len > 0)
    {
        for (ssize_t i = 0; i < len; ++i)
        {
            printk("%02X", id[i]);
        }
        printk("\n");
    }
    else
    {
        printk("n/a (%d)\n", static_cast<int>(len));
    }

    uint32_t cause = 0;
    if (hwinfo_get_reset_cause(&cause) == 0)
    {
        printk("Reset cause : 0x%08X\n", cause);
        hwinfo_clear_reset_cause();
    }
}

/**
 * @brief Check and visually exercise the LED.
 * @retval true when configured, false otherwise
 */
static bool checkLed(Led& led)
{
    if (!led.isConfigured())
    {
        return false;
    }

    for (int i = 0; i < 3; ++i)
    {
        led.turnOn();
        k_msleep(150);
        led.turnOff();
        k_msleep(150);
    }
    return true;
}

/**
 * @brief Capture one block to check the microphone.
 * @retval 0 on success, negative errno otherwise
 */
static int checkMic(Mic& mic, Mic::Level& level)
{
    auto ret = mic.initialize();
    if (ret == 0)
    {
        ret = mic.start();
    }
    if (ret == 0)
    {
        ret = mic.read(level);
        mic.stop();
    }
    return ret;
}
