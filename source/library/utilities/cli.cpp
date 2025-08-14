
#include "cli.h"
#include <string.h>

namespace lib
{   
CLI::CLI( char buffer[], uint32_t sizeBuffer )
 : m_ringBuffer( buffer, sizeBuffer )
{
   memset( m_commandTable, 0, sizeof( m_commandTable ) );
}

// 명령어 등록
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

// 입력 처리
void CLI::processInput( char* input )
{
   char* argv[MAX_ARGS];
   auto argc = parseInput( input, argv );

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
   // "명령어를 찾을 수 없습니다"와 같은 오류 메시지 출력
}

// 파싱 로직
int CLI::parseInput( char* input, char* argv[] )
{
   int argc = 0;
   char* token = strtok(input, " ");
   while ( token != NULL && argc < MAX_ARGS )
   {
      argv[argc++] = token;
      token = strtok(NULL, " ");
   }
   return argc;
}
int CLI::parseInput2( char* input, char* argv[], int maxArgs )
{
   int argc = 0;

   // Find the first space to separate the command
   auto *token = strchr(input, ' ');
   if (token == nullptr)
   {
      // No space found, the entire input is the command
      argv[argc++] = input;
      return argc;
   }

   // Null-terminate the command
   *token = '\0';
   argv[argc++] = input;

   // Move past the first token
   input = token + 1;

   // Parse the rest of the arguments
   while (*input != '\0' && argc < maxArgs)
   {
      // Skip leading spaces
      while (*input == ' ')
      {
         input++;
      }

      if (*input == '\0')
      {
         break;
      }

      // Find the next space
      token = strchr(input, ' ');

      if (token == nullptr)
      {
         // Last argument
         argv[argc++] = input;
         break;
      }

      // Null-terminate the argument
      *token = '\0';
      argv[argc++] = input;
      input = token + 1;
   }

   return argc;
}

void CLI::putCharIntoBuffer( char c )
{
   m_ringBuffer.push(c);
}

}