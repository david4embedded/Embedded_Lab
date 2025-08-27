/************************************************************************************************************
 * 
 * @file semaphore_FreeRTOS.h
 * @brief FreeRTOS implementation of the ISemaphore interface.
 *  
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-08-18
 * @version 1.0
 * 
 ************************************************************************************************************/

#pragma once

/************************************************ Includes **************************************************/ 
#include "semaphore_interface.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "queue.h"

namespace lib
{
/************************************************** Types ***************************************************/
/**
 * @brief FreeRTOS implementation of the ISemaphore interface.
 */
class Semaphore_FreeRTOS : public ISemaphore
{
public:
   Semaphore_FreeRTOS()
   {  }
   
   ~Semaphore_FreeRTOS()
   {  }

   //!< Disable copy and move operations
   Semaphore_FreeRTOS(const Semaphore_FreeRTOS&) = delete;
   Semaphore_FreeRTOS& operator=(const Semaphore_FreeRTOS&) = delete;
   Semaphore_FreeRTOS(Semaphore_FreeRTOS&&) = delete;
   Semaphore_FreeRTOS& operator=(Semaphore_FreeRTOS&&) = delete;

   /**
    * @inheritDoc
    */
   ErrorCode initialize( uint32_t maxCount, uint32_t initialCount ) override
   {
      m_semaphore = xSemaphoreCreateCounting( maxCount, initialCount );
      if ( m_semaphore == nullptr )
      {
         return LibErrorCodes::eSEMAPHORE_INIT_FAILED;
      }
      return LibErrorCodes::eOK;
   }

   /**
    * @inheritDoc
    */
   void put( ) override
   {
      if ( m_semaphore == nullptr )
      {
         return;
      }
      xSemaphoreGive( m_semaphore );
   }

   /**
    * @inheritDoc
    */
   void putISR( ) override
   {
      if ( m_semaphore == nullptr )
      {
         return;
      }
      xSemaphoreGiveFromISR( m_semaphore, nullptr );
   }

   /**
    * @inheritDoc
    */
   ErrorCode get( uint32_t timeout_ms ) override
   {
      if ( m_semaphore == nullptr )
      {
         return LibErrorCodes::eSEMAPHORE_NOT_INITIALIZED;
      }
      
      if( xSemaphoreTake( m_semaphore, pdMS_TO_TICKS(timeout_ms) ) != pdTRUE )
      {
         return LibErrorCodes::eSEMAPHORE_GET_TIME_OUT;
      }   
      return LibErrorCodes::eOK;
   }

private:
    SemaphoreHandle_t m_semaphore{ nullptr };
};
}