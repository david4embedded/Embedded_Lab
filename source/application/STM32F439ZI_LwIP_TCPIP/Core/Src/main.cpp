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
//#define TCP_LOOPBACK_TEST
//#define TCP_ECHO_SERVER_TEST

#if defined (TCP_LOOPBACK_TEST)
#define LOOPBACK_TEST_PORT 1234
#define TEST_MESSAGE "Hello, Loopback!"
#endif

#if defined (TCP_ECHO_SERVER_TEST)
#define ECHO_SERVER_PORT    7
#endif

/**************************************** Static Variables *****************************************/
UART_HandleTypeDef huart2;
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = 
{
   .name = "defaultTask",
   .stack_size = 1024 * 4,
   .priority = (osPriority_t) osPriorityNormal,
};

#if defined (TCP_LOOPBACK_TEST)

#endif

#if defined (TCP_ECHO_SERVER_TEST)
static struct tcp_pcb *echo_server_pcb;
static struct tcp_pcb *client_pcb;
#endif

/**************************************** Local Functions ******************************************/
static void MX_GPIO_Init         ( );
static void MX_USART2_UART_Init  ( );
static void SystemClock_Config   ( );
static void StartDefaultTask     ( void *argument );

#if defined (TCP_ECHO_SERVER_TEST)
static void  tcp_echo_server_init   ( );
static err_t echo_accept_callback   ( void *arg, struct tcp_pcb *newpcb, err_t err );
static err_t echo_recv_callback     ( void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err );
#endif

#if defined (TCP_LOOPBACK_TEST)
static void  tcp_loopback_client_test  ( );
static void  tcp_loopback_server_test  ( );
static void  lwip_loopback_test_init   ( );
static err_t client_connected_callback (void *arg, struct tcp_pcb *tpcb, err_t err);
static err_t server_accept_callback    (void *arg, struct tcp_pcb *tpcb, err_t err);
#endif

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

   LOGGING("Welcome!");

   osKernelInitialize();

   defaultTaskHandle = osThreadNew( StartDefaultTask, NULL, &defaultTask_attributes );

   osKernelStart();

   while (1)
   { }
}

/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
static void StartDefaultTask(void *argument)
{
   MX_LWIP_Init();

#if defined (TCP_ECHO_SERVER_TEST)
   tcp_echo_server_init();
#endif

#if defined (TCP_LOOPBACK_TEST)
   lwip_loopback_test_init();
#endif

   for(;;)
   {    
      osDelay(1000);
      //LOGGING( "Default Task\r\n" );
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
   if (htim->Instance == TIM6) 
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

#if defined (TCP_ECHO_SERVER_TEST)

// TCP 에코 서버 초기화 함수
static void tcp_echo_server_init(void)
{
   ip_addr_t ip_addr;
   IP4_ADDR(&ip_addr, 192, 168, 1, 3); // Nucleo의 IP 주소 설정 (환경에 맞게 변경)

   echo_server_pcb = tcp_new();
   if ( echo_server_pcb == NULL ) 
   {
      LOGGING( "Failed to create PCB." );
      return;
   }

   if ( tcp_bind(echo_server_pcb, &ip_addr, ECHO_SERVER_PORT) == ERR_OK )
   {
      echo_server_pcb = tcp_listen(echo_server_pcb);
      tcp_accept(echo_server_pcb, echo_accept_callback);
      LOGGING( "TCP Echo Server is listening on port %d.", ECHO_SERVER_PORT );
   }
   else
   {
      LOGGING( "Failed to bind PCB." );
      tcp_abort(echo_server_pcb);
      echo_server_pcb = NULL;
   }
}

// 새로운 연결을 수락하는 콜백 함수
static err_t echo_accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
   (void)arg;
   (void)err;

   LOGGING( "Client connected." );
   client_pcb = newpcb;

   // 콜백 함수 설정
   tcp_recv(newpcb, echo_recv_callback);

   return ERR_OK;
}

// 수신된 데이터를 에코하고 해제하는 콜백 함수
static err_t echo_recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
   // 데이터 수신 에러
   if (err != ERR_OK)
   {
      if (p != NULL)
      {
         pbuf_free(p);
      }
      return err;
   }

   // 클라이언트 연결 해제
   if (p == NULL)
   {
      tcp_close(tpcb);
      LOGGING( "Client disconnected." );
      return ERR_OK;
   }

   // 데이터 수신
   LOGGING( "Received data: %s\n", (char*)p->payload );

   // 데이터 에코
   tcp_write(tpcb, p->payload, p->len, 1);
   tcp_output(tpcb);

   pbuf_free(p);
   return ERR_OK;
}

#endif /* TCP_ECHO_SERVER_TEST */

#if defined (TCP_LOOPBACK_TEST)
// main 함수나 Task에서 호출
static void lwip_loopback_test_init(void)
{
   tcp_loopback_server_test(); // 서버 시작
   HAL_Delay(100);             // 서버가 준비될 때까지 잠시 대기
   tcp_loopback_client_test(); // 클라이언트 시작
}

// TCP 클라이언트 테스트 함수
void tcp_loopback_client_test(void)
{
   struct tcp_pcb *tpcb;
   ip_addr_t server_ip;

   // 루프백 주소(127.0.0.1) 설정
   IP4_ADDR(&server_ip, 127, 0, 0, 1);

   tpcb = tcp_new();
   if (tpcb != NULL)
   {
      // 서버에 연결
      tcp_connect(tpcb, &server_ip, LOOPBACK_TEST_PORT, client_connected_callback);
   }
}

// TCP 서버 테스트 함수
static void tcp_loopback_server_test(void)
{
   struct tcp_pcb *tpcb;
   ip_addr_t server_ip;

   // 루프백 주소(127.0.0.1) 설정
   IP4_ADDR(&server_ip, 127, 0, 0, 1);

   tpcb = tcp_new();
   if (tpcb != NULL)
   {
      tcp_bind(tpcb, &server_ip, LOOPBACK_TEST_PORT);
      tpcb = tcp_listen(tpcb);
      tcp_accept(tpcb, server_accept_callback);
   }
}

static err_t client_connected_callback(void *arg, struct tcp_pcb *tpcb, err_t err)
{
   return ERR_OK;
}

static err_t server_accept_callback(void *arg, struct tcp_pcb *tpcb, err_t err)
{
   return ERR_OK;
}
#endif /* TCP_LOOPBACK_TEST */