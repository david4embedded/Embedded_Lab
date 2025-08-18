/************************************************************************************************************
 * 
 * @file ISemaphore.h
 * @brief Interface for semaphore operations.
 *  
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-08-18
 * @version 1.0
 * 
 ************************************************************************************************************/

#pragma once 

/************************************************ Includes **************************************************/ 
#include "error_codes_lib.h"
#include <stdint.h>

namespace lib
{
/************************************************** Types ***************************************************/
/**
 * @brief Interface for semaphore operations.
 */
class ISemaphore
{
public:
   ISemaphore() = default;
   virtual ~ISemaphore() = default;

   //!< Disable copy and move operations
   ISemaphore(const ISemaphore&) = delete;
   ISemaphore& operator=(const ISemaphore&) = delete;
   ISemaphore(ISemaphore&&) = delete;
   ISemaphore& operator=(ISemaphore&&) = delete;

   /**
    * @brief Initializes the semaphore.
    * 
    * @param maxCount Maximum count for the semaphore.
    * @param initialCount Initial count for the semaphore.
    */
   virtual ErrorCode initialize( uint32_t maxCount, uint32_t initialCount ) = 0;

   /**
    * @brief Releases the semaphore.
    */
   virtual void put( ) = 0;

   /**
    * @brief Waits for the semaphore to be available.
    * 
    * @param timeout_ms Timeout in milliseconds.
    * @return ErrorCode 
    */
   virtual ErrorCode get( uint32_t timeout_ms ) = 0;
};
}