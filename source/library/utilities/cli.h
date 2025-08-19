
#pragma once

#include <stdint.h>
#include "ring_buffer.h"
#include "ISemaphore.h"
#include "error_codes_lib.h"

namespace lib
{
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

   static CLI&    getInstance          ( );        //!< Singleton instance accessor. The implementation should be in a configuration file, e.g., config_cli.cpp.

   ErrorCode      initialize           ( );
   ErrorCode      addCommand           ( const char* command, CommandFunction function );
   ErrorCode      addCommands          ( CommandEntry* commands, uint32_t numCommands );
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
   CLI( char buffer[], uint32_t sizeBuffer, char delimiter, lib::ISemaphore& semaphore );

   const char     m_delimiter;                     //!< A config. parameter to decide a new command line
   CommandEntry   m_commandTable[MAX_COMMANDS];
   int            m_commandCount{ 0 };

   lib::RingBuffer<char>   m_ringBuffer;           //!< buffer to hold all the incoming characters
   lib::ISemaphore&        m_semaphore;            //!< to signal there's a new command line
};

}