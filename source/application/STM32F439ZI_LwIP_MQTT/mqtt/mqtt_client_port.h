/*************************************************************************************************************
 * 
 * @file mqtt_client_port.h
 * @brief Header file for the MQTT client port layer.
 * @details This file defines the structures and functions required for the MQTT client to interact with the network.
 *          Currently, it supports only LWIP Socket API.
 * 
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-08-12
 * @version 1.0
 * 
 *************************************************************************************************************/

#pragma once

#if defined __cplusplus
extern "C" {
#endif

/************************************************** Types ****************************************************/
/**
 * @brief Represents a timer.
 */
typedef struct Timer 
{
	unsigned long systick_period;
	unsigned long end_time;
} Timer;

/**
 * @brief Represents a network connection.
 */
typedef struct Network 
{
	int socket;
	int (*mqttread)      ( struct Network*, unsigned char*, int, int );
	int (*mqttwrite)     ( struct Network*, unsigned char*, int, int );
	void (*disconnect)   ( struct Network* );
} Network;

#if defined __cplusplus
}
#endif