/************************************************************************************************************
 * 
 * @file message_passer.cpp
 * @brief Source file for the MessagePasser class, which provides a message passing mechanism between tasks or threads.
 * @details This class allows for the creation, sending, receiving, and deletion of messages in a thread-safe manner.
 *          This file is part of a larger project that implements a message passing system for FreeRTOS.
 * 
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-07-27
 * @version 1.0
 * 
 ************************************************************************************************************/

/************************************************** Includes ************************************************/
#include "message_passer.h"

/********************************************* Function Definitions *****************************************/
/**
 * @brief Initializes the MessagePasser with a lockable resource, a buffer for messages, and the number of receivers.
 * @details This function sets up the message passer by initializing the lockable resource, allocating semaphores for each receiver, and preparing the message buffer.
 *          The buffer used must solely be used by the MessagePasser, and it should not be modified by other parts of the code.
 * 
 * @param lockable a reference to an ILockable object that will be used for synchronization.
 * @param buffer a pointer to the message buffer that will be used to store messages.
 * @param size_buffer the size of the message buffer.
 * @param num_receivers the number of receivers that will be able to receive messages.
 * @return int an error code indicating the result of the initialization.
 */
int MessagePasser::initialize( lib::ILockable& lockable, messsage_t* buffer, uint32_t size_buffer, uint32_t num_receivers )
{
   if ( m_initialized )
   {
      return ErrorCodes::OK;
   }

   if ( ( size_buffer == 0 ) || ( buffer == nullptr ) )
   {
      return ErrorCodes::NO_BUFFER_GIVEN;
   }
   else if ( size_buffer > NUM_BUFFER_MAX )
   {
      return ErrorCodes::BUFFER_SIZE_TOO_BIG;
   }

   m_buffer = buffer;
   m_size_buffer = size_buffer;

   if ( num_receivers == 0 )
   {
      return ErrorCodes::NO_RECEIVERS_GIVEN;
   }
   else if ( num_receivers > NUM_RECEIVER_MAX )
   {
      return ErrorCodes::RECEIVERS_TOO_MANY;
   }

   m_num_receivers = num_receivers;

   //!< Initialize the lockable object
   m_lockable = &lockable;
   if ( m_lockable->initialize() != true )
   {
      return ErrorCodes::MUTEX_INIT_FAILED;
   }

   //!< Create the semaphores for communication with the receiver threads
   for ( unsigned i = 0; i < m_num_receivers; ++i )
   {
      m_sem_messages[i] = xSemaphoreCreateCounting( NUM_BUFFER_MAX, 0 );
      if ( m_sem_messages[i] == nullptr )
      {
         return ErrorCodes::MSG_SEMAPHORE_INIT_FAILED;
      }
   }

   memset( m_buffer, 0, sizeof( messsage_t ) * m_size_buffer );
   memset( m_tbl_buffer_destination, 0, sizeof( m_tbl_buffer_destination ) );
   memset( m_tbl_buffer_state, 0, sizeof( m_tbl_buffer_state ) );

   m_initialized = true;

   return ErrorCodes::OK;
}

/**
 * @brief Creates a new message in the message buffer.
 * @details This function gets a poiter for a new message from the message buffer. As it can be called from multiple threads, it uses a lock to ensure thread safety.
 * 
 * @return messsage_t* a pointer to a message structure in the buffer, or nullptr if the buffer is full or the passer is not initialized.
 */
messsage_t* MessagePasser::new_message( )
{
   if ( !m_initialized )
   {
      LOGGING( "Passer not initialized\r\n" );
      return nullptr;
   }

   lib::lock_guard lock( *m_lockable );

   if ( m_num_buffer_used >= m_size_buffer )
   {
      LOGGING( "Msg. buffer is full\r\n" );
      return nullptr;
   }
   
   for( unsigned i = 0; i < m_size_buffer; ++i )
   {
      if ( m_tbl_buffer_state[ i ] == MsgState::FREE )
      {         
         m_tbl_buffer_state[ i ] = MsgState::ALLOCATED;
         m_num_buffer_used++;
         return &m_buffer[ i ];
      }
   }

   LOGGING( "Msg. buffer is full.\r\n" );
   return nullptr;
}

/**
 * @brief Deletes a message from the message buffer.
 * @details This returns the message to the buffer, allowing it to be reused later. It does not free the memory of the message, but marks it as unused.
 *          As this function can be called from multiple threads, it uses a lock to ensure thread safety.
 * 
 * @param msg a pointer to the message to be deleted. It must be a valid pointer that was obtained from new_message().
 */
void MessagePasser::delete_message( messsage_t* msg )
{
   if ( !m_initialized )
   {
      LOGGING( "Passer not initialized\r\n" );
      return;
   }

   if ( msg == nullptr )
   {
      LOGGING( "Null message pointer\r\n" );
      return;
   }

   auto index = get_message_index( msg );
   if ( index < 0 )
   {
      return;
   }

   lib::lock_guard lock( *m_lockable );

   //!< Change in the table only if the message is currently in use
   if ( m_tbl_buffer_state[ index ] != MsgState::FREE )
   {
      m_tbl_buffer_state[ index ] = MsgState::FREE;
      m_num_buffer_used--;
   }

   return;
}

/**
 * @brief Sends a message to a specific receiver.
 * @details This function sends a message to a specific receiver by marking the message with the receiver's ID and signaling the semaphore for that receiver.
 *         Note that the pointer to the message must be from the buffer that was obtained from new_message().
 *         By design, reuse of the same message pointer after it has been sent is not allowed, and it will return an error if attempted.
 * 
 * @param destination_id the ID of the receiver to which the message should be sent. It must be less than m_num_receivers.
 * @param msg a pointer to the message to be sent. It must be a valid pointer that was obtained from new_message().
 * @return int an error code indicating the result of the send operation.
 */
