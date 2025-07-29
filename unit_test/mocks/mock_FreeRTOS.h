/************************************************************************************************************
 * 
 * @file mock_FreeRTOS.h
 * @brief This file contains the declaration of a mock class for FreeRTOS functions used in unit tests.
 * 
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-07-27
 * @version 1.0
 * 
 ************************************************************************************************************/

#pragma once

//************************************************** Includes ************************************************
#include "gmock/gmock.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

//**************************************************** Types *************************************************
class IFreeRTOSMock
{
public:
    virtual QueueHandle_t xQueueCreateMutex( const uint8_t ucQueueType ) = 0;
    virtual QueueHandle_t xQueueCreateCountingSemaphore( const UBaseType_t uxMaxCount, const UBaseType_t uxInitialCount ) = 0;
    virtual BaseType_t xQueueSemaphoreTake( QueueHandle_t xQueue, TickType_t xTicksToWait ) = 0;
    virtual BaseType_t xQueueGenericSend( QueueHandle_t xQueue, const void * const pvItemToQueue, TickType_t xTicksToWait, const BaseType_t xCopyPosition ) = 0;
    virtual TickType_t xTaskGetTickCount( void ) = 0;    
};

class FreeRTOSMock : public IFreeRTOSMock
{
public:
    MOCK_METHOD( QueueHandle_t, xQueueCreateMutex, ( const uint8_t ) );
    MOCK_METHOD( QueueHandle_t, xQueueCreateCountingSemaphore, ( const UBaseType_t uxMaxCount, const UBaseType_t uxInitialCount ) );
    MOCK_METHOD( BaseType_t, xQueueSemaphoreTake, ( QueueHandle_t xQueue, TickType_t xTicksToWait ) );
    MOCK_METHOD( BaseType_t, xQueueGenericSend, ( QueueHandle_t xQueue, const void * const pvItemToQueue, TickType_t xTicksToWait, const BaseType_t xCopyPosition ) );
    MOCK_METHOD( TickType_t, xTaskGetTickCount, ( ) );
};

extern FreeRTOSMock* g_mockFreeRTOS;

