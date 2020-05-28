#include "dht_net.h"
#include "dht_worker.h"
#include <memory>
#include <string>
#include <algorithm>
#include <chrono>
#include <functional>
#include <iostream>

#define CMD "CMD"
#ifndef _WIN32
#define sprintf_s sprintf
#elif _WIN32
#ifdef _DEBUG
#pragma comment(lib, "libprotobufd.lib")   
#else
#pragma comment(lib, "libprotobuf.lib")   
#endif
#endif

#define DHT_MAX_FRESH_ADDRESS_TIME  120

std::mutex _addr_lock;
std::unordered_set<unsigned long long> _all_addrs;


dht_worker::dht_worker()/* : reg_(false)*/
{ 
#ifdef _WIN32
	// Load Winsock
	int retval;
	WSADATA wsaData;
	if ((retval = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0)
	{
		WSACleanup();
		return;
	}
#endif // _WIN32
	running_ = false;
	memset(&sin_, 0, sizeof(sin_));
	sin_.sin_family = AF_INET;
	sin_.sin_addr.s_addr = htonl(INADDR_ANY);
	memset(&local_sin_, 0, sizeof(local_sin_));
	local_sin_.sin_family = AF_INET;
	memset(&send_sin_, 0, sizeof(send_sin_));
	send_sin_.sin_family = AF_INET;

	memset(&sin6_, 0, sizeof(sin6_));
	sin6_.sin6_family = AF_INET6;
	s_ = socket(PF_INET, SOCK_DGRAM, 0);
	s6_ = socket(PF_INET6, SOCK_DGRAM, 0);
	send_s_ = socket(PF_INET, SOCK_DGRAM, 0);
	callback_ = nullptr;
	rc_callback_ = nullptr;
	memset(my_id_,0,sizeof(my_id_));
}
dht_worker::~dht_worker()
{
	if (running_) stop();
#ifdef _WIN32
	WSACleanup();
#endif
}
bool dht_worker::start(int port,dht_recv_callback *cb)
{
	if (running_) return running_;
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	int rc = 0;
	if (s_ < 0 || send_s_ < 0) {
		perror("socket(IPv4)");
		return false;
	}
	if (s6_ < 0) {
		perror("socket(IPv6)");
		return false;
	}
	srand((unsigned)time(NULL));
	// ipv4 init
	sin_.sin_port = htons(port);
	rc = ::bind(s_, (struct sockaddr*)&sin_, sizeof(sin_));
	if (rc < 0) {
		perror("bind(IPv4)");
		return false;
	}
	// ipv6 init
	int val = 1;
	rc = setsockopt(s6_, IPPROTO_IPV6, IPV6_V6ONLY,
		(char *)&val, sizeof(val));
	if (rc < 0) {
		perror("setsockopt(IPV6_V6ONLY)");
		return false;
	}
	/* BEP-32 mandates that we should bind this socket to one of our
	global IPv6 addresses.  In this simple example, this only
	happens if the user used the -b flag. */
	sin6_.sin6_port = htons(port);
	rc = ::bind(s6_, (struct sockaddr*)&sin6_, sizeof(sin6_));
	if (rc < 0) {
		perror("bind(IPv6)");
		return false;
	}
//#ifdef _WIN32
//	local_sin_.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
//#else
//	local_sin_.sin_addr.s_addr = inet_addr("127.0.0.1");
//#endif
	local_sin_.sin_addr.s_addr = inet_addr("127.0.0.1");
	local_sin_.sin_port = htons(port);

	int tmpport = (port + 1000) % 65535;
	send_sin_.sin_port = htons(tmpport);
	rc = ::bind(send_s_, (struct sockaddr*)&send_sin_, sizeof(send_sin_));
	if (rc < 0) {
		perror("bind(IPv4)");
		return false;
	}
	dht_helper::instance()->simple_random_bytes(my_id_, DHT_KEY_HASH_SIZE);
	/* Init the dht.  This sets the socket into non-blocking mode. */
	rc = dht_.dht_init(s_, s6_, my_id_, (unsigned char*)"JC\0\0", stdout);
	if (rc < 0) {
		perror("dht_init");
		return false;
	}
	running_ = true; rc_callback_ = cb;
	callback_ = dht_worker::search_callback;
	h_dht_ = std::thread(std::bind(&dht_worker::worker,this));
	h_reg_ = std::thread(std::bind(&dht_worker::worker2, this));
	return true;
}
void dht_worker::stop()
{
	running_ = false;
	finit();
	if (h_reg_.joinable()) h_reg_.join();
	if (h_dht_.joinable()) h_dht_.join();
#ifdef _WIN32
	closesocket(s_); s_ = 0;
	closesocket(s6_); s6_ = 0;
	closesocket(send_s_); send_s_ = 0;
#else
	close(s_); s_ = 0;
	close(s6_); s6_ = 0;
	close(send_s_); send_s_ = 0;
#endif
	dht_.dht_uninit();
	while (!recved_msg_.empty()) recved_msg_.pop();
	google::protobuf::ShutdownProtobufLibrary();
}
void dht_worker::worker(dht_worker *punit)
{
	punit->worker_proc();
}
void dht_worker::worker2(dht_worker *punit)
{
	punit->worker_proc2();
}
void dht_worker::worker_proc()
{
	time_t tosleep = 0; char buf[4096] = {0};
	unsigned long long addrs[1000] = { 0 }; 
	int addr_cnt = 1000;
	struct sockaddr_storage from;
	socklen_t fromlen; long tmp = 0;
	while (running_)
	{
		struct timeval tv;
		fd_set readfds;
		tv.tv_sec = tosleep;
		tv.tv_usec = dht_helper::instance()->random() % 1000000;

		FD_ZERO(&readfds);
		if (s_ >= 0)
			FD_SET(s_, &readfds);
		if (s6_ >= 0)
			FD_SET(s6_, &readfds);
		int rc = select(s_ > s6_ ? s_ + 1 : s6_ + 1, &readfds, NULL, NULL, &tv);
		if (rc < 0) {
			if (errno != EINTR) {
				perror("select");
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		}
		if (rc > 0) {
			fromlen = sizeof(from);
			if (s_ >= 0 && FD_ISSET(s_, &readfds))
				rc = recvfrom(s_, buf, sizeof(buf) - 1, 0,
					(struct sockaddr*)&from, &fromlen);
			else if (s6_ >= 0 && FD_ISSET(s6_, &readfds))
				rc = recvfrom(s6_, buf, sizeof(buf) - 1, 0,
					(struct sockaddr*)&from, &fromlen);
		}
		if (rc > 0) 
		{
			if (strncmp(buf, CMD, 3) == 0)
			{
				// must from local
				if (1 != dht_helper::instance()->is_martian((struct sockaddr*)&from))
					continue;
				/* This is how you trigger a search for a torrent hash.  If port
				(the second argument) is non-zero, it also performs an announce.
				Since peers expire announced data after 30 minutes, it's a good
				idea to reannounce every 28 minutes or so. */
				char* pcmd = buf + 3;
				if ('s' == pcmd[0])
				{
					//int sp = 0; long long user_data = 0;
					//char hs[256] = { 0 };
					//sscanf(&pcmd[1], "%s %d %lld", &hs, &sp, &user_data);
					//SHA1_CONTEXT sc;
					//dht_helper::instance()->sha1_init(&sc);
					//dht_helper::instance()->sha1_write(&sc, (const unsigned char*)hs, strlen(hs));
					//dht_helper::instance()->sha1_final(&sc);
					//if (s_ >= 0)
					//{
					//	if (0 > dht_.dht_search(sc.buf, sp, AF_INET, callback_, user_data))
					//	{
					//		if (callback_) (*callback_)(DHT_EVENT_SEARCH_ERROR, sc.buf, NULL, 0, user_data);
					//	}
					//}
					//if (s6_ >= 0)
					//	dht_net::dht_search((dht_obj*)d_, sc.buf, sp, AF_INET6, callback_, NULL);
					long long user_data = 0;
					char key[256] = { 0 };
					sscanf(&pcmd[1], "%s %lld", &key, &user_data);
					if (s_ >= 0)
					{
						if (0 > dht_.dht_search((const unsigned char*)key, 0, AF_INET, callback_, user_data))
						{
							if (callback_) (*callback_)(DHT_EVENT_SEARCH_ERROR, (const unsigned char*)key, NULL, 0, user_data);
						}
					}
				}
				else if ('p' == pcmd[0])
				{
					int ip, port;
					sscanf(&pcmd[1], "%d %d", &ip, &port);
					init_ping_node(ip, port);
				}
				else if ('b' == pcmd[0])
				{
					int bdata_len = 0; short etm = 0;
					memcpy(&etm,pcmd+1,2);
					memcpy(&bdata_len, pcmd + 3, 4);
					dht_.set_broadcast_msg_key((const unsigned char*)(pcmd + 7), bdata_len,etm);
				}
				else if ('g' == pcmd[0])
				{
					addr_cnt = 1000;
					dht_.get_all_addr_inbucket(addrs,addr_cnt);
					if (addr_cnt > 0)
					{
						std::unique_lock<std::mutex> lock(_addr_lock);
						_all_addrs.clear();
						for (int i = 0; i < addr_cnt; i++)
						{
							_all_addrs.insert(addrs[i]);
						}
					}
				}
				else if ('q' == pcmd[0])
					return;
			}
			else if (strncmp(buf, "MSGc", 4) == 0 || strncmp(buf, "BDMs", 4) == 0)
			{
				char pbuf[8] = {0}; int buflen = rc-4;
				struct sockaddr_in *tsin = (struct sockaddr_in*)((struct sockaddr*)&from);
				const char *address = (const char*)&tsin->sin_addr;
				short port = ntohs(tsin->sin_port);
				memcpy(pbuf, address, 4);
				std::string rmsg = pbuf; memset(pbuf,0,sizeof(pbuf));
				memcpy(pbuf, &port, 2); rmsg += pbuf;
				rmsg += (std::string(reinterpret_cast<const char*>(buf + 4), buflen));
				{
					std::unique_lock<std::mutex> lock(msg_lock_);
					recved_msg_.emplace(rmsg);
					msg_cv_.notify_one();
				}
				if (strncmp(buf, "BDMs", 4) == 0)
				{
					rc = dht_.dht_periodic(buf + 4, buflen, (struct sockaddr*)&from, fromlen,
						&tosleep, callback_);
				}
			}
			else
			{
				buf[rc] = '\0';
				rc = dht_.dht_periodic(buf, rc, (struct sockaddr*)&from, fromlen,
					&tosleep, callback_);
			}
		}
		else
		{
			rc = dht_.dht_periodic(nullptr, 0, nullptr, 0, &tosleep, callback_);
		}
		if (rc < 0) {
			if (errno == EINTR) {
				continue;
			}
			else {
				perror("dht_periodic");
				if (rc == EINVAL || rc == EFAULT)
					return;
				tosleep = 1;
			}
		}
		//std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}
void dht_worker::worker_proc2()
{
	int last_get = 0;
	while (running_)
	{
		handle_recv_message();
		manage_all_addresses(last_get);
		//std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}
}
int dht_worker::pre_start_work(const char* ip, int port)
{
	if (!ip || port <= 0 || port > 65535) return -1;
	unsigned int i_ip = inet_addr(ip);
	char buf[64] = { 0 };
	sprintf_s(buf, "CMDp%d %d", i_ip, port);
	struct sockaddr_in sin = sin_;
	sin.sin_addr.s_addr = inet_addr("127.0.0.1");
	int rc = sendto(send_s_, buf, sizeof(buf), 0, (struct sockaddr*)&sin, sizeof(sin));
	return rc;
}
void dht_worker::init_ping_node(unsigned int ip, unsigned short port)
{
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = ip;
	dht_.dht_ping_node((const sockaddr*)&sin, sizeof(sin));
}

int dht_worker::set_value(const unsigned char *key, unsigned short value, long long user_data)
{
	int rc = 0;
	//char buf[64] = { 0 };
	//sprintf_s(buf, "CMDs%s %d %lld", key, value,user_data);
	//rc = sendto(send_s_, buf, sizeof(buf), 0, (struct sockaddr*)&local_sin_, sizeof(local_sin_));
	return rc;
}
int dht_worker::get_value(const unsigned char *key, long long user_data)
{
	int rc = 0;
	//char buf[64] = { 0 };
	//sprintf_s(buf, "CMDs%s %d %lld", key, 0,user_data);
	//rc = sendto(send_s_, buf, sizeof(buf), 0, (struct sockaddr*)&local_sin_, sizeof(local_sin_));
	return rc;
}
int dht_worker::send_get_all_address()
{
	std::string cmd = "CMDg";
	int rc = sendto(send_s_, cmd.c_str(), cmd.length(), 0, (struct sockaddr*)&local_sin_, sizeof(local_sin_));
	if (rc <= 0)
		std::cout << "send get address cmd [err=" << rc << "]" << std::endl;
	return rc;
}
// return value 0 -- buffer too long, >0-- set ok; -1--other error
int dht_worker::set_broadcast_data(const unsigned char* bdata, int len, short etm)
{
	if (len > 2048) return 0;
	std::string cmd = "CMDb";
	char buf[4] = { 0 }; memcpy(buf,&etm,sizeof(short));
	cmd += std::string(buf,2);
	memset(buf,0,sizeof(buf));
	memcpy(buf, &len, sizeof(int));
	cmd += std::string(buf,4);
	cmd += std::string((const char*)bdata,len);
	int rc = sendto(send_s_, cmd.c_str(), cmd.length(), 0, (struct sockaddr*)&local_sin_, sizeof(local_sin_));
	return rc;
}
int dht_worker::send_message(unsigned char *msg, int len, char* addr,int port)
{
	if (!msg || len <= 0) return -1;
	int rc = 0;
	std::string buf = "MSGc";
	buf += std::string(reinterpret_cast<const char*>(msg), len);
	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = inet_addr(addr);
	sin.sin_port = htons(port);
	rc = sendto(send_s_, buf.c_str(), buf.length(), 0, (struct sockaddr*)&sin, sizeof(sin));
	return rc;
}
void dht_worker::finit()
{
	if (send_s_ > 0)
	{
		char buf[1024] = { "CMDq" };
		struct sockaddr_in sin = sin_;
#ifdef _WIN32
		sin.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
#else
		sin.sin_addr.s_addr = inet_addr("127.0.0.1");
#endif
		sendto(send_s_, buf, sizeof(buf), 0, (struct sockaddr*)&sin, sizeof(sin));
	}
}
void dht_worker::manage_all_addresses(int &last_get)
{
	struct timeval now; 
	dht_helper::instance()->dht_gettimeofday(&now, NULL);
	if (now.tv_sec - last_get > DHT_MAX_FRESH_ADDRESS_TIME)
	{
		last_get = now.tv_sec;
		send_get_all_address();
	}
}
bool dht_worker::get_all_dht_address(unsigned long long* addr, int& len)
{
	if (!addr || len <= 0) return false;
	int pos = 0,sum = 0;
	std::unique_lock<std::mutex> lock(_addr_lock);
	for (auto &obj : _all_addrs)
	{
		addr[pos++] = obj;
		if (pos >= len) break;
	}
	len = pos;
	return true;
}
int dht_worker::get_address_count()
{
	std::unique_lock<std::mutex> lock(_addr_lock);
	int cnt = _all_addrs.size();
	return cnt;
}
void dht_worker::handle_recv_message()
{
	std::string msg;
	{
		std::unique_lock<std::mutex> nlock(msg_lock_);
		if (std::cv_status::timeout == msg_cv_.wait_for(nlock, std::chrono::seconds(1)))
		{
			if (recved_msg_.empty()) return;
		}
		if (!recved_msg_.empty())
		{
			msg = recved_msg_.front();
			recved_msg_.pop();
		}
		else
			return;
	}
	int addr; memcpy(&addr,msg.c_str(),4);
	const char *pdata = msg.c_str() + 4;
	int datalen = msg.length() - 4;
	short port = 0; memcpy(&port, pdata, 2);
	pdata += 2; datalen -= 2;
	struct sockaddr_in sin;
	sin.sin_addr.s_addr = addr;
	char* addr_ip = inet_ntoa(sin.sin_addr);
	if (rc_callback_)
	{
		(*rc_callback_)((const void*)pdata, datalen, addr_ip);
	}
}
void dht_worker::search_callback(int event, const unsigned char *info_hash,
	const void *data, size_t data_len, long long user_data)
{
	if (event == DHT_EVENT_SEARCH_DONE)
	{
		dht_worker::instance()->handle_search_result(info_hash, data, data_len);
	}
}
bool dht_worker::handle_search_result(const unsigned char *info_hash,
	const void *data, size_t data_len)
{
	if (!info_hash || !data || data_len < (DHT_KEY_HASH_SIZE + 6)) return false;
	int lip = 0;
	short lport = 0;
	std::string rmsg; 	data_head h;
	h.cmdid = DATA_TYPE_ID_SEARCH_RESULT;
	h.length = 0; h.version = 0;
	if (!dht_.id_check(std::string((const char*)data+6, DHT_KEY_HASH_SIZE)))
	{
		lip = sin_.sin_addr.s_addr;
		lport = sin_.sin_port;
		memcpy((unsigned char*)data,&lip,sizeof(lip));
		memcpy((unsigned char*)data + sizeof(lip), &lport, sizeof(lport));
	}
	else
	{
		memcpy(&lip, (unsigned char*)data, sizeof(lip));
		memcpy(&lport, (unsigned char*)data + sizeof(lip), sizeof(lport));
	}
	short port = ntohs(lport);
	rmsg = (std::string(reinterpret_cast<const char*>(&lip), sizeof(lip)));
	rmsg += (std::string(reinterpret_cast<const char*>(&port), sizeof(port)));
	rmsg += (std::string(reinterpret_cast<const char*>(&h), sizeof(h)));
	rmsg += (std::string(reinterpret_cast<const char*>(data), data_len));
	{
		std::unique_lock<std::mutex> lock(msg_lock_);
		recved_msg_.emplace(rmsg);
		msg_cv_.notify_one();
	}
	return true;
}
bool dht_worker::send_get_closest_node_info(unsigned char *key, int len, long long user_data)
{
	if (!key || len < 20) return -1;
	int rc = 0;
	char buf[64] = { 0 };
	sprintf_s(buf, "CMDs%s %lld", key, user_data);
	rc = sendto(send_s_, buf, sizeof(buf), 0, (struct sockaddr*)&local_sin_, sizeof(local_sin_));
	return rc;
}
bool dht_worker::get_node_id(unsigned char *key, int &len)
{
	if (!key || len < DHT_KEY_HASH_SIZE) return false;
	memcpy(key, my_id_, DHT_KEY_HASH_SIZE);
	len = DHT_KEY_HASH_SIZE;
	return true;
}