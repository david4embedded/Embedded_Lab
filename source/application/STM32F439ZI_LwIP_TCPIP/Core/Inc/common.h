
#pragma once

#include "stm32f4xx_hal.h"

#define USE_LOGGER

#if defined (USE_LOGGER)
//#define LOGGING( format, ... ) printf( "Logging: %08d: " format, get_tick(), ##__VA_ARGS__ )
#define LOGGING( ... ) printf( "Logging: %08d: ", ##__VA_ARGS__ )
#else 
#define LOGGING( format, ... )
#endif


