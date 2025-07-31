/************************************************************************************************************
 * 
 * @file message_passer_test.cpp
 * @brief Unit tests for the MessagePasser class.
 * @details This file contains tests for the initialization, message management, and synchronization functionalities of the MessagePasser class making use of Google Test framework and mocks.
 * 
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-07-27
 * @version 1.0
 * 
 ************************************************************************************************************/

/************************************************** Includes ************************************************/
#include "message_passer.h"
#include "mock_FreeRTOS.h"
#include "mock_Lockable.h"
#include <gtest/gtest.h>

/************************************************ Global Variables ******************************************/
FreeRTOSMock* g_mockFreeRTOS;
LockableMock* g_mockLockable;

/************************************************ Static Variables ******************************************/
static messsage_t g_messageBuffer[MessagePasser::NUM_BUFFER_MAX] = { 0 };

/************************************************** Test Fixture ********************************************/
class MessageParserTest : public ::testing::Test
{
protected:
   void SetUp() override
   {
      g_mockFreeRTOS = &m_mockFreeRTOS;
      g_mockLockable = &m_mockLockable;
   }

   void TearDown() override
   {
      testing::Mock::VerifyAndClear( &m_mockFreeRTOS );
      testing::Mock::VerifyAndClear( &m_mockLockable );
   }

public:
   static constexpr uint32_t  RANDOM_PTR_ADDR      = 0x1234;
   static constexpr bool      INIT_WHEN_CREATED    = true;
   static constexpr bool      LEAVE_UNINITIALIZED  = false;

   FreeRTOSMock m_mockFreeRTOS;
   LockableMock m_mockLockable;

   void initializeMessagePasser( MessagePasser& passer, messsage_t* buffer, uint32_t size_buffer, uint32_t num_receivers )
   {
      EXPECT_CALL( m_mockLockable, initialize() ).WillRepeatedly( ::testing::Return( true ) );
      EXPECT_CALL( m_mockFreeRTOS, xSemaphoreCreateMutex() ).WillRepeatedly( ::testing::Return( reinterpret_cast<SemaphoreHandle_t>( RANDOM_PTR_ADDR ) ) );
      EXPECT_CALL( m_mockFreeRTOS, xQueueCreateCountingSemaphore( MessagePasser::NUM_BUFFER_MAX, 0 ) ).WillRepeatedly( ::testing::Return( reinterpret_cast<QueueHandle_t>( RANDOM_PTR_ADDR ) ) );
      
      auto result = passer.initialize( *g_mockLockable, buffer, size_buffer, num_receivers );
      EXPECT_EQ( result, ErrorCodes::OK );
   }
};

/************************************************** Tests ***************************************************/
/**
 * @brief Test the initialization of the MessagePasser works correctly.
 */
TEST_F( MessageParserTest, initialize_works_correctly )
{
   MessagePasser passer{};

   EXPECT_CALL( m_mockLockable, initialize() ).WillRepeatedly( ::testing::Return( true ) );
   EXPECT_CALL( m_mockFreeRTOS, xSemaphoreCreateMutex() ).WillRepeatedly( ::testing::Return( reinterpret_cast<SemaphoreHandle_t>( RANDOM_PTR_ADDR ) ) );
   EXPECT_CALL( m_mockFreeRTOS, xQueueCreateCountingSemaphore( MessagePasser::NUM_BUFFER_MAX, 0 ) ).WillRepeatedly( ::testing::Return( reinterpret_cast<QueueHandle_t>( RANDOM_PTR_ADDR ) ) );

   auto result = passer.initialize( *g_mockLockable, g_messageBuffer, MessagePasser::NUM_BUFFER_MAX, MessagePasser::NUM_RECEIVER_MAX );
   EXPECT_EQ( result, ErrorCodes::OK );
}

/**
 * @brief Test the initialization of the MessagePasser class fails when no buffer is given.
 */
