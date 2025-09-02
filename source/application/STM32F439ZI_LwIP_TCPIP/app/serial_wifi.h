/************************************************************************************************************
 * 
 * @file serial_wifi.h
 * @brief Header file for the SerialWifi class.
 *  
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-08-23
 * @version 1.0
 * 
 ************************************************************************************************************/
#pragma once

/************************************************ Includes **************************************************/
#include "serial_device.h"
#include "lockable_interface.h"

/************************************************* Types ****************************************************/
/**
 * @brief Class representing a Wi-Fi serial device
 */
class SerialWifi
{
public:
   constexpr static size_t TX_BUFFER_SIZE = 128;

   SerialWifi( lib::SerialDevice& serialDevice, lib::ILockable& lockable ) 
   : m_serialDevice( serialDevice )
   , m_lockable( lockable ) 
   { }
   ~SerialWifi() = default;

   void  initialize           ( );
   void  sendWait             ( const char* message, bool flushRxBuffer = true );
   void  sendAsync            ( const char* message, bool flushRxBuffer = true );
   bool  waitSendComplete     ( );
   void  waitResponse         ( uint32_t timeout_ms );
   bool  waitAsyncResponse    ( char* buffer, uint32_t bufferSize );

   //!< Task function
   static void runTask        ( void const* argument );

   //!< Useful getter
   bool  isInitialized        ( ) const { return m_isInitialized; }

private:
   constexpr static const char* RX_MSG_TYPE_IP_DATA = "+IPD";
   constexpr static const char* RX_MSG_TYPE_IP_DATA_SEND_READY = ">";

   /**
    * @brief Structure representing IP data.
    */
   struct IPData
   {
      constexpr static size_t MAX_DATA_LENGTH = 128;     //!< MTU is 1500 bytes, but it's limited to 128 in the application
      uint8_t  linkId;
      uint8_t  length;                                   //!< Note that the length includes the delimiter
      char     data[MAX_DATA_LENGTH];
      IPData() { memset( this, 0, sizeof(IPData) ); }
   };

   bool  parseResponse        ( const char* message );
   bool  convertToIpData      ( const char* message, IPData& ipData );

   lib::SerialDevice&   m_serialDevice;
   lib::ILockable&      m_lockable;
   bool                 m_isInitialized{ false };
};
