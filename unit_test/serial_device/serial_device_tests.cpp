/************************************************************************************************************
 *
 * @file serial_device_test.cpp
 * @brief Unit tests for the SerialDevice class
 *
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-09-02
 * @version 1.0
 *
 ************************************************************************************************************/

 /************************************************** Includes ************************************************/
#include "serial_device.h"
#include "mock_semaphore.h"
#include "mock_lockable.h"
#include <gtest/gtest.h>

/*********************************************** Global Variables ********************************************/
SemaphoreMock* g_mockSemaphore;
LockableMock* g_mockLockable;

/*********************************************** Local Variables *********************************************/


/*********************************************** Function Definitions ****************************************/


/************************************************** Test Fixture ********************************************/
class SerialDeviceTest : public ::testing::Test
{
protected:
   void SetUp() override
   { 
		g_mockSemaphore = &m_semaphoreMock;
		g_mockLockable = &m_lockableMock;
   }

   void TearDown() override
   { 
      testing::Mock::VerifyAndClear( &m_semaphoreMock );
      testing::Mock::VerifyAndClear( &m_lockableMock );
   }

public:
   SemaphoreMock m_semaphoreMock;
	LockableMock m_lockableMock;
};

/************************************************** Tests ***************************************************/
TEST_F( SerialDeviceTest, test_ )
{
   
}

