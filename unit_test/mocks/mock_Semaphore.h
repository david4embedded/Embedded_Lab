/************************************************************************************************************
 * 
 * @file mock_Lockable.h
 * @brief This file contains the declaration of a mock class for lockable resources used in unit tests.
 * 
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-07-27
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
    MOCK_METHOD( ErrorCode, initialize, ( ) );
    MOCK_METHOD( void, put, () );
    MOCK_METHOD( ErrorCode, get, ( uint32_t timeout_ms ) );
};

extern SemaphoreMock* g_mockSemaphore;