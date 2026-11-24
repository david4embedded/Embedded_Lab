/**
 * @file main.cpp
 * @brief Runs the XIAO nRF54L15 Sense hardware check and main loop.
 * @date 2026-10-28
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

#include <span>

static constexpr uint32_t TICK_MS = 100;
static constexpr uint32_t LED_TOGGLE_TICKS = 5;
static constexpr size_t CHIP_ID_BUFFER_SIZE = 16;
static constexpr int32_t LED_CHECK_BLINK_COUNT = 3;
static constexpr int32_t LED_CHECK_HALF_PERIOD_MS = 150;

static const struct gpio_dt_spec s_gpioLed = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec s_gpioButton = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct i2c_dt_spec s_i2cImu = I2C_DT_SPEC_GET(DT_ALIAS(imu0));
static const struct device* s_dmic = DEVICE_DT_GET(DT_NODELABEL(dmic_dev));

static Led s_led(s_gpioLed);
static Button s_button(s_gpioButton);
static Imu s_imu(s_i2cImu);
static Mic s_mic(*s_dmic);

static const char* formatResultString(bool pass);
static void printChipInfo();
static bool checkLed(const Led& led);
static int checkMic(Mic& mic, Mic::Level& level);

/**
 * @brief Initializes the hardware checks and runs the application loop.
 * @retval 0 when the application exits.
 */
int main(void)
{
	printk("\n==============================================\n");
	printk(" XIAO nRF54L15 Sense - HW check (%s)\n", CONFIG_BOARD_TARGET);
	printk("==============================================\n");

	printChipInfo();

	const bool ledOk = checkLed(s_led);
	const bool buttonOk = s_button.isConfigured();

	const auto imuResult = s_imu.initialize();
	const bool imuOk = (imuResult == 0);

	Mic::Level micLevel;
	const auto micResult = checkMic(s_mic, micLevel);
	const bool micOk = (micResult == 0);

	printk("\n---------------- Self test -------------------\n");
	printk("[%s] LED    (P2.00)\n", formatResultString(ledOk));
	printk("[%s] Button (P0.00)\n", formatResultString(buttonOk));
	printk("[%s] IMU    WHO_AM_I=0x%02X (%s), ret=%d\n",
		formatResultString(imuOk), s_imu.getWhoAmI(), s_imu.getChipName(), imuResult);
	printk("[%s] Mic    PDM 16 kHz mono, ret=%d, peak=%d%s\n",
		formatResultString(micOk), micResult, micLevel.m_peak, micLevel.m_flat ? " (FLAT)" : "");
	printk("----------------------------------------------\n");
	printk("Shell commands: imu info | imu read | imu stream <ms> | mic monitor [ms]\n");
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
 * @brief Returns the printable result for a hardware check.
 * @retval "PASS" for success; otherwise "FAIL".
 */
static const char* formatResultString(const bool pass)
{
	return pass ? "PASS" : "FAIL";
}

/**
 * @brief Prints the device identifier and reset cause when available.
 * @retval None
 */
static void printChipInfo()
{
	uint8_t id[CHIP_ID_BUFFER_SIZE];
	const auto result = hwinfo_get_device_id(id, sizeof(id));

	printk("Device ID   : ");
	if (result > 0)
	{
		const std::span<const uint8_t> deviceId(id, static_cast<size_t>(result));
		for (const uint8_t byte : deviceId)
		{
			printk("%02X", byte);
		}
		printk("\n");
	}
	else
	{
		printk("n/a (%d)\n", static_cast<int>(result));
	}

	uint32_t cause = 0;
	if (hwinfo_get_reset_cause(&cause) == 0)
	{
		printk("Reset cause : 0x%08X\n", cause);
		hwinfo_clear_reset_cause();
	}
}

/**
 * @brief Blinks the LED to verify that it is configured.
 * @retval true if the LED is configured; otherwise false.
 */
static bool checkLed(const Led& led)
{
	if (!led.isConfigured())
	{
		return false;
	}

	for (int32_t index = 0; index < LED_CHECK_BLINK_COUNT; ++index)
	{
		led.turnOn();
		k_msleep(LED_CHECK_HALF_PERIOD_MS);
		led.turnOff();
		k_msleep(LED_CHECK_HALF_PERIOD_MS);
	}
	return true;
}

/**
 * @brief Captures one block to verify that the microphone delivers data.
 * @retval 0 on success; a negative errno value otherwise.
 */
static int checkMic(Mic& mic, Mic::Level& level)
{
	auto result = mic.initialize();
	if (result == 0)
	{
		result = mic.start();
	}
	if (result == 0)
	{
		result = mic.read(level);
		mic.stop();
	}
	return result;
}
