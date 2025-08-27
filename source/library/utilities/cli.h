/************************************************************************************************************
 * 
 * @file cli.h
 * @brief Command Line Interface (CLI) module for processing user commands.
 * @details This module provides functionalities to read, parse, and execute commands.
 *          This is a singleton class as there will be only one CLI instance in the system.
 *  
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-08-20
 * @version 1.0
 * 
 ************************************************************************************************************/

#pragma once

/************************************************ Includes **************************************************/ 
#include "ring_buffer.h"
#include "semaphore_interface.h"
#include "error_codes_lib.h"
#include <stdint.h>
#include <stddef.h>

namespace lib
{
/************************************************ Types *****************************************************/    
class CLI final
{
public:
   //!< Constants
   constexpr static uint32_t MAX_COMMANDS = 10;
   constexpr static uint32_t MAX_ARGS     = 5;     //!< including the command part

   //!< Alias for command function pointer
   using CommandFunction = void (*)( int, char*[] );

   /**
    * @brief Command entry structure
    */
   struct CommandEntry
   {
      const char* commandName;                     //!< Name of the command
      CommandFunction function;                    //!< Pointer to the command function
   } ;

   ~CLI() = default;

   static CLI&    getInstance          ( );        //!< Singleton instance accessor. The implementation should be in a separate file, e.g., config_cli.cpp, not in cpp.cpp.

   ErrorCode      initialize           ( );
   ErrorCode      getNewCommandLine    ( char* buffer, uint32_t sizeBuffer, uint32_t timeout_ms = 3000 );
   void           processInput         ( char* input );
   int            tokenize             ( char* input, char* argv[], int maxArgs );
   void           putCharIntoBuffer    ( char c );

   //!< disable copy and move constructors
   CLI( const CLI& ) = delete;
   CLI& operator=( const CLI& ) = delete;
   CLI( CLI&& ) = delete;
   CLI& operator=( CLI&& ) = delete;

private:
   //!< Constructor
   CLI( char buffer[], uint32_t sizeBuffer, char delimiter, CommandEntry commands[], size_t numCommands, lib::ISemaphore& semaphore );

   lib::RingBuffer<char>   m_ringBuffer;           //!< buffer to hold all the incoming characters
   const char              m_delimiter;            //!< A config. parameter to decide a new command line
   const CommandEntry     *m_commandTable;
   const size_t            m_numCommands{ 0 };
   
   lib::ISemaphore&        m_semaphore;            //!< to signal there's a new command line
};
} /* namespace lib */