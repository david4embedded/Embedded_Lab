/*************************************************************************************************************
 * 
 * @file mqtt_client_port.c
 * @brief Source file for the MQTT client port layer.
 * @details This file implements the structures and functions required for the MQTT client to interact with the network, 
 *          specifically for the timer-related stuffs.
 * 
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-08-12
 * @version 1.0
 * 
 *************************************************************************************************************/

/************************************************** Includes *************************************************/
#include "mqtt_client_port.h"
#include "stm32f4xx_hal.h"

#include <string.h>
#include "lwip.h"
#include "lwip/api.h"
#include "lwip/sockets.h"
#include "common.h"

/********************************************* Global Variables **********************************************/
uint32_t g_mqttTick_ms;

/******************************************** Function Definitions *******************************************/
/**
 * @brief Initializes a timer.
 * @param timer Pointer to the timer to initialize.
 */
void TimerInit(Timer *timer) 
{
	timer->end_time = 0;
}

/**
 * @brief Checks if a timer has expired.
 * 
 * @param timer Pointer to the timer to check.
 * @return char 1 if the timer has expired, 0 otherwise.
 */
char TimerIsExpired(Timer *timer) 
{
	long left = timer->end_time - g_mqttTick_ms;
	return (left < 0);
}

/**
 * @brief Starts a countdown timer in milliseconds.
 * 
 * @param timer Pointer to the timer to start.
 * @param timeout Timeout duration in milliseconds.
 */
void TimerCountdownMS(Timer *timer, unsigned int timeout) 
{
	timer->end_time = g_mqttTick_ms + timeout;
}

/**
 * @brief Starts a countdown timer in seconds.
 * 
 * @param timer Pointer to the timer to start.
 * @param timeout Timeout duration in seconds.
 */
void TimerCountdown(Timer *timer, unsigned int timeout) 
{
	timer->end_time = g_mqttTick_ms + (timeout * 1000);
}

/**
 * @brief Gets the remaining time of a timer in milliseconds.
 * 
 * @param timer Pointer to the timer to check.
 * @return int Remaining time in milliseconds.
 */
int TimerLeftMS(Timer *timer) 
{
	long left = timer->end_time - g_mqttTick_ms;
	return (left < 0) ? 0 : left;
}
