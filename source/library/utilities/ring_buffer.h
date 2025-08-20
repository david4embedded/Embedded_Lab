/***************************************************************************************************
 * @file           : ring_buffer.h
 * @brief          : Ring buffer implementation
 * @details        : This file contains the definition of the RingBuffer class.
 * @author         : Sungsu Kim
 * @date           : 2025-08-05
 * @copyright      : Copyright (c) 2025 Sungsu Kim
 ***************************************************************************************************/

 #pragma once

/****************************************** Includes ***********************************************/ 
#include "error_codes_lib.h"
#include <cstdint>

/******************************************** Types ************************************************/
namespace lib
{
/**
 * @brief RingBuffer class template
 * 
 * @tparam T Type of elements stored in the ring buffer
 */
template<typename T>
class RingBuffer
{
public:
   /**
    * @brief Construct a RingBuffer
    * 
    * @param buffer Pointer to the buffer
    * @param size Size of the buffer
    */
   RingBuffer( T* buffer, uint32_t size )
   {
      if ( buffer == nullptr || size == 0 )
      {
         return;
      }

      m_buffer = buffer;
      m_size = size;
      m_head = 0;
      m_tail = 0;
      m_count = 0;
   }

   ~RingBuffer()
   { }

   //!< Non-copyable and non-movable
   RingBuffer( const RingBuffer& ) = delete;
   RingBuffer& operator=( const RingBuffer& ) = delete;
   RingBuffer( RingBuffer&& ) = delete;
   RingBuffer& operator=( RingBuffer&& ) = delete;

   /**
    * @brief Push an element into the ring buffer
    * 
    * @param data Element to be pushed
    * @return ErrorCode Indicates success or failure (eRING_BUFFER_FULL if the buffer is full)
    */
   ErrorCode push( const T& data )
   {
      if ( isFull() )
      {
         return LibErrorCodes::eRING_BUFFER_FULL;
      }

      m_buffer[m_tail] = data;
      m_tail = ( m_tail + 1 ) % m_size;
      m_count++;

      return LibErrorCodes::eOK;
   }

   /**
    * @brief Push multiple elements into the ring buffer
    * 
    * @param data Pointer to the data to be pushed
    * @param sizeBuffer Size of the data buffer
    * @param countWritten Pointer to store the number of elements written
    */
   void pushBulk( const T* data, uint32_t sizeBuffer, uint32_t *countWritten )
   {
      uint32_t counts = 0;
      if ( data == nullptr || sizeBuffer == 0 || countWritten == nullptr )
      {
         if ( countWritten != nullptr )
         {
            *countWritten = 0;
         }
         return;
      }

      for ( uint32_t i = 0; i < sizeBuffer; i++ )
      {
         if ( push( data[i] ) != LibErrorCodes::eOK )
         {
            break;
         }
         counts++;
      }

      *countWritten = counts;
   }

   /**
    * @brief Pop an element from the ring buffer
    * 
    * @param data Reference to store the popped element
    * @return ErrorCode Indicates success or failure (eRING_BUFFER_EMPTY if the buffer is empty)
    */
   ErrorCode pop( T& data )
   {
      if ( isEmpty() )
      {
         return LibErrorCodes::eRING_BUFFER_EMPTY;
      }

      data = m_buffer[m_head];
      m_head = ( m_head + 1 ) % m_size;
      m_count--;

      return LibErrorCodes::eOK;
   }

   /**
    * @brief Pop multiple elements from the ring buffer
    * 
    * @param data Pointer to the buffer to store the popped elements
    * @param sizeBuffer Size of the buffer
    * @param countRead Pointer to store the number of elements read
    */
   void popBulk( T* data, uint32_t sizeBuffer, uint32_t *countRead )
   {
      if ( data == nullptr || sizeBuffer == 0 || countRead == nullptr )
      {
         return;
      }

      uint32_t count = 0;
      while ( count < sizeBuffer && m_count > 0 )
      {
         data[count++] = m_buffer[m_head];
         m_head = ( m_head + 1 ) % m_size;
         m_count--;
      }

      *countRead = count;
   }

   //!< Useful getters
   inline bool       isEmpty  () const { return m_count == 0; }
   inline bool       isFull   () const { return m_count == m_size; }
   inline uint32_t   count    () const { return m_count; }
   inline uint32_t   size     () const { return m_size; }

private:
   T* m_buffer;            //!< Pointer to the buffer
   uint32_t m_size;        //!< Size of the buffer
   uint32_t m_head;        //!< Index of the head
   uint32_t m_tail;        //!< Index of the tail
   uint32_t m_count;       //!< Number of elements in the buffer
};

}
