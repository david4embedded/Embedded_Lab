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

/************************************************* Consts **************************************************/ 
constexpr size_t CLI_BUFFER_SIZE = 128;

/******************************************* Function Declarations ******************************************/    
static void commandTest( int argc, char* argv[] );

/********************************************* Local Variables **********************************************/    
static lib::CLI::CommandEntry cliCommands[] = 
{
   { "test", commandTest },
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
   static lib::CLI instance{ buffer, sizeof(buffer), "\r\n", cliCommands, sizeof(cliCommands) / sizeof(cliCommands[0]), semaphoreCli };
   return instance; 
}
} /* namespace lib */

/**
 * @brief Put a character into the CLI buffer
 * 
 * @param c character to be added to the buffer
 */
void CLI_putCharIntoBuffer( char c )
{
   auto& cli = lib::CLI::getInstance();
   cli.putCharIntoBuffer(c);
}

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
