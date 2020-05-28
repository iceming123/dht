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

/* Please, please, please.

You are welcome to integrate this code in your favourite Bittorrent
client.  Please remember, however, that it is meant to be usable by
others, including myself.  This means no C++, no relicensing, and no
gratuitious changes to the coding style.  And please send back any
improvements to the author. */

#include "dht_net.h"
#include <time.h>
/* For memmem. */
//#define _GNU_SOURCE
//#ifdef __GNUC__
//__attribute__((format(printf, 1, 2)))
//#endif

#define MAKEMSGID(mid,mtype)	(((unsigned int)((unsigned short)(mid) & 0xffff)) | ((unsigned int)((unsigned short)(mtype) & 0xffff) << 16))
#define GETMID(msgid)			((unsigned short)(((unsigned int)(msgid)) & 0xffff))
#define GETMTYPE(msgid)         ((unsigned short)((((unsigned int)(msgid)) >> 16) & 0xffff))
#define TOKEN_SIZE 8
#define DHT_MESSAGE_PREX  "DHTs"
#define DHT_BROADCAST_MAX_LIVETIME	 60

#define MAKE_DHT_MESSAGE(mid,mtype,msg,msg_head,allbuf)				\
{																	\
	msg_head.set_msg_id(MAKEMSGID(mid, mtype));						\
	msg_head.set_len(msg.ByteSizeLong());							\
	msg_head.set_version(0);										\
	allbuf += DHT_MESSAGE_PREX;										\
	char tmp[4];int head_len = msg_head.ByteSizeLong();				\
	memcpy(tmp,&head_len,sizeof(head_len));							\
	allbuf += std::string(tmp,4);									\
	allbuf += msg_head.SerializeAsString();							\
	allbuf += msg.SerializeAsString();								\
}																	\

///////////////////////dht_obj info/////////////////////////////////////////
dht_obj::dht_obj()
{
	dht_debug = nullptr;
	searches = nullptr;
	numsearches = 0;
	storage = nullptr;
	numstorage = 0;
	buckets = nullptr;
	buckets6 = nullptr;
	memset(myid, 0, sizeof(myid));
	memset(my_v, 0, sizeof(my_v));
	have_v = 0;
	next_blacklisted = 0;
	memset(blacklist, 0, sizeof(blacklist));
	mybucket_grow_time = 0;
	mybucket6_grow_time = 0;
	search_id = 0;
}
bool dht_obj::init(int s, int s6, const unsigned char *v, const unsigned char *id,
	struct timeval now, FILE* df)
{
	int rc;
	dht_debug = df;
	if (s >= 0) {
		buckets = new struct bucket;
		if (buckets == NULL) goto fail;
		memset(buckets, 0, sizeof(struct bucket));
		buckets->af = AF_INET;
		rc = dht_helper::instance()->set_nonblocking(s, 1);
		if (rc < 0) goto fail;
	}
	if (s6 >= 0) {
		buckets6 = new struct bucket;
		if (buckets6 == NULL) goto fail;
		memset(buckets6, 0, sizeof(struct bucket));
		buckets6->af = AF_INET6;
		rc = dht_helper::instance()->set_nonblocking(s6, 1);
		if (rc < 0) goto fail;
	}
	memcpy(myid, id, 20);
	if (v) {
		memcpy(my_v, "1:v4:", 5);
		memcpy(my_v + 5, v, 4);
		have_v = 1;
	}
	else {
		have_v = 0;
	}
	mybucket_grow_time = now.tv_sec;
	mybucket6_grow_time = now.tv_sec;
	search_id = dht_helper::instance()->random() & 0xFFFF;
	return true;
fail:
	if (buckets)
	{
		delete buckets;
		buckets = nullptr;
	}
	if (buckets6)
	{
		delete (buckets6);
		buckets6 = nullptr;
	}
	return false;
}
bool dht_obj::finit()
{
	while (buckets) {
		struct bucket *b = buckets;
		buckets = b->next;
		while (b->nodes) {
			struct node *n = b->nodes;
			b->nodes = n->next;
			delete (n);
		}
		delete(b);
	}
	while (buckets6) {
		struct bucket *b = buckets6;
		buckets6 = b->next;
		while (b->nodes) {
			struct node *n = b->nodes;
			b->nodes = n->next;
			delete(n);
		}
		delete(b);
	}

	while (storage) {
		struct storage *st = storage;
		storage = storage->next;
		delete[](st->peers);
		delete(st);
	}

	while (searches) {
		struct search *sr = searches;
		searches = searches->next;
		delete(sr);
	}
	return true;
}
/* We keep buckets in a sorted linked list.  A bucket b ranges from
b->first inclusive up to b->next->first exclusive.
id in [b->first,b->next->first)==>true ortherwise return false */
bool dht_obj::in_bucket(const unsigned char *id, struct bucket *b)
{
	return dht_helper::instance()->id_cmp(b->first, id) <= 0 &&
		(b->next == NULL || dht_helper::instance()->id_cmp(id, b->next->first) < 0);
}
bucket* dht_obj::find_bucket(unsigned const char *id, int af)
{
	bucket *b = af == AF_INET ? buckets : buckets6;
	if (b == NULL)
		return NULL;
	while (1) {
		if (b->next == NULL)
			return b;
		if (dht_helper::instance()->id_cmp(id, b->next->first) < 0)
			return b;
		b = b->next;
	}
}
bucket * dht_obj::previous_bucket(bucket *b)
{
	bucket *p = b->af == AF_INET ? buckets : buckets6;
	if (b == p)
		return NULL;

	while (1) {
		if (p->next == NULL)
			return NULL;
		if (p->next == b)
			return p;
		p = p->next;
	}
}
/* Every bucket contains an unordered list of nodes. */
node *dht_obj::find_node(const unsigned char *id, int af)
{
	bucket *b = find_bucket(id, af);
	node *n;
	if (b == NULL) return NULL;
	n = b->nodes;
	while (n) {
		if (dht_helper::instance()->id_cmp(n->id, id) == 0)
			return n;
		n = n->next;
	}
	return NULL;
}
/* Return a random node in a bucket. */
node *dht_obj::random_node(bucket *b)
{
	node *n;
	int nn;
	if (b->count == 0) return NULL;
	nn = dht_helper::instance()->random() % b->count;
	n = b->nodes;
	while (nn > 0 && n) {
		n = n->next;
		nn--;
	}
	return n;
}
/* Return the middle id of a bucket. */
int dht_obj::bucket_middle(bucket *b, unsigned char *id_return)
{
	int bit1 = dht_helper::instance()->lowbit(b->first);
	int bit2 = b->next ? dht_helper::instance()->lowbit(b->next->first) : -1;
	int bit = MAX(bit1, bit2) + 1;
	if (bit >= 160) return -1;
	memcpy(id_return, b->first, 20);
	id_return[bit / 8] |= (0x80 >> (bit % 8));
	return 1;
}
/* Return a random id within a bucket. */
int dht_obj::bucket_random(bucket *b, unsigned char *id_return)
{
	int bit1 = dht_helper::instance()->lowbit(b->first);
	int bit2 = b->next ? dht_helper::instance()->lowbit(b->next->first) : -1;
	int bit = MAX(bit1, bit2) + 1;
	if (bit >= 160) {
		memcpy(id_return, b->first, 20);
		return 1;
	}
	memcpy(id_return, b->first, bit / 8);
	id_return[bit / 8] = b->first[bit / 8] & (0xFF00 >> (bit % 8));
	id_return[bit / 8] |= dht_helper::instance()->random() & 0xFF >> (bit % 8);
	for (int i = bit / 8 + 1; i < 20; i++)
		id_return[i] = dht_helper::instance()->random() & 0xFF;
	return 1;
}
/* Insert a new node into a bucket. */
node *dht_obj::insert_node(struct node *node)
{
	struct bucket *b = find_bucket(node->id, node->ss.ss_family);
	if (b == NULL) return NULL;
	node->next = b->nodes;
	b->nodes = node;
	b->count++;
	return node;
}
/* This is our definition of a known-good node. */
int dht_obj::node_good(node *node, struct timeval now)
{
	return (node->pinged <= 2 &&
		node->reply_time >= now.tv_sec - 7200 &&
		node->time >= now.tv_sec - 900);
}

void dht_obj::dump_bucket(FILE *f, struct bucket *b, struct timeval now)
{
	struct node *n = b->nodes;
	fprintf(f, "Bucket ");
	dht_helper::instance()->print_hex(b->first, 20, f);
	fprintf(f, " count %d age %d%s%s:\n",
		b->count, (int)(now.tv_sec - b->time),
		in_bucket(myid, b) ? " (mine)" : "",
		b->cached.ss_family ? " (cached)" : "");
	while (n) 
	{
		char buf[512];
		unsigned short port;
		fprintf(f, "    Node ");
		dht_helper::instance()->print_hex(n->id, 20, f);
		if (n->ss.ss_family == AF_INET) {
			struct sockaddr_in *sin = (struct sockaddr_in*)&n->ss;
			inet_ntop(AF_INET, &sin->sin_addr, buf, 512);
			port = ntohs(sin->sin_port);
		}
		else if (n->ss.ss_family == AF_INET6) {
			struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)&n->ss;
			inet_ntop(AF_INET6, &sin6->sin6_addr, buf, 512);
			port = ntohs(sin6->sin6_port);
		}
		else {
			snprintf(buf, 512, "unknown(%d)", n->ss.ss_family);
			port = 0;
		}

		if (n->ss.ss_family == AF_INET6)
			fprintf(f, " [%s]:%d ", buf, port);
		else
			fprintf(f, " %s:%d ", buf, port);
		if (n->time != n->reply_time)
			fprintf(f, "age %ld, %ld",
			(long)(now.tv_sec - n->time),
			(long)(now.tv_sec - n->reply_time));
		else
			fprintf(f, "age %ld", (long)(now.tv_sec - n->time));
		if (n->pinged)
			fprintf(f, " (%d)", n->pinged);
		if (node_good(n,now))
			fprintf(f, " (good)");
		fprintf(f, "\n");
		n = n->next;
	}

}
void dht_obj::dht_dump_tables(FILE *f, struct timeval now)
{
	int i;
	struct bucket *b;
	struct storage *st = storage;
	struct search *sr = searches;

	fprintf(f, "My id ");
	dht_helper::instance()->print_hex(myid, 20, f);
	fprintf(f, "\n");

	b = buckets;
	while (b) {
		dump_bucket(f, b,now);
		b = b->next;
	}
	fprintf(f, "\n");
	b = buckets6;
	while (b) {
		dump_bucket(f, b,now);
		b = b->next;
	}

	while (sr) {
		fprintf(f, "\nSearch%s id ", sr->af == AF_INET6 ? " (IPv6)" : "");
		dht_helper::instance()->print_hex(sr->id, 20, f);
		fprintf(f, " age %d%s\n", (int)(now.tv_sec - sr->step_time),
			sr->done ? " (done)" : "");
		for (i = 0; i < sr->numnodes; i++) {
			struct search_node *n = &sr->nodes[i];
			fprintf(f, "Node %d id ", i);
			dht_helper::instance()->print_hex(n->id, 20, f);
			fprintf(f, " bits %d age ", dht_helper::instance()->common_bits(sr->id, n->id));
			if (n->request_time)
				fprintf(f, "%d, ", (int)(now.tv_sec - n->request_time));
			fprintf(f, "%d", (int)(now.tv_sec - n->reply_time));
			if (n->pinged)
				fprintf(f, " (%d)", n->pinged);
			fprintf(f, "%s%s.\n",
				find_node(n->id, AF_INET) ? " (known)" : "",
				n->replied ? " (replied)" : "");
		}
		sr = sr->next;
	}
	while (st) {
		fprintf(f, "\nStorage ");
		dht_helper::instance()->print_hex(st->id, 20, f);
		fprintf(f, " %d/%d nodes:", st->numpeers, st->maxpeers);
		for (i = 0; i < st->numpeers; i++) {
			char buf[100];
			if (st->peers[i].len == 4) {
				inet_ntop(AF_INET, st->peers[i].ip, buf, 100);
			}
			else if (st->peers[i].len == 16) {
				buf[0] = '[';
				inet_ntop(AF_INET6, st->peers[i].ip, buf + 1, 98);
				strcat(buf, "]");
			}
			else {
				strcpy(buf, "???");
			}
			fprintf(f, " %s:%u (%ld)",
				buf, st->peers[i].port,
				(long)(now.tv_sec - st->peers[i].time));
		}
		st = st->next;
	}
	fprintf(f, "\n\n");
	fflush(f);
}

