/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name          : freertos.c
 * Description        : Code for freertos applications
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2022 STMicroelectronics.
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
#include "common.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include "lwip.h"
#include "lwip/api.h"
#include "MQTTClient.h"
#include "MQTTInterface.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
constexpr const char* BROKER_IP = "192.168.1.2";
constexpr uint16_t MQTT_PORT = 1883;
constexpr size_t MQTT_BUFSIZE = 1024;

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
extern struct netif gnetif;

osThreadId  mqttClientSubTaskHandle; 
osThreadId  mqttClientPubTaskHandle; 
Network     net; 
MQTTClient  mqttClient; 

uint8_t     mqttSendBuffer[MQTT_BUFSIZE]; 
uint8_t     mqttRecvBuffer[MQTT_BUFSIZE];  

/* USER CODE END Variables */
osThreadId defaultTaskHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void  mqttClientSubTask           ( void const *argument );
void  mqttClientPubTask           ( void const *argument );
int   mqttConnectBroker           ( );
void  mqttCallbackMessageArrived  ( MessageData* msg );

/* USER CODE END FunctionPrototypes */

void startDefaultTask(void const * argument);

/* GetIdleTaskMemory prototype (linked to static allocation support) */
extern "C" void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

/* Hook prototypes */
extern "C" void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName);

/* USER CODE BEGIN 4 */
/* USER CODE END 4 */

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

/* USER CODE END GET_IDLE_TASK_MEMORY */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) 
{
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
   osThreadDef( defaultTask, startDefaultTask, osPriorityNormal, 0, 512 );
   defaultTaskHandle = osThreadCreate( osThread( defaultTask ), NULL );

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
void startDefaultTask(void const * argument)
{
   /* init code for LWIP */
   MX_LWIP_Init();

   /* USER CODE BEGIN startDefaultTask */
   PARAM_NOT_USED( argument );

   osThreadDef( mqttClientSubTask, mqttClientSubTask, osPriorityNormal, 0, configMINIMAL_STACK_SIZE );
   osThreadDef( mqttClientPubTask, mqttClientPubTask, osPriorityNormal, 0, configMINIMAL_STACK_SIZE );

   mqttClientSubTaskHandle = osThreadCreate(osThread(mqttClientSubTask), NULL);
   osDelay( 1000 );
   mqttClientPubTaskHandle = osThreadCreate(osThread(mqttClientPubTask), NULL);

   /* Infinite loop */
   for(;;)
   {
      osDelay( 1000 );
   }
   /* USER CODE END startDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void mqttClientSubTask( void const *argument )
{
   PARAM_NOT_USED( argument );
   LOGGING( "start MQTT Subscribe Task" );

   for(;;)
   {
      if ( gnetif.ip_addr.addr == 0 || gnetif.netmask.addr == 0 )//|| gnetif.gw.addr == 0)
      {
         osDelay(1000);
         continue;
      }
      else
      {
         break;
      }
   }

   LOGGING( "Run MQTT Subscribe Task" );

   for(;;)
   {
      if( !mqttClient.isconnected )
      {
         MQTTDisconnect( &mqttClient );
         mqttConnectBroker();
         osDelay(1000);
      }
      else
      {
         MQTTYield( &mqttClient, 1000 );
         osDelay( 100 );
      }
   }
}

void mqttClientPubTask( void const *argument )
{
   PARAM_NOT_USED( argument );

   int count = 0;
   uint8_t buff[64] = {};
   MQTTMessage message;

   LOGGING( "start MQTT Publish Task");

   for(;;)
   {
      if( mqttClient.isconnected )
      {
         memset( buff, 0, sizeof(buff) );
         snprintf( (char*)buff, sizeof(buff), "%d", count++);
         message.payload = reinterpret_cast<void*>( buff );
         message.payloadlen = strlen((char*)buff);

         if( MQTTPublish( &mqttClient, "test", &message ) != MQTT_SUCCESS )
         {
            LOGGING( "MQTTPublish failed." );
            MQTTCloseSession( &mqttClient );
            net_disconnect( &net );
         }
      }

      osDelay( 500 );
   }
}

int mqttConnectBroker( )
{
   LOGGING( "start MQTT Connect Broker" );

   NewNetwork( &net );
   auto ret = ConnectNetwork( &net, const_cast<char*>( BROKER_IP ), MQTT_PORT );
   if( ret != MQTT_SUCCESS )
   {
      LOGGING( "ConnectNetwork failed." );
      net_disconnect( &net );
      return -1;
   }

   LOGGING( "MQTTPacket_connectData O.K" );

   MQTTClientInit( &mqttClient, &net, 1000, mqttSendBuffer, sizeof(mqttSendBuffer), mqttRecvBuffer, sizeof(mqttRecvBuffer) );

   MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
   data.willFlag = 0;
   data.MQTTVersion = 3;
   data.clientID.cstring = "STM32F4";
   data.username.cstring = "STM32F4";
   data.password.cstring = "";
   data.keepAliveInterval = 60;
   data.cleansession = 1;

   ret = MQTTConnect( &mqttClient, &data );
   if(ret != MQTT_SUCCESS)
   {
      LOGGING( "MQTTConnect failed." );
      MQTTCloseSession( &mqttClient );
      net_disconnect( &net );
      return ret;
   }

   ret = MQTTSubscribe( &mqttClient, "test", QOS0, mqttCallbackMessageArrived );
   if(ret != MQTT_SUCCESS)
   {
      LOGGING( "MQTTSubscribe failed." );
      MQTTCloseSession( &mqttClient );
      net_disconnect( &net );
      return ret;
   }

   LOGGING( "MQTT_ConnectBroker O.K." );
   return MQTT_SUCCESS;
}

void mqttCallbackMessageArrived( MessageData* msg )
{
  HAL_GPIO_TogglePin( LD2_GPIO_Port, LD2_Pin );

  const auto* message = msg->message;

  uint8_t msgBuffer[MQTT_BUFSIZE];
  memset( msgBuffer, 0, sizeof( msgBuffer ) );
  memcpy( msgBuffer, message->payload,message->payloadlen );

  LOGGING( "MQTT MSG[%d]:%s", (int)message->payloadlen, msgBuffer );
}

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
   PARAM_NOT_USED( xTask );
   PARAM_NOT_USED( pcTaskName );

   HAL_GPIO_WritePin( LD3_GPIO_Port, LD3_Pin, GPIO_PIN_SET );
}

/* USER CODE END Application */
