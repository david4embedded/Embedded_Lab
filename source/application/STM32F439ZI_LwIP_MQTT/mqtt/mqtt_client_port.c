#include "mqtt_client_port.h"
#include "stm32f4xx_hal.h"

#include <string.h>
#include "lwip.h"
#include "lwip/api.h"
#include "lwip/sockets.h"
#include "common.h"

uint32_t g_mqttTick_ms;

static int  mqtt_net_read        (Network*, unsigned char*, int, int);
static int  mqtt_net_write       (Network*, unsigned char*, int, int);
static void mqtt_net_disconnect  (Network*);

void TimerInit(Timer *timer) 
{
	timer->end_time = 0;
}

char TimerIsExpired(Timer *timer) 
{
	long left = timer->end_time - g_mqttTick_ms;
	return (left < 0);
}

void TimerCountdownMS(Timer *timer, unsigned int timeout) 
{
	timer->end_time = g_mqttTick_ms + timeout;
}

void TimerCountdown(Timer *timer, unsigned int timeout) 
{
	timer->end_time = g_mqttTick_ms + (timeout * 1000);
}

int TimerLeftMS(Timer *timer) 
{
	long left = timer->end_time - g_mqttTick_ms;
	return (left < 0) ? 0 : left;
}

#if defined ( MQTT_LWIP_SOCKET )
void configureNetworkObject( struct Network *n ) 
{
	n->socket = 0; 
	n->mqttread = mqtt_net_read; 
	n->mqttwrite = mqtt_net_write; 
	n->disconnect = mqtt_net_disconnect; 
}

int connectNetwork( Network *n, char *ip, int port ) 
{
	struct sockaddr_in server_addr;

	if( n->socket )
	{
		close( n->socket );
	}

	n->socket = socket( PF_INET, SOCK_STREAM, 0 );
	if( n->socket < 0 )
	{
		n->socket = 0;
		return -1;
	}

	memset(&server_addr, 0, sizeof(struct sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(ip);
	server_addr.sin_port = htons(port);

	if( connect( n->socket, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in) ) < 0 )
	{
		close( n->socket );
		return -1;
	}

	return 0;
}

static int mqtt_net_read( Network *n, unsigned char *buffer, int len, int timeout_ms ) 
{
   PARAM_NOT_USED( timeout_ms );

   int available;

	/* !!! LWIP_SO_RCVBUF must be enabled !!! */
	if( ioctl(n->socket, FIONREAD, &available ) < 0 ) 
   {
      return -1; //check receive buffer
   }

	if( available > 0 )
	{
		return recv( n->socket, buffer, len, 0 );
	}

	return 0;
}

static int mqtt_net_write( Network *n, unsigned char *buffer, int len, int timeout_ms ) 
{
	PARAM_NOT_USED( timeout_ms );
	return send( n->socket, buffer, len, 0 );
}

static void mqtt_net_disconnect( Network *n )
{
	close( n->socket );
	n->socket = 0;
}

#endif /* MQTT_LWIP_SOCKET */
