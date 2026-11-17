/*
 * Hardware check application for Seeed Studio XIAO nRF54L15 Sense
 *
 * Checks the on-board LED, user button, 6-axis IMU and PDM microphone and prints
 * a PASS/FAIL summary at boot. Live readings are available through the shell
 * ("imu" and "mic" commands, see AppShell.hpp).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "AppShell.hpp"
#include "Button.hpp"
#include "Imu.hpp"
#include "Led.hpp"
#include "Mic.hpp"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define TICK_MS				(100)	// Main loop period
#define LED_TOGGLE_TICKS	(5)		// Heartbeat: toggle every 500 ms

// Device tree nodes
static const struct gpio_dt_spec s_gpioLed		= GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec s_gpioButton	= GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct i2c_dt_spec s_i2cImu		= I2C_DT_SPEC_GET(DT_ALIAS(imu0));
static const struct device *s_dmic				= DEVICE_DT_GET(DT_NODELABEL(dmic_dev));

// Shared with the shell commands, so they live for the whole run time
static Led s_led(s_gpioLed);
static Button s_button(s_gpioButton);
static Imu s_imu(s_i2cImu);
static Mic s_mic(*s_dmic);

static const char* ResultString(bool pass)
{
	return pass ? "PASS" : "FAIL";
}

static void PrintChipInfo()
{
	uint8_t id[16] = {0};
	auto len = hwinfo_get_device_id(id, sizeof(id));

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

static bool CheckLed(Led& led)
{
	if (!led.IsConfigured())
	{
		return false;
	}

	// Blink 3 times so the LED can be checked visually
	for (int i = 0; i < 3; ++i)
	{
		led.TurnOn();
		k_msleep(150);
		led.TurnOff();
		k_msleep(150);
	}
	return true;
}

/**
 * @brief Capture one block to check that the mic delivers data
 */
static int CheckMic(Mic& mic, Mic::Level& level)
{
	auto ret = mic.Initialize();
	if (ret == 0)
	{
		ret = mic.Start();
	}
	if (ret == 0)
	{
		ret = mic.Read(level);
		mic.Stop();
	}
	return ret;
}

int main(void)
{
	printk("\n==============================================\n");
	printk(" XIAO nRF54L15 Sense - HW check (%s)\n", CONFIG_BOARD_TARGET);
	printk("==============================================\n");

	PrintChipInfo();

	const bool ledOk = CheckLed(s_led);
	const bool buttonOk = s_button.IsConfigured();

	const auto imuRet = s_imu.Initialize();
	const bool imuOk = (imuRet == 0);

	Mic::Level micLevel = {};
	const auto micRet = CheckMic(s_mic, micLevel);
	const bool micOk = (micRet == 0);

	printk("\n---------------- Self test -------------------\n");
	printk("[%s] LED    (P2.00)\n", ResultString(ledOk));
	printk("[%s] Button (P0.00)\n", ResultString(buttonOk));
	printk("[%s] IMU    WHO_AM_I=0x%02X (%s), ret=%d\n",
		ResultString(imuOk), s_imu.WhoAmI(), s_imu.ChipName(), imuRet);
	printk("[%s] Mic    PDM 16 kHz mono, ret=%d, peak=%d%s\n",
		ResultString(micOk), micRet, micLevel.peak, micLevel.flat ? " (FLAT)" : "");
	printk("----------------------------------------------\n");
	printk("Shell commands: imu info | imu read | imu stream <ms> | mic monitor [ms]\n");
	printk("Press ESC to stop streaming.\n\n");

	AppShell::Initialize(s_imu, imuOk, s_mic, micOk);

	for (uint32_t tick = 1; ; ++tick)
	{
		// LED stays on while the button is pressed, heartbeat otherwise
		if (s_button.IsPressed())
		{
			s_led.TurnOn();
		}
		else if ((tick % LED_TOGGLE_TICKS) == 0)
		{
			s_led.Toggle();
		}

		k_msleep(TICK_MS);
	}

	return 0;
}
