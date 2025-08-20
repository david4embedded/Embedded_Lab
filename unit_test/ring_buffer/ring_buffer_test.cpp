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
TEST_F(RingBufferTest, test_all)
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

TEST_F(RingBufferTest, test_bulk_operations)
{
   constexpr uint32_t BUFFER_LENGTH = 128;
   constexpr uint32_t BULK_BUFFER_LENGTH = 16;

   uint8_t buffer[BUFFER_LENGTH] = {};
   lib::RingBuffer<uint8_t> ring_buffer(buffer, BUFFER_LENGTH);

   uint8_t bufferForPush[BULK_BUFFER_LENGTH] = { 1,2,4,5,6,7,8,9,10 };

   uint32_t countWritten = 0;
   ring_buffer.pushBulk(bufferForPush, sizeof(bufferForPush), &countWritten); 
   EXPECT_EQ(countWritten, BULK_BUFFER_LENGTH);

   uint8_t bufferForPop[BULK_BUFFER_LENGTH];
   uint32_t countRead = 0;
   ring_buffer.popBulk(bufferForPop, sizeof(bufferForPop), &countRead);
   EXPECT_EQ(countRead, BULK_BUFFER_LENGTH);
}
