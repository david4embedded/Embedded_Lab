
#pragma once

#include "MQTTClient.h"
#include "mqtt_client_port.h"
#include <stdint.h>

struct MqttBroker
{
   const char* ip;
   uint32_t port;
};

class MqttManager 
{
public:
   using MessageArrivedCallback = void(*)( MessageData* );
   constexpr static size_t MQTT_BUFSIZE = 1024;

   MqttManager( const char* clientName, const char* userName, const char* password = nullptr )
   : m_clientName( clientName )
   , m_userName( userName )
   , m_password( password )
   { }

   ~MqttManager()
   { }

   bool  connectToBroker       ( const MqttBroker& broker, uint32_t timeout_ms = 5000 );
   void  disconnect            ( );
   bool  publish               ( const char* topic, const char* payload );
   bool  subscribe             ( const char* topic, MessageArrivedCallback callback );

   void  processBackgroundTask ( );

   bool  isConnected           ( ) const;

protected:
   bool  connectToNetwork      ( const MqttBroker& broker );
   bool  waitNetworkRunning    ( uint32_t timeout_ms = 5000) const;

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
};
