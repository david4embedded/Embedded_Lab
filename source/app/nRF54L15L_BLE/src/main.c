/**
 * @file main.c
 * @brief Runs the Bluetooth LE NUS peripheral demonstration.
 * @date 2026-10-28
 * @author Sungsu Kim
 */

/*
 * Copyright (c) 2024 Croxel, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/services/nus.h>

#include <string.h>

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

static const struct bt_data s_advertisingData[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data s_scanResponseData[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_SRV_VAL),
};

static void MAIN_handleNotificationEnabled(bool enabled, void* context);
static void MAIN_handleReceived(struct bt_conn* conn, const void* data, uint16_t len, void* context);

struct bt_nus_cb g_nusListener = {
	.notif_enabled = MAIN_handleNotificationEnabled,
	.received = MAIN_handleReceived,
};

/**
 * @brief Reports whether NUS notifications have been enabled.
 * @retval None
 */
static void MAIN_handleNotificationEnabled(bool enabled, void* context)
{
	ARG_UNUSED(context);

	printk("%s() - %s\n", __func__, (enabled ? "Enabled" : "Disabled"));
}

/**
 * @brief Reports the length and contents of a received NUS message.
 * @retval None
 */
static void MAIN_handleReceived(struct bt_conn* conn, const void* data, uint16_t len, void* context)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(context);

	printk("%s() - Len: %u, Message: %.*s\n", __func__, (unsigned int)len, len, (const char*)data);
}

/**
 * @brief Registers NUS callbacks, enables Bluetooth and starts advertising.
 * @retval 0 on success; a negative errno value otherwise.
 */
int main(void)
{
	int result;

	printk("Sample - Bluetooth Peripheral NUS\n");

	result = bt_nus_cb_register(&g_nusListener, NULL);
	if (result != 0)
	{
		printk("Failed to register NUS callback: %d\n", result);
		return result;
	}

	result = bt_enable(NULL);
	if (result != 0)
	{
		printk("Failed to enable bluetooth: %d\n", result);
		return result;
	}

	result = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, s_advertisingData, ARRAY_SIZE(s_advertisingData), s_scanResponseData, ARRAY_SIZE(s_scanResponseData));
	if (result != 0)
	{
		printk("Failed to start advertising: %d\n", result);
		return result;
	}

	printk("Initialization complete\n");

	while (true)
	{
		const char* helloWorld = "Hello World!\n";

		k_sleep(K_SECONDS(3));

		result = bt_nus_send(NULL, helloWorld, strlen(helloWorld));
		printk("Data send - Result: %d\n", result);

		if (result < 0 && (result != -EAGAIN) && (result != -ENOTCONN))
		{
			return result;
		}
	}

	return 0;
}
