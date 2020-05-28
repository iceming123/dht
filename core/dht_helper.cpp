
#include "dht_head.h"
#undef BIG_ENDIAN_HOST

#include <time.h>

#if !defined(_WIN32) || defined(__MINGW32__)
#include <sys/time.h>
#endif

#ifdef _WIN32
#pragma comment(lib,"ws2_32.lib")
#endif
#include "dht_helper.h"

/* We set sin_family to 0 to mark unused slots. */
#if AF_INET == 0 || AF_INET6 == 0
#error You lose
#endif

/****************
* Rotate a 32 bit integer by n bytes
****************/
#if defined(__GNUC__) && defined(__i386__)
static inline u32 rol(u32 x, int n)
{
	__asm__("roll %%cl,%0"
		:"=r" (x)
		: "0" (x), "c" (n));
	return x;
}
#else
#define rol(x,n) ( ((x) << (n)) | ((x) >> (32-(n))) )
#endif
//////////////////////////////////////////////////////////////////////////
static const unsigned char zeroes[DHT_KEY_HASH_SIZE] = { 0 };
static const unsigned char v4prefix[16] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 0, 0, 0, 0
};
static const unsigned char ones[20] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF
};
//////////////////////////////////////////////////////////////////////////
void dht_helper::sha1_init(SHA1_CONTEXT *hd)
{
	hd->h0 = 0x67452301;
	hd->h1 = 0xefcdab89;
	hd->h2 = 0x98badcfe;
	hd->h3 = 0x10325476;
	hd->h4 = 0xc3d2e1f0;
	hd->nblocks = 0;
	hd->count = 0;
}


/*
* Transform the message X which consists of 16 32-bit-words
*/
void dht_helper::transform(SHA1_CONTEXT *hd, const unsigned char *data)
{
	u32 a, b, c, d, e, tm;
	u32 x[16];

	/* get values from the chaining vars */
	a = hd->h0;
	b = hd->h1;
	c = hd->h2;
	d = hd->h3;
	e = hd->h4;

#ifdef BIG_ENDIAN_HOST
	memcpy(x, data, 64);
#else
	{ int i;
	unsigned char *p2;
	for (i = 0, p2 = (unsigned char*)x; i < 16; i++, p2 += 4) {
		p2[3] = *data++;
		p2[2] = *data++;
		p2[1] = *data++;
		p2[0] = *data++;
	}
	}
#endif


#define K1 0x5A827999L
#define K2 0x6ED9EBA1L
#define K3 0x8F1BBCDCL
#define K4 0xCA62C1D6L
#define F1(x,y,z) ( z ^ ( x & ( y ^ z ) ) )
#define F2(x,y,z) ( x ^ y ^ z )
#define F3(x,y,z) ( ( x & y ) | ( z & ( x | y ) ) )
#define F4(x,y,z) ( x ^ y ^ z )


#define M(i) ( tm = x[i&0x0f] ^ x[(i-14)&0x0f] \
^ x[(i-8)&0x0f] ^ x[(i-3)&0x0f] \
, (x[i&0x0f] = rol(tm,1)) )

