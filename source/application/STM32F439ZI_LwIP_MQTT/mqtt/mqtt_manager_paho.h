/*************************************************************************************************************
 * 
 * @file mqtt_manager_paho.h
 * @brief Header file for the MQTTManagerPaho class, which provides an interface for MQTT communication.
 * 
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-08-12
 * @version 1.0
 * 
 *************************************************************************************************************/

#pragma once

/************************************************** Includes *************************************************/
#include "MQTTClient.h"
#include "mqtt_client_port.h"
#include "lockable_FreeRTOS.hpp"
#include "lockguard.hpp"
#include <stdint.h>

/************************************************** Types ****************************************************/
/**
 * @brief Represents an MQTT broker.
 */
struct MqttBroker
{
   const char* ip;   //!< IP address information
   uint32_t port;    //!< Port number
};

/**
 * @brief Represents an MQTT client manager.
 * @details This class provides an interface for MQTT communication, including methods for connecting to a broker, publishing messages, and subscribing to topics.
 *          This class, specifically, is designed to work with the Paho MQTT C++ client.
 */
class MqttManagerPaho 
{
public:
   using MessageArrivedCallback = void(*)( MessageData* );
   constexpr static size_t MQTT_BUFSIZE = 1024;

   MqttManagerPaho( lib::ILockable& lockable, const char* clientName, const char* userName, const char* password = nullptr )
   : m_clientName( clientName )
   , m_userName( userName )
   , m_password( password )
   , m_lock( lockable )
   { }

   ~MqttManagerPaho()
   { }

   bool  connectToBroker               ( const MqttBroker& broker, uint32_t timeout_ms = 5000 );
   void  disconnect                    ( );
   bool  publish                       ( const char* topic, const char* payload );
   bool  subscribe                     ( const char* topic, MessageArrivedCallback callback );

   void  processBackgroundTask         ( );

   bool  isConnected                   ( ) const;

protected:
   bool  connectToNetwork              ( const MqttBroker& broker );
   bool  waitNetworkRunning            ( uint32_t timeout_ms = 5000) const;

   static int  readFromNetwork         ( Network*, unsigned char*, int, int );
   static int  writeToNetwork          ( Network*, unsigned char*, int, int );
   static void disconnectFromNetwork   ( Network* );

private:
   MqttBroker        m_broker;
   const char*       m_clientName;
   const char*       m_userName;
   const char*       m_password;

   Network           m_network;
   MQTTClient        m_mqttClient;

   uint8_t           m_sendBuffer [MQTT_BUFSIZE]; 
   uint8_t           m_recvBuffer [MQTT_BUFSIZE];  

   bool              m_connected{ false };

   lib::ILockable    &m_lock;
};
