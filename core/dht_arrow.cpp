// ConsoleApplication1.cpp : Defines the entry point for the console application.
//
#include "dht_helper.h"
#include "dht_demoapi.h"
#include <iostream>
//#include <cstring>
#include <thread>
#include <list>

#ifdef WIN32
	#include <WinSock2.h>
	#include <windows.h>
BOOL CALLBACK CosonleHandler(DWORD ev)
{
	BOOL bRet = FALSE;
	switch (ev)
	{
	case CTRL_CLOSE_EVENT:
		std::cout<<"exiting ....."<<std::endl;
		bRet = TRUE;
		norn_dht_finit();
		break;
	default:
		break;
	}
	return bRet;
}
#else
	#include <arpa/inet.h>
	#include <string.h>
#endif // WIN32

struct PeerNode
{
    std::string ip;
    unsigned int port{8808};
};

void recv_msg_handle(const void *data, size_t data_len, char* from)
{
    std::cout<<"recv callback: data_len=" << data_len << ", from=" << from << std::endl;
}

void init_process(unsigned int localPort, const std::list<PeerNode>& remotePeers)
{
	std::cout << "begin norn_dht init" << std::endl;
	bool ret = norn_dht_init(localPort, recv_msg_handle);
	std::cout << "norn_dht init " << (ret ? "ok" : "failed") << std::endl;
	if (!ret) return;

	std::cout << "begin bootstrap" << std::endl;

    auto iter = remotePeers.begin();
    while (iter != remotePeers.end()) {
        norn_dht_bootstrap(iter->ip.c_str(), iter->port);
        //std::cout<<"bootstrap node"<<(iter->ip)<<":"<<iter->port<<std::endl;

        ++iter;
    }

	std::cout << "end bootstrap" << std::endl;
}

int main(int argc, char* argv[])
{
#ifdef _WIN32
	SetConsoleCtrlHandler(CosonleHandler, TRUE);
#endif

    unsigned int localPort = 8808;
    unsigned int remotePort = localPort;

    if(argc < 2) {
        std::cout<<"please input remote ip address..."<<std::endl;
        return -1;
    }

    std::list<PeerNode> remotePeers;
    for(int i=1; i<argc; ++i) {
        char* ip = argv[i];
        PeerNode p;
        p.ip = ip;
        p.port = remotePort;
        remotePeers.push_back(p);
    }

	init_process(localPort, remotePeers);

	std::cout << "please enter q to quit!" << std::endl;
	while ('q' == getchar())
	{
		break;
	}
	norn_dht_finit();
	return 0;
}

