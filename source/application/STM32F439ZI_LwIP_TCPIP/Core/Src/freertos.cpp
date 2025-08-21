/************************************************************************************************************
 * 
 * @file freertos.cpp
 * @brief This file contains the FreeRTOS initialization and task management functions.
 *        Also, this file includes callbacks or implementations required for libraries or drivers.
 * 
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-08-21
 * @version 1.0
 * 
 ************************************************************************************************************/

/************************************************** Includes **************************************************/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"
#include "common.h"
#include "lwip.h"
#include "lwip/tcp.h"
#include "stm32f4xx_nucleo_144.h"
#include "cli.h"
#include "usart.h"
#include "logger.h"

/************************************************** Consts ****************************************************/
#define ECHO_SERVER_ADDR_0    192
#define ECHO_SERVER_ADDR_1    168
#define ECHO_SERVER_ADDR_2    1
#define ECHO_SERVER_ADDR_3    3
#define ECHO_SERVER_PORT      7

constexpr size_t CLI_BUFFER_SIZE = 128;

/*********************************************** Local Variables **********************************************/
static struct tcp_pcb   *echoServerPcb;
static struct tcp_pcb   *clientPcb;
static osThreadId       defaultTaskHandle;
static osThreadId       cliTaskHandle;

static StaticTask_t     xIdleTaskTCBBuffer;
static StackType_t      xIdleStack[configMINIMAL_STACK_SIZE];

/******************************************** Function Declarations *******************************************/
void           MX_FREERTOS_Init     ( );
static void    taskDefault          ( void const * argument );
static void    taskCli              ( void const * argument );

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
   const osThreadDef_t defaultTaskDef = { const_cast<char*>( "defaultTask" ), taskDefault, osPriorityNormal, 0, configMINIMAL_STACK_SIZE, nullptr, nullptr };
   defaultTaskHandle = osThreadCreate( &defaultTaskDef, nullptr );
   
   const osThreadDef_t cliTaskDef = { const_cast<char*>( "cliTask" ), taskCli, osPriorityNormal, 0, 512, nullptr, nullptr };
   cliTaskHandle = osThreadCreate( &cliTaskDef, nullptr );

   LOGGER_init();
}

/**
 * @brief Get the current tick count
 * 
 * @return uint32_t a tick count in milliseconds
 */
uint32_t LIB_COMMON_getTickMS( void )
{
   return static_cast<uint32_t>( xTaskGetTickCount() );
}

/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
static void taskDefault( void const * argument )
{
   PARAM_NOT_USED( argument );

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
 * @brief Function implementing the CLI task.
 * @param argument Not used
 */
static void taskCli( void const * argument )
{
   PARAM_NOT_USED( argument );

   LOGGING( "CLI Task Started..." );

   auto& cli = lib::CLI::getInstance();
   auto result = cli.initialize();
   if ( result != LibErrorCodes::eOK )
   {
      LOGGING( "CLI initialization failed, ret=0x%lx", result );
      return;
   }

   char buffer[CLI_BUFFER_SIZE] = {0};

   for(;;)
   {
      if ( cli.getNewCommandLine( buffer, sizeof( buffer ), 30000 ) == LibErrorCodes::eOK )
      {
         LOGGING( "Received command line: %s", buffer );
         cli.processInput( buffer );
      }

      osDelay( 10 );
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
   PARAM_NOT_USED( arg );

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
 * @brief UART transmission complete callback.
 * @details This function is called when the UART transmission is complete in the interrupt context through the HAL,
 *          and it signals the logging thread on the completion of the transmission.
 */
extern "C" void HAL_UART_TxCpltCallback( UART_HandleTypeDef *huart )
{
   if ( huart->Instance == USART3 )
   {
      LOGGER_msgXferCompleteCallback();
   }
}

/**
 * @brief Get memory requirements for the Idle task
 * 
 * @param ppxIdleTaskTCBBuffer double pointer to the Idle task's TCB
 * @param ppxIdleTaskStackBuffer double pointer to the Idle task's stack
 * @param pulIdleTaskStackSize pointer to the Idle task's stack size
 */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
   *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
   *ppxIdleTaskStackBuffer = &xIdleStack[0];
   *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

/**
 * @brief Stack overflow hook
 * 
 * @param xTask a task handle
 * @param pcTaskName a pointer to the task name
 */
void vApplicationStackOverflowHook( xTaskHandle xTask, signed char *pcTaskName )
{
   PARAM_NOT_USED( xTask );
   PARAM_NOT_USED( pcTaskName ); 
}
