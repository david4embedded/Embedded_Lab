/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/************************************************** Includes **************************************************/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"
#include "common.h"
#include "lwip.h"
#include "lwip/tcp.h"
#include "stm32f4xx_nucleo_144.h"

/************************************************** Consts ****************************************************/
#define ECHO_SERVER_ADDR_0    192
#define ECHO_SERVER_ADDR_1    168
#define ECHO_SERVER_ADDR_2    1
#define ECHO_SERVER_ADDR_3    3
#define ECHO_SERVER_PORT      7

/*********************************************** Local Variables **********************************************/
static struct tcp_pcb   *echoServerPcb;
static struct tcp_pcb   *clientPcb;
static osThreadId       defaultTaskHandle;

static StaticTask_t     xIdleTaskTCBBuffer;
static StackType_t      xIdleStack[configMINIMAL_STACK_SIZE];

/******************************************** Function Declarations *******************************************/
void           MX_FREERTOS_Init     ( );
static void    startDefaultTask     ( void const * argument );

static void    initTcpEchoServer    ( );
static err_t   echoAcceptCallback   ( void *arg, struct tcp_pcb *newpcb, err_t err );
static err_t   echoRecvCallback     ( void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err );

extern "C" void vApplicationGetIdleTaskMemory   ( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );
extern "C" void vApplicationStackOverflowHook   ( xTaskHandle xTask, signed char *pcTaskName );

/******************************************** Function Definitions ********************************************/
/**
 * @brief  FreeRTOS initialization
 * @param  None
 * @retval None
 */
void MX_FREERTOS_Init(void) 
{
   osThreadDef(defaultTask, startDefaultTask, osPriorityNormal, 0, 512);
   defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);
}

/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
static void startDefaultTask(void const * argument)
{
   MX_LWIP_Init();
   
   initTcpEchoServer();

   for(;;)
   {    
      osDelay(1000);
      LOGGING( "Default Task" );
      BSP_LED_Toggle( LED_BLUE );
   }
}

/**
 * @brief Initializes the TCP Echo Server.
 */
static void initTcpEchoServer( )
{
   ip_addr_t ipAddr;
   IP4_ADDR( &ipAddr, ECHO_SERVER_ADDR_0, ECHO_SERVER_ADDR_1, ECHO_SERVER_ADDR_2, ECHO_SERVER_ADDR_3 );

   echoServerPcb = tcp_new();
   if ( echoServerPcb == nullptr ) 
   {
      LOGGING( "Failed to create PCB." );
      return;
   }

   if ( tcp_bind( echoServerPcb, &ipAddr, ECHO_SERVER_PORT ) == ERR_OK )
   {
      echoServerPcb = tcp_listen( echoServerPcb );
      tcp_accept( echoServerPcb, echoAcceptCallback );
      LOGGING( "TCPIP: Echo Server is listening on port %d...", ECHO_SERVER_PORT );
   }
   else
   {
      LOGGING( "TCPIP: Failed to bind PCB." );
      tcp_abort( echoServerPcb );
      echoServerPcb = nullptr;
   }
}

/**
 * @brief Callback function for accepting new TCP connections.
 * 
 * @param arg argument passed to the callback (not used)
 * @param newpcb pointer to the new TCP PCB
 * @param err error code (not used)
 * @return err_t 
 */
static err_t echoAcceptCallback( void *arg, struct tcp_pcb *newpcb, err_t err )
{
   (void)arg;
   (void)err;

   LOGGING( "TCPIP: Client connected." );
   clientPcb = newpcb;

   //!< Set the receive callback for the new PCB
   tcp_recv( newpcb, echoRecvCallback );

   return ERR_OK;
}

/**
 * @brief Callback function for receiving data on the TCP connection.
 * 
 * @param arg argument passed to the callback (not used)
 * @param tpcb pointer to the TCP PCB
 * @param p pointer to the received pbuf
 * @param err error code (not used)
 * @return err_t 
 */
static err_t echoRecvCallback( void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err )
{
   if ( err != ERR_OK )
   {
      LOGGING( "TCPIP: Receive error: %d", err );
      if ( p != nullptr )
      {
         goto EXIT;
      }
      return err;
   }

   //!< Disconnect if p is null (client disconnected)
   if ( p == nullptr )
   {
      tcp_close( tpcb );
      LOGGING( "TCPIP: Client disconnected." );
      return ERR_OK;
   }

   LOGGING( "TCPIP: Received data: len=%d", p->len );

   //!< Echo the received data back to the client
   tcp_write( tpcb, p->payload, p->len, 1 );
   tcp_output( tpcb );

EXIT:   
   pbuf_free( p );
   return err;
}

/**
 * @brief  Function to handle stack overflow.
 * 
 * @param xTask a handle to the task that overflowed
 * @param pcTaskName the name of the task that overflowed
 * @return __weak 
 */
__weak void vApplicationStackOverflowHook( xTaskHandle xTask, signed char *pcTaskName )
{
   /* Run time stack overflow checking is performed if
   configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2. This hook function is
   called if a stack overflow is detected. */
}

/**
 * @brief  Function to get memory for the idle task.
 * 
 * @param ppxIdleTaskTCBBuffer a double pointer to the task control block
 * @param ppxIdleTaskStackBuffer a double pointer to the stack buffer
 * @param pulIdleTaskStackSize a pointer to the stack size
 */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