#define R(a,b,c,d,e,f,k,m) do { e += rol( a, 5 ) \
+ f( b, c, d ) \
+ k \
+ m; \
b = rol( b, 30 ); \
	} while(0)
	R(a, b, c, d, e, F1, K1, x[0]);
	R(e, a, b, c, d, F1, K1, x[1]);
	R(d, e, a, b, c, F1, K1, x[2]);
	R(c, d, e, a, b, F1, K1, x[3]);
	R(b, c, d, e, a, F1, K1, x[4]);
	R(a, b, c, d, e, F1, K1, x[5]);
	R(e, a, b, c, d, F1, K1, x[6]);
	R(d, e, a, b, c, F1, K1, x[7]);
	R(c, d, e, a, b, F1, K1, x[8]);
	R(b, c, d, e, a, F1, K1, x[9]);
	R(a, b, c, d, e, F1, K1, x[10]);
	R(e, a, b, c, d, F1, K1, x[11]);
	R(d, e, a, b, c, F1, K1, x[12]);
	R(c, d, e, a, b, F1, K1, x[13]);
	R(b, c, d, e, a, F1, K1, x[14]);
	R(a, b, c, d, e, F1, K1, x[15]);
	R(e, a, b, c, d, F1, K1, M(16));
	R(d, e, a, b, c, F1, K1, M(17));
	R(c, d, e, a, b, F1, K1, M(18));
	R(b, c, d, e, a, F1, K1, M(19));
	R(a, b, c, d, e, F2, K2, M(20));
	R(e, a, b, c, d, F2, K2, M(21));
	R(d, e, a, b, c, F2, K2, M(22));
	R(c, d, e, a, b, F2, K2, M(23));
	R(b, c, d, e, a, F2, K2, M(24));
	R(a, b, c, d, e, F2, K2, M(25));
	R(e, a, b, c, d, F2, K2, M(26));
	R(d, e, a, b, c, F2, K2, M(27));
	R(c, d, e, a, b, F2, K2, M(28));
	R(b, c, d, e, a, F2, K2, M(29));
	R(a, b, c, d, e, F2, K2, M(30));
	R(e, a, b, c, d, F2, K2, M(31));
	R(d, e, a, b, c, F2, K2, M(32));
	R(c, d, e, a, b, F2, K2, M(33));
	R(b, c, d, e, a, F2, K2, M(34));
	R(a, b, c, d, e, F2, K2, M(35));
	R(e, a, b, c, d, F2, K2, M(36));
	R(d, e, a, b, c, F2, K2, M(37));
	R(c, d, e, a, b, F2, K2, M(38));
	R(b, c, d, e, a, F2, K2, M(39));
	R(a, b, c, d, e, F3, K3, M(40));
	R(e, a, b, c, d, F3, K3, M(41));
	R(d, e, a, b, c, F3, K3, M(42));
	R(c, d, e, a, b, F3, K3, M(43));
	R(b, c, d, e, a, F3, K3, M(44));
	R(a, b, c, d, e, F3, K3, M(45));
	R(e, a, b, c, d, F3, K3, M(46));
	R(d, e, a, b, c, F3, K3, M(47));
	R(c, d, e, a, b, F3, K3, M(48));
	R(b, c, d, e, a, F3, K3, M(49));
	R(a, b, c, d, e, F3, K3, M(50));
	R(e, a, b, c, d, F3, K3, M(51));
	R(d, e, a, b, c, F3, K3, M(52));
	R(c, d, e, a, b, F3, K3, M(53));
	R(b, c, d, e, a, F3, K3, M(54));
	R(a, b, c, d, e, F3, K3, M(55));
	R(e, a, b, c, d, F3, K3, M(56));
	R(d, e, a, b, c, F3, K3, M(57));
	R(c, d, e, a, b, F3, K3, M(58));
	R(b, c, d, e, a, F3, K3, M(59));
	R(a, b, c, d, e, F4, K4, M(60));
	R(e, a, b, c, d, F4, K4, M(61));
	R(d, e, a, b, c, F4, K4, M(62));
	R(c, d, e, a, b, F4, K4, M(63));
	R(b, c, d, e, a, F4, K4, M(64));
	R(a, b, c, d, e, F4, K4, M(65));
	R(e, a, b, c, d, F4, K4, M(66));
	R(d, e, a, b, c, F4, K4, M(67));
	R(c, d, e, a, b, F4, K4, M(68));
	R(b, c, d, e, a, F4, K4, M(69));
	R(a, b, c, d, e, F4, K4, M(70));
	R(e, a, b, c, d, F4, K4, M(71));
	R(d, e, a, b, c, F4, K4, M(72));
	R(c, d, e, a, b, F4, K4, M(73));
	R(b, c, d, e, a, F4, K4, M(74));
	R(a, b, c, d, e, F4, K4, M(75));
	R(e, a, b, c, d, F4, K4, M(76));
	R(d, e, a, b, c, F4, K4, M(77));
	R(c, d, e, a, b, F4, K4, M(78));
	R(b, c, d, e, a, F4, K4, M(79));

	/* Update chaining vars */
	hd->h0 += a;
	hd->h1 += b;
	hd->h2 += c;
	hd->h3 += d;
	hd->h4 += e;
}


