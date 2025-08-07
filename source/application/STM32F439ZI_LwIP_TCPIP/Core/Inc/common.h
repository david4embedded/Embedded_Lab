
#pragma once

#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include <stdio.h>

#define USE_LOGGER

#if defined (USE_LOGGER)
#define LOGGING( format, ... ) printf( "%08ld: " format "\r\n", xTaskGetTickCount(), ##__VA_ARGS__ )
#else 
#define LOGGING( format, ... )
#endif
