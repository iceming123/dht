#ifndef __DHT_DEMO_HELPER_H__
#define __DHT_DEMO_HELPER_H__


typedef unsigned int u32;
typedef struct SHA1_CONTEXT_tmp {
	u32 h0, h1, h2, h3, h4;
	u32 nblocks;
	unsigned char buf[64];
	int count;
} SHA1_CONTEXT;
typedef struct MD5_CTX_tmp
{
	unsigned int count[2];
	unsigned int state[4];
	unsigned char buffer[64];
}MD5_CTX;

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
struct timezone
{
	int  tz_minuteswest; // minutes W of Greenwich  
	int  tz_dsttime;     // type of dst correction
};
#endif

class dht_helper
{
public:
	static dht_helper* instance() {
		static dht_helper ins;
		return &ins;
	}
	~dht_helper() {};
	//////////////////////////////////////////////////////////////////////////
	void sha1_init(SHA1_CONTEXT *hd);
	void transform(SHA1_CONTEXT *hd, const unsigned char *data);
	void sha1_write(SHA1_CONTEXT *hd, const unsigned char *inbuf, size_t inlen);
	void sha1_final(SHA1_CONTEXT *hd);
	//////////////////////////////////////////////////////////////////////////
	void MD5Init(MD5_CTX *context);
	void MD5Update(MD5_CTX *context, unsigned char *input, unsigned int inputlen);
	void MD5Final(MD5_CTX *context, unsigned char digest[16]);
	void MD5Transform(unsigned int state[4], unsigned char block[64]);
	void MD5Encode(unsigned char *output, unsigned int *input, unsigned int len);
	void MD5Decode(unsigned int *output, unsigned char *input, unsigned int len);
	//////////////////////////////////////////////////////////////////////////
	int set_nonblocking(int fd, int nonblocking);
	void dht_hash(void *hash_return, int hash_size,
		void *v1, int len1,
		void *v2, int len2,
		void *v3, int len3);
	int random(void);
	int dht_gettimeofday(struct timeval *tv, struct timezone *tz);
	/* Forget about the ``XOR-metric''.  An id is just a path from the
	root of the tree, so bits are numbered from the start. */
	int id_cmp(const unsigned char *id1, const unsigned char *id2);
	int is_martian(const struct sockaddr *sa);
	/* Find the lowest 1 bit in an id. */
	int lowbit(const unsigned char *id);
	/* Find how many bits two ids have in common. */
	int common_bits(const unsigned char *id1, const unsigned char *id2);
	// result 0--equal,-1--id1 closer, 1--id2 closer
	int xorcmp(const unsigned char *id1,
		const unsigned char *id2, const unsigned char *ref);
	bool zero_key(const unsigned char* key);
	void simple_random_bytes(void *buf, size_t size);
	/////////////////debug info///////////////////////////////////////////////
	void debugf(const char *format, ...);
	void debug_printable(const unsigned char *buf, int buflen,FILE *f=nullptr);
	void debugf_hex(const  char* head, const  char *buf, int buflen, FILE *f = nullptr);
	void print_hex(const unsigned char *buf, int buflen, FILE *f = nullptr);

private:
	dht_helper() {};
};


#endif
