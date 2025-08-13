/************************************************************************************************************
 * 
 * @file ILockable.hpp
 * @brief Interface for lockable resources, e.g., mutexes.
 *  
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-07-27
 * @version 1.0
 * 
 ************************************************************************************************************/

#pragma once

/************************************************** Includes *************************************************/
#include <stdint.h>

namespace lib
{
/************************************************** Types ***************************************************/
class ILockable
{
public:
   //!< Constructor and destructor
   ILockable() = default;
   virtual ~ILockable() = default;

   //!< Initialize the lockable resource
   virtual bool initialize() = 0;

   //!< Lock the resource
   virtual void lock() = 0;

   //!< Try to lock the resource with a timeout
   virtual bool try_lock( uint32_t timeout_ms ) = 0;

   //!< Unlock the resource
   virtual void unlock() = 0;

   //!< Disable copy and move operations
   ILockable(const ILockable&) = delete;
   ILockable& operator=(const ILockable&) = delete;
   ILockable(ILockable&&) = delete;
   ILockable& operator=(ILockable&&) = delete;
};
} // namespace lib