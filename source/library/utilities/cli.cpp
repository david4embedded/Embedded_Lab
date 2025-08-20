/************************************************************************************************************
 * 
 * @file cli.cpp
 * @brief Implementation of Command Line Interface (CLI) module for processing user commands.
 *  
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-08-20
 * @version 1.0
 * 
 ************************************************************************************************************/

/************************************************ Includes **************************************************/ 
#include "cli.h"
#include <string.h>

/******************************************* Function Definitions *******************************************/    
namespace lib
{   
/**
 * @brief Construct a new CLI::CLI object
 *
 * @param buffer pointer to a buffer that will hold incoming characters
 * @param sizeBuffer size of the buffer
 * @param delimiter character used to delimit commands
 * @param commands array of command entries
 * @param numCommands number of commands in the array
 * @param semaphore semaphore used for signaling new command lines
 */
CLI::CLI( char buffer[], uint32_t sizeBuffer, char delimiter, CommandEntry commands[], size_t numCommands, lib::ISemaphore& semaphore )
 : m_ringBuffer( buffer, sizeBuffer )
 , m_delimiter( delimiter )
 , m_commandTable( commands )
 , m_numCommands( numCommands )
 , m_semaphore( semaphore )
{

}

/**
 * @brief Initialize the CLI
 * 
 * @return ErrorCode 
 */
ErrorCode CLI::initialize( )
{
   auto result = m_semaphore.initialize( 1, 0 );
   if ( result != LibErrorCodes::eOK )
   {
      return result;
   }

   return LibErrorCodes::eOK;
}

/**
 * @brief Get a new command line from the user
 * @details This function is intended to be called in a separate thread to wait a new command line input ending with the delimiter.
 *          If received, the command line is stored in the provided buffer.
 */
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

/**
 * @brief Process the user input
 * @details This function tries to get the input string tokenized into command and arguments,
 *          and then look for a command table whose command is matched with the command in the input.
 *          If found, the corresponding function is executed.
 *
 * @param input pointer to the user input string
 */
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

/**
 * @brief Tokenize the input string into command and arguments
 *
 * @param input pointer to the input string
 * @param argv array to hold the tokenized arguments
 * @param maxArgs maximum number of arguments
 * @return int number of arguments parsed
 */
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
         if ( end != nullptr )
         {
            *end = '\0';
         }
         break;
      }

      //!< Null-terminate the argument
      *token = '\0';
      argv[argc++] = input;
      input = token + 1;
   }

   return argc;
}

/**
 * @brief Put a character into the CLI buffer
 * @details This function is intended to be called in the interrupt context, as indicated, whenever a new character is received.
 *
 * @param c character to be added to the buffer
 */
void CLI::putCharIntoBuffer( char c )
{
   auto result = m_ringBuffer.push(c);
   if ( ( result == LibErrorCodes::eOK ) && ( c == m_delimiter ) )
   {
      m_semaphore.putISR();
   }
}
} /* namespace lib */