TEST_F( MessageParserTest, initialize_fails_on_no_buffer )
{
   MessagePasser passer{};
   auto result = passer.initialize( *g_mockLockable, nullptr, 1, MessagePasser::NUM_RECEIVER_MAX );
   EXPECT_EQ( result, ErrorCodes::NO_BUFFER_GIVEN );	

   result = passer.initialize( *g_mockLockable, g_messageBuffer, 0, MessagePasser::NUM_RECEIVER_MAX );
   EXPECT_EQ( result, ErrorCodes::NO_BUFFER_GIVEN );	
}

/**
 * @brief Test the initialization of the MessagePasser class fails when there are no receivers given.
 */
TEST_F( MessageParserTest, initialize_fails_on_no_receiver )
{
   MessagePasser passer{};
   auto result = passer.initialize( *g_mockLockable, g_messageBuffer, MessagePasser::NUM_BUFFER_MAX, 0 );
   EXPECT_EQ( result, ErrorCodes::NO_RECEIVERS_GIVEN );	
}

/**
 * @brief Test the initialization of the MessagePasser class fails when the buffer size is too big.
 */
TEST_F( MessageParserTest, initialize_fails_on_buffer_size_too_big )
{ 
   MessagePasser passer{};
   auto result = passer.initialize( *g_mockLockable, g_messageBuffer, MessagePasser::NUM_BUFFER_MAX + 1, MessagePasser::NUM_RECEIVER_MAX );
   EXPECT_EQ( result, ErrorCodes::BUFFER_SIZE_TOO_BIG );	
}

/**
 * @brief Test the initialization of the MessagePasser class fails when the number of receivers is too many.
 */
TEST_F( MessageParserTest, initialize_fails_on_too_many_receivers )
{
   MessagePasser passer{};
   auto result = passer.initialize( *g_mockLockable, g_messageBuffer, MessagePasser::NUM_BUFFER_MAX, MessagePasser::NUM_RECEIVER_MAX + 1 );
   EXPECT_EQ( result, ErrorCodes::RECEIVERS_TOO_MANY );	
}

/**
 * @brief Test the initialization of the MessagePasser class fails when the mutex initialization fails.
 */
TEST_F( MessageParserTest, initialize_fails_on_mutex_init_failed )
{
   MessagePasser passer{};
   EXPECT_CALL( m_mockLockable, initialize() ).WillRepeatedly( ::testing::Return( false ) );

   auto result = passer.initialize( *g_mockLockable, g_messageBuffer, MessagePasser::NUM_BUFFER_MAX, MessagePasser::NUM_RECEIVER_MAX );
   EXPECT_EQ( result, ErrorCodes::MUTEX_INIT_FAILED );	
}

/**
 * @brief Test the initialization of the MessagePasser class fails when the semaphore initialization fails.
 */
TEST_F( MessageParserTest, initialize_fails_on_sem_init_failed )
{
   MessagePasser passer{};

   EXPECT_CALL( m_mockLockable, initialize() ).WillRepeatedly( ::testing::Return( true ) );
   EXPECT_CALL( m_mockFreeRTOS, xSemaphoreCreateMutex() ).WillRepeatedly( ::testing::Return( reinterpret_cast<SemaphoreHandle_t>( RANDOM_PTR_ADDR ) ) );
   EXPECT_CALL( m_mockFreeRTOS, xQueueCreateCountingSemaphore( MessagePasser::NUM_BUFFER_MAX, 0 ) ).WillRepeatedly( ::testing::Return( reinterpret_cast<QueueHandle_t>( nullptr ) ) );

   auto result = passer.initialize( *g_mockLockable, g_messageBuffer, MessagePasser::NUM_BUFFER_MAX, MessagePasser::NUM_RECEIVER_MAX );

   EXPECT_EQ( result, ErrorCodes::MSG_SEMAPHORE_INIT_FAILED );	
}

/**
 * @brief Test the new_message function returns a null pointer when the MessagePasser is not initialized.
 */
TEST_F( MessageParserTest, new_message_returns_null_pointer_when_not_initialized )
{
   MessagePasser passer{};

   auto *msg = passer.new_message();
   EXPECT_EQ( msg, nullptr );
}

/**
 * @brief Test the new_message function returns a null pointer when the MessagePasser is initialized with an invalid buffer size.
 */
