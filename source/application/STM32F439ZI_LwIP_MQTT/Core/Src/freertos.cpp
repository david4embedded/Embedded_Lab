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

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"
#include "common.h"

/* Private includes ----------------------------------------------------------*/
#include <string.h>
#include "lwip.h"
#include "lwip/api.h"
#include "MQTTClient.h"
#include "mqtt_client_port.h"
#include "mqtt_manager_paho.h"

/* Private variables ---------------------------------------------------------*/
static osThreadId  mqttClientSubTaskHandle; 
static osThreadId  mqttClientPubTaskHandle; 
static MqttBroker  broker{ "192.168.1.2", 1883 };
static MqttManagerPaho mqttManager{ "NucleoF439", "NucleoF439" };

osThreadId defaultTaskHandle;

/* Private function prototypes -----------------------------------------------*/
void  mqttClientSubTask          ( void const *argument );
void  mqttClientPubTask          ( void const *argument );
int   mqttConnectBroker          ( );
void  mqttMsgArrivedCallback     ( MessageData* msg );

void startDefaultTask(void const * argument);

/* GetIdleTaskMemory prototype (linked to static allocation support) */
extern "C" void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

/* Hook prototypes */
extern "C" void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName);

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
   const osThreadDef_t defaultTaskDef = { const_cast<char*>( "defaultTask" ), startDefaultTask, osPriorityNormal, 0, 512, nullptr, nullptr };
   defaultTaskHandle = osThreadCreate( &defaultTaskDef, nullptr );
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
   MX_LWIP_Init();

   PARAM_NOT_USED( argument );

   const osThreadDef_t subscribeTaskDef = { const_cast<char*>( "mqttSubscribeTask" ), mqttClientSubTask, osPriorityNormal, 0, configMINIMAL_STACK_SIZE, nullptr, nullptr };
   const osThreadDef_t publishTaskDef = { const_cast<char*>( "mqttPublishTask" ), mqttClientPubTask, osPriorityNormal, 0, configMINIMAL_STACK_SIZE, nullptr, nullptr };

   //!< Connect to the broker
   mqttManager.connectToBroker( broker, 5000 );

   //!< Subscribe to the 'test' topic and register the callback
   if(  mqttManager.subscribe( "test", mqttMsgArrivedCallback ) == true )
   {
      mqttClientSubTaskHandle = osThreadCreate( &subscribeTaskDef, nullptr );
      mqttClientPubTaskHandle = osThreadCreate( &publishTaskDef, nullptr );
   }

   for(;;)
   {
      osDelay( 10000 );
   }
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
/**
 * @brief MQTT Client Subscribe Task
 * 
 * @param argument task argument
 */
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

/**
 * @brief MQTT Client Publish Task
 * 
 * @param argument task argument
 */
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

/**
 * @brief MQTT Message Arrived Callback
 * 
 * @param msg a pointer to the MessageData structure
 */
void mqttMsgArrivedCallback( MessageData* msg )
{
   HAL_GPIO_TogglePin( LD2_GPIO_Port, LD2_Pin );

   //!< Set the last byte to zero to not see garbage
   auto length = msg->message->payloadlen;
   char* payload = static_cast<char*>( msg->message->payload );
   payload[length] = '\0';

   LOGGING( "MQTT: MSG: %s (len=%d)", payload, length );
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
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
   PARAM_NOT_USED( xTask );
   PARAM_NOT_USED( pcTaskName );

   HAL_GPIO_WritePin( LD3_GPIO_Port, LD3_Pin, GPIO_PIN_SET );
}