int dht_obj::dht_get_nodes(struct sockaddr_in *sin, int *num,struct sockaddr_in6 *sin6,
	int *num6, struct timeval now)
{
	int i=0, j=0;
	bucket *b=nullptr;
	node *n=nullptr;
	/* For restoring to work without discarding too many nodes, the list
	must start with the contents of our bucket. */
	b = find_bucket(myid, AF_INET);
	if (b && sin && num)
	{
		n = b->nodes;
		while (n && i < *num) 
		{
			if (node_good(n,now)) 
			{
				sin[i] = *(struct sockaddr_in*)&n->ss;
				i++;
			}
			n = n->next;
		}
		b = buckets;
		while (b && i < *num) 
		{
			if (!in_bucket(myid, b)) 
			{
				n = b->nodes;
				while (n && i < *num) 
				{
					if (node_good(n,now)) {
						sin[i] = *(struct sockaddr_in*)&n->ss;
						i++;
					}
					n = n->next;
				}
			}
			b = b->next;
		}
	}
	b = find_bucket(myid, AF_INET6);
	if (b && sin6 && num6)
	{
		n = b->nodes;
		while (n && j < *num6) 
		{
			if (node_good(n,now)) 
			{
				sin6[j] = *(struct sockaddr_in6*)&n->ss;
				j++;
			}
			n = n->next;
		}
		b = buckets6;
		while (b && j < *num6) 
		{
			if (!in_bucket(myid, b)) {
				n = b->nodes;
				while (n && j < *num6) 
				{
					if (node_good(n,now)) {
						sin6[j] = *(struct sockaddr_in6*)&n->ss;
						j++;
					}
					n = n->next;
				}
			}
			b = b->next;
		}
	}
	if(num)	*num = i;
	if(num6) *num6 = j;
	return (i + j);
}
bool dht_obj::storage_store(const unsigned char *id, const struct sockaddr *sa,
	unsigned short port, struct timeval now)
{
	int i=0, len=0;
	struct storage *st = nullptr;
	unsigned char *ip = nullptr;
	if (sa->sa_family == AF_INET) {
		struct sockaddr_in *sin = (struct sockaddr_in*)sa;
		ip = (unsigned char*)&sin->sin_addr;
		len = 4;
	}
	else if (sa->sa_family == AF_INET6) {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)sa;
		ip = (unsigned char*)&sin6->sin6_addr;
		len = 16;
	}
	else {
		return false;
	}
	st = find_storage(id);
	if (st == nullptr)
	{
		if (numstorage >= DHT_MAX_HASHES) return false;
		st = new struct storage;
		if (st == NULL) return false;
		memset(st, 0, sizeof(struct storage));
		memcpy(st->id, id, DHT_KEY_HASH_SIZE);
		st->next = storage;
		storage = st;
		numstorage++;
	}
	for (i = 0; i < st->numpeers; i++) 
	{
		if (st->peers[i].port == port && st->peers[i].len == len &&
			memcmp(st->peers[i].ip, ip, len) == 0)
			break;
	}
	if (i < st->numpeers) {
		/* Already there, only need to refresh */
		st->peers[i].time = now.tv_sec;
		return true;
	}
	else {
		struct peer *p;
		if (i >= st->maxpeers) {
			/* Need to expand the array. */
			struct peer *new_peers;
			int n;
			if (st->maxpeers >= DHT_MAX_PEERS) return false;
			n = st->maxpeers == 0 ? 2 : 2 * st->maxpeers;
			n = MIN(n, DHT_MAX_PEERS);
			new_peers = new struct peer[n];
			if (new_peers == NULL) return false;
			memset(new_peers, 0, sizeof(struct peer)*n);
			st->peers = new_peers;
			st->maxpeers = n;
		}
		p = &st->peers[st->numpeers++];
		p->time = now.tv_sec;
		p->len = len;
		memcpy(p->ip, ip, len);
		p->port = port;
	}
	return true;
}
struct storage* dht_obj::find_storage(const unsigned char *id)
{
	struct storage *st = storage;
	while (st) {
		if (dht_helper::instance()->id_cmp(id, st->id) == 0)
			break;
		st = st->next;
	}
	return st;
}
int dht_obj::dht_nodes(int af, int *good_return, int *dubious_return, 
	int *cached_return, int *incoming_return, struct timeval now)
{
	int good = 0, dubious = 0, cached = 0, incoming = 0;
	struct bucket *b = af == AF_INET ? buckets : buckets6;

	while (b) {
		struct node *n = b->nodes;
		while (n) {
			if (node_good(n,now)) {
				good++;
				if (n->time > n->reply_time)
					incoming++;
			}
			else {
				dubious++;
			}
			n = n->next;
		}
		if (b->cached.ss_family > 0)
			cached++;
		b = b->next;
	}
	if (good_return)
		*good_return = good;
	if (dubious_return)
		*dubious_return = dubious;
	if (cached_return)
		*cached_return = cached;
	if (incoming_return)
		*incoming_return = incoming;
	return good + dubious;
}
/* While a search is in progress, we don't necessarily keep the nodes being
walked in the main bucket table.  A search in progress is identified by
a unique transaction id, a short (and hence small enough to fit in the
transaction id of the protocol packets). */
struct search* dht_obj::find_search(unsigned short tid, int af)
{
	struct search *sr = searches;
	while (sr) {
		if (sr->tid == tid && sr->af == af)
			return sr;
		sr = sr->next;
	}
	return nullptr;
}

/* A search contains a list of nodes, sorted by decreasing distance to the
target.  We just got a new candidate, insert it at the right spot or
discard it. */
int dht_obj::insert_search_node(const unsigned char *id, const struct sockaddr *sa, int salen,
	struct search *sr, int replied, const unsigned char *token, int token_len, struct timeval now)
{
	struct search_node *n;
	int i, j;
	if (sa->sa_family != sr->af) {
		dht_helper::instance()->debugf("Attempted to insert node in the wrong family.\n");
		return 0;
	}

	for (i = 0; i < sr->numnodes; i++) {
		if (dht_helper::instance()->id_cmp(id, sr->nodes[i].id) == 0)
		{
			n = &sr->nodes[i];
			goto found;
		}
		if (dht_helper::instance()->xorcmp(id, sr->nodes[i].id, sr->id) < 0)
			break;
	}

	if (i == SEARCH_NODES)
		return 0;

	if (sr->numnodes < SEARCH_NODES)
		sr->numnodes++;

	for (j = sr->numnodes - 1; j > i; j--) {
		sr->nodes[j] = sr->nodes[j - 1];
	}

	n = &sr->nodes[i];

	memset(n, 0, sizeof(struct search_node));
	memcpy(n->id, id, 20);

found:
	memcpy(&n->ss, sa, salen);
	n->sslen = salen;
	if (replied) {
		n->replied = 1;
		n->reply_time = now.tv_sec;
		n->request_time = 0;
		n->pinged = 0;
	}
	if (token) {
		if (token_len >= 40) {
			dht_helper::instance()->debugf("Eek!  Overlong token.\n");
		}
		else {
			memcpy(n->token, token, token_len);
			n->token_len = token_len;
		}
	}

	return 1;
}
	/* Insert the contents of a bucket into a search structure. */
// 将桶中的节点加入到指定目标的search列表中
void dht_obj::insert_search_bucket(struct bucket *b, struct search *sr, struct timeval now)
{
	struct node *n;
	n = b->nodes;
	while (n) {
		insert_search_node(n->id, (struct sockaddr*)&n->ss, n->sslen,
			sr, 0, NULL, 0,now);
		n = n->next;
	}
}
void dht_obj::flush_search_node(struct search_node *n, struct search *sr)
{
	int i = n - sr->nodes, j;
	for (j = i; j < sr->numnodes - 1; j++)
		sr->nodes[j] = sr->nodes[j + 1];
	sr->numnodes--;
}
int dht_obj::insert_closest_node(unsigned char *nodes, int numnodes,
	const unsigned char *id, struct node *n)
{
	int i, size;
	if (n->ss.ss_family == AF_INET)
		size = 26;
	else if (n->ss.ss_family == AF_INET6)
		size = 38;
	else
		abort();

	for (i = 0; i < numnodes; i++) {
		if (dht_helper::instance()->id_cmp(n->id, nodes + size * i) == 0)
			return numnodes;
		if (dht_helper::instance()->xorcmp(n->id, nodes + size * i, id) < 0)
			break;
	}
	if (i == 8) return numnodes;
	if (numnodes < 8) numnodes++;
	if (i < numnodes - 1)
		memmove(nodes + size * (i + 1), nodes + size * i,
		size * (numnodes - i - 1));

	if (n->ss.ss_family == AF_INET) {
		struct sockaddr_in *sin = (struct sockaddr_in*)&n->ss;
		memcpy(nodes + size * i, n->id, 20);
		memcpy(nodes + size * i + 20, &sin->sin_addr, 4);
		memcpy(nodes + size * i + 24, &sin->sin_port, 2);
	}
	else if (n->ss.ss_family == AF_INET6) {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)&n->ss;
		memcpy(nodes + size * i, n->id, 20);
		memcpy(nodes + size * i + 20, &sin6->sin6_addr, 16);
		memcpy(nodes + size * i + 36, &sin6->sin6_port, 2);
	}
	else {
		abort();
	}
	return numnodes;
}
int dht_obj::buffer_closest_nodes(unsigned char *nodes, int numnodes,
	const unsigned char *id, struct bucket *b, struct timeval now)
{
	struct node *n = b->nodes;
	while (n) {
		if (node_good(n,now))
			numnodes = insert_closest_node(nodes, numnodes, id, n);
		n = n->next;
	}
	return numnodes;
}
int dht_obj::make_closest_nodes(const unsigned char *id, int want, unsigned char *nodes,
	int &numnodes, unsigned char *nodes6, int &numnodes6, struct timeval now)
{
	if (!nodes || !nodes6 || numnodes != 8 || numnodes6 != 8)
	{
		numnodes = 0; numnodes6 = 0;
		return 0;
	}
	numnodes = 0; numnodes6 = 0;
	struct bucket *b;
	if ((want & WANT4)) {
		b = find_bucket(id, AF_INET);
		if (b) {
			numnodes = buffer_closest_nodes(nodes, numnodes, id, b,now);
			if (b->next)
				numnodes = buffer_closest_nodes(nodes, numnodes, id, b->next,now);
			b = previous_bucket(b);
			if (b)
				numnodes = buffer_closest_nodes(nodes, numnodes, id, b,now);
		}
	}
	if ((want & WANT6)) {
		b = find_bucket(id, AF_INET6);
		if (b) {
			numnodes6 = buffer_closest_nodes(nodes6, numnodes6, id, b,now);
			if (b->next)
				numnodes6 =
				buffer_closest_nodes(nodes6, numnodes6, id, b->next,now);
			b = previous_bucket(b);
			if (b)
				numnodes6 = buffer_closest_nodes(nodes6, numnodes6, id, b,now);
		}
	}
	dht_helper::instance()->debugf("  (%d+%d nodes.)\n", numnodes, numnodes6);
	return 0;
}
struct search* dht_obj::new_search(struct timeval now)
{
	struct search *sr=nullptr, *oldest = nullptr;
	/* Find the oldest done search */
	sr = searches;
	while (sr)
	{
		if (sr->done &&
			(oldest == nullptr || oldest->step_time > sr->step_time))
			oldest = sr;
		sr = sr->next;
	}
	/* The oldest slot is expired. */
	if (oldest && oldest->step_time < now.tv_sec - DHT_SEARCH_EXPIRE_TIME)
		return oldest;

