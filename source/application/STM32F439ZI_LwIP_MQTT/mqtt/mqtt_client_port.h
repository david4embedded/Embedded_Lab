#pragma once

#if defined __cplusplus
extern "C" {
#endif

#define MQTT_LWIP_SOCKET

typedef struct Timer 
{
	unsigned long systick_period;
	unsigned long end_time;
} Timer;

typedef struct Network 
{
#ifdef MQTT_LWIP_SOCKET
	int socket;
#elif MQTT_LWIP_NETCONN
	struct netconn *conn;
	struct netbuf *buf;
	int offset;
#endif
	int (*mqttread)      ( struct Network*, unsigned char*, int, int );
	int (*mqttwrite)     ( struct Network*, unsigned char*, int, int );
	void (*disconnect)   ( struct Network* );
} Network;

void configureNetworkObject   ( Network* );
int  connectNetwork           ( Network*, char*, int);

#if defined __cplusplus
}
#endif