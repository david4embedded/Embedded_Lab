/************************************************************************************************************
 * 
 * @file lockable_FreeRTOS.hpp
 * @brief A class that implements a lockable resource using FreeRTOS mutexes.
 *  
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-07-27
 * @version 1.0
 * 
 ************************************************************************************************************/

#pragma once

#include "lockable_interface.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <cstdint>

namespace lib
{
/************************************************** Types ***************************************************/
/**
 * @brief A class that implements a lockable resource using FreeRTOS mutexes.
 * @details This class provides a simple interface for locking and unlocking resources in a FreeRTOS
 */
class LockableFreeRTOS : public ILockable
{
public:
   constexpr static uint32_t DEFAULT_TIMEOUT_MS = 2000;

   //!< Constructor and destructor
   LockableFreeRTOS( ) = default;
   ~LockableFreeRTOS() override = default;

   //!< Disable copy and move operations
   LockableFreeRTOS(const LockableFreeRTOS&) = delete;
   LockableFreeRTOS& operator=(const LockableFreeRTOS&) = delete;
   LockableFreeRTOS(LockableFreeRTOS&&) = delete;
   LockableFreeRTOS& operator=(LockableFreeRTOS&&) = delete;

   //!< Initialize the lockable resource
   bool initialize() override
   {
      m_mutex = xSemaphoreCreateMutex();
      if ( m_mutex == nullptr ) 
      {
         return false;
      }

      return true;
   }

   //!< Lock the resource
   void lock() override 
   {
      xSemaphoreTake( m_mutex, portMAX_DELAY );
   }

   //!< Try to lock the resource with a timeout
   bool try_lock( uint32_t timeout_ms = DEFAULT_TIMEOUT_MS ) override 
   {
      return xSemaphoreTake( m_mutex, timeout_ms ) == pdTRUE;
   }

   //!< Unlock the resource
   void unlock() override 
   {
      xSemaphoreGive( m_mutex );
   }

private:
   SemaphoreHandle_t m_mutex; //!< Handle to the FreeRTOS mutex used for locking
};
} // namespace lib