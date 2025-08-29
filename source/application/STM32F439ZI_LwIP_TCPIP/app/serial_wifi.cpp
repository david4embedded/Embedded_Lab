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
#include "semaphore_FreeRTOS.h"
#include "usart.h"
#include "common.h"
#include "cmsis_os.h"
#include <string.h>

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
      uint8_t buffer[256] = {0};
      auto result = serialWifi.waitAsyncResponse( buffer, sizeof( buffer ) );
      if ( result )
      {
         size_t length = strlen( reinterpret_cast<const char*>( buffer ) );
         LOGGING( "SerialWifi: Async Resp.(%d) [%s] ", length, buffer  );
      }

      osDelay( 10 );
   }
}

/**
 * @brief Send a message over the Wi-Fi serial device.
 * @details At the end of the message given, "<CR><LF>" will be appended as it's required by the protocol.
 * 
 * @param message The message to be sent.
 * @param flushRxBuffer Whether to flush the Rx buffer before sending the message. Default is true.
 */
void SerialWifi::sendWait( const char* message, bool flushRxBuffer /* = true */ )
{
   lib::lock_guard lock( m_lockable );

   if ( flushRxBuffer )
   {
      m_serialDevice.flushRxBuffer();
   }

   LOGGING( "SerialWifi: Send Msg.(%d) [%s]", strlen( message ), message );
   
   const char* DELIMITER = "\r\n";
   char buffer[128] = {0};
   snprintf( buffer, sizeof(buffer), "%s%s", message, DELIMITER );

   auto result = m_serialDevice.sendAsync( reinterpret_cast<const uint8_t*>( buffer ), strlen( buffer ) );
   if ( result != LibErrorCodes::eOK )
   {
      LOGGING( "SerialWifi: Send failed, ret=0x%lx", result );
      return;
   }

   result = m_serialDevice.waitSendComplete( 1000 );
   if ( result != LibErrorCodes::eOK )
   {
      LOGGING( "SerialWifi: Wait failed, ret=0x%lx", result );
   }
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
bool SerialWifi::waitAsyncResponse( uint8_t* buffer, uint32_t bufferSize )     
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

      //!< Better to ignore this or it makes the parsing harder.
      if ( byte == '\r' )
      {
         continue;
      }

      buffer[i++] = byte;

      //!< Look for the delimiter for a line of response and finish.
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
