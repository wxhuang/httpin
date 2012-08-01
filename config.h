#ifndef _HTTPIN_CONFIG_H_
#define _HTTPIN_CONFIG_H_

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <netdb.h>
#include <arpa/inet.h>

//#define HTTPIN_HTTPS_SUPPORT
#ifdef HTTPIN_HTTPS_SUPPORT
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#endif


#if 1
typedef int INT32;
typedef unsigned int UINT32;
typedef char INT8;
typedef unsigned char UINT8;
typedef unsigned short UINT16;
#endif
/********************************
*memory
********************************/

#define httpin_malloc malloc
#define httpin_free free

#define httpin_gethostbyname gethostbyname
#define httpin_socket socket
#define httpin_connect connect
#define httpin_close close
#define httpin_select select
#define httpin_send send
#define httpin_recv recv


#define httpin_debug printf

#endif









