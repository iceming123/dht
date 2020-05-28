// ConsoleApplication1.cpp : Defines the entry point for the console application.
//
#include "dht_helper.h"
#include "dht_demoapi.h"
#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <thread>

std::string print_buf(const unsigned char *buf, int size)
{
	std::string str;
	char tmp[16] = { 0 };
	for (int i = 0; i < size; i++)
	{
		sprintf(tmp, "%02x", buf[i]);
		str += tmp; memset(tmp, 0, sizeof(tmp));
	}
	return str;
}
void callback1(void *closure, int event,
	const unsigned char *info_hash,
	const void *data, size_t data_len, long long user_data)
{

}

void callback2(const void *data, size_t data_len, char* from)
{
	std::string ss = print_buf((const unsigned char*)data, data_len);
	std::cout << "recv msg from ip=" << from << "data_len---is: " << data_len << " msg = " << ss << std::endl;
}
void test(char* lip,int port, int peerport1=0, int peerport2=0, int peerport3=0)
{
	char buf[256] = { 0 };
	for (int i = 0; i < 256; i++) buf[i] = i % 255;
	//IniFile ff; 
	//if (!ff.Init(file_name.c_str())) return;
	//int port = ff.GetInt("localport");
	norn_dht_init(port,callback1,callback2);
	std::cout << "norn_dht_init()=test1!!" << std::endl;
	getchar();
	norn_send_message((unsigned char*)buf, 256, "10.0.11.78", port);
	//int peerport[3] = { 0 };
	//peerport[0] = ff.GetInt("peerport1");
	//peerport[1] = ff.GetInt("peerport2");
	//peerport[2] = ff.GetInt("peerport3");
	std::cout << "begin bootstrap" << std::endl;
	std::cout << "�ȴ�30s" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(1000 * 30));
	if (peerport1 > 0)
		norn_dht_bootstrap(lip, peerport1);
	if (peerport2 > 0)
		norn_dht_bootstrap(lip, peerport2);
	if (peerport3 > 0)
		norn_dht_bootstrap(lip, peerport3);
	std::cout << "end bootstrap" << std::endl;
	std::cout << "�ȴ�10s" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds((1000 * 10)));
	std::cout << "��������" << std::endl;
	getchar();
	int addr_count = norn_get_address_count();
	if (addr_count > 0)
	{
		unsigned long long *peer_addr = new unsigned long long[addr_count];
		norn_get_address(peer_addr, addr_count);
		if (addr_count > 0)
		{
			std::cout << "get address size=" << addr_count << std::endl;
			int ip, port; struct sockaddr_in sin;
			for (int i = 0; i < addr_count; i++)
			{
				port = peer_addr[i] & 0x00000000ffffffff;
				ip = (peer_addr[i] >> 32) & 0x00000000ffffffff;
				sin.sin_addr.s_addr = ip;
				std::cout << i + 1 << " ip=" << inet_ntoa(sin.sin_addr)
					<< " port=" << ntohs(port) << std::endl;
			}
		}
		delete []peer_addr;
	}
	else
		std::cout << "get all address size = 0" << std::endl;
	// test send message
	std::cout << "test send message" << std::endl;
	
	std::string ss = print_buf((const unsigned char*)buf, 256);
	std::cout << "send message to peer[10.0.11.78]port=" << peerport1 
		<<"msg="<<ss<< std::endl;
	for (int i = 0; i < 77; i++)
	{
		std::cout << "send message current times is :" << i << std::endl;
		norn_send_message((unsigned char*)buf,256,"10.0.11.78",peerport1);
		std::this_thread::sleep_for(std::chrono::milliseconds(1000 * 3));
	//	sleep(1000 *5);
	}
}
void test2(char* lip, int port, int peerport1 = 0, int peerport2 = 0, int peerport3 = 0)
{
	char buf[256] = { 0 };
	for (int i = 0; i < 256; i++) buf[i] = i % 255;
	norn_dht_init(port, callback1, callback2);
	std::cout << "norn_dht_init!!!" << std::endl;
	getchar();
	std::cout << "begin bootstrap" << std::endl;
	if (peerport1 > 0)
		norn_dht_bootstrap(lip, peerport1);
	if (peerport2 > 0)
		norn_dht_bootstrap(lip, peerport2);
	if (peerport3 > 0)
		norn_dht_bootstrap(lip, peerport3);
	std::cout << "end bootstrap" << std::endl;
	std::cout << "�ȴ�10s" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds((1000 * 10)));
	std::cout << "��������" << std::endl;
	getchar();
	std::cout << "��ʼ��ȡDHT��ַ" << std::endl;
	for (int i = 0; i < 100; i++)
	{
		int addr_count = norn_get_address_count();
		if (addr_count > 0)
		{
			unsigned long long *peer_addr = new unsigned long long[addr_count];
			norn_get_address(peer_addr, addr_count);
			if (addr_count > 0)
			{
				std::cout <<i<< " get address size=" << addr_count << std::endl;
				int ip, port; struct sockaddr_in sin;
				for (int i = 0; i < addr_count; i++)
				{
					port = peer_addr[i] & 0x00000000ffffffff;
					ip = (peer_addr[i] >> 32) & 0x00000000ffffffff;
					sin.sin_addr.s_addr = ip;
					std::cout << i + 1 << " ip=" << inet_ntoa(sin.sin_addr)
						<< " port=" << ntohs(port) << std::endl;
				}
			}
			delete[] peer_addr;
		}
		else
			std::cout <<i<< " get all address size = 0" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds((1000 * 10)));
	}
}

int main(int argc, char* argv[])
{
	//getchar();
	char *lip = 0; int lport=0, pport1=0;
	if (argc >= 2) lip = argv[1];
	if (argc >= 3) lport = atoi(argv[2]);
	if (argc >= 4) pport1 = atoi(argv[3]);
	std::cout << "local ip=" << lip << "local port=" << lport
		<<"peer port="<<pport1<<std::endl;
	test(lip, lport, pport1);
	//test2(lip, lport, pport1);
	return 0;
}

