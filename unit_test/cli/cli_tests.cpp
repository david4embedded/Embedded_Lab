/************************************************************************************************************
 *
 * @file cli_test.cpp
 * @brief Unit tests for the CLI class
 *
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-08-14
 * @version 1.0
 *
 ************************************************************************************************************/

 /************************************************** Includes ************************************************/
#include "ring_buffer.h"
#include "cli.h"
#include "mock_Semaphore.h"
#include <gtest/gtest.h>

/*********************************************** Global Variables ********************************************/
SemaphoreMock* g_mockSemaphore;

/*********************************************** Local Variables *********************************************/
static lib::CLI::CommandEntry cliCommands[] =
{
   { "test", []( int argc, char* argv[] ) {} },
};

/*********************************************** Function Definitions ****************************************/
namespace lib
{
//!< Singleton instance accessor
CLI& CLI::getInstance()
{
   static char buffer[128];
   static lib::CLI instance{ buffer, sizeof( buffer ), '\r', cliCommands, sizeof( cliCommands ) / sizeof( cliCommands[0] ), *g_mockSemaphore };
   return instance;
}
}

/************************************************** Test Fixture ********************************************/
class CliTest : public ::testing::Test
{
protected:
   void SetUp() override
   { 
		g_mockSemaphore = &m_semaphoreMock;
   }

   void TearDown() override
   { 
      testing::Mock::VerifyAndClear( &m_semaphoreMock );
   }

public:
   SemaphoreMock m_semaphoreMock;
};

/************************************************** Tests ***************************************************/
TEST_F( CliTest, test_tokenize_works_properly )
{
   char str[128] = { 0 };
   snprintf( str, sizeof( str ), "%s", "command arg1 arg2 arg3 arg4\r" );
    
   auto& cli = lib::CLI::getInstance();

   char* argv[10];
   auto argc = cli.tokenize( str, argv, 10 );

   EXPECT_EQ( argc, 5 );
   EXPECT_EQ( strcmp(argv[0], "command"), 0 );
   EXPECT_EQ( strcmp(argv[1], "arg1"), 0 );
   EXPECT_EQ( strcmp(argv[2], "arg2"), 0 );
   EXPECT_EQ( strcmp(argv[3], "arg3"), 0 );
}

TEST_F( CliTest, test_putting_characters_and_getting_new_lines_work_properly )
{
   auto& cli = lib::CLI::getInstance();

   char str[128] = { 0 };
   snprintf( str, sizeof( str ), "%s", "test arg1\rtest arg2\r" );

   EXPECT_CALL( m_semaphoreMock, putISR() ).Times( 2 );

   auto length = strlen( str );
   auto* ptr = str;
   while ( length-- )
   {
      cli.putCharIntoBuffer( *ptr++ );
   }

   char bufferNewCommand[128];

   EXPECT_CALL( m_semaphoreMock, get( ::testing::_ ) ).Times( 1 );
   memset( bufferNewCommand, 0, sizeof( bufferNewCommand ) );
   cli.getNewCommandLine( bufferNewCommand, sizeof( bufferNewCommand ) );

   
   EXPECT_CALL( m_semaphoreMock, get( ::testing::_ ) ).Times( 1 );
   memset( bufferNewCommand, 0, sizeof( bufferNewCommand ) );
   cli.getNewCommandLine( bufferNewCommand, sizeof( bufferNewCommand ) );
}
