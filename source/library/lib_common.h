/************************************************************************************************************
 * 
 * @file lib_common.h
 * @brief Common definitions and macros for the library
 *  
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-08-20
 * @version 1.0
 * 
 ************************************************************************************************************/

#pragma once

#if defined (__cplusplus)
extern "C" {
#endif

/************************************************ Includes *************************************************/    
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/************************************************* Consts **************************************************/    
#define USE_LOGGER

/************************************************* Macros **************************************************/    
#define PARAM_NOT_USED(x) (void)(x)

#if defined (USE_LOGGER)
#define LOGGING( format, ... ) printf( "%08ld: " format "\r\n", LIB_COMMON_getTickMS(), ##__VA_ARGS__ )
#else 
#define LOGGING( format, ... )
#endif

#define ZERO_BUFFER( buf ) memset( buf, 0, sizeof( buf ) )

/******************************************* Function Declarations *****************************************/    
/**
 * @brief Get the current tick count
 * @note The implementation must be provieded on the application side for this, and this allows utilzing the LOGGING macro even in the library code.
 * 
 * @return uint32_t a tick count in milliseconds
 */
uint32_t LIB_COMMON_getTickMS( void );

#if defined (__cplusplus)
}
#endif
