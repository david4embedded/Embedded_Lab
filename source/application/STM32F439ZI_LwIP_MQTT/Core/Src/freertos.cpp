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

/************************************************** Includes *************************************************/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"
#include "common.h"
#include "lwip.h"
#include "lwip/api.h"
#include "MQTTClient.h"
#include "mqtt_client_port.h"
#include "mqtt_manager_paho.h"
#include "cli.h"
#include "logger.h"
#include <string.h>

/************************************************** Consts ****************************************************/
constexpr size_t CLI_BUFFER_SIZE = 128;

/*********************************************** Local Variables *********************************************/
static osThreadId             defaultTaskHandle;
static osThreadId             mqttClientSubTaskHandle; 
static osThreadId             mqttClientPubTaskHandle; 
static osThreadId             cliTaskHandle;

static lib::LockableFreeRTOS  m_lock;
static MqttBroker             broker{ "192.168.1.2", 1883 };
static MqttManagerPaho        mqttManager{ m_lock, "NucleoF439", "NucleoF439" };

static StaticTask_t           xIdleTaskTCBBuffer;
static StackType_t            xIdleStack[configMINIMAL_STACK_SIZE];

/******************************************** Function Declarations *******************************************/
void  startDefaultTask        ( void const *argument );
void  mqttClientSubTask       ( void const *argument );
void  mqttClientPubTask       ( void const *argument );
int   mqttConnectBroker       ( );
void  mqttMsgArrivedCallback  ( MessageData* msg );
static void    taskCli              ( void const * argument );

extern "C" void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );
extern "C" void vApplicationStackOverflowHook( xTaskHandle xTask, signed char *pcTaskName );

/******************************************** Function Definitions ********************************************/
/**
 * @brief  FreeRTOS initialization
 * @param  None
 * @retval None
 */
void MX_FREERTOS_Init(void) 
{
   const osThreadDef_t defaultTaskDef = { const_cast<char*>( "defaultTask" ), startDefaultTask, osPriorityNormal, 0, configMINIMAL_STACK_SIZE, nullptr, nullptr };
   defaultTaskHandle = osThreadCreate( &defaultTaskDef, nullptr );
   
   osThreadDef( cliTask, taskCli, osPriorityNormal, 0, 512 );
   cliTaskHandle = osThreadCreate( osThread( cliTask ), nullptr );
   LOGGER_init();
}

/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
void startDefaultTask(void const * argument)
{
   PARAM_NOT_USED( argument );

   MX_LWIP_Init();

   /* NOTE: Wait until the network is ready. Note that this currently requires an additional delay as seen at the end to better ensure
    *       stability in the connection process. */
   auto tickStarted = osKernelSysTick();
   while( !LWIP_isNetworkReady() )
   {
      osDelay( 100 );
      if ( osKernelSysTick() - tickStarted > 5000 )
      {
         LOGGING( "Network not ready after 5 seconds" );
         return;
      }
   }
   osDelay( 2000 );

   //!< Connect to the broker
   mqttManager.connectToBroker( broker, 5000 );

   //!< Subscribe to the 'test' topic and register the callback
   if(  mqttManager.subscribe( "test", mqttMsgArrivedCallback ) == true )
   {
      const osThreadDef_t subscribeTaskDef = { const_cast<char*>( "mqttSubscribeTask" ), mqttClientSubTask, osPriorityNormal, 0, configMINIMAL_STACK_SIZE, nullptr, nullptr };
      mqttClientSubTaskHandle = osThreadCreate( &subscribeTaskDef, nullptr );

      const osThreadDef_t publishTaskDef = { const_cast<char*>( "mqttPublishTask" ), mqttClientPubTask, osPriorityNormal, 0, configMINIMAL_STACK_SIZE, nullptr, nullptr };
      mqttClientPubTaskHandle = osThreadCreate( &publishTaskDef, nullptr );
   }

   for(;;)
   {
      osDelay( 10000 );
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
   char buffer[64] = {};

   //!< Lambda function to create the message payload
   auto lambdaCreatePayload = [&buffer]( auto count ) 
   {
      memset( buffer, 0, sizeof( buffer ) );
      snprintf( buffer, sizeof( buffer ), "Hello, #%lu", count );
      return buffer;
   };

   for(;;)
   {
      if ( mqttManager.isConnected() )
      {
         mqttManager.publish( "test", lambdaCreatePayload( count++ ) );
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

   LOGGING( "MQTT: MSG[%d]: %s", length, payload );
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

   HAL_GPIO_WritePin( LD3_GPIO_Port, LD3_Pin, GPIO_PIN_SET );
}