	/* Allocate a new slot. */
	if (numsearches < DHT_MAX_SEARCHES) {
		sr = new struct search;
		if (sr != NULL) {
			memset(sr, 0, sizeof(struct search));
			sr->next = searches;
			searches = sr;
			numsearches++;
			return sr;
		}
	}
	/* Oh, well, never mind.  Reuse the oldest slot. */
	return oldest;
}
// 过期的search将被从search链表中清除。(step_time超过62*60秒)
void dht_obj::expire_searches(struct timeval now)
{
	struct search *sr = searches, *previous = nullptr;
	while (sr) {
		struct search *next = sr->next;
		if (sr->step_time < now.tv_sec - DHT_SEARCH_EXPIRE_TIME) {
			if (previous)
				previous->next = next;
			else
				searches = next;
			delete sr;
			numsearches--;
		}
		else {
			previous = sr;
		}
		sr = next;
	}
}
int dht_obj::expire_storage(struct timeval now)
{
	struct storage *st = storage, *previous = nullptr;
	while (st) {
		int i = 0;
		while (i < st->numpeers) {
			if (st->peers[i].time < now.tv_sec - 32 * 60) {
				if (i != st->numpeers - 1)
					st->peers[i] = st->peers[st->numpeers - 1];
				st->numpeers--;
			}
			else {
				i++;
			}
		}
		if (st->numpeers == 0)
		{
			delete st->peers;
			if (previous)
				previous->next = st->next;
			else
				storage = st->next;
			delete st;
			if (previous)
				st = previous->next;
			else
				st = storage;
			numstorage--;
			if (numstorage < 0) {
				dht_helper::instance()->debugf("Eek... numstorage became negative.\n");
				numstorage = 0;
			}
		}
		else {
			previous = st;
			st = st->next;
		}
	}
	return 1;
}
///////////////////////dht_obj info/////////////////////////////////////////
dht_net::dht_net()
{
	//reg_self = false;
	confirm_nodes_time_ = 0;
	search_time_ = 0;
	expire_stuff_time_ = 0;
	dht_socket_ = 0;
	dht_socket6_ = 0;
	token_bucket_tokens_ = MAX_TOKEN_BUCKET_TOKENS;
	token_bucket_time_ = 0;
	rotate_secrets_time_ = 0;
	memset(secret_, 0, sizeof(secret_));
	memset(oldsecret_, 0, sizeof(oldsecret_));
}
/* Our transaction-ids are 4-bytes long, with the first two bytes identi-
fying the kind of request, and the remaining two a sequence number in
host order. */

void dht_net::make_tid(unsigned char *tid_return, const char *prefix, unsigned short seqno)
{
	tid_return[0] = prefix[0] & 0xFF;
	tid_return[1] = prefix[1] & 0xFF;
	memcpy(tid_return + 2, &seqno, 2);
}
int dht_net::tid_match(const unsigned char *tid, const char *prefix,unsigned short *seqno_return)
{
	if (tid[0] == (prefix[0] & 0xFF) && tid[1] == (prefix[1] & 0xFF)) {
		if (seqno_return)
			memcpy(seqno_return, tid + 2, 2);
		return 1;
	}
	else
		return 0;
}
/* Rate control for requests we receive. */
int dht_net::token_bucket()
{
	if (token_bucket_tokens_ == 0)
	{
		token_bucket_tokens_ = MIN(MAX_TOKEN_BUCKET_TOKENS,
			100 * (now_.tv_sec - (int)token_bucket_time_));
		token_bucket_time_ = now_.tv_sec;
	}
	if (token_bucket_tokens_ == 0) return 0;
	token_bucket_tokens_--;
	return 1;
}
int dht_net::rotate_secrets()
{
	rotate_secrets_time_ = now_.tv_sec + 900 + dht_helper::instance()->random() % 1800;
	memcpy(oldsecret_, secret_, sizeof(secret_));
	dht_helper::instance()->simple_random_bytes(secret_, sizeof(secret_));
	return 1;
}
void dht_net::make_token(const struct sockaddr *sa, int old, unsigned char *token_return)
{
	void *ip;
	int iplen;
	unsigned short port;

	if (sa->sa_family == AF_INET) {
		struct sockaddr_in *sin = (struct sockaddr_in*)sa;
		ip = &sin->sin_addr;
		iplen = 4;
		port = htons(sin->sin_port);
	}
	else if (sa->sa_family == AF_INET6) {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)sa;
		ip = &sin6->sin6_addr;
		iplen = 16;
		port = htons(sin6->sin6_port);
	}
	else {
		abort();
	}

	dht_helper::instance()->dht_hash(token_return, TOKEN_SIZE,
		old ? oldsecret_ : secret_, sizeof(secret_),
		ip, iplen, (unsigned char*)&port, 2);
}
int dht_net::token_match(const unsigned char *token, int token_len, const struct sockaddr *sa)
{
	unsigned char t[TOKEN_SIZE];
	if (token_len != TOKEN_SIZE)
		return 0;
	make_token(sa, 0, t);
	if (memcmp(t, token, TOKEN_SIZE) == 0)
		return 1;
	make_token(sa, 1, t);
	if (memcmp(t, token, TOKEN_SIZE) == 0)
		return 1;
	return 0;
}
/* Every bucket caches the address of a likely node.  Ping it. */
int dht_net::send_cached_ping(struct bucket *b)
{
	unsigned char tid[4];
	int rc;
	/* We set family to 0 when there's no cached node. */
	if (b->cached.ss_family == 0)
		return 0;
	dht_helper::instance()->debugf(nullptr, "Sending ping to cached node.\n");
	make_tid(tid, "pn", 0);
	rc = send_ping((struct sockaddr*)&b->cached, b->cachedlen, tid, 4);
	b->cached.ss_family = 0;
	b->cachedlen = 0;
	return rc;
}

/* Called whenever we send a request to a node, increases the ping count
and, if that reaches 3, sends a ping to a new candidate. */
void dht_net::pinged(struct node *n, struct bucket *b)
{
	n->pinged++;
	n->pinged_time = now_.tv_sec;
	if (n->pinged >= 3)
		send_cached_ping(b ? b : one_.find_bucket(n->id, n->ss.ss_family));
}

/* The internal blacklist is an LRU cache of nodes that have sent
incorrect messages. */
void dht_net::blacklist_node(const unsigned char *id, const struct sockaddr *sa, int salen)
{
	int i;
	dht_helper::instance()->debugf("Blacklisting broken node.\n");

	if (id) {
		struct node *n;
		struct search *sr;
		/* Make the node easy to discard. */
		n = one_.find_node(id, sa->sa_family);
		if (n) {
			n->pinged = 3;
			pinged(n, NULL);
		}
		/* Discard it from any searches in progress. */
		sr = one_.searches;
		while (sr) {
			for (i = 0; i < sr->numnodes; i++)
				if (dht_helper::instance()->id_cmp(sr->nodes[i].id, id) == 0)
					one_.flush_search_node(&sr->nodes[i], sr);
			sr = sr->next;
		}
	}
	/* And make sure we don't hear from it again. */
	memcpy(&one_.blacklist[one_.next_blacklisted], sa, salen);
	one_.next_blacklisted = (one_.next_blacklisted + 1) % DHT_MAX_BLACKLISTED;
}

int dht_net::node_blacklisted(const struct sockaddr *sa, int salen)
{
	int i;
	if ((unsigned)salen > sizeof(struct sockaddr_storage))
		abort();

	if (dht_blacklisted(sa, salen))
		return 1;
	for (i = 0; i < DHT_MAX_BLACKLISTED; i++) {
		if (memcmp(&one_.blacklist[i], sa, salen) == 0)
			return 1;
	}
	return 0;
}

/* Split a bucket into two equal parts. */
struct bucket *dht_net::split_bucket(struct bucket *b)
{
	struct bucket *new_bucket;
	struct node *nodes;
	int rc;
	unsigned char new_id[20];

	rc = one_.bucket_middle(b, new_id);
	if (rc < 0) return nullptr;
	new_bucket = new struct bucket;
	if (new_bucket == nullptr) return nullptr;
	memset(new_bucket,0,sizeof(struct bucket));
	new_bucket->af = b->af;

	send_cached_ping(b);

	memcpy(new_bucket->first, new_id, 20);
	new_bucket->time = b->time;

	nodes = b->nodes;
	b->nodes = NULL;
	b->count = 0;
	new_bucket->next = b->next;
	b->next = new_bucket;
	while (nodes) {
		struct node *n;
		n = nodes;
		nodes = nodes->next;
		one_.insert_node(n);
	}
	return b;
}

