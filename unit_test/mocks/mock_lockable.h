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
#include "lockable_interface.h"
#include "gmock/gmock.h"

//**************************************************** Types *************************************************
class LockableMock : public lib::ILockable
{
public:
    MOCK_METHOD( bool, initialize, ( ) );
    MOCK_METHOD( void, lock, () );
    MOCK_METHOD( bool, try_lock, ( uint32_t timeout_ms ) );
    MOCK_METHOD( void, unlock, () );
};

extern LockableMock* g_mockLockable;