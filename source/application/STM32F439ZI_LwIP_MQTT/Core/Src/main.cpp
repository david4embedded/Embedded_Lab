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
#include "usart.h"
#include "common.h"
#include "stm32f4xx_nucleo_144.h"
#include "cmsis_os.h"
#include "lwip.h"
#include "lwip/tcp.h"
#include "MQTTClient.h"
#include "MQTTInterface.h"
#include <string.h>

/******************************************** Consts ***********************************************/
#define  BROKER_IP     "192.168.1.2"
#define  MQTT_PORT	  1883
#define  MQTT_BUFSIZE  1024

/**************************************** Global Variables *****************************************/
extern struct netif gnetif;
extern uint32_t MilliTimer;

/**************************************** Static Variables *****************************************/
osThreadId  defaultTaskHandle;
osThreadId  mqttClientSubTaskHandle;
osThreadId  mqttClientPubTaskHandle;
Network     net;
MQTTClient  mqttClient;

uint8_t sndBuffer[MQTT_BUFSIZE]; 
uint8_t rcvBuffer[MQTT_BUFSIZE]; 
uint8_t msgBuffer[MQTT_BUFSIZE];

/**************************************** Local Functions ******************************************/
static void    MX_GPIO_Init         ( );
static void    SystemClock_Config   ( );
static void    startDefaultTask     ( const void *argument );

static void    mqttClientSubTask    ( const void *argument );    
static void    mqttClientPubTask    ( const void *argument ); 
static int     mqttConnectBroker    ( ); 				  
static void    mqttMessageArrived   ( MessageData* msg ); 

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
   MX_USART3_UART_Init();
   BSP_LED_Init( LED_RED );
   BSP_LED_Init( LED_GREEN );
   BSP_LED_Init( LED_BLUE );

   LOGGING( "Welcome to STM32F439ZI LwIP TCP/IP Application" );

   osThreadDef( defaultTask, startDefaultTask, osPriorityNormal, 0, 1024 );
   defaultTaskHandle = osThreadCreate(osThread(defaultTask), nullptr );

   osKernelStart();

   while (1)
   { }
}

/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
static void startDefaultTask( const void *argument )
{
   MX_LWIP_Init();

   // osDelay(1000);      
   // osThreadDef( mqttClientSub, mqttClientSubTask, osPriorityNormal, 0, 2048 );
   // mqttClientSubTaskHandle = osThreadCreate( osThread( mqttClientSub ), nullptr );

   // osDelay(1000);      
   // osThreadDef( mqttClientPub, mqttClientPubTask, osPriorityNormal, 0, 2048 );
   // mqttClientPubTaskHandle = osThreadCreate( osThread( mqttClientPub ), nullptr );

   for(;;)
   {    
      osDelay(1000);
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

   if (htim->Instance == TIM6) 
   {
      MilliTimer++;
   }
}

static void mqttClientSubTask( const void *argument )
{
   PARAM_NOT_USED( argument );

   printf( "start MQTT Subscribe Task\r\n");

  while(1)
  {
    //waiting for valid ip address
    if (gnetif.ip_addr.addr == 0 || gnetif.netmask.addr == 0 )//|| gnetif.gw.addr == 0) //system has no valid ip address
    {
      osDelay(1000);
      continue;
    }
    else
    {
      break;
    }
  }

  bool connectedOnce = false;

  while(1)
  {
    if( !mqttClient.isconnected && !connectedOnce )
    {
      //try to connect to the broker
      MQTTDisconnect(&mqttClient);
      auto result = mqttConnectBroker();
      if ( result == MQTT_SUCCESS )
      {
         connectedOnce = true;
         printf( "mqtt connected once!\r\n" );
      }
      else
      {
         printf("mqttConnectBroker failed...abort\r\n");
         abort();
      }
      osDelay(1000);
    }
    else
    {
      MQTTYield(&mqttClient, 1000); //handle timer
      osDelay(100);
    }

    //printf( "Run MQTT Subscribe Task\r\n");
  }
}

static void mqttClientPubTask( const void *argument )
{
   PARAM_NOT_USED( argument );

  int count = 0;
  uint8_t buff[64];

  MQTTMessage message;

  printf( "start MQTT Publish Task\r\n");

  while(1)
  {
    if(mqttClient.isconnected)
    {
      memset(buff, 0, sizeof(buff));
      snprintf((char*)buff, sizeof(buff), "%d", count++);
      message.payload = (void*)buff;
      message.payloadlen = strlen((char*)buff);

      if(MQTTPublish(&mqttClient, "test", &message) != MQTT_SUCCESS)
      {
        printf( "MQTTPublish failed.\r\n" );
        MQTTCloseSession(&mqttClient);
        net_disconnect(&net);
      }
    }

    osDelay(1000);
    //printf( "Run MQTT Publish Task\r\n");
  }
}

static int mqttConnectBroker()
{
  int ret;

  printf( "start MQTT Connect Broker\r\n");

  NewNetwork(&net);
  ret = ConnectNetwork(&net, BROKER_IP, MQTT_PORT);
  if(ret != MQTT_SUCCESS)
  {
    printf( "ConnectNetwork failed, ret=%d\r\n", ret );
    net_disconnect(&net);
    return -1;
  }

  MQTTClientInit(&mqttClient, &net, 1000, sndBuffer, sizeof(sndBuffer), rcvBuffer, sizeof(rcvBuffer));

  MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
  data.willFlag = 0;
  data.MQTTVersion = 3;
  data.clientID.cstring = "STM32F4";
  data.username.cstring = "STM32F4";
  data.password.cstring = "";
  data.keepAliveInterval = 60;
  data.cleansession = 1;

  ret = MQTTConnect(&mqttClient, &data);
  if(ret != MQTT_SUCCESS)
  {
    printf("MQTTConnect failed.\r\n");
    MQTTCloseSession(&mqttClient);
    net_disconnect(&net);
    return ret;
  }

  ret = MQTTSubscribe(&mqttClient, "test", QOS0, mqttMessageArrived);
  if(ret != MQTT_SUCCESS)
  {
    printf("MQTTSubscribe failed.\r\n");
    MQTTCloseSession(&mqttClient);
    net_disconnect(&net);
    return ret;
  }

  printf( "mqttConnectBroker... done\r\n " );
  return MQTT_SUCCESS;
}

static void mqttMessageArrived(MessageData* msg)
{
  //HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin); //toggle pin when new message arrived

  MQTTMessage* message = msg->message;
  memset(msgBuffer, 0, sizeof(msgBuffer));
  memcpy(msgBuffer, message->payload,message->payloadlen);

  printf("MQTT MSG[%d]:%s\r\n", (int)message->payloadlen, msgBuffer);
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