TEST_F( MessageParserTest, new_message_returns_null_pointer_when_initialize_failed )
{
   MessagePasser passer{};

   //!< Initialize fails with the null buffer
   auto result = passer.initialize( *g_mockLockable, nullptr, MessagePasser::NUM_BUFFER_MAX, MessagePasser::NUM_RECEIVER_MAX );
   EXPECT_EQ( result, ErrorCodes::NO_BUFFER_GIVEN );

   //!< Initialize fails with a zero buffer size
   result = passer.initialize( *g_mockLockable, g_messageBuffer, 0, MessagePasser::NUM_RECEIVER_MAX );
   EXPECT_EQ( result, ErrorCodes::NO_BUFFER_GIVEN );

   //!< Initialize fails with a too big buffer size
   result = passer.initialize( *g_mockLockable, g_messageBuffer, MessagePasser::NUM_BUFFER_MAX + 1, MessagePasser::NUM_RECEIVER_MAX);
   EXPECT_EQ( result, ErrorCodes::BUFFER_SIZE_TOO_BIG );

   auto *msg = passer.new_message();
   EXPECT_EQ( msg, nullptr );
}

/**
 * @brief Test the new_message function returns a null pointer when there is no buffer available.
 */
TEST_F( MessageParserTest, new_message_returns_null_pointer_when_there_is_no_buffer_available )
{
   constexpr uint32_t BUFFER_SIZE = 1;
   
   MessagePasser passer{};
   initializeMessagePasser( passer, g_messageBuffer, BUFFER_SIZE, MessagePasser::NUM_RECEIVER_MAX );

   EXPECT_EQ( passer.get_buffer_available(), BUFFER_SIZE );

   //!< Prepare the mock functions
   EXPECT_CALL( m_mockLockable, lock() ).WillRepeatedly( ::testing::Return( ) );
   EXPECT_CALL( m_mockLockable, unlock() ).WillRepeatedly( ::testing::Return( ) );
   EXPECT_CALL( m_mockFreeRTOS, xQueueSemaphoreTake( ::testing::_, ::testing::_ ) ).WillRepeatedly( ::testing::Return( pdTRUE ) );
   EXPECT_CALL( m_mockFreeRTOS, xQueueGenericSend( ::testing::_, ::testing::_, ::testing::_, ::testing::_ ) ).WillRepeatedly( ::testing::Return( pdTRUE ) );

   auto *msg = passer.new_message();
   EXPECT_EQ( msg != nullptr, true );
   EXPECT_EQ( passer.get_buffer_available(), 0 );

   msg = passer.new_message();
   EXPECT_EQ( msg, nullptr );
   EXPECT_EQ( passer.get_buffer_available(), 0 );
}

/**
 * @brief Test the new_message function works correctly and returns valid message pointers.
 */
TEST_F( MessageParserTest, new_message_works_correctly )
{
   MessagePasser passer{};
   initializeMessagePasser( passer, g_messageBuffer, MessagePasser::NUM_BUFFER_MAX, MessagePasser::NUM_RECEIVER_MAX );
   
   //!< Prepare the mock functions
   EXPECT_CALL( m_mockLockable, lock() ).WillRepeatedly( ::testing::Return( ) );
   EXPECT_CALL( m_mockLockable, unlock() ).WillRepeatedly( ::testing::Return( ) );
   EXPECT_CALL( m_mockFreeRTOS, xQueueSemaphoreTake( ::testing::_, ::testing::_ ) ).WillRepeatedly( ::testing::Return( pdTRUE ) );
   EXPECT_CALL( m_mockFreeRTOS, xQueueGenericSend( ::testing::_, ::testing::_, ::testing::_, ::testing::_ ) ).WillRepeatedly( ::testing::Return( pdTRUE ) );

   //!< Get new message pointers
   EXPECT_EQ( passer.get_buffer_available(), MessagePasser::NUM_BUFFER_MAX );
   for( unsigned i = 0; i < MessagePasser::NUM_BUFFER_MAX; ++i )
   {
      auto *msg = passer.new_message();
      ASSERT_TRUE( ( msg != nullptr ), true );
      ASSERT_EQ( passer.get_buffer_available(), MessagePasser::NUM_BUFFER_MAX - i - 1 );
   }
   EXPECT_EQ( passer.get_buffer_available(), 0u );

   //!< Trying to get more messages than the buffer size should return nullptr
   auto *msg = passer.new_message();
   EXPECT_EQ( msg, nullptr );
}

