/************************************************************************************************************
 *
 * @file ring_buffer_test.cpp
 * @brief Unit tests for the RingBuffer class
 *
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-08-05
 * @version 1.0
 *
 ************************************************************************************************************/

 /************************************************** Includes ************************************************/
#include "ring_buffer.h"
#include <gtest/gtest.h>

/************************************************** Test Fixture ********************************************/
class RingBufferTest : public ::testing::Test
{
protected:
   void SetUp() override
   { }
   void TearDown() override
   { }
public:
};

/************************************************** Tests ***************************************************/
/**
 * @brief Test for RingBuffer push and pop operations
 */
TEST_F(RingBufferTest, test_all )
{
   constexpr uint32_t LENGTH = 128;

   uint32_t buffer[LENGTH] = {};
   lib::RingBuffer<uint32_t> ring_buffer( buffer, LENGTH);

   auto count = ring_buffer.count();
   EXPECT_EQ(count, 0);

   for (unsigned i = 0; i < LENGTH; i++)
   {
      auto result = ring_buffer.push( i ); 
      EXPECT_EQ(result, LibErrorCodes::eOK);
   }

   auto result = ring_buffer.push( 0 );
   EXPECT_EQ(result, LibErrorCodes::eRING_BUFFER_FULL);

   for (unsigned i = 0; i < LENGTH; i++)
   {
      uint32_t data;
      auto result = ring_buffer.pop( data );

      EXPECT_EQ( result, LibErrorCodes::eOK );
      EXPECT_EQ( data, i );
   }

   uint32_t data;
   result = ring_buffer.pop( data );
   EXPECT_EQ(result, LibErrorCodes::eRING_BUFFER_EMPTY);
}
