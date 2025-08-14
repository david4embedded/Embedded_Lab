
#pragma once

#include <stdint.h>
#include "ring_buffer.h"

namespace lib
{
class CLI final
{
public:
   constexpr static uint32_t MAX_COMMANDS = 10;
   constexpr static uint32_t MAX_ARGS = 5;
   constexpr static uint32_t BUFFER_SIZE = 256;

   using CommandFunction = void (*)( int, char*[] );

   struct CommandEntry
   {
      const char* commandName;
      CommandFunction function;
   } ;
   
   CLI();
   ~CLI() = default;

   // 명령어 등록
   void addCommand( const char* command, CommandFunction function );

   // 입력 처리
   void processInput( char* input );

   // 파싱 로직
   int parseInput( char* input, char* argv[] );

   void putCharIntoBuffer( char c );

private:
   CommandEntry  m_commandTable[MAX_COMMANDS];
   int           m_commandCount{ 0 };

   lib::RingBuffer<char, BUFFER_SIZE> m_ringBuffer;
};

};