/**
 * @brief Tests the delete_message function works correctly.
 */
TEST_F( MessageParserTest, delete_message_works_correctly )
{
   constexpr uint32_t BUFFER_SIZE = 1;

   MessagePasser passer{};
   initializeMessagePasser( passer, g_messageBuffer, BUFFER_SIZE, MessagePasser::NUM_RECEIVER_MAX );

   //!< Prepare the mock functions
   EXPECT_CALL( m_mockLockable, lock() ).WillRepeatedly( ::testing::Return( ) );
   EXPECT_CALL( m_mockLockable, unlock() ).WillRepeatedly( ::testing::Return( ) );
   EXPECT_CALL( m_mockFreeRTOS, xQueueSemaphoreTake( ::testing::_, ::testing::_ ) ).WillRepeatedly( ::testing::Return( pdTRUE ) );
   EXPECT_CALL( m_mockFreeRTOS, xQueueGenericSend( ::testing::_, ::testing::_, ::testing::_, ::testing::_ ) ).WillRepeatedly( ::testing::Return( pdTRUE ) );

   //!< Get a new message pointer
   EXPECT_EQ( passer.get_buffer_available(), BUFFER_SIZE );
   
   auto *msg = passer.new_message();
   EXPECT_EQ( msg != nullptr, true );
   EXPECT_EQ( passer.get_buffer_available(), BUFFER_SIZE - 1 );

   //!< Return the message. Deleting repeatedly should also be fine.
   passer.delete_message( msg );
   EXPECT_EQ( passer.get_buffer_available(), BUFFER_SIZE );
   passer.delete_message( msg );
   EXPECT_EQ( passer.get_buffer_available(), BUFFER_SIZE );
}

/**
 * @brief Test the send function fails when the MessagePasser is not initialized.
 */
TEST_F( MessageParserTest, send_fails_when_not_initialized )
{
   MessagePasser passer{};
   
   //!< Trying to send a message when the passer is not initialized returns an error
   ReceiverId id = 0;

   auto result = passer.send( id, nullptr );
   EXPECT_EQ( result, ErrorCodes::NOT_INITIALIZED );
}

/**
 * @brief Test the send function fails when the MessagePasser's initialization failed.
 */
TEST_F( MessageParserTest, send_fails_when_initialize_failed )
{
   MessagePasser passer{};

   auto result = passer.initialize( *g_mockLockable, g_messageBuffer, MessagePasser::NUM_BUFFER_MAX + 1, MessagePasser::NUM_RECEIVER_MAX );
   EXPECT_EQ( result, ErrorCodes::BUFFER_SIZE_TOO_BIG );

   ReceiverId id = 0;

   //!< Trying to send a message when the passer is not initialized returns an error
   result = passer.send( id, nullptr );
   EXPECT_EQ( result, ErrorCodes::NOT_INITIALIZED );
}

/**
 * @brief Test the send function fails when the message pointer is not valid.
 */
TEST_F( MessageParserTest, send_fails_when_message_pointer_is_not_valid )
{
   MessagePasser passer{};
   initializeMessagePasser( passer, g_messageBuffer, MessagePasser::NUM_BUFFER_MAX, MessagePasser::NUM_RECEIVER_MAX );

   ReceiverId id = 0;

   //!< Trying to send a message with a nullptr pointer returns an error
   auto result = passer.send( id, nullptr );
   EXPECT_EQ( result, ErrorCodes::NO_MESSAGE_INDEX_IN_BUFFER );

   //!< Trying to send a message with an invalid pointer returns an error
   result = passer.send( id, reinterpret_cast<messsage_t*>( 0x12345678 ) );
   EXPECT_EQ( result, ErrorCodes::NO_MESSAGE_INDEX_IN_BUFFER );
}

