
#include "cli.h"
#include <string.h>

namespace lib
{   
CLI::CLI( char buffer[], uint32_t sizeBuffer, char delimiter, CommandEntry commands[], size_t numCommands, lib::ISemaphore& semaphore )
 : m_ringBuffer( buffer, sizeBuffer )
 , m_delimiter( delimiter )
 , m_commandTable( commands )
 , m_numCommands( numCommands )
 , m_semaphore( semaphore )
{

}

ErrorCode CLI::initialize( )
{
   auto result = m_semaphore.initialize( 1, 0 );
   if ( result != LibErrorCodes::eOK )
   {
      return result;
   }

   return LibErrorCodes::eOK;
}

ErrorCode CLI::getNewCommandLine( char* buffer, uint32_t sizeBuffer, uint32_t timeout_ms /* = 3000 */ )
{
   if ( m_semaphore.get( timeout_ms ) != LibErrorCodes::eOK )
   {
      return LibErrorCodes::eCLI_NO_COMMAND;
   }

   char oneChar;
   while ( ( m_ringBuffer.pop( oneChar ) == LibErrorCodes::eOK ) && 
           ( sizeBuffer-- ) )
   {
      *buffer++ = oneChar;
      if ( oneChar == m_delimiter )
      {
         break;
      }
   }

   return LibErrorCodes::eOK;
}

void CLI::processInput( char* input )
{
   char* argv[MAX_ARGS];
   const auto argc = tokenize( input, argv, MAX_ARGS );
   if (argc == 0)
   {
      return;
   }

   char* command = argv[0];

   for ( unsigned i = 0; i < m_numCommands; i++ )
   {
      if ( strcmp( m_commandTable[i].commandName, command ) == 0 )
      {
         m_commandTable[i].function( argc, argv );
         return;
      }
   }
}

int CLI::tokenize( char* input, char* argv[], int maxArgs )
{
   int argc = 0;

   //!< Parse the command and the arguments
   while ( ( *input != '\0' ) && ( argc < maxArgs ) )
   {
      //!< Skip leading spaces
      while ( *input == ' ' )
      {
         input++;
      }

      if ( *input == '\0' )
      {
         break;
      }

      //!< Find the next space
      auto* token = strchr( input, ' ' );
      if ( token == nullptr )
      {
         //!< Last argument
         argv[argc++] = input;

         //!< Replace delimiter with null terminator, if found.
         auto* end = strchr( input, m_delimiter );
         *end = '\0';
         break;
      }

      //!< Null-terminate the argument
      *token = '\0';
      argv[argc++] = input;
      input = token + 1;
   }

   return argc;
}

void CLI::putCharIntoBuffer( char c )
{
   auto result = m_ringBuffer.push(c);
   if ( ( result == LibErrorCodes::eOK ) && ( c == m_delimiter ) )
   {
      m_semaphore.putISR();
   }
}
}