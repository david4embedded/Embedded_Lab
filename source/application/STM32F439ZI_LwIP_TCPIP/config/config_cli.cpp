/************************************************************************************************************
 * 
 * @file config_cli.cpp
 * @brief Configuration of CLI module and user-defined commands
 *  
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-08-20
 * @version 1.0
 * 
 ************************************************************************************************************/

/************************************************ Includes **************************************************/ 
#include "config_cli.h"
#include "cli.h"
#include "semaphore_freertos.h"
#include "common.h"
#include "cmsis_os.h"
#include "config_serial_wifi.h"
#include <stdlib.h>

/************************************************* Consts ***************************************************/
constexpr size_t CLI_BUFFER_SIZE = 128;

/******************************************* Function Declarations ******************************************/    
static void showArgs          ( int argc, char* argv[] );
static void commandTest       ( int argc, char* argv[] );
static void commandSerialWifi ( int argc, char* argv[] );

/********************************************* Local Variables **********************************************/    
static lib::CLI::CommandEntry cliCommands[] = 
{
   { "test", commandTest },
   { "wifi", commandSerialWifi }
};

/******************************************* Function Definitions *******************************************/    
namespace lib
{
/**
 * @brief Get the singleton instance of the CLI
 * 
 * @return CLI& 
 */
CLI& CLI::getInstance()
{
   static char buffer[CLI_BUFFER_SIZE];
   static lib::Semaphore_FreeRTOS semaphoreCli;
   static lib::CLI instance{ buffer, sizeof(buffer), '\r', cliCommands, sizeof(cliCommands) / sizeof(cliCommands[0]), semaphoreCli };
   return instance; 
}
} /* namespace lib */

/**
 * @brief Show the command-line arguments
 * 
 * @param argc the number of arguments
 * @param argv the argument values
 */
static void showArgs( int argc, char* argv[] )
{
   LOGGING( "CLI: [%s] command executed", argv[0] );

   for( int i = 0; i < argc; ++i )
   {
      LOGGING( "CLI: arg[%d]: %s", i, argv[i] );
   }
}

/**
 * @brief Process the 'test' command
 * 
 * @param argc the number of arguments
 * @param argv the argument values
 */
static void commandTest( int argc, char* argv[] )
{
   showArgs( argc, argv );
}

/**
 * @brief Process the 'wifi' command
 * 
 * @param argc the number of arguments
 * @param argv the argument values
 */
static void commandSerialWifi( int argc, char* argv[] )
{
   showArgs( argc, argv );

   if ( argc < 3 )
   {
      LOGGING( "CLI: 'wifi' command requires at least 2 arguments" );
      return;
   }

   char buffer[128] = {0};
   snprintf( buffer, sizeof(buffer), "%s\r\n", argv[1] );
   const auto timeout_ms = static_cast<uint32_t>( atoi( argv[2] ) );

   auto& serialWifi = SERIAL_WIFI_get();
   serialWifi.sendWait( reinterpret_cast<const char*>(buffer) );

   LOGGING( "CLI: Wait for [%d]ms for response", timeout_ms ); 

   serialWifi.waitResponse( timeout_ms );
}