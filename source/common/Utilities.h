/************************************************************************************************************
 * 
 * @file Utilities.h
 * @brief This file contains utility functions and macros for logging and tick count retrieval.
 * @details It provides a simple logging mechanism that can be used throughout the application for debugging and tracing.
 * 
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-07-27
 * @version 1.0
 * 
 ************************************************************************************************************/

#pragma once

/************************************************** Includes ************************************************/
#include "FreeRTOS.h"
#include <cstdio>

/************************************************** Macros **************************************************/
#if !defined ( UNIT_TESTING )
#define LOGGING( format, ... ) printf( "%08d: " format, GET_TICK(), ##__VA_ARGS__ )
#else 
#define LOGGING( format, ... )
#endif

#define GET_TICK  xTaskGetTickCount