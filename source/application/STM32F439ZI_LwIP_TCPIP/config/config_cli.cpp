
#include "config_cli.h"
#include "cli.h"
#include "semaphore_FreeRTOS.h"
#include "common.h"

static void commandTest1( int argc, char* argv[] );

lib::CLI::CommandEntry cliCommands[] = 
{
   { "test1", commandTest1 },
};

namespace lib
{
CLI& CLI::getInstance()
{
   static char buffer[128];
   static lib::Semaphore_FreeRTOS semaphoreCli;
   static lib::CLI instance{ buffer, sizeof(buffer), '\r', cliCommands, sizeof(cliCommands) / sizeof(cliCommands[0]), semaphoreCli };
   return instance; 
}
}

void CLI_putCharIntoBuffer( char c )
{
   auto& cli = lib::CLI::getInstance();
   cli.putCharIntoBuffer(c);
}

static void commandTest1( int argc, char* argv[] )
{
   LOGGING( "CLI: 'test1' command executed" );

   for( unsigned i = 0; i < argc; ++i )
   {
      LOGGING( "CLI: arg[%d]: %s", i, argv[i] );
   }
}