/* Update the message digest with the contents
* of INBUF with length INLEN.
*/
void dht_helper::sha1_write(SHA1_CONTEXT *hd, const unsigned char *inbuf, size_t inlen)
{
	if (hd->count == 64) { /* flush the buffer */
		transform(hd, hd->buf);
		hd->count = 0;
		hd->nblocks++;
	}
	if (!inbuf)
		return;
	if (hd->count) {
		for (; inlen && hd->count < 64; inlen--)
			hd->buf[hd->count++] = *inbuf++;
		sha1_write(hd, NULL, 0);
		if (!inlen)
			return;
	}

	while (inlen >= 64) {
		transform(hd, inbuf);
		hd->count = 0;
		hd->nblocks++;
		inlen -= 64;
		inbuf += 64;
	}
	for (; inlen && hd->count < 64; inlen--)
		hd->buf[hd->count++] = *inbuf++;
}


/* The routine final terminates the computation and
* returns the digest.
* The handle is prepared for a new cycle, but adding bytes to the
* handle will the destroy the returned buffer.
* Returns: 20 bytes representing the digest.
*/

void dht_helper::sha1_final(SHA1_CONTEXT *hd)
{
	u32 t, msb, lsb;
	unsigned char *p;

	sha1_write(hd, NULL, 0); /* flush */;

	t = hd->nblocks;
	/* multiply by 64 to make a byte count */
	lsb = t << 6;
	msb = t >> 26;
	/* add the count */
	t = lsb;
	if ((lsb += hd->count) < t)
		msb++;
	/* multiply by 8 to make a bit count */
	t = lsb;
	lsb <<= 3;
	msb <<= 3;
	msb |= t >> 29;

	if (hd->count < 56) { /* enough room */
		hd->buf[hd->count++] = 0x80; /* pad */
		while (hd->count < 56)
			hd->buf[hd->count++] = 0; /* pad */
	}
	else { /* need one extra block */
		hd->buf[hd->count++] = 0x80; /* pad character */
		while (hd->count < 64)
			hd->buf[hd->count++] = 0;
		sha1_write(hd, NULL, 0); /* flush */;
		memset(hd->buf, 0, 56); /* fill next block with zeroes */
	}
	/* append the 64 bit count */
	hd->buf[56] = msb >> 24;
	hd->buf[57] = msb >> 16;
	hd->buf[58] = msb >> 8;
	hd->buf[59] = msb;
	hd->buf[60] = lsb >> 24;
	hd->buf[61] = lsb >> 16;
	hd->buf[62] = lsb >> 8;
	hd->buf[63] = lsb;
	transform(hd, hd->buf);

	p = hd->buf;
#ifdef BIG_ENDIAN_HOST
#define X(a) do { *(u32*)p = hd->h##a ; p += 4; } while(0)
#else /* little endian */
#define X(a) do { *p++ = hd->h##a >> 24; *p++ = hd->h##a >> 16; \
*p++ = hd->h##a >> 8; *p++ = hd->h##a; } while(0)
#endif
	X(0);
	X(1);
	X(2);
	X(3);
	X(4);
#undef X
}

//////////////////////////////////////////////////////////////////////////
////////////////////////md5////////////////////////////////////
#define F(x,y,z) ((x & y) | (~x & z))  
#define G(x,y,z) ((x & z) | (y & ~z))  
#define H(x,y,z) (x^y^z)  
#define I(x,y,z) (y ^ (x | ~z))  
#define ROTATE_LEFT(x,n) ((x << n) | (x >> (32-n)))  
#define FF(a,b,c,d,x,s,ac) \
{ \
a += F(b, c, d) + x + ac; \
a = ROTATE_LEFT(a, s); \
a += b; \
}
#define GG(a,b,c,d,x,s,ac) \
{ \
a += G(b, c, d) + x + ac; \
a = ROTATE_LEFT(a, s); \
a += b; \
}
#define HH(a,b,c,d,x,s,ac) \
{ \
a += H(b, c, d) + x + ac; \
a = ROTATE_LEFT(a, s); \
a += b; \
}
#define II(a,b,c,d,x,s,ac) \
{ \
a += I(b, c, d) + x + ac; \
a = ROTATE_LEFT(a, s); \
a += b; \
}

