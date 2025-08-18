
#pragma once

#include <stdint.h>
#include "ring_buffer.h"

namespace lib
{
class CLI final
{
public:
   //!< Constants
   constexpr static uint32_t MAX_COMMANDS = 10;
   constexpr static uint32_t MAX_ARGS     = 5;     //!< including the command part
   constexpr static uint32_t BUFFER_SIZE  = 256;

   //!< Alias for command function pointer
   using CommandFunction = void (*)( int, char*[] );

   struct CommandEntry
   {
      const char* commandName;
      CommandFunction function;
   } ;

   CLI( char buffer[], uint32_t sizeBuffer, char delimiter );
   ~CLI() = default;

   void  addCommand           ( const char* command, CommandFunction function );
   void  processInput         ( char* input );
   int   tokenize             ( char* input, char* argv[], int maxArgs );
   void  putCharIntoBuffer    ( char c );

   bool  isNewCommandLine     ( ) const { return m_newCommandLines > 0; }
   void  getNewCommandLine    ( char* buffer, uint32_t sizeBuffer );

private:
   char          m_delimiter;
   
   CommandEntry  m_commandTable[MAX_COMMANDS];
   int           m_commandCount{ 0 };

   lib::RingBuffer<char> m_ringBuffer;
   uint32_t      m_newCommandLines{ 0 };
};

};