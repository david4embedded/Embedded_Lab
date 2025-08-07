/***************************************************************************************************
 * @file           : main.cpp
 * @brief          : Main program body
 * @details        : This file contains the main function and initializes the system.
 * @author         : Sungsu Kim
 * @date           : 2025-08-04
 * @copyright      : Copyright (c) 2025 Sungsu Kim
 ***************************************************************************************************/

/****************************************** Includes ***********************************************/
#include "main.h"
#include "common.h"
#include "stm32f4xx_nucleo_144.h"
#include "cmsis_os.h"
#include "lwip.h"
#include "lwip/tcp.h"

/******************************************** Consts ***********************************************/
#define ECHO_SERVER_ADDR_0  192
#define ECHO_SERVER_ADDR_1  168
#define ECHO_SERVER_ADDR_2  1
#define ECHO_SERVER_ADDR_3  3
#define ECHO_SERVER_PORT    7

/**************************************** Static Variables *****************************************/
UART_HandleTypeDef huart2;
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = 
{
   .name = "defaultTask",
   .stack_size = 1024 * 4,
   .priority = (osPriority_t) osPriorityNormal,
};

static struct tcp_pcb *echoServerPcb;
static struct tcp_pcb *clientPcb;

/**************************************** Local Functions ******************************************/
static void    MX_GPIO_Init         ( );
static void    MX_USART2_UART_Init  ( );
static void    SystemClock_Config   ( );
static void    startDefaultTask     ( void *argument );

static void    initTcpEchoServer   ( );
static err_t   echoAcceptCallback  ( void *arg, struct tcp_pcb *newpcb, err_t err );
static err_t   echoRecvCallback    ( void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err );


/*************************************** Function Definitions **************************************/
/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{
   HAL_Init();
   SystemClock_Config();

   MX_GPIO_Init();
   MX_USART2_UART_Init();
   BSP_LED_Init( LED_RED );
   BSP_LED_Init( LED_GREEN );
   BSP_LED_Init( LED_BLUE );

   LOGGING( "Welcome to STM32F439ZI LwIP TCP/IP Application" );

   osKernelInitialize();

   defaultTaskHandle = osThreadNew( startDefaultTask, NULL, &defaultTask_attributes );

   osKernelStart();

   while (1)
   { }
}

/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
static void startDefaultTask(void *argument)
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
 * @brief System Clock Configuration
 * @retval None
 */
static void SystemClock_Config(void)
{
   RCC_OscInitTypeDef RCC_OscInitStruct = {0};
   RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

   /** Configure the main internal regulator output voltage
    */
   __HAL_RCC_PWR_CLK_ENABLE();
   __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

   /** Initializes the RCC Oscillators according to the specified parameters
    * in the RCC_OscInitTypeDef structure.
    */
   RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
   RCC_OscInitStruct.HSIState = RCC_HSI_ON;
   RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
   RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
   RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
   RCC_OscInitStruct.PLL.PLLM = 8;
   RCC_OscInitStruct.PLL.PLLN = 180;
   RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
   RCC_OscInitStruct.PLL.PLLQ = 4;
   if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
   {
      Error_Handler();
   }

   /** Activate the Over-Drive mode
    */
   if (HAL_PWREx_EnableOverDrive() != HAL_OK)
   {
      Error_Handler();
   }

   /** Initializes the CPU, AHB and APB buses clocks
    */
   RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                               |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
   RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
   RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
   RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
   RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

   if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
   {
      Error_Handler();
   }
}

/**
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void)
{
   huart2.Instance = USART2;
   huart2.Init.BaudRate = 115200;
   huart2.Init.WordLength = UART_WORDLENGTH_8B;
   huart2.Init.StopBits = UART_STOPBITS_1;
   huart2.Init.Parity = UART_PARITY_NONE;
   huart2.Init.Mode = UART_MODE_TX_RX;
   huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
   huart2.Init.OverSampling = UART_OVERSAMPLING_16;
   if (HAL_UART_Init(&huart2) != HAL_OK)
   {
      Error_Handler();
   }
}

/**
 * @brief Redirects the C library printf function to the USART2.
 * @param file: File descriptor (not used)
 * @param ptr: Pointer to the data to be sent
 * @param len: Length of the data to be sent
 * @retval Number of bytes written
 */
extern "C" int _write(int file, char *ptr, int len)
{
   HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, HAL_MAX_DELAY);
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{
   /* GPIO Ports Clock Enable */
   __HAL_RCC_GPIOC_CLK_ENABLE();
   __HAL_RCC_GPIOH_CLK_ENABLE();
   __HAL_RCC_GPIOA_CLK_ENABLE();
   __HAL_RCC_GPIOB_CLK_ENABLE();
   __HAL_RCC_GPIOD_CLK_ENABLE();
   __HAL_RCC_GPIOG_CLK_ENABLE();
}

/**
 * @brief  Period elapsed callback in non blocking mode
 * @note   This function is called  when TIM6 interrupt took place, inside
 *         HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment a global variable "uwTick" used as application time base.
 * @param  htim : TIM handle
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
   if ( htim->Instance == TIM6 ) 
   {
      HAL_IncTick();
   }
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
   __disable_irq();
   while (1)
   { }
}

#ifdef  USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{

}
#endif /* USE_FULL_ASSERT */

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
      LOGGING( "TCP Echo Server is listening on port %d...", ECHO_SERVER_PORT );
   }
   else
   {
      LOGGING( "Failed to bind PCB." );
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

   LOGGING( "Client connected." );
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
      LOGGING( "Receive error: %d", err );
      if ( p != nullptr )
      {
         pbuf_free( p );
      }
      return err;
   }

   //!< Disconnect if p is null (client disconnected)
   if ( p == nullptr )
   {
      tcp_close( tpcb );
      LOGGING( "Client disconnected." );
      return ERR_OK;
   }

   LOGGING( "Received data: len=%d, %s\n", p->len, (char*)p->payload );

   //!< Echo the received data back to the client
   tcp_write( tpcb, p->payload, p->len, 1 );
   tcp_output( tpcb );

   pbuf_free( p );

   return ERR_OK;
}
