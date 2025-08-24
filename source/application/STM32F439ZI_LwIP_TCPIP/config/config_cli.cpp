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
#include "semaphore_FreeRTOS.h"
#include "common.h"
#include "cmsis_os.h"
#include "config_serial_wifi.h"

/************************************************* Consts ***************************************************/
constexpr size_t CLI_BUFFER_SIZE = 128;

/******************************************* Function Declarations ******************************************/    
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
 * @brief Process the 'test' command
 * 
 * @param argc the number of arguments
 * @param argv the argument values
 */
static void commandTest( int argc, char* argv[] )
{
   LOGGING( "CLI: 'test' command executed" );

   for( int i = 0; i < argc; ++i )
   {
      LOGGING( "CLI: arg[%d]: %s", i, argv[i] );
   }
}

/**
 * @brief Process the 'wifi' command
 * 
 * @param argc the number of arguments
 * @param argv the argument values
 */
static void commandSerialWifi( int argc, char* argv[] )
{
   LOGGING( "CLI: 'wifi' command executed" );

   if ( argc < 2 )
   {
      LOGGING( "CLI: 'wifi' command requires at least 2 arguments" );
      return;
   }

   char buffer[128] = {0};
   snprintf( buffer, sizeof(buffer), "%s\r\n", argv[1] );

   auto& serialWifi = SERIAL_WIFI_get();
   serialWifi.sendWait( (const char*)buffer );

   osDelay( 10 );
   serialWifi.showResponse();
}