/* We just learnt about a node, not necessarily a new one.  Confirm is 1 if
the node sent a message, 2 if it sent us a reply. */
struct node * dht_net::new_node(const unsigned char *id,
	const struct sockaddr *sa, int salen, int confirm)
{
	struct bucket *b = one_.find_bucket(id, sa->sa_family);
	struct node *n = nullptr;
	int split;
	if (b == nullptr) return nullptr;

	if (dht_helper::instance()->id_cmp(id, one_.myid) == 0)
		return nullptr;
	if (dht_helper::instance()->is_martian(sa) || node_blacklisted(sa, salen))
		return nullptr;

	bool mybucket = one_.in_bucket(one_.myid, b);
	if (confirm == 2)
		b->time = now_.tv_sec;

	n = b->nodes;
	while (n) {
		if (dht_helper::instance()->id_cmp(n->id, id) == 0)		// 更新节点时间
		{
			if (confirm || n->time < now_.tv_sec - 15 * 60) {
				/* Known node.  Update stuff. */
				memcpy((struct sockaddr*)&n->ss, sa, salen);
				if (confirm)
					n->time = now_.tv_sec;
				if (confirm >= 2) {
					n->reply_time = now_.tv_sec;
					n->pinged = 0;
					n->pinged_time = 0;
				}
			}
			return n;
		}
		n = n->next;
	}
	/* New node. */
	if (mybucket) {
		if (sa->sa_family == AF_INET)
			one_.mybucket_grow_time = now_.tv_sec;
		else
			one_.mybucket6_grow_time = now_.tv_sec;
	}
	/* First, try to get rid of a known-bad node. */
	n = b->nodes;
	while (n) {
		if (n->pinged >= 3 && n->pinged_time < now_.tv_sec - 15) {
			memcpy(n->id, id, 20);
			memcpy((struct sockaddr*)&n->ss, sa, salen);
			n->time = confirm ? now_.tv_sec : 0;
			n->reply_time = confirm >= 2 ? now_.tv_sec : 0;
			n->pinged_time = 0;
			n->pinged = 0;
			return n;
		}
		n = n->next;
	}

	if (b->count >= 8) {
		/* Bucket full.  Ping a dubious node */
		int dubious = 0;
		n = b->nodes;
		while (n) {
			/* Pick the first dubious node that we haven't pinged in the
			last 15 seconds.  This gives nodes the time to reply, but
			tends to concentrate on the same nodes, so that we get rid
			of bad nodes fast. */
			if (!one_.node_good(n,now_)) {
				dubious = 1;
				if (n->pinged_time < now_.tv_sec - 15) {
					unsigned char tid[4];
					dht_helper::instance()->debugf("Sending ping to dubious node.\n");
					make_tid(tid, "pn", 0);
					send_ping((struct sockaddr*)&n->ss, n->sslen, tid, 4);
					n->pinged++;
					n->pinged_time = now_.tv_sec;
					break;
				}
			}
			n = n->next;
		}
		split = 0;
		if (mybucket) {
			if (!dubious)
				split = 1;
			/* If there's only one bucket, split eagerly.  This is
			incorrect unless there's more than 8 nodes in the DHT. */
			else if (b->af == AF_INET && one_.buckets->next == nullptr)
				split = 1;
			else if (b->af == AF_INET6 && one_.buckets6->next == nullptr)
				split = 1;
		}

		if (split) {
			dht_helper::instance()->debugf("Splitting.\n");
			b = split_bucket(b);
			return new_node(id, sa, salen, confirm);
		}

		/* No space for this node.  Cache it away for later. */
		if (confirm || b->cached.ss_family == 0) {
			memcpy(&b->cached, sa, salen);
			b->cachedlen = salen;
		}
		return nullptr;
	}

	/* Create a new node. */
	n = new struct node;
	if (n == nullptr) return nullptr;
	memset(n,0,sizeof(struct node));
	memcpy(n->id, id, 20);
	memcpy(&n->ss, sa, salen);
	n->sslen = salen;
	n->time = confirm ? now_.tv_sec : 0;
	n->reply_time = confirm >= 2 ? now_.tv_sec : 0;
	n->next = b->nodes;
	b->nodes = n;
	b->count++;
	return n;
}

/* Called periodically to purge known-bad nodes.  Note that we're very
conservative here: broken nodes in the table don't do much harm, we'll
recover as soon as we find better ones. */
int dht_net::expire_buckets(bool v4, struct bucket *pb)
{
	struct bucket *b = pb;
	if (!b){
		b = v4 ? one_.buckets : one_.buckets6;
	}
	while (b) {
		struct node *n, *p;
		int changed = 0;
		while (b->nodes && b->nodes->pinged >= 4) {
			n = b->nodes;
			b->nodes = n->next;
			b->count--;
			changed = 1;
			delete n;
		}
		p = b->nodes;
		while (p) {
			while (p->next && p->next->pinged >= 4) {
				n = p->next;
				p->next = n->next;
				b->count--;
				changed = 1;
				delete n;
			}
			p = p->next;
		}
		if (changed) send_cached_ping(b);
		b = b->next;
	}
	expire_stuff_time_ = now_.tv_sec + 120 + dht_helper::instance()->random() % 240;
	return 1;
}

/* This must always return 0 or 1, never -1, not even on failure (see below). */
int dht_net::search_send_get_peers(struct search *sr, struct search_node *n)
{
	struct node *node = nullptr;
	unsigned char tid[4] = {0};
	if (n == nullptr) {
		int i;
		for (i = 0; i < sr->numnodes; i++) {
			if (sr->nodes[i].pinged < 3 && !sr->nodes[i].replied &&
				sr->nodes[i].request_time < now_.tv_sec - 15)
				n = &sr->nodes[i];
		}
	}

	if (!n || n->pinged >= 3 || n->replied ||
		n->request_time >= now_.tv_sec - 15)
		return 0;

	dht_helper::instance()->debugf("Sending get_peers.\n");
	make_tid(tid, "gp", sr->tid);
	dht_helper::instance()->debugf_hex("tid:", (char*)tid, 4);
	send_get_peers((struct sockaddr*)&n->ss, n->sslen, tid, 4, sr->id, -1,
		n->reply_time >= now_.tv_sec - 15);
	n->pinged++;
	n->request_time = now_.tv_sec;
	/* If the node happens to be in our main routing table, mark it
	as pinged. */
	node = one_.find_node(n->id, n->ss.ss_family);
	if (node) pinged(node, nullptr);
	return 1;
}

/* When a search is in progress, we periodically call search_step to send
further requests. */
void dht_net::search_step(struct search *sr)
{
	int i, j;
	int all_done = 1;

	/* Check if the first 8 live nodes have replied. */
	j = 0;
	for (i = 0; i < sr->numnodes && j < 8; i++) {
		struct search_node *n = &sr->nodes[i];
		if (n->pinged >= 3)
			continue;
		if (!n->replied) {
			all_done = 0;
			break;
		}
		j++;
	}

	if (all_done) {
		if (sr->port == 0) {  // pure search
			goto done;
		}
		else {
			int all_acked = 1; j = 0;
			for (i = 0; i < sr->numnodes && j < 8; i++)
			{
				struct search_node *n = &sr->nodes[i];
				struct node *node;
				unsigned char tid[4];
				if (n->pinged >= 3)
					continue;
				/* A proposed extension to the protocol consists in
				omitting the token when storage tables are full.  While
				I don't think this makes a lot of sense -- just sending
				a positive reply is just as good --, let's deal with it. */
				if (n->token_len == 0)
					n->acked = 1;
				if (!n->acked) {
					all_acked = 0;
					dht_helper::instance()->debugf("Sending announce_peer.\n");
					make_tid(tid, "ap", sr->tid);
					send_announce_peer((struct sockaddr*)&n->ss,
						sizeof(struct sockaddr_storage),
						tid, 4, sr->id, sr->port,
						n->token, n->token_len,
						n->reply_time >= now_.tv_sec - 15);
					n->pinged++;
					n->request_time = now_.tv_sec;
					node = one_.find_node(n->id, n->ss.ss_family);
					if (node) pinged(node, nullptr);
				}
				j++;
			}
			if (all_acked)
				goto done;
		}
		sr->step_time = now_.tv_sec;
		return;
	}

	if (sr->step_time + 15 >= now_.tv_sec) return;

	j = 0;
	for (i = 0; i < sr->numnodes; i++) {
		j += search_send_get_peers(sr, &sr->nodes[i]);
		if (j >= 3)
			break;
	}
	sr->step_time = now_.tv_sec;
	return;
done:
	sr->done = 1;
	if (sr->callback)
	{
		//(*sr->callback)(sr->af == AF_INET ? DHT_EVENT_SEARCH_DONE : DHT_EVENT_SEARCH_DONE6,
		//	sr->id, NULL, 0, sr->user_data);
		std::string tmp((char*)sr->values, 6);
		tmp.append((char*)sr->closet_key, DHT_KEY_HASH_SIZE);
		tmp.append((char*)sr->id, DHT_KEY_HASH_SIZE);
		(*sr->callback)(sr->af == AF_INET ? DHT_EVENT_SEARCH_DONE : DHT_EVENT_SEARCH_DONE6,
			sr->id, (const void*)tmp.c_str(), tmp.length(), sr->user_data);
	}
	sr->step_time = now_.tv_sec;
}

/* Start a search.  If port is non-zero, perform an announce when the
search is complete. */
int dht_net::dht_search(const unsigned char *id, int port, int af,
	dht_callback *callback, long long user_data)
{
	struct search *sr;
	//struct storage *st;
	struct bucket *b = one_.find_bucket(id, af);

	if (b == NULL) {
		errno = EAFNOSUPPORT;
		return -1;
	}

	/* Try to answer this search locally.  In a fully grown DHT this
	is very unlikely, but people are running modified versions of
	this code in private DHTs with very few nodes.  What's wrong
	with flooding? */
	//if (callback) {
	//	//st = one_.find_storage(id);
	//	//if (st) {
	//	//	unsigned short swapped;
	//	//	unsigned char buf[18];
	//	//	int i;
	//	//	dht_helper::instance()->debugf("Found local data (%d peers).\n", st->numpeers);
	//	//	for (i = 0; i < st->numpeers; i++)
	//	//	{
	//	//		swapped = htons(st->peers[i].port);
	//	//		if (st->peers[i].len == 4) {
	//	//			memcpy(buf, st->peers[i].ip, 4);
	//	//			memcpy(buf + 4, &swapped, 2);
	//	//			(*callback)(DHT_EVENT_VALUES, id,
	//	//				(void*)buf, 6, user_data);
	//	//		}
	//	//		else if (st->peers[i].len == 16) {
	//	//			memcpy(buf, st->peers[i].ip, 16);
	//	//			memcpy(buf + 16, &swapped, 2);
	//	//			(*callback)(DHT_EVENT_VALUES6, id,
	//	//				(void*)buf, 18, user_data);
	//	//		}
	//	//	}
	//	//}
	//}
	// 查看是否在搜索列表中
	sr = one_.searches;
	while (sr) {
		if (sr->af == af && dht_helper::instance()->id_cmp(sr->id, id) == 0)
			break;
		sr = sr->next;
	}

	if (sr)
	{
		if (sr->done)
		{
			/* We're reusing data from an old search.  Reusing the same tid
			means that we can merge replies for both searches. */
			int i;
			sr->done = 0;
		again:
			for (i = 0; i < sr->numnodes; i++) {
				struct search_node *n;
				n = &sr->nodes[i];
				/* Discard any doubtful nodes. */
				if (n->pinged >= 3 || n->reply_time < now_.tv_sec - 7200) {
					one_.flush_search_node(n, sr);
					goto again;
				}
				n->pinged = 0;
				n->token_len = 0;
				n->replied = 0;
				n->acked = 0;
			}
		}
		else
			return -1;///Reject other requests in searching
	}
	else
	{
		sr = one_.new_search(now_);
		if (sr == NULL) {
			errno = ENOSPC;
			return -1;
		}
		sr->af = af;
		sr->tid = one_.search_id++;
		sr->step_time = 0;
		memcpy(sr->id, id, DHT_KEY_HASH_SIZE);
		sr->done = 0;
		sr->numnodes = 0;
		sr->user_data = user_data;
	}
	sr->port = port;
	sr->callback = callback;
	memcpy(sr->closet_key,one_.get_myid(),DHT_KEY_HASH_SIZE);
	memset(sr->values,0,sizeof(sr->values));
	one_.insert_search_bucket(b, sr,now_);
	if (sr->numnodes < SEARCH_NODES) {
		struct bucket *p = one_.previous_bucket(b);
		if (b->next)
			one_.insert_search_bucket(b->next, sr,now_);
		if (p)
			one_.insert_search_bucket(p, sr,now_);
	}
	if (sr->numnodes < SEARCH_NODES)
		one_.insert_search_bucket(one_.find_bucket(one_.myid, af), sr,now_);
	if (sr->numnodes <= 0) {	// 单节点时直接返回
		sr->done = 1;	sr->step_time = now_.tv_sec;
		return -1;
	}
	search_step(sr);
	search_time_ = now_.tv_sec;
	return 1;
}

