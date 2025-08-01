/************************************************************************************************************
 * 
 * @file mock_FreeRTOS.cpp
 * @brief This file contains the implementation of mock functions for FreeRTOS used in unit tests.
 * 
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-07-27
 * @version 1.0
 * 
 ************************************************************************************************************/

//************************************************** Includes ************************************************
#include "mock_FreeRTOS.h"

//********************************************** Mock Definitions ********************************************
QueueHandle_t xQueueCreateMutex( const uint8_t ucQueueType )
{
   return g_mockFreeRTOS->xQueueCreateMutex( ucQueueType );
}

QueueHandle_t xQueueCreateCountingSemaphore( const UBaseType_t uxMaxCount, const UBaseType_t uxInitialCount ) 
{
   return g_mockFreeRTOS->xQueueCreateCountingSemaphore( uxMaxCount, uxInitialCount );
}

QueueHandle_t xQueueCreateCountingSemaphoreStatic( const UBaseType_t uxMaxCount, const UBaseType_t uxInitialCount, StaticQueue_t* pxStaticQueue )
{
	return g_mockFreeRTOS->xQueueCreateCountingSemaphoreStatic( uxMaxCount, uxInitialCount, pxStaticQueue );
}

BaseType_t xQueueSemaphoreTake( QueueHandle_t xQueue, TickType_t xTicksToWait )
{
   return g_mockFreeRTOS->xQueueSemaphoreTake( xQueue, xTicksToWait );
}

BaseType_t xQueueGenericSend( QueueHandle_t xQueue, const void * const pvItemToQueue, TickType_t xTicksToWait, const BaseType_t xCopyPosition )
{
   return g_mockFreeRTOS->xQueueGenericSend( xQueue, pvItemToQueue, xTicksToWait, xCopyPosition );
}

TickType_t xTaskGetTickCount( void )
{
   return g_mockFreeRTOS->xTaskGetTickCount();
}