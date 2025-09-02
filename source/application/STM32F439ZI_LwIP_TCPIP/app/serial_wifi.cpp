/************************************************************************************************************
 * 
 * @file serial_wifi.cpp
 * @brief Implementation of the SerialWifi class.
 *  
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-08-23
 * @version 1.0
 * 
 ************************************************************************************************************/

/************************************************ Includes **************************************************/
#include "serial_wifi.h"
#include "common.h"
#include "cmsis_os.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define ECHO_SERVER_TEST

/******************************************* Function Definitions *******************************************/    
/**
 * @brief Initialize the Wi-Fi serial device.
 */
void SerialWifi::initialize()
{
   m_serialDevice.initialize();
   m_lockable.initialize();

   m_isInitialized = true;
}

/**
 * @brief Run the Serial Wi-Fi task.
 * @details This is a static method intended to be handed over to FreeRTOS kernel to run as a thread function.
 * 
 * @param argument Pointer to the SerialWifi instance.
 */
void SerialWifi::runTask( void const* argument )
{
   auto& serialWifi = *reinterpret_cast<SerialWifi*>( const_cast<void*>( argument ) );

   LOGGING( "SerialWiFi: Task Started..." );

   while( serialWifi.isInitialized() )
   {
      osDelay( 10 );
   }

   for(;;)
   {
      char message[IPData::MAX_DATA_LENGTH] = {0};
      auto result = serialWifi.waitAsyncResponse( message, sizeof( message ) );
      if ( !result )
      {
         continue;
      }

      result = serialWifi.parseResponse( message );
      if ( !result )
      {
         //!< Log if the message received was not parsed successfully.
         size_t length = strlen( reinterpret_cast<const char*>( message ) );
         LOGGING( "SerialWifi: Async Resp.[%d] [%s] ", length, message  );
      }
   }
}

/**
 * @brief Parse the response message from the Wi-Fi serial device.
 * 
 * @param message The response message to parse.
 */
bool SerialWifi::parseResponse( const char* message )
{
   auto result = true;
   const auto msgType = getMessageType( message );

   switch ( msgType )
   {
      case eRxMessageType::IP_DATA:
         LOGGING( "SerialWifi: Received IP Data" );
         
         if ( convertToIpData( message, m_ipDataCached ) == true )
         {
            #if defined (ECHO_SERVER_TEST)
            LOGGING( "SerialWifi: Echo the message" );
            char echoMessage[IPData::MAX_DATA_LENGTH] = {0};
            snprintf( echoMessage, sizeof(echoMessage), "AT+CIPSEND=%d,%d", m_ipDataCached.linkId, m_ipDataCached.length );
            result = sendWait( echoMessage );
            #endif
         }
         break;

      case eRxMessageType::IP_DATA_SEND_READY:
         LOGGING( "SerialWifi: IP Data Send Ready" );
         
         #if defined (ECHO_SERVER_TEST)
         result = sendWait( m_ipDataCached.data );
         #endif
         break;

      default:
         result = false;
         break;
   }

   return result;
}

/**
 * @brief Get the type of message received from the Wi-Fi serial device.
 * 
 * @param message  The response message to parse.
 * @return SerialWifi::eRxMessageType 
 */
SerialWifi::eRxMessageType SerialWifi::getMessageType( const char* message )
{
   if ( strstr( message, RX_MSG_TYPE_IP_DATA ) != nullptr )
   {
      return eRxMessageType::IP_DATA;
   }
   else if ( strstr( message, RX_MSG_TYPE_IP_DATA_SEND_READY ) != nullptr )
   {
      return eRxMessageType::IP_DATA_SEND_READY;
   }

   return eRxMessageType::UNDEFINED;
}

/**
 * @brief Convert a message to IPData structure.
 * 
 * @param message a pointer to the message string
 * @param ipData a reference to the IPData structure to populate
 * @return true if the conversion was successful, false otherwise
 */
bool SerialWifi::convertToIpData( const char* message, IPData& ipData )
{
   const auto* posStart = strstr( message, RX_MSG_TYPE_IP_DATA );
   if ( posStart == nullptr )
   {
      return false;
   }

   const auto* posLinkId = strchr( posStart, ',' );
   if ( posLinkId == nullptr )
   {
      return false;
   }

   const auto* posLength = strchr( posLinkId + 1, ',' );
   if ( posLength == nullptr )
   {
      return false;
   }

   const auto* posData = strchr( posLength + 1, ':' );
   if ( posData == nullptr )
   {
      return false;
   }

   //!< Populate the IPData structure
   ipData.linkId = static_cast<uint8_t>( atoi( posLinkId + 1 ) );
   ipData.length = static_cast<uint8_t>( atoi( posLength + 1 ) );
   strncpy( ipData.data, posData + 1, IPData::MAX_DATA_LENGTH - 1 );

   //!< Set the byte to null terminator after the data
   auto *posEnd = strchr( ipData.data, '0' );
   if ( posEnd != nullptr )
   {
      *posEnd = '\0';
   }

   LOGGING( "SerialWifi: IP Data - linkId:[%d], length:[%d], data:[%s]", ipData.linkId, ipData.length, ipData.data );
   return true;
}

/**
 * @brief Send a message over the Wi-Fi serial device.
 * @details At the end of the message given, "<CR><LF>" will be appended as it's required by the protocol.
 * 
 * @param message The message to be sent.
 * @param flushRxBuffer Whether to flush the Rx buffer before sending the message. Default is true.
 */