int MessagePasser::send( ReceiverId destination_id, messsage_t* msg )
{
   if ( !m_initialized )
   {
      LOGGING( "Passer not initialized\r\n" );
      return ErrorCodes::NOT_INITIALIZED;
   }

   /* NOTE: Destination ids are assumed to be in increasing order from zero, smaller than m_num_receivers
    */
   if ( destination_id >= m_num_receivers )
   {
      return ErrorCodes::DESTINATION_ID_OUT_OF_RANGE;
   }

   auto index = get_message_index( msg );
   if ( index < 0 )
   {
      return ErrorCodes::NO_MESSAGE_INDEX_IN_BUFFER;
   }

   lib::lock_guard lock( *m_lockable );

   if ( m_tbl_buffer_state[ index ] != MsgState::ALLOCATED )
   {
      LOGGING( "Message not in use\r\n" );
      return ErrorCodes::INVALID_MESSAGE_POINTER;
   }

   m_tbl_buffer_state[ index ] = MsgState::SENT;
   m_tbl_buffer_destination[ index ] = destination_id;

#if defined (PRINT_BUFFER_STATUS)
   print_buffer_status();
#endif
   return give_message_sem(destination_id);;
}

/**
 * @brief Receives a message for a specific receiver.
 * @details This function waits for a message to be available for the specified receiver. It uses a semaphore to block until a message is sent to that receiver.
 *          This function doesn't delete the message, but it only retrieves it; the message must separately be deleted using delete_message() after processing for reuse.
 * 
 * @param receiver_id the ID of the receiver for which the message should be received. It must be less than m_num_receivers.
 * @param msg a pointer to a pointer where the received message will be stored. If a message is found, it will point to the message structure in the buffer.
 * @return int an error code indicating the result of the receive operation.
 */
int MessagePasser::recv( ReceiverId receiver_id, messsage_t** msg )
{
   if ( !m_initialized )
   {
      LOGGING( "Passer not initialized\r\n" );
      return ErrorCodes::NOT_INITIALIZED;
   }

   /* NOTE: Receiver ids are assumed to be increasing order from zero, smaller than m_num_receivers
    */   
   if ( receiver_id >= m_num_receivers )
   {
      return ErrorCodes::DESTINATION_ID_OUT_OF_RANGE;
   }

   auto result = take_message_sem( receiver_id );
   if ( result != ErrorCodes::OK )
   {
      return result;
   }

   lib::lock_guard lock( *m_lockable );

   //!< It returns the first message found in the buffer that was sent for the receiver.
   for( unsigned i = 0; i < m_size_buffer; ++i )
   {
      if ( ( m_tbl_buffer_state[i] == MsgState::SENT ) &&
           ( m_tbl_buffer_destination[i] == receiver_id ) )
      {
         m_tbl_buffer_state[i] = MsgState::RECEIVED;
         *msg = &m_buffer[i];
         return ErrorCodes::OK;
      }
   }
   
   return NO_MESSAGE_FOUND_FOR_DESTINATION;
}

/**
 * @brief Gets the index of a message in the buffer.
 * @details This function is used to validate if the message poitner given is within the buffer and to find its index.
 * 
 * @param msg a pointer to the message whose index is to be found.
 * @return int the index of the message in the buffer, or -1 if the message is not found.
 */
int MessagePasser::get_message_index( messsage_t* msg )
{
   for( unsigned i = 0; i < m_size_buffer; ++i )
   {
      //!< Compare the address if the msg pointer is within the buffer.
      if ( reinterpret_cast<long>( msg ) == reinterpret_cast<long>( &m_buffer[ i ] ) )
      {
         return i;
      }
   }

   LOGGING( "No message index found" );
   return -1;
}

/**
 * @brief Prints the current status of the message buffer.
 */
void MessagePasser::print_buffer_status( )
{
    LOGGING( "  Buffer usage: %d/%d, Rem:%d\r\n", m_num_buffer_used, m_size_buffer, ( m_size_buffer - m_num_buffer_used ) );
}

/**
 * @brief Gives a message semaphore for a specific receiver.
 * @details This function signals the semaphore for the specified receiver, indicating that a message is available for that receiver.
 *          The internal count of messages for the receiver is increased whenever a new message is sent to that receiver up to the maximum number of messages allowed, which is set during initialization of the semaphore.
 * 
 * @param receiver_id 
 * @return int 
 */
int MessagePasser::give_message_sem( ReceiverId receiver_id )
{
   if ( receiver_id >= m_num_receivers )
   {
      LOGGING( "Destination out of range\r\n" );
      return ErrorCodes::DESTINATION_ID_OUT_OF_RANGE;
   }

   xSemaphoreGive( m_sem_messages[ receiver_id ] );

   return ErrorCodes::OK;
}

/**
 * @brief Takes a message semaphore for a specific receiver.
 * @details This function waits for a message to be available for the specified receiver by taking the semaphore. 
 *          If the semaphore is not available within the specified timeout, it returns an error. 
 */
int MessagePasser::take_message_sem( ReceiverId receiver_id, uint32_t timeout_ms /* = 2000 */ )
{
   if ( receiver_id >= m_num_receivers )
   {
      LOGGING( "Destination out of range\r\n" );
      return ErrorCodes::DESTINATION_ID_OUT_OF_RANGE;
   }

   if ( xSemaphoreTake( m_sem_messages[ receiver_id ], timeout_ms ) != pdTRUE )
   {
      return ErrorCodes::MSG_SEMAPHORE_TAKE_TIMEOUT;
   }

   return ErrorCodes::OK;
}