int dht_net::dht_init(int s, int s6, const unsigned char *id, const unsigned char *v, FILE* df)
{
	dht_helper::instance()->dht_gettimeofday(&now_, NULL);
	if (one_.init(s, s6, v, id, now_, df))
	{
		dht_socket_ = s; dht_socket6_ = s6;
		token_bucket_time_ = now_.tv_sec;
		confirm_nodes_time_ = now_.tv_sec + dht_helper::instance()->random() % 3;
		broadcast_maintenance_time_ = now_.tv_sec;
		rotate_secrets();
		expire_buckets();
		expire_buckets(false);
		return 1;
	}
	return -1;
}

int dht_net::dht_uninit()
{
	if (dht_socket_ < 0 && dht_socket6_ < 0) {
		errno = EINVAL;
		return false;
	}
	dht_socket_ = 0;
	dht_socket6_ = 0;
	return one_.finit();
}
// 维护时id的最后一字节为FF，选择与myid最近的一个桶；
// 对桶中随机选择的节点发送findnode命令
int dht_net::neighbourhood_maintenance(int af)
{
	unsigned char id[20] = {0};
	struct bucket *b = one_.find_bucket(one_.myid, af);
	struct bucket *q = nullptr;
	struct node *n = nullptr;

	if (b == nullptr) return 0;

	memcpy(id, one_.myid, 20);
	id[19] = dht_helper::instance()->random() & 0xFF;
	q = b;
	if (q->next && (q->count == 0 || (dht_helper::instance()->random() & 7) == 0))
		q = b->next;
	if (q->count == 0 || (dht_helper::instance()->random() & 7) == 0) {
		struct bucket *r;
		r = one_.previous_bucket(b);
		if (r && r->count > 0)
			q = r;
	}

	if (q)
	{
		/* Since our node-id is the same in both DHTs, it's probably
		profitable to query both families. */
		int want = dht_socket_ >= 0 && dht_socket6_ >= 0 ? (WANT4 | WANT6) : -1;
		n = one_.random_node(q);
		if (n) {
			unsigned char tid[4];
			dht_helper::instance()->debugf("Sending find_node for%s neighborhood maintenance.\n",
				af == AF_INET6 ? " IPv6" : "");
			make_tid(tid, "fn", 0);
			send_find_node((struct sockaddr*)&n->ss, n->sslen,
				tid, 4, id, want,
				n->reply_time >= now_.tv_sec - 15);
			pinged(n, q);
		}
		return 1;
	}
	return 0;
}
int dht_net::bucket_maintenance(int af)
{
	struct bucket *b = nullptr;
	b = af == AF_INET ? one_.buckets : one_.buckets6;

	while (b) {
		struct bucket *q;
		if (b->time < now_.tv_sec - 600)
		{
			/* This bucket hasn't seen any positive confirmation for a long
			time.  Pick a random id in this bucket's range, and send
			a request to a random node. */
			unsigned char id[20];
			struct node *n;
			int rc;

			rc = one_.bucket_random(b, id);
			if (rc < 0)
				memcpy(id, b->first, 20);

			q = b;
			/* If the bucket is empty, we try to fill it from a neighbour.
			We also sometimes do it gratuitiously to recover from
			buckets full of broken nodes. */
			if (q->next && (q->count == 0 || (dht_helper::instance()->random() & 7) == 0))
				q = b->next;
			if (q->count == 0 || (dht_helper::instance()->random() & 7) == 0) {
				struct bucket *r;
				r = one_.previous_bucket(b);
				if (r && r->count > 0)
					q = r;
			}

			if (q) {
				n = one_.random_node(q);
				if (n) {
					unsigned char tid[4];
					int want = -1;

					if (dht_socket_ >= 0 && dht_socket6_ >= 0) {
						struct bucket *otherbucket;
						otherbucket =
							one_.find_bucket(id, af == AF_INET ? AF_INET6 : AF_INET);
						if (otherbucket && otherbucket->count < 8)
							/* The corresponding bucket in the other family
							is emptyish -- querying both is useful. */
							want = WANT4 | WANT6;
						else if (dht_helper::instance()->random() % 37 == 0)
							/* Most of the time, this just adds overhead.
							However, it might help stitch back one of
							the DHTs after a network collapse, so query
							both, but only very occasionally. */
							want = WANT4 | WANT6;
					}
					dht_helper::instance()->debugf("Sending find_node for%s bucket maintenance.\n",
						af == AF_INET6 ? " IPv6" : "");
					make_tid(tid, "fn", 0);
					send_find_node((struct sockaddr*)&n->ss, n->sslen,
						tid, 4, id, want,
						n->reply_time >= now_.tv_sec - 15);
					pinged(n, q);
					/* In order to avoid sending queries back-to-back,
					give up for now and reschedule us soon. */
					return 1;
				}
			}
		}
		b = b->next;
	}
	return 0;
}

int dht_net::dht_periodic(const void *buf, size_t buflen, const struct sockaddr *from,
	int fromlen, time_t *tosleep, dht_callback *callback)
{
	dht_helper::instance()->dht_gettimeofday(&now_, NULL);
	if (buflen > 0) {
		if (dht_helper::instance()->is_martian(from))
			goto dontread;
		if (node_blacklisted(from, fromlen)) {
			dht_helper::instance()->debugf("Received packet from blacklisted node.\n");
			goto dontread;
		}
		if (-1 == parse_and_handle_message((const unsigned char*)buf, buflen, from, fromlen))
		{
			dht_helper::instance()->debugf("Unparseable message: ");
			dht_helper::instance()->debug_printable((const unsigned char*)buf, buflen);
			dht_helper::instance()->debugf("\n");
		}
	}

dontread:
	if (now_.tv_sec >= rotate_secrets_time_)
		rotate_secrets();

	if (now_.tv_sec >= expire_stuff_time_) {
		expire_buckets();
		expire_buckets(false);
		one_.expire_storage(now_);
		one_.expire_searches(now_);
	}
	if (search_time_ > 0 && now_.tv_sec >= search_time_)
	{
		struct search *sr;
		sr = one_.searches;
		while (sr)
		{
			if (!sr->done && sr->step_time + 5 <= now_.tv_sec) {
				search_step(sr);
			}
			sr = sr->next;
		}
		search_time_ = 0;
		sr = one_.searches;
		while (sr)
		{
			if (!sr->done) {
				time_t tm = sr->step_time + 15 + dht_helper::instance()->random() % 10;
				if (search_time_ == 0 || search_time_ > tm)
					search_time_ = tm;
			}
			sr = sr->next;
		}
	}
	if (now_.tv_sec >= confirm_nodes_time_)
	{
		int soon = 0;
		soon |= bucket_maintenance(AF_INET);
		soon |= bucket_maintenance(AF_INET6);

		if (!soon) {
			if (one_.mybucket_grow_time >= now_.tv_sec - 150)
				soon |= neighbourhood_maintenance(AF_INET);
			if (one_.mybucket6_grow_time >= now_.tv_sec - 150)
				soon |= neighbourhood_maintenance(AF_INET6);
		}
		/* In order to maintain all buckets' age within 600 seconds, worst
		case is roughly 27 seconds, assuming the table is 22 bits deep.
		We want to keep a margin for neighborhood maintenance, so keep
		this within 25 seconds. */
		if (soon)
			confirm_nodes_time_ = now_.tv_sec + 5 + dht_helper::instance()->random() % 20;
		else
			confirm_nodes_time_ = now_.tv_sec + 60 + dht_helper::instance()->random() % 120;
	}
	if (confirm_nodes_time_ > now_.tv_sec)
		*tosleep = confirm_nodes_time_ - now_.tv_sec;
	else
		*tosleep = 0;
	if (search_time_ > 0)
	{
		if (search_time_ <= now_.tv_sec)
			*tosleep = 0;
		else if (*tosleep > search_time_ - now_.tv_sec)
			*tosleep = search_time_ - now_.tv_sec;
	}
	if (now_.tv_sec - broadcast_maintenance_time_ > 10)
	{
		broadcast_maintenance_time_ = now_.tv_sec;
		bd_data_maintenance();
	}
	return 1;
}
bool dht_net::dht_insert_node(const unsigned char *id, struct sockaddr *sa, int salen)
{
	struct node *n;
	if (sa->sa_family != AF_INET) {
		errno = EAFNOSUPPORT;
		return false;
	}
	n = new_node(id, (struct sockaddr*)sa, salen, 0);
	return (n != nullptr);
}
int dht_net::dht_ping_node(const struct sockaddr *sa, int salen)
{
	unsigned char tid[4];

	dht_helper::instance()->debugf("Sending ping.\n");
	make_tid(tid, "pn", 0);
	return send_ping(sa, salen, tid, 4);
}
int dht_net::dht_send(const void *buf, size_t len, int flags, const struct sockaddr *sa, int salen)
{
	if (salen == 0)
		abort();
	if (node_blacklisted(sa, salen)) {
		dht_helper::instance()->debugf("Attempting to send to blacklisted node.\n");
		errno = EPERM;
		return -1;
	}
	int s;
	if (salen == 0) abort();
	if (sa->sa_family == AF_INET)
		s = dht_socket_;
	else if (sa->sa_family == AF_INET6)
		s = dht_socket6_;
	else
		s = -1;
	if (s < 0) {
		errno = EAFNOSUPPORT;
		return -1;
	}
	return sendto(s, (const char*)buf, len, flags, sa, salen);
}
int dht_net::send_ping(const struct sockaddr *sa, int salen,const unsigned char *tid, int tid_len)
{
	dht_msg_common msg;
	msg.set_my_id(std::string((const char*)one_.myid, DHT_KEY_HASH_SIZE));
	msg.set_targer_id(std::string((const char*)tid,tid_len));
	if (one_.have_v)
		msg.set_data_v(std::string((const char*)one_.my_v, sizeof(one_.my_v)));
	dht_msg_head msg_head;
	std::string buf;
	MAKE_DHT_MESSAGE(DHT_METHOD_ID_PING, DHT_MESSAGE_REQUEST,msg,msg_head,buf);
	return dht_send((const void*)buf.c_str(), buf.length(), 0, sa, salen);
}
int dht_net::send_pong(const struct sockaddr *sa, int salen,const unsigned char *tid, int tid_len)
{
	dht_msg_common msg;
	msg.set_my_id(std::string((const char*)one_.myid, DHT_KEY_HASH_SIZE));
	msg.set_targer_id(std::string((const char*)tid, tid_len));
	if (one_.have_v)
		msg.set_data_v(std::string((const char*)one_.my_v, sizeof(one_.my_v)));
	dht_msg_head msg_head;
	std::string buf;
	MAKE_DHT_MESSAGE(DHT_METHOD_ID_PONG, DHT_MESSAGE_REPLY, msg, msg_head,buf);
	return dht_send((const void*)buf.c_str(), buf.length(), 0, sa, salen);
}
int dht_net::send_find_node(const struct sockaddr *sa, int salen, const unsigned char *tid,
	int tid_len,const unsigned char *target, int want, int confirm)
{
	dht_msg_find_node msg;
	msg.mutable_msg_com()->set_my_id(std::string((const char*)one_.myid, DHT_KEY_HASH_SIZE));
	msg.mutable_msg_com()->set_targer_id(std::string((const char*)tid, tid_len));
	if (one_.have_v)
		msg.mutable_msg_com()->set_data_v(std::string((const char*)one_.my_v, sizeof(one_.my_v)));
	msg.set_target_hash(std::string((const char*)target, DHT_KEY_HASH_SIZE));
	msg.set_want(want);
	dht_msg_head msg_head;
	std::string buf;
	MAKE_DHT_MESSAGE(DHT_METHOD_ID_FIND_NODE, DHT_MESSAGE_REQUEST, msg, msg_head,buf);
	return dht_send((const void*)buf.c_str(), buf.length(), confirm ? MSG_CONFIRM : 0, sa, salen);
}
int dht_net::send_nodes_peers(const struct sockaddr *sa, int salen,const unsigned char *tid, int tid_len,
	const unsigned char *nodes, int nodes_len,const unsigned char *nodes6, int nodes6_len,
	int af, struct storage *st,const unsigned char *token, int token_len)
{
	char values[4096] = { 0 }; int pos = 0;
	dht_msg_closest_peer msg;
	msg.mutable_msg_com()->set_my_id(std::string((const char*)one_.myid, DHT_KEY_HASH_SIZE));
	msg.mutable_msg_com()->set_targer_id(std::string((const char*)tid, tid_len));
	if (one_.have_v)
		msg.mutable_msg_com()->set_data_v(std::string((const char*)one_.my_v, sizeof(one_.my_v)));
	if (nodes_len > 0)
		msg.set_nodes4(std::string((const char*)nodes, nodes_len));
	if (nodes6_len > 0)
		msg.set_nodes4(std::string((const char*)nodes6, nodes6_len));
	if (st && st->numpeers > 0)
	{
		/* We treat the storage as a circular list, and serve a randomly
		chosen slice.  In order to make sure we fit within 1024 octets,
		we limit ourselves to 50 peers. */
		short len = af == AF_INET ? 4 : 16;
		int j0 = dht_helper::instance()->random() % st->numpeers;
		int j = j0,	k = 0;
		do {
			if (st->peers[j].len == len) {
				unsigned short swapped;
				swapped = htons(st->peers[j].port);
				memcpy(values + pos, st->peers[j].ip,len);
				memcpy(values+pos+len,&swapped,2);
				pos += (len+2);
				k++;
			}
			j = (j + 1) % st->numpeers;
		} while (j != j0 && k < 50);
	}
	msg.set_token(std::string((const char*)token, token_len));
	if (af == AF_INET)
		msg.set_values(std::string((const char*)values, pos));
	else
		msg.set_values6(std::string((const char*)values, pos));
	dht_msg_head msg_head;
	std::string allbuf;
	MAKE_DHT_MESSAGE(DHT_METHOD_ID_CLOSEST_NODE, DHT_MESSAGE_REPLY, msg, msg_head, allbuf);
	return dht_send((const void*)allbuf.c_str(), allbuf.length(), 0, sa, salen);
}

