
#pragma once

#include "stm32f4xx_hal.h"
#include <stdio.h>

#define USE_LOGGER

#if defined (USE_LOGGER)
//#define LOGGING( format, ... ) printf( "Logging: %08d: " format, get_tick(), ##__VA_ARGS__ )
#define LOGGING( format, ... ) printf( "Logging: " format, ##__VA_ARGS__ )
#else 
#define LOGGING( format, ... )
#endif
