/************************************************************************************************************
 * 
 * @file config_serial_device.cpp
 * @brief Configuration for the serial device.
 * @details This file contains the interrupt handlers for the UART as well, and thus, 
 *          callback functions that need to be called within those interrupt contexts are referenced here.
 *  
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-08-23
 * @version 1.0
 * 
 ************************************************************************************************************/

/************************************************ Includes **************************************************/
#include "config_serial_device.h"
#include "config_cli.h"
#include "config_serial_wifi.h"
#include "lockable_FreeRTOS.hpp"
#include "semaphore_FreeRTOS.h"
#include "usart.h"

/************************************************* Consts ***************************************************/
constexpr size_t RX_BUFFER_SIZE = 128;

/******************************************* Function Declarations ******************************************/    
static bool isNewUartRxData   ( UART_HandleTypeDef *huart );
static void sendUart1         ( const uint8_t* data, size_t length );
static void sendUart2         ( const uint8_t* data, size_t length );

/********************************************* Local Variables **********************************************/    
//!< For SerialDevice #1 (Logger module)
static uint8_t                 rxBuffer1[RX_BUFFER_SIZE] = {0};
static lib::LockableFreeRTOS   lockable1;
static lib::Semaphore_FreeRTOS semTxComplete1;
static lib::Semaphore_FreeRTOS semNewRxBytes1;
static lib::SerialDevice       serialDevice1{ sendUart1, rxBuffer1, sizeof(rxBuffer1), lockable1, semTxComplete1, semNewRxBytes1 };

//!< For SerialDevice #2 (SerialWifi module)
static uint8_t                 rxBuffer2[RX_BUFFER_SIZE] = {0};
static lib::LockableFreeRTOS   lockable2;
static lib::Semaphore_FreeRTOS semTxComplete2;
static lib::Semaphore_FreeRTOS semNewRxBytes2;
static lib::SerialDevice       serialDevice2{ sendUart2, rxBuffer2, sizeof(rxBuffer2), lockable2, semTxComplete2, semNewRxBytes2 };

/******************************************* Function Definitions *******************************************/    
/**
 * @brief Get the serial device instance.
 * @param device The serial device type.
 * @return Reference to the serial device.
 */
lib::SerialDevice& SERIAL_DEVICE_get( eSerialDevice device )
{
   return ( device == eSerialDevice::DEVICE_1 ) ? serialDevice1 : serialDevice2;
}

/**
 * @brief This function handles USART2 global interrupt.
 */
extern "C" void USART2_IRQHandler(void)
{
   HAL_UART_IRQHandler( &huart2 );
   if ( isNewUartRxData( &huart2 ) )
   {
      auto &serialDevice = SERIAL_DEVICE_get( eSerialDevice::DEVICE_2 );
      serialDevice.pushRxByte( static_cast<uint8_t>( huart2.Instance->DR ) );
   }
}

/**
 * @brief This function handles USART3 global interrupt.
 */
extern "C" void USART3_IRQHandler(void)
{
   HAL_UART_IRQHandler( &huart3 );
   if ( isNewUartRxData( &huart3 ) )
   {
      auto& cli = lib::CLI::getInstance();
      cli.putCharIntoBuffer( static_cast<uint8_t>( huart3.Instance->DR ) );
   }
}

/**
 * @brief UART transmission complete callback.
 * @details This function is called when the UART transmission is complete in the interrupt context through the HAL,
 *          and it signals the logging thread on the completion of the transmission.
 */
extern "C" void HAL_UART_TxCpltCallback( UART_HandleTypeDef *huart )
{
   if ( huart->Instance == USART2 )
   {
      auto &serialDevice = SERIAL_DEVICE_get( eSerialDevice::DEVICE_2 );
      serialDevice.notifySendComplete();
   }

   if ( huart->Instance == USART3 )
   {
      auto &serialDevice = SERIAL_DEVICE_get( eSerialDevice::DEVICE_1 );
      serialDevice.notifySendComplete();
   }
}

/**
 * @brief Send data over UART.
 * 
 * @param data Pointer to the data to be sent.
 * @param length Length of the data to be sent.
 */
static void sendUart1( const uint8_t* data, size_t length )
{
   HAL_UART_Transmit_IT( &huart3, (uint8_t*)data, length );
}

/**
 * @brief Send data over UART.
 * 
 * @param data Pointer to the data to be sent.
 * @param length Length of the data to be sent.
 */
static void sendUart2( const uint8_t* data, size_t length )
{
   HAL_UART_Transmit_IT( &huart2, (uint8_t*)data, length );
}

/**
 * @brief Check if there is new UART RX data.
 * @param huart Pointer to the UART handle.
 * @return True if there is new RX data, false otherwise.
 */
static bool isNewUartRxData( UART_HandleTypeDef *huart )
{
   if ( huart->gState == HAL_UART_STATE_BUSY_RX )
   {
      return false;
   }

   uint32_t isrflags  = READ_REG(huart->Instance->SR);
   uint32_t cr1its    = READ_REG(huart->Instance->CR1);
   return ( ( ( isrflags & USART_SR_RXNE ) != RESET ) && ( ( cr1its & USART_CR1_RXNEIE ) != RESET ) );
}