int dht_net::send_closest_nodes(const struct sockaddr *sa, int salen,
	const unsigned char *tid, int tid_len, const unsigned char *id, int want,
	int af, struct storage *st, const unsigned char *token, int token_len)
{
	unsigned char nodes[8 * 26] = {0};
	unsigned char nodes6[8 * 38] = {0};
	int numnodes = 8, numnodes6 = 8;
	if (want < 0)
		want = sa->sa_family == AF_INET ? WANT4 : WANT6;
	one_.make_closest_nodes(id, want, nodes, numnodes, nodes6, numnodes6,now_);
	return send_nodes_peers(sa, salen, tid, tid_len,
		nodes, numnodes * 26,
		nodes6, numnodes6 * 38,
		af, st, token, token_len);
}

int dht_net::send_get_peers(const struct sockaddr *sa, int salen, unsigned char *tid,
	int tid_len, unsigned char *infohash,int want, int confirm)
{
	dht_msg_get_peer msg;
	msg.mutable_msg_com()->set_my_id(std::string((const char*)one_.myid, DHT_KEY_HASH_SIZE));
	msg.mutable_msg_com()->set_targer_id(std::string((const char*)tid, tid_len));
	if (one_.have_v)
		msg.mutable_msg_com()->set_data_v(std::string((const char*)one_.my_v, sizeof(one_.my_v)));
	msg.set_hash_info(std::string((const char*)infohash, DHT_KEY_HASH_SIZE));
	msg.set_want(want);
	dht_msg_head msg_head;
	std::string allbuf;
	MAKE_DHT_MESSAGE(DHT_METHOD_ID_GET_PEER, DHT_MESSAGE_REQUEST, msg, msg_head, allbuf);
	return dht_send((const void*)allbuf.c_str(), allbuf.length(), confirm ? MSG_CONFIRM : 0, sa, salen);
}
int dht_net::send_announce_peer(const struct sockaddr *sa, int salen,
	unsigned char *tid, int tid_len,
	unsigned char *infohash, unsigned short port,
	unsigned char *token, int token_len, int confirm)
{
	dht_msg_announced_peer msg;
	msg.mutable_msg_com()->set_my_id(std::string((const char*)one_.myid, DHT_KEY_HASH_SIZE));
	msg.mutable_msg_com()->set_targer_id(std::string((const char*)tid, tid_len));
	if (one_.have_v)
		msg.mutable_msg_com()->set_data_v(std::string((const char*)one_.my_v, sizeof(one_.my_v)));
	msg.set_hash_info(std::string((const char*)infohash, DHT_KEY_HASH_SIZE));
	msg.set_token(std::string((const char*)token, token_len));
	msg.set_port(port);
	dht_msg_head msg_head;
	std::string allbuf;
	MAKE_DHT_MESSAGE(DHT_METHOD_ID_ANNOUNCE_PEER, DHT_MESSAGE_REQUEST, msg, msg_head, allbuf);
	return dht_send((const void*)allbuf.c_str(), allbuf.length(), confirm ? 0 : MSG_CONFIRM, sa, salen);
}

