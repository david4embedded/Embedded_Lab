/************************************************************************************************************
 * 
 * @file mock_Semaphore.h
 * @brief This file contains the implementation of ISemaphore as a mock used in unit tests.
 * 
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-08-20
 * @version 1.0
 * 
 ************************************************************************************************************/

#pragma once

//************************************************** Includes ************************************************
#include "ISemaphore.h"
#include "gmock/gmock.h"

//**************************************************** Types *************************************************
class SemaphoreMock : public lib::ISemaphore
{
public:
	MOCK_METHOD( ErrorCode, initialize, ( uint32_t maxCount, uint32_t initialCount ) );
	MOCK_METHOD( void, put, ( ) );
	MOCK_METHOD( void, putISR, ( ) );
	MOCK_METHOD( ErrorCode, get, ( uint32_t timeout_ms ) );
};

extern SemaphoreMock* g_mockSemaphore;