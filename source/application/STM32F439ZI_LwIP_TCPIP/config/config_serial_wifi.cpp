/************************************************************************************************************
 * 
 * @file config_serial_wifi.cpp
 * @brief Configuration for the Wi-Fi serial device.
 *  
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-08-23
 * @version 1.0
 * 
 ************************************************************************************************************/

/************************************************ Includes **************************************************/
#include "serial_wifi.h"
#include "config_serial_wifi.h"
#include "config_serial_device.h"
#include "lockable_freertos.h"

/********************************************* Local Variables **********************************************/    
static lib::LockableFreeRTOS lockable;
static SerialWifi serialWifi{ SERIAL_DEVICE_get( eSerialDevice::DEVICE_2 ), lockable };

/******************************************* Function Definitions *******************************************/   
/**
 * @brief Get the SerialWifi instance.
 */ 
SerialWifi& SERIAL_WIFI_get( )
{
   return serialWifi;
}
