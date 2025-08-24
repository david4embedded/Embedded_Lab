/***************************************************************************************************
 * @file           : error_codes_lib.h
 * @brief          : Error codes for the library
 * @details        : This file contains the error codes used in the library.
 *                   Note that the application should define its own error codes in a separate file, and the codes should only be using the last 6 digits for compatibility.
 * @author         : Sungsu Kim
 * @date           : 2025-08-05
 * @copyright      : Copyright (c) 2025 Sungsu Kim
 ***************************************************************************************************/

#pragma once

/****************************************** Includes ***********************************************/ 
#include <stdint.h>

using ErrorCode = uint32_t;

#define ERROR_LIB( x )        ( (uint32_t)( ErrorClass::eLIBRARY | x ) )
#define ERROR_APP( x )        ( (uint32_t)( ErrorClass::eAPPLICATION | x ) )
#define GET_ERROR_CLASS( x )  ( (uint32_t)x & 0xFF000000 )

/******************************************** Types ************************************************/
/**
 * @brief Error class enumeration
 */
enum ErrorClass
{
   eLIBRARY     = 0x10000000,
   eAPPLICATION = 0x20000000,
};

/**
 * @brief Library error codes
 */
enum LibErrorCodes
{
   eOK = 0x00000000,
   
   eRING_BUFFER_INVALID_ARGUMENT   = ( eLIBRARY | 0x00000001 ),
   eRING_BUFFER_EMPTY              = ( eLIBRARY | 0x00000002 ),
   eRING_BUFFER_FULL               = ( eLIBRARY | 0x00000003 ),
   
   eSEMAPHORE_INIT_FAILED          = ( eLIBRARY | 0x00000004 ),
   eSEMAPHORE_NOT_INITIALIZED      = ( eLIBRARY | 0x00000005 ),
   eSEMAPHORE_GET_TIME_OUT         = ( eLIBRARY | 0x00000006 ),
   
   eCLI_NO_COMMAND                 = ( eLIBRARY | 0x00000007 ),
   eCLI_TOO_MANY_COMMANDS          = ( eLIBRARY | 0x00000008 ),

   eSERIAL_DEVICE_NOT_INITIALIZED  = ( eLIBRARY | 0x00000009 ),
   eSERIAL_DEVICE_SEND_ACTIVE      = ( eLIBRARY | 0x0000000A ),
   eSERIAL_DEVICE_NO_SEND_ACTIVE   = ( eLIBRARY | 0x0000000B ),
   eSERIAL_DEVICE_SEND_TIMEOUT     = ( eLIBRARY | 0x0000000C )
};

