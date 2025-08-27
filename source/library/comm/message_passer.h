/************************************************************************************************************
 * 
 * @file message_passer.h
 * @brief Header file for the MessagePasser class, which provides a message passing mechanism between tasks or threads.
 * @details This class allows for the creation, sending, receiving, and deletion of messages in a thread-safe manner.
 *          This file is part of a larger project that implements a message passing system for FreeRTOS.
 *          It uses FreeRTOS semaphores for synchronization and supports multiple receivers.
 * 
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-07-27
 * @version 1.0
 * 
 ************************************************************************************************************/

#pragma once

/************************************************** Includes ************************************************/
#include "Utilities.h"
#include "ErrorCodes.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "lockable_interface.h"
#include "lockguard.h"
#include <stdint.h>

/************************************************** Types ***************************************************/
/**
 * @brief Structure representing a message that can be passed between tasks or threads.
 */
struct messsage_t
{
   uint8_t len;
   uint8_t data[255];
};

//!< Type alias for receiver ID, which is used to identify the destination of messages
using ReceiverId = uint8_t;

/**
 * @brief A class that provides a message passing mechanism between different tasks or threads in a system.
 * @details Note that the the maximum number of messages that can be passed is defined by NUM_BUFFER_MAX, and the maximum number of receivers is defined by NUM_RECEIVER_MAX.
 */
class MessagePasser
{
public:
   //!< Compile-time config parameters, which can be template arguments for more of flexibility as required
   constexpr static uint32_t NUM_BUFFER_MAX = 32;
   constexpr static uint32_t NUM_RECEIVER_MAX = 5; 

   //!< Constructor and destructor
   MessagePasser( ) = default;
   ~MessagePasser( ) = default;

   //!< Disable copy and move operations
   MessagePasser( const MessagePasser& ) = delete;
   MessagePasser& operator=( const MessagePasser& ) = delete;
   MessagePasser( MessagePasser&& ) = delete;
   MessagePasser& operator=( MessagePasser&& ) = delete;

   int               initialize           ( lib::ILockable& lockable, messsage_t* buffer, uint32_t size_buffer, uint32_t num_receivers );

   //!< Message management
   messsage_t*       new_message          ( );
   void              delete_message       ( messsage_t* msg );
   int               send                 ( ReceiverId receiver_id, messsage_t* msg );
   int               recv                 ( ReceiverId receiver_id, messsage_t** msg );

   //!< Getters
   uint32_t          get_buffer_available ( ) const { return m_size_buffer - m_num_buffer_used; }

private:
   /**
    * @brief Enumeration representing the status of a message in the buffer.
    * @details These states are used to trace the lifecycle of a message in the buffer, from being free to being sent and received.
    */
   enum class MsgState : uint8_t
   {
      FREE = 0,      //!< The message slot is free and can be used for a new message
      ALLOCATED,     //!< The message slot is allocated and in use
      SENT,          //!< The message has been sent to a receiver but not yet received
      RECEIVED       //!< The message has been received by the receiver
   };

   //!< Helpers
   int               get_message_index    ( messsage_t* msg );
   void              print_buffer_status  ( );

   //!< Synchronization
   int               give_message_sem     ( ReceiverId receiver_id );
   int               take_message_sem     ( ReceiverId receiver_id, uint32_t timeout_ms = 2000 );

   //!< Passer control/configuration
   bool              m_initialized{ false };
   uint32_t          m_num_receivers;                             //!< Number of receivers that can receive messages, which can be up to NUM_RECEIVER_MAX.

   //!< Message Buffer control
   messsage_t*       m_buffer;                                    //!< Pointer to the message buffer
   uint32_t          m_size_buffer{ 0 };                          //!< Size of the message buffer            
   uint32_t          m_num_buffer_used{ 0 };                      //!< Number of messages currently in use in the buffer
   MsgState          m_tbl_buffer_state[NUM_BUFFER_MAX];          //!< Table to track the state of each message in the buffer
   uint8_t           m_tbl_buffer_destination[NUM_BUFFER_MAX];    //!< Table to track the destination ID of each message in the buffer

   //!< Synchronization
   lib::ILockable*   m_lockable;                                  //!< Pointer to the lockable object used for synchronization
   SemaphoreHandle_t m_sem_messages[NUM_RECEIVER_MAX];            //!< Semaphore handles for each receiver to signal when a message is available
};