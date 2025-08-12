/*************************************************************************************************************
 * 
 * @file mqtt_manager_paho.cpp
 * @brief Implementation file for the MQTTManagerPaho class, which provides an interface for MQTT communication.
 * 
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-08-12
 * @version 1.0
 * 
 *************************************************************************************************************/

 /************************************************** Includes ************************************************/
#include "mqtt_manager_paho.h"
#include "stm32f4xx_hal.h"

#include "lwip.h"
#include "lwip/api.h"
#include "lwip/sockets.h"
#include "common.h"
#include <string.h>

/*********************************************** Global Variables *********************************************/
extern struct netif gnetif;

/******************************************** Function Definitions ********************************************/
/**
 * @brief Connect to the MQTT broker
 * @details Establish a connection to the specified MQTT broker.
 * 
 * @param broker The MQTT broker to connect to.
 * @param timeout_ms The timeout for the connection attempt in milliseconds.
 * @return true if the connection was successful, false otherwise.
 */
bool MqttManagerPaho::connectToBroker( const MqttBroker& broker, uint32_t timeout_ms /* = 5000 */ ) 
{
   if ( m_mqttClient.isconnected )
   {
      LOGGING( "MQTT: Already connected" );
      return true;
   }

   if ( waitNetworkRunning( timeout_ms ) == false )
   {
      LOGGING( "MQTT: Network not ready" );
      return false;
   }

   MQTTDisconnect( &m_mqttClient );

   if ( !connectToNetwork( broker ) )
   {
      LOGGING( "MQTT: Network connection failed" );
      return false;
   }

   m_network.socket = 0;
   m_network.mqttread = MqttManagerPaho::readFromNetwork;
   m_network.mqttwrite = MqttManagerPaho::writeToNetwork;
   m_network.disconnect = MqttManagerPaho::disconnectFromNetwork;

   memset( m_sendBuffer, 0, sizeof( m_sendBuffer ) );
   memset( m_recvBuffer, 0, sizeof( m_recvBuffer ) );

   MQTTClientInit( &m_mqttClient, &m_network, 1000, m_sendBuffer, sizeof( m_sendBuffer ), m_recvBuffer, sizeof( m_recvBuffer ) );

   MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
   data.willFlag = 0;
   data.MQTTVersion = 3;
   data.clientID.cstring = const_cast<char*>( m_clientName );
   data.username.cstring = const_cast<char*>( m_userName );
   data.password.cstring = ( m_password != nullptr ) ? const_cast<char*>(m_password) : nullptr;
   data.keepAliveInterval = 60;
   data.cleansession = 1;   

   if( MQTTConnect( &m_mqttClient, &data ) != MQTT_SUCCESS )
   {
      LOGGING( "MQTT: Connect failed." );
      disconnect();
      return false;
   }

   m_connected = true;

   LOGGING( "MQTT: Connect to the broker succeeded" );

   return true;
}

/**
 * @brief Connect to the network, i.e., the TCP/IP stack.
 * 
 * @param broker a MqttBroker object that contains the broker's IP address and port.
 * @return true if the connection was successful, false otherwise.
 */
bool MqttManagerPaho::connectToNetwork( const MqttBroker& broker )
{
	struct sockaddr_in server_addr;

	if( m_network.socket )
	{
		close( m_network.socket );
	}

	m_network.socket = socket( PF_INET, SOCK_STREAM, 0 );
	if( m_network.socket < 0 )
	{
		m_network.socket = 0;
		return false;
	}

	memset( &server_addr, 0, sizeof( server_addr ) );
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr( broker.ip );
	server_addr.sin_port = htons( broker.port );

	if( connect( m_network.socket, reinterpret_cast<struct sockaddr*>( &server_addr ), sizeof( server_addr ) ) < 0 )
	{
		close( m_network.socket );
		return false;
	}

   LOGGING( "MQTT: Connect to the network succeeded" );
	return true;
}

/**
 * @brief Wait for the network to be ready.
 * 
 * @param timeout_ms The maximum time to wait for the network to be ready in milliseconds.
 * @return true if the network is ready, false otherwise.
 */
