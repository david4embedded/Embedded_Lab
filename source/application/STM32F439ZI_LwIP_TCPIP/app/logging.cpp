
#include "logging.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "common.h"
#include "lockable_FreeRTOS.hpp"
#include "semaphore_FreeRTOS.h"
#include "lockguard.hpp"
#include "ring_buffer.h"
#include "usart.h"
#include <string.h>

constexpr size_t   LOGGING_BUFFER_SIZE = 512;
constexpr size_t   SERIAL_BUFFER_SIZE  = 256;
constexpr uint32_t TIMEOUT_MS          = 10000;

static osThreadId loggingTaskHandle;
static lib::LockableFreeRTOS loggingLock;

static uint8_t buffer[LOGGING_BUFFER_SIZE];
static lib::RingBuffer<uint8_t> loggingBuffer{ buffer, sizeof( buffer ) };

static lib::Semaphore_FreeRTOS semLogAvailable;
static lib::Semaphore_FreeRTOS semTxComplete;
static bool loggingInit = false;

static void taskLogging ( void const * argument );
static void writeLog    ( const char *message );

void LOGGING_init( )
{
   semLogAvailable.initialize( LOGGING_BUFFER_SIZE, 0 );
   semTxComplete.initialize( 1, 0 );

   loggingLock.initialize();

   osThreadDef( loggingTask, taskLogging, osPriorityNormal, 0, 512 );
   loggingTaskHandle = osThreadCreate( osThread(loggingTask), nullptr );

   loggingInit = true;
}

static void taskLogging( void const * argument )
{
   PARAM_NOT_USED( argument );

   for(;;)
   {
      if ( semLogAvailable.get( TIMEOUT_MS ) != LibErrorCodes::eOK )
      {
         continue;
      }

      uint8_t serialTxbuffer[SERIAL_BUFFER_SIZE] = {0};      
      uint32_t countRead = 0;

      lib::lock_guard guard( loggingLock );
      loggingBuffer.popBulk( serialTxbuffer, sizeof(serialTxbuffer), &countRead );
      guard.~lock_guard();

      if ( countRead )
      {
         serialTxbuffer[countRead] = '\0';
         HAL_UART_Transmit_IT( &huart3, serialTxbuffer, countRead );
         semTxComplete.get( TIMEOUT_MS );
      }
   }
}

static void writeLog( const char *message )
{
   if ( !loggingInit )
   {
      return;
   }

   uint32_t countWritten = 0;

   lib::lock_guard guard( loggingLock );
   loggingBuffer.pushBulk( reinterpret_cast<const uint8_t*>( message ), strlen( message ), &countWritten );
   semLogAvailable.put();
}

/**
 * @brief Redirects the C library printf function to the USART2.
 * @param file: File descriptor (not used)
 * @param ptr: Pointer to the data to be sent
 * @param len: Length of the data to be sent
 * @retval Number of bytes written
 */
extern "C" int _write( int file, char *ptr, int len )
{
   ptr[len] = '\0';
   writeLog( ptr );
}

extern "C" void HAL_UART_TxCpltCallback( UART_HandleTypeDef *huart )
{
   if ( huart->Instance == USART3 )
   {
      memset( buffer, 0, sizeof(buffer) );
      semTxComplete.putISR();
   }
}