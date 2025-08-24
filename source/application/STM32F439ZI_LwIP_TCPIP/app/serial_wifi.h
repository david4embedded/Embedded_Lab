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

/************************************************* Types ****************************************************/
/**
 * @brief Class representing a Wi-Fi serial device
 */
class SerialWifi
{
public:
   constexpr static size_t TX_BUFFER_SIZE = 128;

   SerialWifi( lib::SerialDevice& serialDevice ) 
   : m_serialDevice( serialDevice ) 
   { }
   ~SerialWifi() = default;

   void  initialize   ( );
   void  sendWait     ( const char* message );
   void  showResponse ( );

private:
   lib::SerialDevice& m_serialDevice;
};