/**
 * @brief Test the send function fails when the receiver ID is out of range.
 */
TEST_F( MessageParserTest, send_fails_when_receiver_is_out_of_range )
{
   constexpr uint32_t BUFFER_SIZE = 1;
   constexpr uint32_t NUM_RECEIVERS = 1;

   MessagePasser passer{};
   initializeMessagePasser( passer, g_messageBuffer, BUFFER_SIZE, NUM_RECEIVERS );

   //!< Trying to send a message when the passer is not initialized returns an error
   ReceiverId id = NUM_RECEIVERS; // Out of range receiver id
   
   auto result = passer.send( id, nullptr );
   EXPECT_EQ( result, ErrorCodes::DESTINATION_ID_OUT_OF_RANGE );
}

/**
 * @brief Test the send function fails when a deleted message is reused.
 */
TEST_F( MessageParserTest, send_fails_if_deleted_message_is_reused )
{
   MessagePasser passer{};
   initializeMessagePasser( passer, g_messageBuffer, MessagePasser::NUM_BUFFER_MAX, MessagePasser::NUM_RECEIVER_MAX );

   //!< Prepare the mock functions
   EXPECT_CALL( m_mockLockable, lock() ).WillRepeatedly( ::testing::Return( ) );
   EXPECT_CALL( m_mockLockable, unlock() ).WillRepeatedly( ::testing::Return( ) );
   EXPECT_CALL( m_mockFreeRTOS, xQueueSemaphoreTake( ::testing::_, ::testing::_ ) ).WillRepeatedly( ::testing::Return( pdTRUE ) );
   EXPECT_CALL( m_mockFreeRTOS, xQueueGenericSend( ::testing::_, ::testing::_, ::testing::_, ::testing::_ ) ).WillRepeatedly( ::testing::Return( pdTRUE ) );

   auto *msg = passer.new_message();
   EXPECT_EQ( msg != nullptr, true );

   ReceiverId id = 0;

   //!< Send message to a valid receiver
   auto result = passer.send( id, msg );
   EXPECT_EQ( result, ErrorCodes::OK );

   passer.delete_message( msg );

   //!< Send message to a valid receiver
   result = passer.send( id, msg );
   EXPECT_EQ( result, ErrorCodes::INVALID_MESSAGE_POINTER );
}

/**
 * @brief Test the send function fails when the message, which hasn't been deleted yet, is reused.
 */
TEST_F( MessageParserTest, send_fails_if_the_message_is_reused )
{
   constexpr uint32_t BUFFER_SIZE = 1;
   constexpr uint32_t NUM_RECEIVERS = 2;

   MessagePasser passer{};
   initializeMessagePasser( passer, g_messageBuffer, BUFFER_SIZE, NUM_RECEIVERS );

   //!< Prepare the mock functions
   EXPECT_CALL( m_mockLockable, lock() ).WillRepeatedly( ::testing::Return( ) );
   EXPECT_CALL( m_mockLockable, unlock() ).WillRepeatedly( ::testing::Return( ) );
   EXPECT_CALL( m_mockFreeRTOS, xQueueSemaphoreTake( ::testing::_, ::testing::_ ) ).WillRepeatedly( ::testing::Return( pdTRUE ) );
   EXPECT_CALL( m_mockFreeRTOS, xQueueGenericSend( ::testing::_, ::testing::_, ::testing::_, ::testing::_ ) ).WillRepeatedly( ::testing::Return( pdTRUE ) );

   auto *msg = passer.new_message();
   EXPECT_EQ( msg != nullptr, true );

   ReceiverId id1 = 0;
   ReceiverId id2 = 1;

   //!< Send message to a valid receiver
   auto result = passer.send( id1, msg );
   EXPECT_EQ( result, ErrorCodes::OK );

   //!< Sending the same message to the receiver again is not allowed.
   result = passer.send( id1, msg );
   EXPECT_EQ( result, ErrorCodes::INVALID_MESSAGE_POINTER );

   //!< Sending the same message to another receiver is not allowed.
   result = passer.send( id2, msg );
   EXPECT_EQ( result, ErrorCodes::INVALID_MESSAGE_POINTER );
}