unsigned char PADDING[] = { 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

void dht_helper::MD5Init(MD5_CTX *context)
{
	context->count[0] = 0;
	context->count[1] = 0;
	context->state[0] = 0x67452301;
	context->state[1] = 0xEFCDAB89;
	context->state[2] = 0x98BADCFE;
	context->state[3] = 0x10325476;
}
void dht_helper::MD5Update(MD5_CTX *context, unsigned char *input, unsigned int inputlen)
{
	unsigned int i = 0, index = 0, partlen = 0;
	index = (context->count[0] >> 3) & 0x3F;
	partlen = 64 - index;
	context->count[0] += inputlen << 3;
	if (context->count[0] < (inputlen << 3))
		context->count[1]++;
	context->count[1] += inputlen >> 29;

	if (inputlen >= partlen)
	{
		memcpy(&context->buffer[index], input, partlen);
		MD5Transform(context->state, context->buffer);
		for (i = partlen; i + 64 <= inputlen; i += 64)
			MD5Transform(context->state, &input[i]);
		index = 0;
	}
	else
	{
		i = 0;
	}
	memcpy(&context->buffer[index], &input[i], inputlen - i);
}
void dht_helper::MD5Final(MD5_CTX *context, unsigned char digest[16])
{
	unsigned int index = 0, padlen = 0;
	unsigned char bits[8];
	index = (context->count[0] >> 3) & 0x3F;
	padlen = (index < 56) ? (56 - index) : (120 - index);
	MD5Encode(bits, context->count, 8);
	MD5Update(context, PADDING, padlen);
	MD5Update(context, bits, 8);
	MD5Encode(digest, context->state, 16);
}
void dht_helper::MD5Encode(unsigned char *output, unsigned int *input, unsigned int len)
{
	unsigned int i = 0, j = 0;
	while (j < len)
	{
		output[j] = input[i] & 0xFF;
		output[j + 1] = (input[i] >> 8) & 0xFF;
		output[j + 2] = (input[i] >> 16) & 0xFF;
		output[j + 3] = (input[i] >> 24) & 0xFF;
		i++;
		j += 4;
	}
}
void dht_helper::MD5Decode(unsigned int *output, unsigned char *input, unsigned int len)
{
	unsigned int i = 0, j = 0;
	while (j < len)
	{
		output[i] = (input[j]) |
			(input[j + 1] << 8) |
			(input[j + 2] << 16) |
			(input[j + 3] << 24);
		i++;
		j += 4;
	}
}
void dht_helper::MD5Transform(unsigned int state[4], unsigned char block[64])
{
	unsigned int a = state[0];
	unsigned int b = state[1];
	unsigned int c = state[2];
	unsigned int d = state[3];
	unsigned int x[64];
	MD5Decode(x, block, 64);
	FF(a, b, c, d, x[0], 7, 0xd76aa478);
	FF(d, a, b, c, x[1], 12, 0xe8c7b756);
	FF(c, d, a, b, x[2], 17, 0x242070db);
	FF(b, c, d, a, x[3], 22, 0xc1bdceee);
	FF(a, b, c, d, x[4], 7, 0xf57c0faf);
	FF(d, a, b, c, x[5], 12, 0x4787c62a);
	FF(c, d, a, b, x[6], 17, 0xa8304613);
	FF(b, c, d, a, x[7], 22, 0xfd469501);
	FF(a, b, c, d, x[8], 7, 0x698098d8);
	FF(d, a, b, c, x[9], 12, 0x8b44f7af);
	FF(c, d, a, b, x[10], 17, 0xffff5bb1);
	FF(b, c, d, a, x[11], 22, 0x895cd7be);
	FF(a, b, c, d, x[12], 7, 0x6b901122);
	FF(d, a, b, c, x[13], 12, 0xfd987193);
	FF(c, d, a, b, x[14], 17, 0xa679438e);
	FF(b, c, d, a, x[15], 22, 0x49b40821);


	GG(a, b, c, d, x[1], 5, 0xf61e2562);
	GG(d, a, b, c, x[6], 9, 0xc040b340);
	GG(c, d, a, b, x[11], 14, 0x265e5a51);
	GG(b, c, d, a, x[0], 20, 0xe9b6c7aa);
	GG(a, b, c, d, x[5], 5, 0xd62f105d);
	GG(d, a, b, c, x[10], 9, 0x2441453);
	GG(c, d, a, b, x[15], 14, 0xd8a1e681);
	GG(b, c, d, a, x[4], 20, 0xe7d3fbc8);
	GG(a, b, c, d, x[9], 5, 0x21e1cde6);
	GG(d, a, b, c, x[14], 9, 0xc33707d6);
	GG(c, d, a, b, x[3], 14, 0xf4d50d87);
	GG(b, c, d, a, x[8], 20, 0x455a14ed);
	GG(a, b, c, d, x[13], 5, 0xa9e3e905);
	GG(d, a, b, c, x[2], 9, 0xfcefa3f8);
	GG(c, d, a, b, x[7], 14, 0x676f02d9);
	GG(b, c, d, a, x[12], 20, 0x8d2a4c8a);


	HH(a, b, c, d, x[5], 4, 0xfffa3942);
	HH(d, a, b, c, x[8], 11, 0x8771f681);
	HH(c, d, a, b, x[11], 16, 0x6d9d6122);
	HH(b, c, d, a, x[14], 23, 0xfde5380c);
	HH(a, b, c, d, x[1], 4, 0xa4beea44);
	HH(d, a, b, c, x[4], 11, 0x4bdecfa9);
	HH(c, d, a, b, x[7], 16, 0xf6bb4b60);
	HH(b, c, d, a, x[10], 23, 0xbebfbc70);
	HH(a, b, c, d, x[13], 4, 0x289b7ec6);
	HH(d, a, b, c, x[0], 11, 0xeaa127fa);
	HH(c, d, a, b, x[3], 16, 0xd4ef3085);
	HH(b, c, d, a, x[6], 23, 0x4881d05);
	HH(a, b, c, d, x[9], 4, 0xd9d4d039);
	HH(d, a, b, c, x[12], 11, 0xe6db99e5);
	HH(c, d, a, b, x[15], 16, 0x1fa27cf8);
	HH(b, c, d, a, x[2], 23, 0xc4ac5665);


	II(a, b, c, d, x[0], 6, 0xf4292244);
	II(d, a, b, c, x[7], 10, 0x432aff97);
	II(c, d, a, b, x[14], 15, 0xab9423a7);
	II(b, c, d, a, x[5], 21, 0xfc93a039);
	II(a, b, c, d, x[12], 6, 0x655b59c3);
	II(d, a, b, c, x[3], 10, 0x8f0ccc92);
	II(c, d, a, b, x[10], 15, 0xffeff47d);
	II(b, c, d, a, x[1], 21, 0x85845dd1);
	II(a, b, c, d, x[8], 6, 0x6fa87e4f);
	II(d, a, b, c, x[15], 10, 0xfe2ce6e0);
	II(c, d, a, b, x[6], 15, 0xa3014314);
	II(b, c, d, a, x[13], 21, 0x4e0811a1);
	II(a, b, c, d, x[4], 6, 0xf7537e82);
	II(d, a, b, c, x[11], 10, 0xbd3af235);
	II(c, d, a, b, x[2], 15, 0x2ad7d2bb);
	II(b, c, d, a, x[9], 21, 0xeb86d391);
	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
}

//////////////////////////////////////////////////////////////////////////
int dht_helper::set_nonblocking(int fd, int nonblocking)
{
#ifdef _WIN32
	int rc;

	unsigned long mode = !!nonblocking;
	rc = ioctlsocket(fd, FIONBIO, &mode);
	if (rc != 0)
		errno = WSAGetLastError();
	return (rc == 0 ? 0 : -1);
#else
	int rc;
	rc = fcntl(fd, F_GETFL, 0);
	if (rc < 0)
		return -1;

	rc = fcntl(fd, F_SETFL, nonblocking ? (rc | O_NONBLOCK) : (rc & ~O_NONBLOCK));
	if (rc < 0)
		return -1;

	return 0;
#endif
}
void dht_helper::dht_hash(void *hash_return, int hash_size,
	void *v1, int len1,
	void *v2, int len2,
	void *v3, int len3)
{
#ifdef _WIN32
	static MD5_CTX ctx;
	unsigned char decrypt[16];
	MD5Init(&ctx);
	MD5Update(&ctx, (unsigned char*)v1, len1);
	MD5Update(&ctx, (unsigned char*)v2, len2);
	MD5Update(&ctx, (unsigned char*)v3, len3);
	MD5Final(&ctx, decrypt);
	if (hash_size > 16)
		memset((char*)hash_return + 16, 0, hash_size - 16);
	memcpy(hash_return, ctx.buffer, hash_size > 16 ? 16 : hash_size);
#else
	const char *c1 = (const char*)v1, *c2 = (const char*)v2, *c3 = (const char*)v3;
	char key[9];                /* crypt is limited to 8 characters */
	int i;

	memset(key, 0, 9);
#define CRYPT_HAPPY(c) ((c % 0x60) + 0x20)

	for (i = 0; i < 2 && i < len1; i++)
		key[i] = CRYPT_HAPPY(c1[i]);
	for (i = 0; i < 4 && i < len1; i++)
		key[2 + i] = CRYPT_HAPPY(c2[i]);
	for (i = 0; i < 2 && i < len1; i++)
		key[6 + i] = CRYPT_HAPPY(c3[i]);
	strncpy((char*)hash_return, crypt(key, "jc"), hash_size);
#endif

}

int dht_helper::random(void)
{
	return rand();
}
int dht_helper::dht_gettimeofday(struct timeval *tv, struct timezone *tz)
{
#if !defined(_WIN32) || defined(__MINGW32__)
	return gettimeofday(tv, tz);
#else
	static int tzflag = 0;
	time_t clock;
	struct tm tm;
	SYSTEMTIME wtm;
	GetLocalTime(&wtm);
	tm.tm_year = wtm.wYear - 1900;
	tm.tm_mon = wtm.wMonth - 1;
	tm.tm_mday = wtm.wDay;
	tm.tm_hour = wtm.wHour;
	tm.tm_min = wtm.wMinute;
	tm.tm_sec = wtm.wSecond;
	tm.tm_isdst = -1;
	clock = mktime(&tm);
	tv->tv_sec = (long)clock;
	tv->tv_usec = wtm.wMilliseconds * 1000;

	if (tz) {
		if (!tzflag) {
#if !TSK_UNDER_WINDOWS_RT
			_tzset();
#endif
			tzflag++;
		}
		tz->tz_minuteswest = _timezone / 60;
		tz->tz_dsttime = _daylight;
	}
	return (0);
#endif

}

int dht_helper::id_cmp(const unsigned char *id1, const unsigned char *id2)
{
	return memcmp(id1, id2, DHT_KEY_HASH_SIZE);
}
int dht_helper::is_martian(const struct sockaddr *sa)
{
	switch (sa->sa_family) {
	case AF_INET: {
		struct sockaddr_in *sin = (struct sockaddr_in*)sa;
		const unsigned char *address = (const unsigned char*)&sin->sin_addr;
		return sin->sin_port == 0 ||
			(address[0] == 0) ||
			(address[0] == 127) ||
			((address[0] & 0xE0) == 0xE0);
	}
	case AF_INET6: {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)sa;
		const unsigned char *address = (const unsigned char*)&sin6->sin6_addr;
		return sin6->sin6_port == 0 ||
			(address[0] == 0xFF) ||
			(address[0] == 0xFE && (address[1] & 0xC0) == 0x80) ||
			(memcmp(address, zeroes, 15) == 0 &&
			(address[15] == 0 || address[15] == 1)) ||
			(memcmp(address, v4prefix, 12) == 0);
	}
	default:
		return 0;
	}
	return 0;
}
int dht_helper::lowbit(const unsigned char *id)
{
	int i, j;
	for (i = DHT_KEY_HASH_SIZE-1; i >= 0; i--)
		if (id[i] != 0)
			break;

	if (i < 0)
		return -1;

	for (j = 7; j >= 0; j--)
		if ((id[i] & (0x80 >> j)) != 0)
			break;

	return 8 * i + j;
}
int dht_helper::common_bits(const unsigned char *id1, const unsigned char *id2)
{
	int i, j;
	unsigned char xor1;
	for (i = 0; i < DHT_KEY_HASH_SIZE; i++) {
		if (id1[i] != id2[i])
			break;
	}

	if (i == DHT_KEY_HASH_SIZE)
		return DHT_KEY_HASH_SIZE*8;

	xor1 = id1[i] ^ id2[i];

	j = 0;
	while ((xor1 & 0x80) == 0) {
		xor1 <<= 1;
		j++;
	}

	return 8 * i + j;
}
/* Determine whether id1 or id2 is closer to ref */
// result 0--equal,-1--id1 closer, 1--id2 closer
int dht_helper::xorcmp(const unsigned char *id1,
	const unsigned char *id2, const unsigned char *ref)
{
	int i;
	for (i = 0; i < DHT_KEY_HASH_SIZE; i++) {
		unsigned char xor1, xor2;
		if (id1[i] == id2[i])
			continue;
		xor1 = id1[i] ^ ref[i];
		xor2 = id2[i] ^ ref[i];
		if (xor1 < xor2)
			return -1;
		else
			return 1;
	}
	return 0;
}
bool dht_helper::zero_key(const unsigned char* key)
{
	if (!key) true;
	return 0 == id_cmp(key, zeroes);
}
void dht_helper::simple_random_bytes(void *buf, size_t size)
{
	srand((unsigned int)time(0));
	char* pbuf = (char*)buf;
	for (size_t i = 0; i < size; i++)
	{
		pbuf[i] = rand();
	}
}
//////////////////////////////////////////////////////////////////////////
void dht_helper::debugf(const char *format, ...)
{
#ifdef DHT_DEBUG_LOG
	//FILE* dfile = (f == nullptr) ? stdout : f;
	FILE* dfile = stdout;
	va_list args;
	va_start(args, format);
	vfprintf(dfile, format, args);
	va_end(args);
	fflush(dfile);		
#endif
}
void dht_helper::debug_printable(const unsigned char *buf, int buflen,FILE *f)
{
#ifdef DHT_DEBUG_LOG
	int i;
	FILE* dfile = (f == nullptr) ? stdout : f;
	for (i = 0; i < buflen; i++)
		putc(buf[i] >= 32 && buf[i] <= 126 ? buf[i] : '.', dfile);
#endif
}
void dht_helper::debugf_hex(const  char* head, const  char *buf, int buflen, FILE *f)
{
#ifdef DHT_DEBUG_LOG
	FILE* dfile = (f == nullptr) ? stdout : f;
	fprintf(dfile, (const char*)head);

	for (int i = 0; i < buflen; i++)
		fprintf(dfile, "%02x", buf[i]);

	fprintf(dfile, "\n");
	fflush(dfile);
#endif
}
void dht_helper::print_hex(const unsigned char *buf, int buflen, FILE *f)
{
#ifdef DHT_DEBUG_LOG
	FILE* dfile = (f == nullptr) ? stdout : f;
	for (int i = 0; i < buflen; i++)
		fprintf(dfile, "%02x", buf[i]);
#endif
}