int dht_net::send_peer_announced(const struct sockaddr *sa, int salen,
	const unsigned char *tid, int tid_len)
{
	dht_msg_common msg;
	msg.set_my_id(std::string((const char*)one_.myid, DHT_KEY_HASH_SIZE));
	msg.set_targer_id(std::string((const char*)tid, tid_len));
	if (one_.have_v)
		msg.set_data_v(std::string((const char*)one_.my_v, sizeof(one_.my_v)));
	dht_msg_head msg_head;
	std::string allbuf;
	MAKE_DHT_MESSAGE(DHT_METHOD_ID_ANNOUNCE_REPLY, DHT_MESSAGE_REPLY, msg, msg_head, allbuf);
	return dht_send((const void*)allbuf.c_str(), allbuf.length(), 0, sa, salen);
}
int dht_net::send_error(const struct sockaddr *sa, int salen, const unsigned char *tid,
	 int tid_len,int code, const char *message)
{
	dht_msg_error msg;
	msg.mutable_msg_com()->set_my_id(std::string((const char*)one_.myid, DHT_KEY_HASH_SIZE));
	msg.mutable_msg_com()->set_targer_id(std::string((const char*)tid, tid_len));
	if (one_.have_v)
		msg.mutable_msg_com()->set_data_v(std::string((const char*)one_.my_v, sizeof(one_.my_v)));
	msg.set_code(code);
	msg.set_err_msg(message);
	dht_msg_head msg_head;
	std::string allbuf;
	MAKE_DHT_MESSAGE(DHT_METHOD_ID_ERROR, DHT_MESSAGE_ERROR, msg, msg_head, allbuf);
	return dht_send((const void*)allbuf.c_str(), allbuf.length(), 0, sa, salen);
}
int dht_net::dht_blacklisted(const struct sockaddr *sa, int salen)
{
	return 0;
}
bool dht_net::dht_orign_key_pki(const unsigned char* key, const struct sockaddr *sa)
{
	//if (sa->sa_family == AF_INET)
	//{
	//	unsigned char *ip;
	//	struct sockaddr_in *sin = (struct sockaddr_in*)sa;
	//	ip = (unsigned char*)&sin->sin_addr;
	//	return true;
	//}
	return true;
}
// -1--wrong message;0--skip request command; 1--success handle
int dht_net::parse_and_handle_message(const unsigned char *buf, int buflen,
	const struct sockaddr *from, int fromlen)
{
	if (!buf && buflen < DHT_MESSAGE_HEAD_SIZE) return -1;
	if (0 != memcmp((const void*)buf, (const void*)DHT_MESSAGE_PREX, 4))
	{
		dht_helper::instance()->debugf("wrong message head!\n");
		return -1;
	}
	int head_len = 0; memcpy(&head_len,(const void*)(buf+4),4);
	if (head_len > buflen) return -1;
	dht_msg_head head; buf += 8;
	head.ParseFromArray((const void*)buf, head_len);
	if (head.len() + head_len + 8 > buflen)
	{
		dht_helper::instance()->debugf("Truncated message.[buflen=%d,head.len=%d]\n",
			buflen,head.len());
		return -1;
	}
	if (GETMTYPE(head.msg_id()) == DHT_MESSAGE_REQUEST)
	{
		/* Rate limit requests. */
		if (!token_bucket()) {
			dht_helper::instance()->debugf("Dropping request due to rate limiting.\n");
			return 0;
		}
	}
	bool ret = 0;
	switch (GETMID(head.msg_id()))
	{
	case DHT_METHOD_ID_PING:
		ret = handle_ping_msg(buf, buflen, from, fromlen,head);
		break;
	case DHT_METHOD_ID_PONG:
		ret = handle_pong_msg(buf, buflen, from, fromlen, head);
		break;
	case DHT_METHOD_ID_FIND_NODE:
		ret = handle_find_node_msg(buf, buflen, from, fromlen, head);
		break;
	case DHT_METHOD_ID_CLOSEST_NODE:
		ret = handle_closest_node_msg(buf, buflen, from, fromlen, head);
		break;
	case DHT_METHOD_ID_GET_PEER:
		ret = handle_get_peer_msg(buf, buflen, from, fromlen, head);
		break;
	case DHT_METHOD_ID_ANNOUNCE_PEER:
		ret = handle_announce_peer_msg(buf, buflen, from, fromlen, head);
		break;
	case DHT_METHOD_ID_ANNOUNCE_REPLY:
		ret = handle_announce_reply_msg(buf, buflen, from, fromlen, head);
		break;
	case DHT_METHOD_ID_ERROR:
		ret = handle_error_msg(buf, buflen, from, fromlen, head);
		break;
	case DHT_METHOD_ID_BD_KEY:
	case DHT_METHOD_ID_BD_DATA:
	case DHT_METHOD_ID_BD_KEY_REPLY:
		ret = handle_broadcast_msg_key(buf,buflen,from,fromlen,head);
		break;
	case DHT_METHOD_ID_NULL:
	default:
		dht_helper::instance()->debugf("unknown method id=%d \n", GETMID(head.msg_id()));
		return -1;
	}
	return ret? 1 : -1;
}
bool dht_net::id_check(std::string id)
{
	if (dht_helper::instance()->zero_key((const unsigned char*)id.c_str())){
		dht_helper::instance()->debugf("Zero key.\n");
		return false;
	}
	if (dht_helper::instance()->id_cmp((const unsigned char*)id.c_str(), one_.myid) == 0) {
		dht_helper::instance()->debugf("Received message from self.\n");
		return false;
	}
	return true;
}
bool dht_net::handle_ping_msg(const unsigned char *buf, int buflen, const struct sockaddr *from, int fromlen, dht_msg_head &head)
{
	dht_msg_common msg;
	msg.ParseFromArray(buf + head.ByteSizeLong(), head.len());
	if (GETMTYPE(head.msg_id()) != DHT_MESSAGE_REQUEST) return false;
	if (!id_check(msg.my_id())) return true;
	dht_helper::instance()->debugf("Ping (%d)!\n", msg.targer_id().length());
	new_node((const unsigned char*)msg.my_id().c_str(), from, fromlen, 1);
	dht_helper::instance()->debugf("Sending pong.\n");
	send_pong(from, fromlen, (const unsigned char*)msg.targer_id().c_str(), msg.targer_id().length());
	return true;
}
bool dht_net::handle_pong_msg(const unsigned char *buf, int buflen, const struct sockaddr *from, int fromlen, dht_msg_head &head)
{
	dht_msg_common msg;
	msg.ParseFromArray(buf + head.ByteSizeLong(), head.len());
	if (GETMTYPE(head.msg_id()) != DHT_MESSAGE_REPLY) return false;
	if (!id_check(msg.my_id())) return true;

	if (4 != msg.targer_id().length())
	{
		dht_helper::instance()->debugf("Broken node truncates transaction ids\n");
		/* This is really annoying, as it means that we will
		time-out all our searches that go through this node.
		Kill it. */
		blacklist_node((const unsigned char*)msg.my_id().c_str(), from, fromlen);
		return false;
	}
	new_node((const unsigned char*)msg.my_id().c_str(), from, fromlen, 1);
	dht_helper::instance()->debugf("pong!\n");
	return true;
}
bool dht_net::handle_find_node_msg(const unsigned char *buf, int buflen, const struct sockaddr *from, int fromlen, dht_msg_head &head)
{
	// find node命令以closest node命令作为响应
	dht_msg_find_node msg;
	msg.ParseFromArray(buf + head.ByteSizeLong(), head.len());
	if (DHT_MESSAGE_REQUEST != GETMTYPE(head.msg_id())) return false;
	if (!id_check(msg.msg_com().my_id())) return true;

	dht_helper::instance()->debugf("Find node!\n");
	new_node((const unsigned char*)msg.msg_com().my_id().c_str(), from, fromlen, 1);
	dht_helper::instance()->debugf("Sending closest nodes (%d).\n", msg.want());
	send_closest_nodes(from, fromlen, (const unsigned char*)msg.msg_com().targer_id().c_str(),
		msg.msg_com().targer_id().length(), (const unsigned char*)msg.target_hash().c_str(),
		msg.want(),0, NULL, NULL, 0);
	return true;
}
bool dht_net::handle_get_peer_msg(const unsigned char *buf, int buflen, const struct sockaddr *from, int fromlen, dht_msg_head &head)
{
	// get peer命令以closest node命令作为响应
	dht_msg_get_peer msg;
	msg.ParseFromArray(buf + head.ByteSizeLong(), head.len());
	if (DHT_MESSAGE_REQUEST != GETMTYPE(head.msg_id())) return false;
	if (!id_check(msg.msg_com().my_id())) return true;
	dht_helper::instance()->debugf("Get_peers!\n");
	dht_helper::instance()->debugf_hex("tid:", msg.msg_com().targer_id().c_str(), msg.msg_com().targer_id().length());
	new_node((const unsigned char*)msg.msg_com().my_id().c_str(), from, fromlen, 1);
	if (dht_helper::instance()->zero_key((const unsigned char*)msg.hash_info().c_str())) {
		dht_helper::instance()->debugf("Eek!  Got get_peers with no info_hash.\n");
		send_error(from, fromlen, (const unsigned char*)msg.msg_com().targer_id().c_str(), 
			msg.msg_com().targer_id().length(),203, "Get_peers with no info_hash");
	}
	else {
		struct storage *st = one_.find_storage((const unsigned char*)msg.hash_info().c_str());
		unsigned char token[TOKEN_SIZE] = {0};
		make_token(from, 0, token);
		if (st && st->numpeers > 0) {
			dht_helper::instance()->debugf("Sending found %s peers.\n",
				from->sa_family == AF_INET6 ? " IPv6" : "");
			send_closest_nodes(from, fromlen, (const unsigned char*)msg.msg_com().targer_id().c_str(),
				msg.msg_com().targer_id().length(), (const unsigned char*)msg.hash_info().c_str(), 
				msg.want(),from->sa_family, st,token, TOKEN_SIZE);
		}
		else {
			dht_helper::instance()->debugf("Sending nodes for get_peers.\n");
			send_closest_nodes(from, fromlen, (const unsigned char*)msg.msg_com().targer_id().c_str(),
				msg.msg_com().targer_id().length(), (const unsigned char*)msg.hash_info().c_str(), 
				msg.want(),0, NULL, token, TOKEN_SIZE);
		}
	}
	return true;
}
bool dht_net::handle_closest_node_msg(const unsigned char *buf, int buflen, const struct sockaddr *from, int fromlen, dht_msg_head &head)
{
	// 作为find node和get peer命令的响应命令
	dht_msg_closest_peer msg;
	msg.ParseFromArray(buf + head.ByteSizeLong(), head.len());
	if (DHT_MESSAGE_REPLY != GETMTYPE(head.msg_id())) return false;
	if (!id_check(msg.msg_com().my_id())) return true;
	int gp = 0, nodes_len = msg.nodes4().length(), nodes6_len = msg.nodes6().length();
	unsigned short ttid = 0; struct search *sr = nullptr;
	const unsigned char *nodes = (const unsigned char*)msg.nodes4().c_str();
	const unsigned char *nodes6 = (const unsigned char*)msg.nodes6().c_str();
	int values_len = msg.values().length();
	int values6_len = msg.values6().length();

	if (tid_match((const unsigned char*)msg.msg_com().targer_id().c_str(), "gp", &ttid)) {
		gp = 1;
		sr = one_.find_search(ttid, from->sa_family);
	}
	dht_helper::instance()->debugf("Nodes found (%d+%d)%s!\n", nodes_len / 26, nodes6_len / 38,
		gp ? " for get_peers" : "");
	if (nodes_len % 26 != 0 || nodes6_len % 38 != 0) {
		dht_helper::instance()->debugf("Unexpected length for node info!\n");
		blacklist_node((const unsigned char*)msg.msg_com().my_id().c_str(), from, fromlen);
	}
	else if (gp && sr == NULL) {
		dht_helper::instance()->debugf("Unknown search!\n");
		new_node((const unsigned char*)msg.msg_com().my_id().c_str(), from, fromlen, 1);
	}
	else 
	{
		new_node((const unsigned char*)msg.msg_com().my_id().c_str(), from, fromlen, 2);
		for (int i = 0; i < nodes_len / 26; i++) 
		{
			const unsigned char *ni = nodes + i * 26;
			struct sockaddr_in sin;
			if (dht_helper::instance()->id_cmp(ni, one_.myid) == 0)
				continue;
			memset(&sin, 0, sizeof(sin));
			sin.sin_family = AF_INET;
			memcpy(&sin.sin_addr, ni + 20, 4);
			memcpy(&sin.sin_port, ni + 24, 2);
			new_node(ni, (struct sockaddr*)&sin, sizeof(sin), 0);
			if (sr && sr->af == AF_INET) {
				one_.insert_search_node(ni,(struct sockaddr*)&sin,
					sizeof(sin),sr, 0, NULL, 0,now_);
			}
		}
		for (int i = 0; i < nodes6_len / 38; i++) 
		{
			const unsigned char *ni = nodes6 + i * 38;
			struct sockaddr_in6 sin6;
			if (dht_helper::instance()->id_cmp(ni, one_.myid) == 0)
				continue;
			memset(&sin6, 0, sizeof(sin6));
			sin6.sin6_family = AF_INET6;
			memcpy(&sin6.sin6_addr, ni + 20, 16);
			memcpy(&sin6.sin6_port, ni + 36, 2);
			new_node(ni, (struct sockaddr*)&sin6, sizeof(sin6), 0);
			if (sr && sr->af == AF_INET6) {
				one_.insert_search_node(ni,(struct sockaddr*)&sin6,
					sizeof(sin6),sr, 0, NULL, 0,now_);
			}
		}
		if (sr)
			/* Since we received a reply, the number of
			requests in flight has decreased.  Let's push
			another request. */
			search_send_get_peers(sr, NULL);
	}
	if (sr) 
	{
		one_.insert_search_node((const unsigned char*)msg.msg_com().my_id().c_str(), from, fromlen,
			 sr, 1, (const unsigned char*)msg.token().c_str(), msg.token().length(),now_);
		if (dht_helper::instance()->xorcmp(sr->closet_key,
			(const unsigned char*)msg.msg_com().my_id().c_str(), sr->id) >= 0)
		{
			memcpy(sr->closet_key, msg.msg_com().my_id().c_str(), DHT_KEY_HASH_SIZE);
			int node_ip = ((struct sockaddr_in*)from)->sin_addr.s_addr;
			short node_port = ((struct sockaddr_in*)from)->sin_port;
			memcpy(sr->values,&node_ip,sizeof(node_ip));
			memcpy(sr->values+sizeof(node_ip),&node_port,sizeof(node_port));
		}
		//if (values_len > 0 || values6_len > 0) {
		//	dht_helper::instance()->debugf("Got values (%d+%d)!\n",
		//		values_len / 6, values6_len / 18);
		//	//if (sr->callback) {
		//	//	if (values_len > 0)
		//	//		(*sr->callback)(DHT_EVENT_VALUES, sr->id,
		//	//		(const void*)msg.values().c_str(), values_len, sr->user_data);
		//	//	if (values6_len > 0)
		//	//		(*sr->callback)(DHT_EVENT_VALUES6, sr->id,
		//	//		(const void*)msg.values6().c_str(), values6_len, sr->user_data);
		//	//}
		//}
	}
	return true;
}
bool dht_net::handle_announce_peer_msg(const unsigned char *buf, int buflen, const struct sockaddr *from, int fromlen, dht_msg_head &head)
{
	dht_msg_announced_peer msg;
	msg.ParseFromArray(buf + head.ByteSizeLong(), head.len());
	if (DHT_MESSAGE_REQUEST != GETMTYPE(head.msg_id())) return false;
	if (!id_check(msg.msg_com().my_id())) return true;
	dht_helper::instance()->debugf("Announce peer!\n");
	new_node((const unsigned char*)msg.msg_com().my_id().c_str(), from, fromlen, 1);
	if (dht_helper::instance()->zero_key((const unsigned char*)msg.hash_info().c_str())) 
	{
		dht_helper::instance()->debugf("Announce_peer with no info_hash.\n");
		send_error(from, fromlen, (const unsigned char*)msg.msg_com().targer_id().c_str(), 
			msg.msg_com().targer_id().length(),203, "Announce_peer with no info_hash");
		return false;
	}
	if (!token_match((const unsigned char*)msg.token().c_str(), msg.token().length(), from))
	{
		dht_helper::instance()->debugf("Incorrect token for announce_peer.\n");
		send_error(from, fromlen, (const unsigned char*)msg.msg_com().targer_id().c_str(), 
			msg.msg_com().targer_id().length(),203, "Announce_peer with wrong token");
		return false;
	}
	if (msg.port() == 0) {
		dht_helper::instance()->debugf("Announce_peer with forbidden port %d.\n", msg.port());
		send_error(from, fromlen, (const unsigned char*)msg.msg_com().targer_id().c_str(), 
			msg.msg_com().targer_id().length(),203, "Announce_peer with forbidden port number");
		return false;
	}
	if (!dht_orign_key_pki((const unsigned char*)msg.hash_info().c_str(), from))   // PKI判断
	{
		send_peer_announced(from, fromlen, (const unsigned char*)msg.msg_com().targer_id().c_str(), 
			msg.msg_com().targer_id().length());
		return false;
	}
	one_.storage_store((const unsigned char*)msg.hash_info().c_str(), from, msg.port(),now_);
	/* Note that if storage_store failed, we lie to the requestor.
	This is to prevent them from backtracking, and hence
	polluting the DHT. */
	dht_helper::instance()->debugf("Sending peer announced.\n");
	send_peer_announced(from, fromlen, (const unsigned char*)msg.msg_com().targer_id().c_str(), 
		msg.msg_com().targer_id().length());
	return true;
}
bool dht_net::handle_announce_reply_msg(const unsigned char *buf, int buflen, const struct sockaddr *from, int fromlen, dht_msg_head &head)
{
	dht_msg_common msg;
	msg.ParseFromArray(buf + head.ByteSizeLong(), head.len());
	if (DHT_MESSAGE_REPLY != GETMTYPE(head.msg_id())) return false;
	if (!id_check(msg.my_id())) return true;
	dht_helper::instance()->debugf("Got reply to announce_peer.\n");
	unsigned short ttid;
	tid_match((const unsigned char*)msg.targer_id().c_str(), "ap", &ttid);
	struct search *sr = one_.find_search(ttid, from->sa_family);
	if (!sr) 
	{
		dht_helper::instance()->debugf("Unknown search!\n");
		new_node((const unsigned char*)msg.my_id().c_str(), from, fromlen, 1);
	}
	else {
		new_node((const unsigned char*)msg.my_id().c_str(), from, fromlen, 2);
		for (int i = 0; i < sr->numnodes; i++)
		{
			if (dht_helper::instance()->id_cmp(sr->nodes[i].id, (const unsigned char*)msg.my_id().c_str()) == 0) 
			{
				sr->nodes[i].request_time = 0;
				sr->nodes[i].reply_time = now_.tv_sec;
				sr->nodes[i].acked = 1;
				sr->nodes[i].pinged = 0;
				break;
			}
		}
		/* See comment for gp above. */
		search_send_get_peers(sr, NULL);
	}
	return true;
}
bool dht_net::handle_error_msg(const unsigned char *buf, int buflen, const struct sockaddr *from, int fromlen, dht_msg_head &head)
{
	dht_msg_error msg;
	msg.ParseFromArray(buf + head.ByteSizeLong(), head.len());
	if (DHT_MESSAGE_ERROR != GETMTYPE(head.msg_id())) return false;
	if (!id_check(msg.msg_com().my_id())) return true;
	dht_helper::instance()->debugf("ecode=%d,err=",msg.code(),
		msg.err_msg().c_str());
	return true;
}
int dht_net::set_broadcast_msg_key(const unsigned char* bdata, int len,int etm)
{
	SHA1_CONTEXT sc;
	dht_helper::instance()->sha1_init(&sc);
	dht_helper::instance()->sha1_write(&sc, bdata, len);
	dht_helper::instance()->sha1_final(&sc);
	std::string hash_key = std::string((char*)sc.buf, DHT_KEY_HASH_SIZE);
	return send_broadcast_msg_key(hash_key, std::string((const char*)bdata, len), etm);
}
int dht_net::send_broadcast_msg_key(const std::string &key, const std::string &data, int etm)
{
	struct sockaddr_in sin[500] = { 0 }; int cnt = 500;
	one_.dht_get_nodes(sin, &cnt, nullptr, 0, now_);
	dht_msg_additional msg;
	msg.mutable_msg_com()->set_my_id(std::string((const char*)one_.myid, DHT_KEY_HASH_SIZE));
	//msg.mutable_msg_com()->set_targer_id(std::string((const char*)tid, tid_len));
	if (one_.have_v)
		msg.mutable_msg_com()->set_data_v(std::string((const char*)one_.my_v, sizeof(one_.my_v)));
	msg.set_time_expire(0);
	msg.set_user_msg_key(key);
	dht_msg_head msg_head;
	std::string allbuf;
	MAKE_DHT_MESSAGE(DHT_METHOD_ID_BD_KEY, DHT_MESSAGE_REQUEST, msg, msg_head, allbuf);
	for (int i = 0; i < cnt; i++)
	{
		dht_send((const void*)allbuf.c_str(), allbuf.length(), 0, (struct sockaddr*)&sin[i], sizeof(struct sockaddr_in));
	}
	// 缓存数据
	auto obj = bd_data_.find(key);
	int etmp = (etm == 0) ? now_.tv_sec : etm;
	if (obj == bd_data_.end())
	{
		bd_data_[key] = std::move(bd_data(data, etmp));
	}
	//else
	//	obj->second.stamp_ = 30;
	return 0;
}
int dht_net::send_broadcast_msg_key_reply(const unsigned char *key, const struct sockaddr *from, int fromlen)
{
	dht_msg_additional msg;
	msg.mutable_msg_com()->set_my_id(std::string((const char*)one_.myid, DHT_KEY_HASH_SIZE));
	//msg_com.set_targer_id(std::string((const char*)tid, tid_len));
	if (one_.have_v)
		msg.mutable_msg_com()->set_data_v(std::string((const char*)one_.my_v, sizeof(one_.my_v)));
	msg.set_time_expire(0);
	msg.set_user_msg_key(std::string((const char*)key,DHT_KEY_HASH_SIZE));
	dht_msg_head msg_head;
	std::string allbuf;
	MAKE_DHT_MESSAGE(DHT_METHOD_ID_BD_KEY_REPLY, DHT_MESSAGE_REQUEST, msg, msg_head, allbuf);
	return dht_send((const void*)allbuf.c_str(), allbuf.length(), 0, from, fromlen);
}
int dht_net::send_broadcast_msg(const std::string &key, const std::string &data,int etm, const struct sockaddr *from, int fromlen)
{
	dht_msg_additional msg;
	msg.mutable_msg_com()->set_my_id(std::string((const char*)one_.myid, DHT_KEY_HASH_SIZE));
	//msg_com.set_targer_id(std::string((const char*)tid, tid_len));
	if (one_.have_v)
		msg.mutable_msg_com()->set_data_v(std::string((const char*)one_.my_v, sizeof(one_.my_v)));
	msg.set_time_expire(etm);
	msg.set_user_msg_key(key);
	msg.set_user_msg(data);
	dht_msg_head msg_head;
	std::string allbuf;
	allbuf += "BDMs";	
	MAKE_DHT_MESSAGE(DHT_METHOD_ID_BD_DATA, DHT_MESSAGE_REPLY, msg, msg_head, allbuf);
	return dht_send((const void*)allbuf.c_str(), allbuf.length(), 0, from, fromlen);
}
bool dht_net::handle_broadcast_msg_key(const unsigned char *buf, int buflen,
	const struct sockaddr *from, int fromlen, dht_msg_head &head)
{
	dht_msg_additional msg;
	msg.ParseFromArray(buf + head.ByteSizeLong(), head.len());
	if (DHT_MESSAGE_REQUEST != GETMTYPE(head.msg_id()) && DHT_MESSAGE_REPLY != GETMTYPE(head.msg_id()))
		return false;
	if (!id_check(msg.msg_com().my_id())) return true;
	if (DHT_METHOD_ID_BD_KEY == GETMID(head.msg_id()))
	{
		auto obj = bd_data_.find(msg.user_msg_key());
		if (obj == bd_data_.end())
		{
			send_broadcast_msg_key_reply((const unsigned char*)msg.user_msg_key().c_str(), from,fromlen);
		}
	}
	else if (DHT_METHOD_ID_BD_KEY_REPLY == GETMID(head.msg_id()))
	{
		auto obj = bd_data_.find(msg.user_msg_key());
		if (obj != bd_data_.end())
		{
			send_broadcast_msg(obj->first,obj->second.data_,obj->second.stamp_, from, fromlen);
		}
		else
			dht_helper::instance()->debugf("broadcast key was expire\n");
	}
	else if (DHT_METHOD_ID_BD_DATA == GETMID(head.msg_id()))
	{
		auto obj = bd_data_.find(msg.user_msg_key());
		if (obj == bd_data_.end())
			send_broadcast_msg_key(msg.user_msg_key(), msg.user_msg(),msg.time_expire());
	}
	else
		return false;
	return true;
}
int dht_net::bd_data_maintenance()
{
	for (auto obj = begin(bd_data_); obj != end(bd_data_);)
	{
		if (obj->second.stamp_ - now_.tv_sec > (int)DHT_BROADCAST_MAX_LIVETIME)
			obj = bd_data_.erase(obj);
		else
			obj++;
	}
	return 0;
}
bool dht_net::get_all_addr_inbucket(unsigned long long* addrs, int &cnt)
{
	struct sockaddr_in sin[1000] = { 0 }; int sin_cnt = 1000;
	one_.dht_get_nodes(sin, &sin_cnt, nullptr, 0, now_);
	unsigned short uport = 0;
	int ip = 0,i=0;
	unsigned long long addr;
	for (; i < sin_cnt && i < cnt; i++)
	{
		ip = sin[i].sin_addr.s_addr;
		uport = sin[i].sin_port;
		addr = ((unsigned long long)ip) << 32 | uport;
		addrs[i] = addr;
	}
	cnt = i;
	return true;
}