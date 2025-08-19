
#include "config_cli.h"
#include "cli.h"
#include "semaphore_FreeRTOS.h"

namespace lib
{
CLI& CLI::getInstance()
{
   static char buffer[128];
   static lib::Semaphore_FreeRTOS semaphoreCli;
   static lib::CLI instance{ buffer, sizeof(buffer), '\r', semaphoreCli };
   return instance; 
}
}

void putCharIntoBuffer( char c )
{
   auto& cli = lib::CLI::getInstance();
   cli.putCharIntoBuffer(c);
}