bool SerialWifi::sendWait( const char* message, bool flushRxBuffer /* = true */ )
{
   lib::lock_guard lock( m_lockable );

   if ( flushRxBuffer )
   {
      m_serialDevice.flushRxBuffer();
   }

   LOGGING( "SerialWifi: Send [%d] [%s]", strlen( message ), message );
   
   const char* DELIMITER = "\r\n";
   char buffer[IPData::MAX_DATA_LENGTH] = {0};
   snprintf( buffer, sizeof(buffer), "%s%s", message, DELIMITER );

   auto result = m_serialDevice.sendAsync( reinterpret_cast<const uint8_t*>( buffer ), strlen( buffer ) );
   if ( result != LibErrorCodes::eOK )
   {
      LOGGING( "SerialWifi: Send failed, ret=0x%lx", result );
      return false;
   }

   result = m_serialDevice.waitSendComplete( 1000 );
   if ( result != LibErrorCodes::eOK )
   {
      LOGGING( "SerialWifi: Wait failed, ret=0x%lx", result );
      return false;
   }

   return true;
}

/**
 * @brief Send a message over the Wi-Fi serial device asynchronously.
 * @details This method will not block and will return immediately after queuing the message for sending.
 * 
 * @param message The message to be sent.
 * @param flushRxBuffer Whether to flush the Rx buffer before sending the message. Default is true.
 */
void SerialWifi::sendAsync( const char* message, bool flushRxBuffer /* = true */ )
{
   lib::lock_guard lock( m_lockable );

   if ( flushRxBuffer )
   {
      m_serialDevice.flushRxBuffer();
   }

   LOGGING( "SerialWifi: Send Async.(%d) [%s]", strlen( message ), message );
   
   const char* DELIMITER = "\r\n";
   char buffer[IPData::MAX_DATA_LENGTH] = {0};
   snprintf( buffer, sizeof(buffer), "%s%s", message, DELIMITER );

   auto result = m_serialDevice.sendAsync( reinterpret_cast<const uint8_t*>( buffer ), strlen( buffer ) );
   if ( result != LibErrorCodes::eOK )
   {
      LOGGING( "SerialWifi: Send failed, ret=0x%lx", result );
      return;
   }
}

/**
 * @brief Wait for the send operation to complete.
 * 
 * @return true if the send operation completed successfully, false otherwise.
 */
bool SerialWifi::waitSendComplete( )
{
   return m_serialDevice.waitSendComplete( 1000 );
}

/**
 * @brief Show the response from the Wi-Fi serial device.
 * 
 * @param timeout_ms the amount of time to finish waiting if no response is received within
 */
void SerialWifi::waitResponse( uint32_t timeout_ms )
{
   lib::lock_guard lock( m_lockable );

   uint8_t rxBuffer[128];
   ZERO_BUFFER( rxBuffer );
   
   const auto tickStarted = LIB_COMMON_getTickMS();
   
   auto i = 0;
   auto timeoutRemaining = timeout_ms;
   
   while( timeoutRemaining )
   {
      uint8_t byte = 0;
      const auto result = m_serialDevice.getRxByte( byte, timeoutRemaining );
      if ( result == LibErrorCodes::eOK )
      {
         if ( i >= sizeof(rxBuffer) )
         {
            LOGGING( "SerialWifi: Response buffer overflow", rxBuffer );
            break;
         }
         rxBuffer[i++] = byte;
      }
      else
      {
         break;
      }

      const auto elapsed = LIB_COMMON_getTickMS() - tickStarted;
      timeoutRemaining = timeout_ms - elapsed;
   }

   LOGGING( "SerialWifi: Response: %s", rxBuffer );
}

/**
 * @brief Wait for an asynchronous response from the Wi-Fi serial device.
 * @details This method waits until it receives a full message, and until then it will block on the SerialDevice's internal semaphore,
 *          and thus this call should be called in a dedicated thread to handle async responses from the SerialWifi device.
 * 
 * @param buffer The buffer to store the response.
 * @param bufferSize The size of the buffer.
 * @return true if a response was received, false otherwise.
 */
bool SerialWifi::waitAsyncResponse( char* buffer, uint32_t bufferSize )     
{
   auto result = false;

   const char* DELIMITER = "\n";
   constexpr uint32_t WAIT_INFINITE = 0xFFFFFFFF;
   size_t i = 0;

   while( 1 )
   {
      uint8_t byte = 0;
      const auto waitResult = m_serialDevice.getRxByte( byte, WAIT_INFINITE );
      if ( waitResult != LibErrorCodes::eOK )
      {
         continue;
      }

      if ( i >= bufferSize )
      {
         LOGGING( "SerialWifi: Async Resp. buffer overflow", buffer );
         break;
      }

      //!< Better to ignore this or it makes parsing harder.
      if ( byte == '\r' )
      {
         continue;
      }

      buffer[i++] = byte;

      //!< Complete the message for a new line if:
      //!< the prompt is received, 
      if ( byte == '>' )
      {
         result = true;
         break;
      }

      //!< or the delimiter is found
      auto* pos = strstr( reinterpret_cast<const char*>( buffer ), DELIMITER );
      if ( pos != nullptr )
      {
         *pos = '\0';
         const auto length = strlen( reinterpret_cast<const char*>( buffer ) );
         
         //!< Valid only if the length is not zero.
         result = length > 0;
         break;
      }
   }

   return result;
}
