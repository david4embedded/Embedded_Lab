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
#include <memory>

/*********************************************** Global Variables ********************************************/
SemaphoreMock* g_mockSemaphore;
LockableMock* g_mockLockable;

/*********************************************** Local Variables *********************************************/
static uint8_t g_rxBuffer[128];
static bool    g_senderCalled = false;

/*********************************************** Function Definitions ****************************************/
auto sender = []( const uint8_t* data, size_t length ) { ( void )data; ( void )length; g_senderCalled = true; };

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

	std::unique_ptr<lib::SerialDevice> getSerialDevice(  )
   {
      return std::make_unique<lib::SerialDevice>( sender, g_rxBuffer, sizeof( g_rxBuffer ), m_lockableMock, m_semaphoreMock, m_semaphoreMock );
	}
};

/************************************************** Tests ***************************************************/
TEST_F( SerialDeviceTest, test_device_is_initialized_properly )
{
	auto serialDevice = getSerialDevice();

   EXPECT_CALL( m_lockableMock, initialize() ).Times( 1 ).WillOnce( testing::Return( LibErrorCodes::eOK ) );
   EXPECT_CALL( m_semaphoreMock, initialize( 1, 0 ) ).WillOnce( testing::Return( LibErrorCodes::eOK ) );
   EXPECT_CALL( m_semaphoreMock, initialize( sizeof( g_rxBuffer ), 0 ) ).WillOnce( testing::Return( LibErrorCodes::eOK ) );

   auto result = serialDevice->initialize();
	EXPECT_EQ( result, LibErrorCodes::eOK );
}

TEST_F( SerialDeviceTest, test_send_fails_if_not_initialized )
{
   auto serialDevice = getSerialDevice();

	EXPECT_CALL( m_lockableMock, lock() ).Times( 1 );
   EXPECT_CALL( m_lockableMock, unlock() ).Times( 1 );

	auto result = serialDevice->sendAsync( nullptr, 0 );

	EXPECT_EQ( result, LibErrorCodes::eSERIAL_DEVICE_NOT_INITIALIZED );
}

TEST_F( SerialDeviceTest, test_send_fails_if_already_sending )
{
   auto serialDevice = getSerialDevice();

   EXPECT_CALL( m_lockableMock, initialize() ).Times( 1 ).WillOnce( testing::Return( LibErrorCodes::eOK ) );
   EXPECT_CALL( m_semaphoreMock, initialize( 1, 0 ) ).WillOnce( testing::Return( LibErrorCodes::eOK ) );
   EXPECT_CALL( m_semaphoreMock, initialize( sizeof( g_rxBuffer ), 0 ) ).WillOnce( testing::Return( LibErrorCodes::eOK ) );

   auto result = serialDevice->initialize();

   EXPECT_EQ( result, LibErrorCodes::eOK );
   EXPECT_CALL( m_lockableMock, lock() ).Times( 2 );
   EXPECT_CALL( m_lockableMock, unlock() ).Times( 2 );

   result = serialDevice->sendAsync( nullptr, 0 );
   EXPECT_EQ( result, LibErrorCodes::eOK );

   result = serialDevice->sendAsync( nullptr, 0 );
   EXPECT_EQ( result, LibErrorCodes::eSERIAL_DEVICE_SEND_ACTIVE );
}

TEST_F( SerialDeviceTest, test_send_fails_if_message_too_long )
{
   auto serialDevice = getSerialDevice();

   EXPECT_CALL( m_lockableMock, initialize() ).Times( 1 ).WillOnce( testing::Return( LibErrorCodes::eOK ) );
   EXPECT_CALL( m_semaphoreMock, initialize( 1, 0 ) ).WillOnce( testing::Return( LibErrorCodes::eOK ) );
   EXPECT_CALL( m_semaphoreMock, initialize( sizeof( g_rxBuffer ), 0 ) ).WillOnce( testing::Return( LibErrorCodes::eOK ) );
   
   auto result = serialDevice->initialize();
   
   EXPECT_EQ( result, LibErrorCodes::eOK );
   EXPECT_CALL( m_lockableMock, lock() ).Times( 1 );
   EXPECT_CALL( m_lockableMock, unlock() ).Times( 1 );
   
   result = serialDevice->sendAsync( nullptr, lib::SerialDevice::TX_BUFFER_SIZE + 1 );
   EXPECT_EQ( result, LibErrorCodes::eSERIAL_DEVICE_TX_MSG_TOO_LONG );
}

TEST_F( SerialDeviceTest, test_wait_send_complete_fails_if_not_sending )
{
   auto serialDevice = getSerialDevice();
   
   EXPECT_CALL( m_lockableMock, initialize() ).Times( 1 ).WillOnce( testing::Return( LibErrorCodes::eOK ) );
   EXPECT_CALL( m_semaphoreMock, initialize( 1, 0 ) ).WillOnce( testing::Return( LibErrorCodes::eOK ) );
   EXPECT_CALL( m_semaphoreMock, initialize( sizeof( g_rxBuffer ), 0 ) ).WillOnce( testing::Return( LibErrorCodes::eOK ) );
   
   auto result = serialDevice->initialize();
   EXPECT_EQ( result, LibErrorCodes::eOK );
   
   result = serialDevice->waitSendComplete( 100 );
   EXPECT_EQ( result, LibErrorCodes::eSERIAL_DEVICE_NO_SEND_ACTIVE );
}

