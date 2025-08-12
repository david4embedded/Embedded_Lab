
#include "mqtt_manager_paho.h"
#include "stm32f4xx_hal.h"

#include "lwip.h"
#include "lwip/api.h"
#include "lwip/sockets.h"
#include "common.h"
#include <string.h>

extern struct netif gnetif;

bool MqttManager::connectToBroker( const MqttBroker& broker, uint32_t timeout_ms /* = 5000 */ ) 
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
   m_network.mqttread = MqttManager::readFromNetwork;
   m_network.mqttwrite = MqttManager::writeToNetwork;
   m_network.disconnect = MqttManager::disconnectFromNetwork;

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

   LOGGING( "MQTT: Connect succeeded" );

   return true;
}

bool MqttManager::connectToNetwork( const MqttBroker& broker )
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

	return true;
}

bool MqttManager::waitNetworkRunning( uint32_t timeout_ms /* = 5000 */ ) const
{
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

   return true;
}

bool MqttManager::isConnected( ) const
{
   return m_mqttClient.isconnected;
}

void MqttManager::disconnect() 
{
   MQTTCloseSession( &m_mqttClient );
   m_network.disconnect( &m_network );

   LOGGING( "MQTT: Disconnected" );
}

bool MqttManager::publish( const char* topic, const char* payload ) 
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

bool MqttManager::subscribe( const char* topic, MessageArrivedCallback callback ) 
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

void MqttManager::processBackgroundTask( )
{
   if ( !m_mqttClient.isconnected )
   {
      return;
   }

   MQTTYield( &m_mqttClient, 1000 );
}

int MqttManager::readFromNetwork( Network *n, unsigned char *buffer, int len, int timeout_ms )
{
   PARAM_NOT_USED( timeout_ms );

   int available;

	/* !!! LWIP_SO_RCVBUF must be enabled !!! */
	if( ioctl( n->socket, FIONREAD, &available) < 0 ) 
   {
      return -1;
   }

	if( available > 0 )
	{
		return recv( n->socket, buffer, len, 0 );
	}

	return 0;
}

int MqttManager::writeToNetwork( Network* n, unsigned char* buffer, int len, int timeout_ms )
{
	PARAM_NOT_USED( timeout_ms );
	return send( n->socket, buffer, len, 0 );
}

void MqttManager::disconnectFromNetwork( Network* n )
{
	close( n->socket );
	n->socket = 0;
}