/**
 * @brief Test the send function works correctly.
 */
TEST_F( MessageParserTest, send_works_correctly )
{
   constexpr uint32_t BUFFER_SIZE = 1;
   constexpr uint32_t NUM_RECEIVERS = 2;

   MessagePasser passer{};
   initializeMessagePasser( passer, g_messageBuffer, BUFFER_SIZE, NUM_RECEIVERS );

   //!< Prepare the mock functions
   EXPECT_CALL( m_mockLockable, lock() ).WillRepeatedly( ::testing::Return( ) );
   EXPECT_CALL( m_mockLockable, unlock() ).WillRepeatedly( ::testing::Return( ) );
   EXPECT_CALL( m_mockFreeRTOS, xQueueSemaphoreTake( ::testing::_, ::testing::_ ) ).WillRepeatedly( ::testing::Return( pdTRUE ) );
   EXPECT_CALL( m_mockFreeRTOS, xQueueGenericSend( ::testing::_, ::testing::_, ::testing::_, ::testing::_ ) ).WillRepeatedly( ::testing::Return( pdTRUE ) );

   auto *msg = passer.new_message();
   EXPECT_EQ( msg != nullptr, true );

   ReceiverId id1 = 0;
   ReceiverId id2 = 1;

   //!< Send message to a valid receiver
   auto result = passer.send( id1, msg );
   EXPECT_EQ( result, ErrorCodes::OK );
}

/**
 * @brief Test the recv function fails when the MessagePasser is not initialized.
 */
TEST_F( MessageParserTest, recv_fails_when_not_initialized )
{
   MessagePasser passer{};

   ReceiverId id = 0;

   messsage_t* msg = nullptr;
   auto result = passer.recv( id, &msg );
   EXPECT_EQ( result, ErrorCodes::NOT_INITIALIZED );
   EXPECT_EQ( msg, nullptr );
}

/**
 * @brief Test the recv function fails immediately when the receiver ID is out of range.
 */
TEST_F( MessageParserTest, recv_fails_when_receiver_is_out_of_range )
{
   constexpr uint32_t BUFFER_SIZE = 1;
   constexpr uint32_t NUM_RECEIVERS = 1;

   MessagePasser passer{};
   initializeMessagePasser( passer, g_messageBuffer, BUFFER_SIZE, NUM_RECEIVERS );

   ReceiverId id = NUM_RECEIVERS; // Out of range receiver id

   messsage_t* msg = nullptr;
   auto result = passer.recv( id, &msg );
   EXPECT_EQ( result, ErrorCodes::DESTINATION_ID_OUT_OF_RANGE );
   EXPECT_EQ( msg, nullptr );
}

/**
 * @brief Test the recv function fails when taking the semaphore times out.
 */
TEST_F( MessageParserTest, recv_fails_when_taking_semaphore_times_out )
{
   constexpr uint32_t BUFFER_SIZE = 1;
   constexpr uint32_t NUM_RECEIVERS = 1;

   MessagePasser passer{};
   initializeMessagePasser( passer, g_messageBuffer, BUFFER_SIZE, NUM_RECEIVERS );

   //!< Taking the semaphore should fail
   EXPECT_CALL( m_mockFreeRTOS, xQueueSemaphoreTake( ::testing::_, ::testing::_ ) ).WillOnce( ::testing::Return( pdFALSE ) );

   ReceiverId id = 0;

   messsage_t* msg = nullptr;
   auto result = passer.recv( id, &msg );
   EXPECT_EQ( result, ErrorCodes::MSG_SEMAPHORE_TAKE_TIMEOUT );
   EXPECT_EQ( msg, nullptr );
}

/**
 * @brief Test the recv function fails when there is no message for the receiver.
 * @details Note that this scenario assumes that the semaphore counting works correctly, but there is no message available for the receiver.
 */
