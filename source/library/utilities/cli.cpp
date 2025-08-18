
#include "cli.h"
#include <string.h>

namespace lib
{   
CLI::CLI( char buffer[], uint32_t sizeBuffer, char delimiter )
 : m_ringBuffer( buffer, sizeBuffer )
 , m_delimiter( delimiter )
{
   memset( m_commandTable, 0, sizeof( m_commandTable ) );
}

void CLI::addCommand( const char* command, CommandFunction function )
{
   if ( m_commandCount > MAX_COMMANDS )
   {
      return;
   }

   m_commandTable[m_commandCount].commandName = command;
   m_commandTable[m_commandCount].function = function;
   m_commandCount++;
}

void CLI::getNewCommandLine( char* buffer, uint32_t sizeBuffer )
{
   if ( m_newCommandLines == 0 )
   {
      return;
   }

   char oneChar;
   while ( ( m_ringBuffer.pop( oneChar ) == LibErrorCodes::eOK ) && 
           ( sizeBuffer-- ) )
   {
      *buffer++ = oneChar;
      if ( oneChar == m_delimiter )
      {
         m_newCommandLines--;
         break;
      }
   }
}

void CLI::processInput( char* input )
{
   char* argv[MAX_ARGS];
   auto argc = tokenize( input, argv, MAX_ARGS );

   if (argc == 0)
   {
      return;
   }

   char* command = argv[0];

   for ( unsigned i = 0; i < m_commandCount; i++ )
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
      m_newCommandLines++;
   }
}
}