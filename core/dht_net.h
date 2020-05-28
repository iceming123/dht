#ifndef __DHT_NET_H__
#define __DHT_NET_H__
/*
Copyright (c) 2009-2011 by Juliusz Chroboczek

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "dht_head.h"
#include "dht_helper.h"
#include "dht_data.pb.h"

class bd_data
{
public:
	bd_data(){}
	bd_data(const std::string &buf,int tt=30)
	{
		stamp_ = tt;
		data_ = buf;
	}
	bd_data(const bd_data &obj)
	{
		this->stamp_ = obj.stamp_;
		this->data_ = obj.data_;
		this->vnodes_ = obj.vnodes_;
	}
	bd_data(bd_data&& obj)
	{
		this->stamp_ = std::move(obj.stamp_);
		this->data_ = std::move(obj.data_);
		this->vnodes_ = std::move(obj.vnodes_);
	}
	bd_data &operator =(const bd_data &obj)
	{
		this->stamp_ = obj.stamp_;
		this->data_ = obj.data_;
		this->vnodes_ = obj.vnodes_;
		return *this;
	}
	bd_data &operator =(bd_data && obj)
	{
		if(this == &obj) return *this;
		this->stamp_ = std::move(obj.stamp_);
        this->data_ = std::move(obj.data_);
        this->vnodes_ = std::move(obj.vnodes_);
		return *this;
	}

	std::string data_;
	int stamp_;
	std::vector<int> vnodes_;
};
// the class not thread-safe class
class dht_obj
{
public:
	dht_obj();
	~dht_obj(){}
	bool init(int s, int s6, const unsigned char *v, const unsigned char *id,
	struct timeval now, FILE* df);
	bool finit();
	const unsigned char* get_myid(){ return myid; }

	bool in_bucket(const unsigned char *id, bucket *b);
	bucket* find_bucket(unsigned const char *id, int af);
	bucket *previous_bucket(bucket *b);
	node * find_node(const unsigned char *id, int af);
	node *random_node(bucket *b);
	int bucket_middle(bucket *b, unsigned char *id_return);
	int bucket_random(bucket *b, unsigned char *id_return);
	node *insert_node(node *node);
	int node_good(node *node, struct timeval now);

	void dht_dump_tables(FILE *f, struct timeval now);
	void dump_bucket(FILE *f, struct bucket *b, struct timeval now);
	int dht_get_nodes(struct sockaddr_in *sin, int *num,struct sockaddr_in6 *sin6,
		int *num6, struct timeval now);
	int dht_nodes(int af, int *good_return, int *dubious_return, int *cached_return,
		int *incoming_return, struct timeval now);
	//////////////////////////////////////////////////////////////////////////
	struct search* find_search(unsigned short tid, int af);
	int insert_search_node(const unsigned char *id, const struct sockaddr *sa, int salen,
	struct search *sr, int replied, const unsigned char *token, int token_len, struct timeval now);
	void insert_search_bucket(struct bucket *b, struct search *sr, struct timeval now);
	void flush_search_node(struct search_node *n, struct search *sr);
	int insert_closest_node(unsigned char *nodes, int numnodes,	const unsigned char *id, struct node *n);
	int buffer_closest_nodes(unsigned char *nodes, int numnodes,const unsigned char *id,
	struct bucket *b, struct timeval now);
	int make_closest_nodes(const unsigned char *id, int want, unsigned char *nodes, int &numnodes,
		unsigned char *nodes6, int &numnodes6, struct timeval now);
	struct search* new_search(struct timeval now);
	//////////////////////////////////////////////////////////////////////////
	bool storage_store(const unsigned char *id, const struct sockaddr *sa,
		unsigned short port, struct timeval now);
	struct storage* find_storage(const unsigned char *id);
	void expire_searches(struct timeval now);
	int expire_storage(struct timeval now);
	//////////////////////////////////////////////////////////////////////////
public:
	time_t mybucket_grow_time, mybucket6_grow_time;
	unsigned char myid[DHT_KEY_HASH_SIZE];
	int have_v;
	unsigned char my_v[9];
	struct bucket *buckets;
	struct bucket *buckets6;
	struct storage *storage;
	int numstorage;
	struct search *searches;
	int numsearches;
	unsigned short search_id;
	struct sockaddr_storage blacklist[DHT_MAX_BLACKLISTED];
	int next_blacklisted;
	FILE *dht_debug;
};


class dht_net
{
public:
	dht_net();
	~dht_net() {};
	//////////////////////////////////////////////////////////////////////////
	static void make_tid(unsigned char *tid_return, const char *prefix, unsigned short seqno);
	static int tid_match(const unsigned char *tid, const char *prefix,unsigned short *seqno_return);
	int token_bucket();
	int rotate_secrets();
	void make_token(const struct sockaddr *sa, int old, unsigned char *token_return);
	int token_match(const unsigned char *token, int token_len, const struct sockaddr *sa);
	int expire_buckets(bool v4 = true,struct bucket *pb = nullptr);
	int neighbourhood_maintenance(int af);
	int bucket_maintenance(int af);
	int bd_data_maintenance();
	//void set_regflag(bool reg){ reg_self = reg; }
	//bool get_regflag(){ return reg_self; }
	//////////////////////////////////////////////////////////////////////////
	int send_cached_ping(struct bucket *b);
	void pinged(struct node *n, struct bucket *b);
	void blacklist_node(const unsigned char *id, const struct sockaddr *sa, int salen);
	int node_blacklisted(const struct sockaddr *sa, int salen);
	struct bucket *split_bucket(struct bucket *b);
	struct node * new_node(const unsigned char *id,const struct sockaddr *sa, int salen, int confirm);
	//////////////////////////////////////////////////////////////////////////
	void search_step(struct search *sr);
	int search_send_get_peers(struct search *sr, struct search_node *n);
	int dht_search(const unsigned char *id, int port, int af,
		dht_callback *callback,long long user_data);
	//////////////////////////////////////////////////////////////////////////
private:
	int dht_send(const void *buf, size_t len, int flags,const struct sockaddr *sa, int salen);
	int send_ping(const struct sockaddr *sa, int salen,const unsigned char *tid, int tid_len);
	int send_pong(const struct sockaddr *sa, int salen,const unsigned char *tid, int tid_len);
	int send_find_node(const struct sockaddr *sa, int salen, const unsigned char *tid,
		int tid_len,const unsigned char *target, int want, int confirm);
	int send_nodes_peers(const struct sockaddr *sa, int salen,const unsigned char *tid, int tid_len,
		const unsigned char *nodes, int nodes_len,const unsigned char *nodes6, int nodes6_len,
		int af, struct storage *st,const unsigned char *token, int token_len);
	int send_closest_nodes(const struct sockaddr *sa, int salen,
		const unsigned char *tid, int tid_len, const unsigned char *id, int want,
		int af, struct storage *st, const unsigned char *token, int token_len);
	int send_get_peers(const struct sockaddr *sa, int salen, unsigned char *tid,
		int tid_len, unsigned char *infohash,int want, int confirm);
	int send_announce_peer(const struct sockaddr *sa, int salen,
		unsigned char *tid, int tid_len,
		unsigned char *infohash, unsigned short port,
		unsigned char *token, int token_len, int confirm);
	int send_peer_announced(const struct sockaddr *sa, int salen,
		const unsigned char *tid, int tid_len);
	int send_error(const struct sockaddr *sa, int salen,
		const unsigned char *tid, int tid_len, int code, const char *message);
	int send_broadcast_msg_key_reply(const unsigned char *key, const struct sockaddr *from, int fromlen);
	int send_broadcast_msg(const std::string &key, const std::string &data, int etm, const struct sockaddr *from, int fromlen);
	int send_broadcast_msg_key(const std::string &key, const std::string &data, int etm);
	//////////////////////////////////////////////////////////////////////////
public:
	int dht_init(int s, int s6, const unsigned char *id, const unsigned char *v, FILE* df);
	int dht_uninit();
	int dht_periodic(const void *buf, size_t buflen, const struct sockaddr *from,
		int fromlen, time_t *tosleep, dht_callback *callback);
	bool dht_insert_node(const unsigned char *id, struct sockaddr *sa, int salen);
	int dht_ping_node(const struct sockaddr *sa, int salen);
	/* This must be provided by the user. */
	int dht_blacklisted(const struct sockaddr *sa, int salen);
	bool dht_orign_key_pki(const unsigned char* key, const struct sockaddr *sa);
	int set_broadcast_msg_key(const unsigned char* bdata, int len, int etm=0);
	bool get_all_addr_inbucket(unsigned long long* addrs,int &cnt);
	bool id_check(std::string id);
