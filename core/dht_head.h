#pragma once

#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <atomic>

#include "dht_demoapi.h"

#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#else
#ifndef _WIN32_WINNT
//#define _WIN32_WINNT 0x0501 /* Windows XP */
#endif
#ifndef WINVER
#define WINVER _WIN32_WINNT
#endif
#endif

#ifndef HAVE_MEMMEM
#ifdef __GLIBC__
#define HAVE_MEMMEM
#endif
#endif

#ifndef MSG_CONFIRM
#define MSG_CONFIRM 0
#endif



#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
/* nothing */
#elif defined(__GNUC__)
#define inline __inline
#if  (__GNUC__ >= 3)
#define restrict __restrict
#else
#define restrict /**/
#endif
#else
#define restrict /**/
#endif

#ifdef _WIN32

#undef EAFNOSUPPORT
#define EAFNOSUPPORT WSAEAFNOSUPPORT
#include <ws2tcpip.h>
/* Windows Vista and later already provide the implementation. */
#if _WIN32_WINNT < 0x0600
extern const char *inet_ntop(int, const void *, char *, socklen_t);
#endif
#ifdef _MSC_VER
/* There is no snprintf in MSVCRT. */
#define snprintf _snprintf
#endif
#endif

#ifdef DHT_DEBUG_LOG
	#define DHT_DEBUG_LOG 2
#endif

#define DHT_KEY_HASH_SIZE 20

#define MAX(x, y) ((x) >= (y) ? (x) : (y))
#define MIN(x, y) ((x) <= (y) ? (x) : (y))
/* When performing a search, we search for up to SEARCH_NODES closest nodes
to the destination, and use the additional ones to backtrack if any of
the target 8 turn out to be dead. */
#define SEARCH_NODES 14
/* The maximum number of peers we store for a given hash. */
#ifndef DHT_MAX_PEERS
#define DHT_MAX_PEERS 2048
#endif

/* The maximum number of hashes we're willing to track. */
#ifndef DHT_MAX_HASHES
#define DHT_MAX_HASHES 16384
#endif

/* The maximum number of searches we keep data about. */
#ifndef DHT_MAX_SEARCHES
#define DHT_MAX_SEARCHES 1024
#endif

/* The time after which we consider a search to be expirable. */
#ifndef DHT_SEARCH_EXPIRE_TIME
#define DHT_SEARCH_EXPIRE_TIME (62 * 60)
#endif

#define ERROR 0
#define REPLY 1
#define PING 2
#define FIND_NODE 3
#define GET_PEERS 4
#define ANNOUNCE_PEER 5

#define WANT4 1
#define WANT6 2

/* The maximum number of nodes that we snub.  There is probably little
reason to increase this value. */
#ifndef DHT_MAX_BLACKLISTED
#define DHT_MAX_BLACKLISTED 10
#endif

#define MAX_TOKEN_BUCKET_TOKENS 400
#define DHT_MESSAGE_HEAD_SIZE 20
//////////////////////////////////////////////////////////////////////////
struct node {
	unsigned char id[20];
	struct sockaddr_storage ss;
	int sslen;
	time_t time;                /* time of last message received */
	time_t reply_time;          /* time of last correct reply received */
	time_t pinged_time;         /* time of last request */
	int pinged;                 /* how many requests we sent since last reply */
	struct node *next;
};
struct bucket {
	int af;
	unsigned char first[20];
	int count;                  /* number of nodes */
	time_t time;                /* time of last reply in this bucket */
	struct node *nodes;
	struct sockaddr_storage cached;  /* the address of a likely candidate */
	int cachedlen;
	struct bucket *next;
};
struct search_node {
	unsigned char id[20];
	struct sockaddr_storage ss;
	int sslen;
	time_t request_time;        /* the time of the last unanswered request */
	time_t reply_time;          /* the time of the last reply */
	int pinged;
	unsigned char token[40];
	int token_len;
	int replied;                /* whether we have received a reply */
	int acked;                  /* whether they acked our announcement */
};
struct search {
	unsigned short tid;
	int af;
	time_t step_time;           /* the time of the last search_step */
	unsigned char id[20];		// info_hash
	unsigned short port;        /* 0 for pure searches */
	int done;
	struct search_node nodes[SEARCH_NODES];
	int numnodes;
	long long user_data;
	dht_callback *callback;
	unsigned char closet_key[20];
	unsigned char values[32];
	struct search *next;
};
struct peer {
	time_t time;
	unsigned char ip[16];
	unsigned short len;
	unsigned short port;
};
struct storage {
	unsigned char id[20];
	int numpeers, maxpeers;
	struct peer *peers;
	struct storage *next;
};
struct data_head{
	int cmdid;
	int length;
	int version;
};

enum data_type_id
{
	DATA_TYPE_ID_SEARCH_RESULT = 0xff01,
	DATA_TYPE_ID_STORAGE_FIND
};

#define DHT_EVENT_NONE 0
#define DHT_EVENT_VALUES 1
#define DHT_EVENT_VALUES6 2
#define DHT_EVENT_SEARCH_DONE 3
#define DHT_EVENT_SEARCH_DONE6 4
#define DHT_EVENT_SEARCH_ERROR 5
#define DHT_EVENT_REFUSE_NODE 6


