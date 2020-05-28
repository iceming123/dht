#ifndef __DHT_WORKER_H__
#define __DHT_WORKER_H__

#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <unordered_set>
#include <queue>

struct arrow_address_info
{
	arrow_address_info(){}
	arrow_address_info(unsigned long long a, int b){
		addr_ = a; time_ = b;
	}
	unsigned long long addr_;
	int time_;
	inline bool operator==(const arrow_address_info &robj) const
	{
		return addr_ == robj.addr_;
	}
	arrow_address_info &operator =(arrow_address_info &obj)
	{
		this->addr_ = obj.addr_;
		this->time_ = obj.time_;
		return *this;
	}
};

struct addresshash
{
	size_t operator()(arrow_address_info val) const
	{
		return static_cast<size_t>(val.addr_);
	}
};

class dht_worker
{
public:
	~dht_worker();
	static dht_worker* instance() {
		static dht_worker ins;
		return &ins;
	}
	bool start(int port,dht_recv_callback *cb);
	void stop();
	static void worker(dht_worker *punit);
	static void worker2(dht_worker *punit);
	static void search_callback(int event, const unsigned char *info_hash,
		const void *data, size_t data_len, long long user_data);

	int pre_start_work(const char* ip, int port);
	int set_value(const unsigned char *key, unsigned short value,long long user_data);
	int get_value(const unsigned char *key, long long user_data);
	bool get_all_dht_address(unsigned long long* addr,int& len);
	int get_address_count();
	int send_message(unsigned char *msg, int len, char* addr, int port);
	int set_broadcast_data(const unsigned char* bdata,int len,short etm);
	bool handle_search_result(const unsigned char *info_hash,
		const void *data, size_t data_len);
	bool send_get_closest_node_info(unsigned char *key, int len,long long user_data=0);
	bool get_node_id(unsigned char *key, int &len);
private:
	void worker_proc();
	void worker_proc2();
	void init_ping_node(unsigned int ip,unsigned short port);
	void finit();
	int send_get_all_address();
	void manage_all_addresses(int &last_get);
	void handle_recv_message();
	dht_worker();

	bool running_;
	std::thread h_dht_,h_reg_;
	dht_net dht_;
	long long s_, s6_,send_s_;
	unsigned char my_id_[DHT_KEY_HASH_SIZE];
	struct sockaddr_in sin_,local_sin_,send_sin_;
	struct sockaddr_in6 sin6_;
	dht_callback *callback_;
	dht_recv_callback *rc_callback_;
	std::queue<std::string> recved_msg_;
	std::mutex msg_lock_;
	std::condition_variable msg_cv_;
};

#endif // __DHT_WORKER_H__
