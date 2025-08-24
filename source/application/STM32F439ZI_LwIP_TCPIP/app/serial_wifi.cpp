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
 */
void SerialWifi::sendWait( const char* message )
{
   const auto msgLength = strlen( message );
   if ( msgLength > sizeof( m_txBuffer ) )
   {
      LOGGING( "SerialWifi: Message too long (%d bytes)", msgLength );
      return;
   }

   LOGGING( "SerialWifi: Sending '%s' (%d bytes)", message, msgLength );

   ZERO_BUFFER( m_txBuffer );
   memcpy( m_txBuffer, message, msgLength );

   m_serialDevice.flushRxBuffer();

   auto result = m_serialDevice.sendDataAsync( m_txBuffer, msgLength );
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
 */
void SerialWifi::showResponse()
{
   uint8_t rxBuffer[128];
   ZERO_BUFFER( rxBuffer );

   auto *buffer = rxBuffer;

   constexpr uint32_t TIMEOUT_MS = 5000;

   while( 1 )
   {
      uint8_t byte = 0;
      auto result = m_serialDevice.getRxByte( byte, TIMEOUT_MS );
      if ( result == LibErrorCodes::eOK )
      {
         *buffer++ = byte;
      }
      else
      {
         if ( result != LibErrorCodes::eSEMAPHORE_GET_TIME_OUT )
         {
            LOGGING( "SerialWifi: GetRxByte failed, ret=0x%lx", result );
         }
         break;
      }
   }

   LOGGING( "SerialWifi: Response: %s", rxBuffer );
}
