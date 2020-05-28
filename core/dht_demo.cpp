// dht_demo.cpp : 
//
#include "dht_helper.h"
#include "dht_demoapi.h"
#include "dht_net.h"
#include "dht_worker.h"

#ifndef _WIN32
#include <unistd.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/signal.h>
#define MY_FILE int
#else
#include <ws2tcpip.h>
#include <time.h>
#include <windows.h>
#define sleep Sleep
#define MY_FILE FILE*
#endif




//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
bool DHT_DEMOAPI_CALLRULE norn_dht_init(int port,dht_recv_callback rv_callback)
{
	return dht_worker::instance()->start(port, rv_callback);
}
DHT_DEMOAPI int DHT_DEMOAPI_CALLRULE norn_dht_bootstrap(const char* ip,int port)
{
	return dht_worker::instance()->pre_start_work(ip, port);
}
DHT_DEMOAPI int DHT_DEMOAPI_CALLRULE norn_dht_finit()
{
	dht_worker::instance()->stop();
	return 0;
}
DHT_DEMOAPI int DHT_DEMOAPI_CALLRULE norn_put_value(const unsigned char *key,
	unsigned short value,long long user_data)
{
	return dht_worker::instance()->set_value(key, value,user_data);
}

DHT_DEMOAPI int DHT_DEMOAPI_CALLRULE norn_get_value(const unsigned char *key,long long user_data)
{
	return dht_worker::instance()->get_value(key,user_data);
}

DHT_DEMOAPI int DHT_DEMOAPI_CALLRULE norn_get_address_count()
{
	return dht_worker::instance()->get_address_count();
}

DHT_DEMOAPI int DHT_DEMOAPI_CALLRULE norn_get_address(unsigned long long *address, int &len)
{
	return dht_worker::instance()->get_all_dht_address(address, len);
}
DHT_DEMOAPI int DHT_DEMOAPI_CALLRULE norn_send_message(unsigned char *msg, int len, char* addr, int port)
{
	return dht_worker::instance()->send_message(msg, len, addr, port);
}
DHT_DEMOAPI int DHT_DEMOAPI_CALLRULE norn_set_broadcast_msg(unsigned char *msg, int len, int time)
{
	return 1;
}
DHT_DEMOAPI int DHT_DEMOAPI_CALLRULE norn_get_closest_node_info(unsigned char *key,
	int len, long long user_data)
{
	return dht_worker::instance()->send_get_closest_node_info(key, len, user_data);
}
DHT_DEMOAPI int DHT_DEMOAPI_CALLRULE norn_get_my_node_id(unsigned char *key, int &len)
{
	if (dht_worker::instance()->get_node_id(key, len)) return 0;
	return -1;
}



