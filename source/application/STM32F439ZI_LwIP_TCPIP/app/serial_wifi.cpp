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
#include <string.h>

/******************************************* Function Definitions *******************************************/    
/**
 * @brief Initialize the Wi-Fi serial device.
 */
void SerialWifi::initialize()
{
   m_serialDevice.initialize();
}

/**
 * @brief Send a message over the Wi-Fi serial device.
 * 
 * @param message The message to be sent.
 * @param flushRxBefore Whether to flush the RX buffer before sending the message. Default is true.
 */
void SerialWifi::sendWait( const char* message, bool flushRxBefore /* = true */ )
{
   if ( flushRxBefore )
   {
      m_serialDevice.flushRxBuffer();
   }

   auto result = m_serialDevice.sendDataAsync( reinterpret_cast<const uint8_t*>( message ), strlen( message ) );
   if ( result != LibErrorCodes::eOK )
   {
      LOGGING( "SerialWifi: Send failed, ret=0x%lx", result );
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
   uint8_t rxBuffer[128];
   ZERO_BUFFER( rxBuffer );
   
   auto tickStart = LIB_COMMON_getTickMS();
   
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

      const auto elapsed = LIB_COMMON_getTickMS() - tickStart;
      timeoutRemaining = timeout_ms - elapsed;
   }

   LOGGING( "SerialWifi: Response: %s", rxBuffer );
}