bool MqttManagerPaho::waitNetworkRunning( uint32_t timeout_ms /* = 5000 */ ) const
{
   LOGGING( "MQTT: Waiting for network to be ready..." );
   auto tick_started = osKernelSysTick();
   while ( 1 )
   {
      if ( ip4_addr_isany_val( gnetif.ip_addr ) || ip4_addr_isany_val( gnetif.netmask ) )
      {
         if ( ( osKernelSysTick() - tick_started ) > timeout_ms )
         {
            return false;
         }
         osDelay( 100 );
         continue;
      }
      else
      {
         break;
      }
   }

   LOGGING( "MQTT: Waiting ... done" );
   return true;
}

/**
 * @brief Check if the MQTT client is connected.
 * 
 * @return true if the client is connected, false otherwise.
 */
bool MqttManagerPaho::isConnected( ) const
{
   return m_mqttClient.isconnected;
}

/**
 * @brief Disconnect from the MQTT broker.
 */
void MqttManagerPaho::disconnect() 
{
   MQTTCloseSession( &m_mqttClient );
   m_network.disconnect( &m_network );

   LOGGING( "MQTT: Disconnected" );
}

/**
 * @brief Publish a message to a topic.
 * 
 * @param topic The topic to publish the message to.
 * @param payload The message payload.
 * @return true if the publish was successful, false otherwise.
 */
bool MqttManagerPaho::publish( const char* topic, const char* payload ) 
{
   if ( !m_mqttClient.isconnected )
   {
      LOGGING( "MQTT: Not connected" );
      return false;
   }

   MQTTMessage message = {};
   message.qos = QOS0;
   message.payload = const_cast<void*>( reinterpret_cast<const void*>( payload ) );
   message.payloadlen = strlen((char*)payload);

   if( MQTTPublish( &m_mqttClient, topic, &message ) != MQTT_SUCCESS )
   {
      LOGGING( "MQTT: Publish failed." );
      disconnect();
   }

   return true;
}

/**
 * @brief Subscribe to a topic with a callback function that will be called when a message arrives.
 * 
 * @param topic The topic to subscribe to.
 * @param callback The callback function to be called when a message arrives.
 * @return true if the subscription was successful, false otherwise.
 */
bool MqttManagerPaho::subscribe( const char* topic, MessageArrivedCallback callback ) 
{
   if ( !m_mqttClient.isconnected )
   {
      LOGGING( "MQTT: Not connected" );
      return false;
   }
   
   auto result = MQTTSubscribe( &m_mqttClient, topic, QOS0, callback );
   if( result != MQTT_SUCCESS )
   {
      LOGGING( "MQTT: Subscribe failed." );
      disconnect();
      return false;
   }

   return true;
}

/**
 * @brief Process background tasks for the MQTT client.
 * @details This method must be called periodically to allow the MQTT client to process incoming messages and maintain the connection.
 */
void MqttManagerPaho::processBackgroundTask( )
{
   if ( !m_mqttClient.isconnected )
   {
      return;
   }

   MQTTYield( &m_mqttClient, 1000 );
}

/**
 * @brief Read data from the network.
 * @details This function, being a static method, is connected to the MQTT client's network interface.
 * @note For this method to work, 'LWIP_SO_RCVBUF' must be enabled.
 * 
 * @param n The network context.
 * @param buffer The buffer to store the received data.
 * @param len The maximum length of data to read.
 * @param timeout_ms The timeout for the read operation in milliseconds.
 * @return int The number of bytes read, or -1 on error.
 */
int MqttManagerPaho::readFromNetwork( Network *n, unsigned char *buffer, int len, int timeout_ms )
{
   PARAM_NOT_USED( timeout_ms );

   int available;
	if ( ioctl( n->socket, FIONREAD, &available ) < 0 ) 
   {
      return -1;
   }

	if ( available > 0 )
	{
		return recv( n->socket, buffer, len, 0 );
	}

	return 0;
}

/**
 * @brief Write data to the network.
 * @details This function, being a static method, is connected to the MQTT client's network interface.
 * 
 * @param n The network context.
 * @param buffer The buffer containing the data to send.
 * @param len The length of the data to send.
 * @return int The number of bytes sent, or -1 on error.
 */
int MqttManagerPaho::writeToNetwork( Network* n, unsigned char* buffer, int len, int timeout_ms )
{
	PARAM_NOT_USED( timeout_ms );
	return send( n->socket, buffer, len, 0 );
}

/**
 * @brief Disconnect from the network.
 * 
 * @param n The network context.
 */
void MqttManagerPaho::disconnectFromNetwork( Network* n )
{
	close( n->socket );
	n->socket = 0;
}
