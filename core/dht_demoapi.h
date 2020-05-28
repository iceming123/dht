#ifndef __DHT_DEMOAPI_H__ 
#define __DHT_DEMOAPI_H__

#ifdef WIN32
#ifdef DHT_DEMO_EXPORTS
#  define DHT_DEMOAPI extern "C" __declspec(dllexport)
#else
#  define DHT_DEMOAPI extern "C" __declspec(dllimport)
#endif

#define DHT_DEMOAPI_CALLRULE _stdcall
#endif //WIN32


#ifndef WIN32
#define DHT_DEMOAPI extern "C"
#define DHT_DEMOAPI_CALLRULE
#endif

#define DHT void*

typedef void
dht_callback(int event,const unsigned char *info_hash,
	const void *data, size_t data_len,long long user_data);

typedef void
dht_recv_callback(const void *data,size_t data_len,char* from);

/**
* 命名：norn_dht_init
* 功能：初始化DHT网络
* 参数：
*      [port]				输入，DHT网络监听的端口
*      [callback]			输入，DHT网络搜索结果的回调，可指定为空
*      [rv_callback]		输入，接收UDP报文后的处理过程
*	返回true表示成功获取，false表示失败
*/
DHT_DEMOAPI bool DHT_DEMOAPI_CALLRULE norn_dht_init(
	int port,
	dht_recv_callback rv_callback=nullptr
	);

/**
* 命名：norn_dht_bootstrap
* 功能：初始化DHT网络的引导节点
* 参数：
*      [ip]				输入，引导节点的IP
*      [port]			输入，引导节点的端口
*	备注：最少初始化3个引导节点
*/
DHT_DEMOAPI
int
DHT_DEMOAPI_CALLRULE
norn_dht_bootstrap(const char* ip, int port);


DHT_DEMOAPI
int
DHT_DEMOAPI_CALLRULE
norn_dht_finit();

// 暂时停用
DHT_DEMOAPI
int
DHT_DEMOAPI_CALLRULE
norn_put_value(const unsigned char *key, unsigned short value,long long user_data=0);

// 暂时停用
DHT_DEMOAPI
int DHT_DEMOAPI_CALLRULE
norn_get_value(const unsigned char *key,long long user_data=0);

DHT_DEMOAPI
int DHT_DEMOAPI_CALLRULE
norn_get_address_count();

/**
* 命名：norn_get_address
* 功能：取得DHT网络中已注册的节点IP及端口
* 参数：
*      [address]      输出，IP及PORT,高32位表示网络序IP，低32位表示网络序PORT
*      [len]		  输入输出，输入表示address数组长度，输出表示实际返回的长度
*	返回true表示成功获取
*/
DHT_DEMOAPI
int DHT_DEMOAPI_CALLRULE
norn_get_address(unsigned long long *address,int &len);

/**
* 命名：norn_send_message
* 功能：发送UDP报文，报文接收后由dht_recv_callback回调处理
* 参数：
*      [msg]      输入，表示需要发送的报文
*      [len]	  输入，表示报文的长度
*      [addr]     输入，指定发送的ip地址
*      [port]	  输入，指定发送的端口
*	   返回大于0表示发送成功，其余表示发送失败
*/
DHT_DEMOAPI
int DHT_DEMOAPI_CALLRULE
norn_send_message(unsigned char *msg, int len, char* addr, int port);

/**
* 命名：norn_send_broadcast_msg
* 功能：发送DHT广播报文，由DHT网络内部时序发送
* 参数：
*      [msg]      输入，表示需要发送的报文
*      [len]	  输入，表示报文的长度
*	   [time]	  输入，表示报文的存活时间,单位秒
*	   返回大于0表示发送成功，其余表示发送失败
*/
DHT_DEMOAPI
int DHT_DEMOAPI_CALLRULE
norn_set_broadcast_msg(unsigned char *msg, int len,int time=60);

/**
* 命名：norn_get_closest_node_info
* 功能：获取在DHT环上指定key最近的节点信息
* 参数：
*      [key]			输入，DHT上指定的key值
*      [len]			输入，key值的长度，20字节
*	   [user_data]		输入，用户自定义数据
*	   返回大于0表示发送成功，其余表示发送失败
* 备注:获取值的信息通过初始化时传入的回掉函数输出
*/
DHT_DEMOAPI
int DHT_DEMOAPI_CALLRULE
norn_get_closest_node_info(unsigned char *key, int len, long long user_data=0);

/**
* 命名：norn_get_my_node_id
* 功能：获取本节点的node id
* 参数：
*      [key]		输入/输出，保持node id的缓冲区
*      [len]		输入/输出，node id值的长度，最少20字节
*	   返回0表示成功，其余表示失败
*/
DHT_DEMOAPI
int DHT_DEMOAPI_CALLRULE
norn_get_my_node_id(unsigned char *key, int &len);

#endif
