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
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "common.h"
#include "lwip.h"
#include "lwip/tcp.h"
#include "stm32f4xx_nucleo_144.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ECHO_SERVER_ADDR_0  192
#define ECHO_SERVER_ADDR_1  168
#define ECHO_SERVER_ADDR_2  1
#define ECHO_SERVER_ADDR_3  3
#define ECHO_SERVER_PORT    7
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
static struct tcp_pcb *echoServerPcb;
static struct tcp_pcb *clientPcb;
/* USER CODE END Variables */
osThreadId defaultTaskHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static void    initTcpEchoServer    ( );
static err_t   echoAcceptCallback   ( void *arg, struct tcp_pcb *newpcb, err_t err );
static err_t   echoRecvCallback     ( void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err );
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void const * argument);

extern void MX_LWIP_Init(void);
void MX_FREERTOS_Init(void);

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

/* Hook prototypes */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName);

/* USER CODE BEGIN 4 */
__weak void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
   /* Run time stack overflow checking is performed if
   configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2. This hook function is
   called if a stack overflow is detected. */
}
/* USER CODE END 4 */

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
  /* place for user code */
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 512);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{
  /* init code for LWIP */
  MX_LWIP_Init();
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
   
   initTcpEchoServer();

   for(;;)
   {    
      osDelay(1000);
      LOGGING( "Default Task" );
      BSP_LED_Toggle( LED_BLUE );
   }

  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
/**
 * @brief Initializes the TCP Echo Server.
 */
static void initTcpEchoServer( )
{
   ip_addr_t ipAddr;
   IP4_ADDR( &ipAddr, ECHO_SERVER_ADDR_0, ECHO_SERVER_ADDR_1, ECHO_SERVER_ADDR_2, ECHO_SERVER_ADDR_3 );

   echoServerPcb = tcp_new();
   if ( echoServerPcb == NULL ) 
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
      echoServerPcb = NULL;
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
      if ( p != NULL )
      {
         goto EXIT;
      }
      return err;
   }

   //!< Disconnect if p is null (client disconnected)
   if ( p == NULL )
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

/* USER CODE END Application */
