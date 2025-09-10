/************************************************************************************************************
 * 
 * @file lockguard.hpp
 * @brief A simple lock guard implementation for managing locks more conveniently, i.e., RAII-style.
 *  
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-07-27
 * @version 1.0
 * 
 ************************************************************************************************************/

#pragma once

namespace lib 
{
/************************************************** Types ***************************************************/
/**
 * @brief A simple lock guard class that locks a resource upon construction and unlocks it upon destruction.
 * @details This class is used to manage locks in a RAII (Resource Acquisition Is Initialization) style, ensuring that the lock is released when the guard goes out of scope.
 */
class lock_guard
{
public:
   //!< Constructor that locks the resource
   explicit lock_guard( ILockable& lockable, uint32_t timeout_ms = 0 )
    : m_lockable( lockable )
    , m_locked( false )
   {
      if ( timeout_ms == 0 ) 
      {
         m_lockable.lock();
         m_locked = true;
      } 
      else 
      {
         m_locked = m_lockable.try_lock( timeout_ms );
      }
   }

   //!< Destructor that unlocks the resource if it was locked
   ~lock_guard()
   {
      if ( m_locked ) 
      {
         m_lockable.unlock();
      }
      m_locked = false;
   }

   //!< Disable copy and move operations
   lock_guard(const lock_guard&) = delete;
   lock_guard& operator=(const lock_guard&) = delete;
   lock_guard(lock_guard&&) = delete;
   lock_guard& operator=(lock_guard&&) = delete;

private:
   ILockable& m_lockable;  //!< Reference to the lockable resource
   bool m_locked;          //!< Flag indicating if the resource is locked   
};
} // namespace lib