TEST_F( MessageParserTest, recv_fails_when_there_is_no_message_for_receiver )
{
   constexpr uint32_t BUFFER_SIZE = 1;
   constexpr uint32_t NUM_RECEIVERS = 1;

   MessagePasser passer{};
   initializeMessagePasser( passer, g_messageBuffer, BUFFER_SIZE, NUM_RECEIVERS );

   /* NOTE:
    * In this scenario, the signaling by the semaphore works correctly. */

   //!< Prepare the mock functions
   EXPECT_CALL( m_mockLockable, lock() ).WillRepeatedly( ::testing::Return( ) );
   EXPECT_CALL( m_mockLockable, unlock() ).WillRepeatedly( ::testing::Return( ) );
   EXPECT_CALL( m_mockFreeRTOS, xQueueSemaphoreTake( ::testing::_, ::testing::_ ) ).WillRepeatedly( ::testing::Return( pdTRUE ) );
   EXPECT_CALL( m_mockFreeRTOS, xQueueGenericSend( ::testing::_, ::testing::_, ::testing::_, ::testing::_ ) ).WillRepeatedly( ::testing::Return( pdTRUE ) );

   //!< Try to receive a message when there is no message sent to the receiver
   ReceiverId id = 0;

   messsage_t* msg = nullptr;
   auto result = passer.recv( id, &msg );
   EXPECT_EQ( result, ErrorCodes::NO_MESSAGE_FOUND_FOR_DESTINATION );
   EXPECT_EQ( msg, nullptr );
}

/**
 * @brief Test the recv function works correctly between a sender and a receiver.
 */
TEST_F( MessageParserTest, recv_works_correctly )
{
   constexpr uint32_t BUFFER_SIZE = 1;
   constexpr uint32_t NUM_RECEIVERS = 1;

   MessagePasser passer{};
   initializeMessagePasser( passer, g_messageBuffer, BUFFER_SIZE, NUM_RECEIVERS );

   //!< Mutex and Semaphore should work correctly
   EXPECT_CALL( m_mockLockable, lock() ).WillRepeatedly( ::testing::Return( ) );
   EXPECT_CALL( m_mockLockable, unlock() ).WillRepeatedly( ::testing::Return( ) );
   EXPECT_CALL( m_mockFreeRTOS, xQueueSemaphoreTake( ::testing::_, ::testing::_ ) ).WillRepeatedly( ::testing::Return( pdTRUE ) );
   EXPECT_CALL( m_mockFreeRTOS, xQueueGenericSend( ::testing::_, ::testing::_, ::testing::_, ::testing::_ ) ).WillRepeatedly( ::testing::Return( pdTRUE ) );
   
   auto *msg = passer.new_message();
   EXPECT_EQ( msg != nullptr, true );

   ReceiverId id = 0;

   //!< Send a message to a valid receiver
   auto result = passer.send( id, msg );
   EXPECT_EQ( result, ErrorCodes::OK );
   EXPECT_EQ( passer.get_buffer_available(), BUFFER_SIZE - 1 );

   //!< Receive the message
   messsage_t* received_msg = nullptr;
   result = passer.recv( id, &received_msg );
   EXPECT_EQ( result, ErrorCodes::OK );
   EXPECT_EQ( received_msg, msg );
   EXPECT_EQ( passer.get_buffer_available(), BUFFER_SIZE - 1 );

   //!< A message becomes not available after it has been received
   EXPECT_CALL( m_mockFreeRTOS, xQueueSemaphoreTake( ::testing::_, ::testing::_ ) ).WillOnce( ::testing::Return( pdFALSE ) );

   received_msg = nullptr;
   result = passer.recv( id, &received_msg );
   EXPECT_EQ( result, ErrorCodes::MSG_SEMAPHORE_TAKE_TIMEOUT );
}

/**
 * @brief Test the recv function works correctly when multiple messages are buffered between a sender and a receiver.
 */