TEST_F( SerialDeviceTest, test_wait_send_complete_succeeds )
{
   auto serialDevice = getSerialDevice();
   
   EXPECT_CALL( m_lockableMock, initialize() ).Times( 1 ).WillOnce( testing::Return( LibErrorCodes::eOK ) );
   EXPECT_CALL( m_semaphoreMock, initialize( 1, 0 ) ).WillOnce( testing::Return( LibErrorCodes::eOK ) );
   EXPECT_CALL( m_semaphoreMock, initialize( sizeof( g_rxBuffer ), 0 ) ).WillOnce( testing::Return( LibErrorCodes::eOK ) );
   
   auto result = serialDevice->initialize();
   EXPECT_EQ( result, LibErrorCodes::eOK );
   
   EXPECT_CALL( m_lockableMock, lock() ).Times( 1 );
   EXPECT_CALL( m_lockableMock, unlock() ).Times( 1 );
   
	const uint8_t testData[] = { 0x01, 0x02, 0x03, 0x04, 0x05 };
   
   g_senderCalled = false;
   result = serialDevice->sendAsync( testData, sizeof( testData ) );
   EXPECT_EQ( result, LibErrorCodes::eOK );
	EXPECT_TRUE( g_senderCalled );

   uint32_t timeout = 100;
	EXPECT_CALL( m_semaphoreMock, get( timeout ) ).Times( 1 ).WillOnce( testing::Return( LibErrorCodes::eOK ) );

   result = serialDevice->waitSendComplete( timeout );
   EXPECT_EQ( result, LibErrorCodes::eOK );
}

TEST_F( SerialDeviceTest, test_push_rx_byte_fails_if_not_initialized )
{
   auto serialDevice = getSerialDevice();
   
   auto result = serialDevice->pushRxByte( 0x00 );
   EXPECT_EQ( result, LibErrorCodes::eSERIAL_DEVICE_NOT_INITIALIZED );
}

TEST_F( SerialDeviceTest, test_push_rx_byte_succeeds )
{
   auto serialDevice = getSerialDevice();
   
   EXPECT_CALL( m_lockableMock, initialize() ).Times( 1 ).WillOnce( testing::Return( LibErrorCodes::eOK ) );
   EXPECT_CALL( m_semaphoreMock, initialize( 1, 0 ) ).WillOnce( testing::Return( LibErrorCodes::eOK ) );
   EXPECT_CALL( m_semaphoreMock, initialize( sizeof( g_rxBuffer ), 0 ) ).WillOnce( testing::Return( LibErrorCodes::eOK ) );

   auto result = serialDevice->initialize();
   EXPECT_EQ( result, LibErrorCodes::eOK );
   
   EXPECT_CALL( m_semaphoreMock, putISR() ).Times( 1 );
   
   result = serialDevice->pushRxByte( 0x00 );
   EXPECT_EQ( result, LibErrorCodes::eOK );
}

TEST_F( SerialDeviceTest, test_get_rx_byte_succeeds )
{
   auto serialDevice = getSerialDevice();

   EXPECT_CALL( m_lockableMock, initialize() ).Times( 1 ).WillOnce( testing::Return( LibErrorCodes::eOK ) );
   EXPECT_CALL( m_semaphoreMock, initialize( 1, 0 ) ).WillOnce( testing::Return( LibErrorCodes::eOK ) );
   EXPECT_CALL( m_semaphoreMock, initialize( sizeof( g_rxBuffer ), 0 ) ).WillOnce( testing::Return( LibErrorCodes::eOK ) );
   
   serialDevice->initialize();

   EXPECT_CALL( m_semaphoreMock, putISR() ).Times( 1 );

   uint8_t data = 1;
   auto result = serialDevice->pushRxByte( data );
   EXPECT_EQ( result, LibErrorCodes::eOK );

   uint32_t timeout = 100;
	EXPECT_CALL( m_semaphoreMock, get( timeout ) ).Times( 1 ).WillOnce( testing::Return( LibErrorCodes::eOK ) );

   uint8_t dataReceived = 0;
   result = serialDevice->getRxByte( dataReceived, timeout );
   EXPECT_EQ( result, LibErrorCodes::eOK );
	EXPECT_EQ( dataReceived, data );
}

TEST_F( SerialDeviceTest, test_get_rx_byte_fails_if_no_data )
{
   auto serialDevice = getSerialDevice();

   EXPECT_CALL( m_lockableMock, initialize() ).Times( 1 ).WillOnce( testing::Return( LibErrorCodes::eOK ) );
   EXPECT_CALL( m_semaphoreMock, initialize( 1, 0 ) ).WillOnce( testing::Return( LibErrorCodes::eOK ) );
   EXPECT_CALL( m_semaphoreMock, initialize( sizeof( g_rxBuffer ), 0 ) ).WillOnce( testing::Return( LibErrorCodes::eOK ) );
   
   serialDevice->initialize();

   uint32_t timeout = 100;
   EXPECT_CALL( m_semaphoreMock, get( timeout ) ).Times( 1 ).WillOnce( testing::Return( LibErrorCodes::eOK ) );
   
   uint8_t dataReceived = 0;
   auto result = serialDevice->getRxByte( dataReceived, timeout );
   EXPECT_EQ( result, LibErrorCodes::eRING_BUFFER_EMPTY );
}

TEST_F( SerialDeviceTest, test_flush_rx_buffer_succeeds )
{
   auto serialDevice = getSerialDevice();
   
	std::fill( g_rxBuffer, g_rxBuffer + sizeof( g_rxBuffer ), 0xFF );

   EXPECT_CALL( m_lockableMock, lock() ).Times( 1 );
   EXPECT_CALL( m_lockableMock, unlock() ).Times( 1 );
   
   serialDevice->flushRxBuffer();

   for( const auto& byte : g_rxBuffer )
   {
      EXPECT_EQ( byte, 0x00 );
	}
}