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
#include "mqtt_client_port.h"
#include "mqtt_manager_paho.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
static osThreadId  mqttClientSubTaskHandle; 
static osThreadId  mqttClientPubTaskHandle; 
static MqttBroker  broker{ "192.168.1.2", 1883 };
static MqttManager mqttManager{ "NucleoF439", "NucleoF439" };

/* USER CODE END Variables */
osThreadId defaultTaskHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void  mqttClientSubTask          ( void const *argument );
void  mqttClientPubTask          ( void const *argument );
int   mqttConnectBroker          ( );
void  mqttMsgArrivedCallback     ( MessageData* msg );

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
   const osThreadDef_t defaultTaskDef = { const_cast<char*>( "defaultTask" ), startDefaultTask, osPriorityNormal, 0, 512, nullptr, nullptr };
   defaultTaskHandle = osThreadCreate( &defaultTaskDef, nullptr );

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

   const osThreadDef_t subscribeTaskDef = { const_cast<char*>( "mqttSubscribeTask" ), mqttClientSubTask, osPriorityNormal, 0, configMINIMAL_STACK_SIZE, nullptr, nullptr };
   const osThreadDef_t publishTaskDef = { const_cast<char*>( "mqttPublishTask" ), mqttClientPubTask, osPriorityNormal, 0, configMINIMAL_STACK_SIZE, nullptr, nullptr };

   mqttManager.connectToBroker( broker, 5000 );

   if(  mqttManager.subscribe( "test", mqttMsgArrivedCallback ) == true )
   {
      mqttClientSubTaskHandle = osThreadCreate( &subscribeTaskDef, nullptr );
      mqttClientPubTaskHandle = osThreadCreate( &publishTaskDef, nullptr );
   }

   for(;;)
   {
      osDelay( 10000 );
   }
   /* USER CODE END startDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void mqttClientSubTask( void const *argument )
{
   PARAM_NOT_USED( argument );
   LOGGING( "Start MQTT Subscribe Task" );

   for(;;)
   {
      if ( mqttManager.isConnected() )
      {
         mqttManager.processBackgroundTask();
      }
      osDelay(100);
   }
}

void mqttClientPubTask( void const *argument )
{
   PARAM_NOT_USED( argument );

   LOGGING( "Start MQTT Publish Task");
   uint32_t count = 0;

   for(;;)
   {
      if ( mqttManager.isConnected() )
      {
         uint8_t buff[64] = {};
         memset( buff, 0, sizeof( buff ) );
         snprintf( (char*)buff, sizeof( buff ), "Hello, #%lu", count++ );
         mqttManager.publish( "test", (char*)buff );
      }

      osDelay( 500 );
   }
}

void mqttMsgArrivedCallback( MessageData* msg )
{
  HAL_GPIO_TogglePin( LD2_GPIO_Port, LD2_Pin );

  const auto* message = msg->message;

  uint8_t msgBuffer[1024];
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