TEST_F( MessageParserTest, recv_works_correctly_when_multiple_messages_are_buffered )
{
   MessagePasser passer{};
   initializeMessagePasser( passer, g_messageBuffer, MessagePasser::NUM_BUFFER_MAX, MessagePasser::NUM_RECEIVER_MAX );

   //!< Prepare the mock functions
   EXPECT_CALL( m_mockLockable, lock() ).WillRepeatedly( ::testing::Return( ) );
   EXPECT_CALL( m_mockLockable, unlock() ).WillRepeatedly( ::testing::Return( ) );
   EXPECT_CALL( m_mockFreeRTOS, xQueueSemaphoreTake( ::testing::_, ::testing::_ ) ).WillRepeatedly( ::testing::Return( pdTRUE ) );
   EXPECT_CALL( m_mockFreeRTOS, xQueueGenericSend( ::testing::_, ::testing::_, ::testing::_, ::testing::_ ) ).WillRepeatedly( ::testing::Return( pdTRUE ) );

   ReceiverId id = 0;

   EXPECT_EQ( passer.get_buffer_available(), MessagePasser::NUM_BUFFER_MAX );

   //!< Send the messages to the receiver until the buffer is full
   for( unsigned i = 0; i < MessagePasser::NUM_BUFFER_MAX; ++i )
   {
      auto *msg = passer.new_message();
      EXPECT_EQ( msg != nullptr, true );

      auto result = passer.send( id, msg );
      EXPECT_EQ( result, ErrorCodes::OK );
   }

   EXPECT_EQ( passer.get_buffer_available(), 0 );

   //!< Receive the messages from the receiver
   for( unsigned i = 0; i < MessagePasser::NUM_BUFFER_MAX; ++i )
   {
      messsage_t* received_msg = nullptr;
      auto result = passer.recv( id, &received_msg );
      EXPECT_EQ( result, ErrorCodes::OK );
      EXPECT_EQ( received_msg, &g_messageBuffer[i] );

      passer.delete_message( received_msg );
      EXPECT_EQ( passer.get_buffer_available(), i + 1 );
   }

   EXPECT_EQ( passer.get_buffer_available(), MessagePasser::NUM_BUFFER_MAX );
}

/**
 * @brief Test the recv function works correctly in a multi-threaded scenario involding more than two threads.
 */
TEST_F( MessageParserTest, recv_works_correctly_in_multi_threaded_scenario )
{
   constexpr uint32_t BUFFER_SIZE = 2;
   constexpr uint32_t NUM_RECEIVERS = 2;

   MessagePasser passer{};
   initializeMessagePasser( passer, g_messageBuffer, BUFFER_SIZE, NUM_RECEIVERS );

   //!< Prepare the mock functions
   EXPECT_CALL( m_mockLockable, lock() ).WillRepeatedly( ::testing::Return( ) );
   EXPECT_CALL( m_mockLockable, unlock() ).WillRepeatedly( ::testing::Return( ) );
   EXPECT_CALL( m_mockFreeRTOS, xQueueSemaphoreTake( ::testing::_, ::testing::_ ) ).WillRepeatedly( ::testing::Return( pdTRUE ) );
   EXPECT_CALL( m_mockFreeRTOS, xQueueGenericSend( ::testing::_, ::testing::_, ::testing::_, ::testing::_ ) ).WillRepeatedly( ::testing::Return( pdTRUE ) );

   ReceiverId id1 = 0;
   ReceiverId id2 = 1;

   //!< Create two messages
   auto *msg1 = passer.new_message();
   EXPECT_EQ( msg1 != nullptr, true );

   auto *msg2 = passer.new_message();
   EXPECT_EQ( msg2 != nullptr, true );

   //!< Send messages to both receivers
   auto result = passer.send( id1, msg1 );
   EXPECT_EQ( result, ErrorCodes::OK );
   
   result = passer.send( id2, msg2 );
   EXPECT_EQ( result, ErrorCodes::OK );

   //!< Receive messages from both receivers
   messsage_t* received_msg1 = nullptr;
   result = passer.recv( id1, &received_msg1 );
   EXPECT_EQ( result, ErrorCodes::OK );
   EXPECT_EQ( received_msg1, msg1 );

   messsage_t* received_msg2 = nullptr;   
   result = passer.recv( id2, &received_msg2 );
   EXPECT_EQ( result, ErrorCodes::OK );
   EXPECT_EQ( received_msg2, msg2 );

   EXPECT_EQ( passer.get_buffer_available(), BUFFER_SIZE - 2 );
}
