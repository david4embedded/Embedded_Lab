/************************************************************************************************************
 * 
 * @file config_serial_device.h
 * @brief Configuration for the serial device.
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
 * @brief Enumeration of serial device types
 */
enum class eSerialDevice
{
   DEVICE_1,   //!< LOGGER
   DEVICE_2,   //!< SerialWifi
};

/******************************************* Function Declarations ******************************************/    
lib::SerialDevice& SERIAL_DEVICE_get( eSerialDevice device );