private:
	int parse_and_handle_message(const unsigned char *buf, int buflen,
		const struct sockaddr *from, int fromlen);
	// all handles return-1--wrong message;0--wrong node;1--success handle
	bool handle_ping_msg(const unsigned char *buf, int buflen, 
		const struct sockaddr *from, int fromlen, dht_msg_head &head);
	bool handle_pong_msg(const unsigned char *buf, int buflen,
		const struct sockaddr *from, int fromlen, dht_msg_head &head);
	bool handle_find_node_msg(const unsigned char *buf, int buflen, 
		const struct sockaddr *from, int fromlen, dht_msg_head &head);
	bool handle_closest_node_msg(const unsigned char *buf, int buflen, 
		const struct sockaddr *from, int fromlen, dht_msg_head &head);
	bool handle_get_peer_msg(const unsigned char *buf, int buflen, 
		const struct sockaddr *from, int fromlen, dht_msg_head &head);
	bool handle_announce_peer_msg(const unsigned char *buf, int buflen, 
		const struct sockaddr *from, int fromlen, dht_msg_head &head);
	bool handle_announce_reply_msg(const unsigned char *buf, int buflen, 
		const struct sockaddr *from, int fromlen, dht_msg_head &head);
	bool handle_error_msg(const unsigned char *buf, int buflen,
		const struct sockaddr *from, int fromlen, dht_msg_head &head);
	bool handle_broadcast_msg_key(const unsigned char *buf, int buflen,
		const struct sockaddr *from, int fromlen, dht_msg_head &head);
private:
	dht_obj one_;
	int dht_socket_, dht_socket6_;
	//std::atomic_bool reg_self;
	time_t confirm_nodes_time_;
	time_t search_time_;
	time_t expire_stuff_time_;
	int token_bucket_tokens_;
	time_t token_bucket_time_;
	time_t rotate_secrets_time_;
	struct timeval now_;
	unsigned char secret_[8];
	unsigned char oldsecret_[8];

	std::unordered_map<std::string, bd_data> bd_data_;
	time_t broadcast_maintenance_time_;
};

#endif

