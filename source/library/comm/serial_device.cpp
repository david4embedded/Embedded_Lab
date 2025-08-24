/************************************************************************************************************
 * 
 * @file serial_device.cpp
 * @brief Implementation of the SerialDevice class.
 *  
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-08-23
 * @version 1.0
 * 
 ************************************************************************************************************/

/************************************************ Includes **************************************************/
#include "serial_device.h"
#include "common.h"

/******************************************* Function Definitions *******************************************/    
namespace lib
{
/**
 * @brief Initialize the serial device.
 * 
 * @return ErrorCode 
 */
ErrorCode SerialDevice::initialize()
{
   ZERO_BUFFER( m_txBuffer );

   m_lockable.initialize();
   m_semTxComplete.initialize( 1, 0 );
   m_semNewRxBytes.initialize( m_rxBuffer.size(), 0 );
   
   m_isInitialized = true;

   return LibErrorCodes::eOK;
}

/**
 * @brief Send data over UART.
 * @details This function is non-blocking and returns immediately after initiating the send operation.
 *          However, sending is allowed only if the previous sending has completed, which is confirmed through the waitSendComplete() function.
 * 
 * @param data Pointer to the data to be sent.
 * @param length Length of the data to be sent.
 * @return ErrorCode 
 */
ErrorCode SerialDevice::sendDataAsync( const uint8_t* data, size_t length )
{
   lib::lock_guard lock( m_lockable );

   if ( !m_isInitialized )
   {
      return LibErrorCodes::eSERIAL_DEVICE_NOT_INITIALIZED;
   }

   if ( length > TX_BUFFER_SIZE )
   {
      return LibErrorCodes::eSERIAL_DEVICE_TX_MSG_TOO_LONG;
   }

   if ( m_isSending )
   {
      return LibErrorCodes::eSERIAL_DEVICE_SEND_ACTIVE;
   }
   
   m_isSending = true;

   ZERO_BUFFER( m_txBuffer );
   memcpy( m_txBuffer, data, length );

   m_sender( m_txBuffer, length );

   return LibErrorCodes::eOK;
}

/**
 * @brief Wait for the UART transmission to complete.
 * 
 * @param timeout_ms Timeout in milliseconds.
 * @return ErrorCode 
 */
ErrorCode SerialDevice::waitSendComplete( uint32_t timeout_ms )
{
   if ( !m_isSending )
   {
      return LibErrorCodes::eSERIAL_DEVICE_NO_SEND_ACTIVE;
   }

   const auto result = m_semTxComplete.get( timeout_ms );

   //!< Set the flag to false, regardless of the result
   m_isSending = false;

   return result;
}

/**
 * @brief Notify that the UART transmission is complete.
 */
void SerialDevice::notifySendComplete( )
{
   m_semTxComplete.putISR();
}

/**
 * @brief Flush the RX buffer.
 */
void SerialDevice::flushRxBuffer( )
{
   lib::lock_guard lock( m_lockable );
   m_rxBuffer.clear();
}

/**
 * @brief Push a byte into the RX buffer.
 * 
 * @param data The byte to be pushed.
 * @return ErrorCode 
 */
ErrorCode SerialDevice::pushRxByte( uint8_t data )
{
   if ( !m_isInitialized )
   {
      return LibErrorCodes::eSERIAL_DEVICE_NOT_INITIALIZED;
   }

   auto result = m_rxBuffer.push( data );
   if ( result != LibErrorCodes::eOK )
   {
      return result;
   }

   m_semNewRxBytes.putISR();
   return LibErrorCodes::eOK;
}

/**
 * @brief Push a byte into the RX buffer.
 * 
 * @param data The byte to be pushed.
 * @param timeout_ms Timeout in milliseconds.
 * @return ErrorCode 
 */
ErrorCode SerialDevice::getRxByte( uint8_t& data, uint32_t timeout_ms )
{
   const auto result = m_semNewRxBytes.get( timeout_ms );
   if ( result != LibErrorCodes::eOK )
   {
      return result;
   }
   return m_rxBuffer.pop( data );
}
} /* namespace lib */