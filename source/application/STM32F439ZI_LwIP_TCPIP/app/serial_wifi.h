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

   void  initialize   ( );
   void  sendWait     ( const char* message, bool flushRxBuffer = true, bool expectResponse = true );
   void  waitResponse ( uint32_t timeout_ms );

private:
   lib::SerialDevice& m_serialDevice;
   lib::ILockable&    m_lockable;
   bool               m_waitingforResponse{ false };
};
