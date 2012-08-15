#include "config.h"
#include "httpin.h"

#define HTTP_PROTOCAL           "http://"
#define HTTP_CONTENT_LENGTH     "Content-Length:"
#define HTTP_LOCATION			"Location:"
#define TMPBUFSIZE				2048
#define HEADER_BUFSIZE			1024
#define RECVBUFSIZE				0x10000
#define HTTPIN_USER_AGENT		"Httpin-0.1"
#define REDIRECT_MAX			6

/*********************************************************************/

typedef struct _header_info
{
	INT32 content_length;
	UINT8* redirection_url;
}header_info;

enum
{
	RESPOND_OK,
	RESPOND_REDIRECTION,
	RESPOND_ERROR,
};


/*********************************************************************/
static INT32 httpin_set_header_from_url(const UINT8* url, const UINT8* ex_header, httpin_method method,  UINT8* header_str, UINT32 bufsize);
static INT32 httpin_get_host_from_url(const UINT8* url, UINT8* buf, UINT32 bufsize);
static INT32 httpin_get_header_from_sock(INT32 sock, UINT8* buf);
static INT32 httpin_default_cb(void* buf, UINT32 size, void* ctx, UINT32 status);
static INT32 parse_header_info(UINT8* header_str, header_info* inf);
static INT32 httpin_recv_content(INT32 sock, UINT8* buf, INT32 bufsize, INT32* recvsize, UINT8* running_flag);
static INT32 httpin_sock_create(const UINT8* url, INT32* sock);
/*********************************************************************/

httpin_error httpin_get(httpin_param* param,
						httpin_cb cb,
						void* ctx)
{
	INT32 ret = HERROR_UNKNOWN;
	INT32 ret2;
	INT32 sock = -1;
	INT32 cnt;
	header_info inf;
	UINT8* tmpbuf = NULL;
	UINT8* recvbuf = NULL;
	do
	{
		if(NULL == param)
		{
			ret = HERROR_PARAM;
			break;
		}
		if(param->redirect_count > REDIRECT_MAX)
		{
			ret = HERROR_STACK;
			break;
		}
		if(NULL == param->url)
		{
			ret = HERROR_PARAM;
			break;
		}
		if(NULL == cb)
			cb = httpin_default_cb;

		/* socket create */
		if(httpin_sock_create(param->url, &sock))
		{
			ret = HERROR_SOCK;
			break;
		}
		tmpbuf = httpin_malloc(TMPBUFSIZE);
		if(NULL == tmpbuf)
		{
			ret = HERROR_MEM;
			break;
		}
		
		/* send http header */
		if(0 != httpin_set_header_from_url(param->url,param->ex_header,
											HMETHOD_GET, tmpbuf,
											TMPBUFSIZE))
		{
			ret = HERROR_HEADER;
			break;
		}
		httpin_send(sock, tmpbuf, strlen(tmpbuf), 0);
		
		/* get returned header */
		if(httpin_get_header_from_sock(sock, tmpbuf))
		{
			ret = HERROR_SOCK;
			break;
		}
		
		/* parse header */
		inf.redirection_url = param->url;
		ret2 = parse_header_info(tmpbuf, &inf);
		switch(ret2)
		{
			case RESPOND_OK:
				break;
			case RESPOND_REDIRECTION:
				ret = HERROR_REDIRECT;
				break;
			case RESPOND_ERROR:
			default:
				ret = HERROR_RESPOND;
				break;	
		}
		if(RESPOND_OK != ret2)
		{
			break;
		}
		
		/* callback:parse header */
		if(cb(tmpbuf, strlen(tmpbuf), ctx, HTTPIN_HEADER))
		{
			ret = HERROR_CALLBACK;
			break;
		}
		/* callback:start */
		if(cb(NULL, 0, ctx, HTTPIN_START))
		{
			ret = HERROR_CALLBACK;
			break;
		}
		
		
		/* receive content */
		recvbuf = httpin_malloc(RECVBUFSIZE);
		if(NULL == recvbuf)
		{
			ret = HERROR_MEM;
			break;
		}
		do
		{
			ret2 = httpin_recv_content(sock, recvbuf,
										RECVBUFSIZE, 
										&cnt, 											&(param->running_flag));
			if(cb(recvbuf, cnt, ctx, HTTPIN_PROGRESS))
			{
				ret = -2;
				break;
			}
		}while(!ret2);
		if(-2 == ret2)
			ret = HERROR_CALLBACK;
		else
			ret = HERROR_OK;	
	}while(0);
	
	/* clean up */
	if(-1 != sock)
		httpin_close(sock);
	httpin_free(recvbuf);
	httpin_free(tmpbuf);
	switch(ret)
	{
		case HERROR_REDIRECT:
			(param->redirect_count)++;
			ret = httpin_get(param, cb, ctx);
			(param->redirect_count)--;
			break;
		case HERROR_OK:
			cb(NULL, 0, ctx, HTTPIN_FINISHED);
			break;
		default:
			cb(NULL, 0, ctx, HTTPIN_ERROR);
			break;	
	}	
	printf("ret = %d\n", ret);
	return ret;	
}


/***************************************************************/
static INT32 httpin_get_host_from_url(const UINT8* url, UINT8* buf, UINT32 bufsize)
{
	UINT32 cnt;
	UINT8* host;
	UINT8* pA;
	host = strstr(url, HTTP_PROTOCAL);
	if(NULL == host)
		return -1;
	host += sizeof(HTTP_PROTOCAL) - 1;
	pA = strpbrk(host, ":/");
//	pA = strchr(host, '/');
	if(NULL == pA)
		cnt = strlen(host);
	else
		cnt = pA - host;
	if(cnt >= bufsize)
		return -2;
	memcpy(buf, host, cnt);
	buf[cnt] = 0;		
	return 0;
} 

static INT32 httpin_set_header_from_url(const UINT8* url, const UINT8* ex_header,httpin_method method, UINT8* header_str, UINT32 bufsize)
{
	UINT16 cnt;
	UINT8* pA;
	UINT8 tmpbuf[HEADER_BUFSIZE];
	UINT8 tmpbuf2[HEADER_BUFSIZE];

	/* method*/
	switch(method)
	{
		case HMETHOD_GET:
			strcpy(tmpbuf, "GET ");
			break;
		case HMETHOD_POST:
			strcpy(tmpbuf, "POST ");
			break;
		default:
			break;
	}
	pA = strstr(url, HTTP_PROTOCAL);
	if(NULL == pA)
		return -2;
	pA += sizeof(HTTP_PROTOCAL) - 1;
	pA = strchr(pA, '/');
	if(NULL == pA)
		strcat(tmpbuf, "/");
	else
		strcat(tmpbuf, pA);
	sprintf(tmpbuf2, "%s HTTP/1.0\r\n", tmpbuf);
	cnt = strlen(tmpbuf2);
	if(bufsize <= cnt)
		return -1;
	else
		sprintf(header_str, "%s", tmpbuf2);

	/* user agent*/
	sprintf(tmpbuf2, "User-Agent: %s\r\n", HTTPIN_USER_AGENT);
	cnt += strlen(tmpbuf2);
	if(bufsize <= cnt)
		return -1;
	else
		strcat(header_str, tmpbuf2);
	
	/* host */
	if(0 != httpin_get_host_from_url(url, tmpbuf, HEADER_BUFSIZE))
		return -2;
	sprintf(tmpbuf2, "Host: %s\r\n", tmpbuf);
	cnt += strlen(tmpbuf2);
	if(bufsize <= cnt)
		return -1;
	else
		strcat(header_str, tmpbuf2);
	
	/* extra header*/
	if(NULL != ex_header)
	{
		cnt += strlen(ex_header);
		if(bufsize <= cnt)
			return -1;
		else
			strcat(header_str, ex_header);
	}
	
	/* ended \r\n */
	cnt += 2;
	if(bufsize <= cnt)
		return -1;
	else
		strcat(header_str, "\r\n");
	return 0;
}
static INT32 httpin_get_header_from_sock(INT32 sock, UINT8* buf)
{
	INT32 bytes_read = 0;
	INT32 ret = -1;
	INT32 ret2;
	fd_set set;
	struct timeval tv = {10, 0};
	FD_ZERO(&set);
	FD_SET(sock, &set);
	while(httpin_select(sock + 1, &set, NULL, NULL, &tv) > 0)
	{
		ret2 = httpin_recv(sock, buf + bytes_read, 1, 0);
//		httpin_debug("%c", *(buf + bytes_read));
		bytes_read += ret2;
		if(bytes_read >= 4 && !strncmp(&buf[bytes_read - 4], "\r\n\r\n", 4))
		{
			ret = 0;
			buf[bytes_read] = 0;
			break;
		}
		if(ret2 <= 0)
		{
			break;
		}
	}
	return ret;
}

static INT32 parse_header_info(UINT8* header_str, header_info* inf)
{
	INT32 ret = RESPOND_ERROR;
	UINT8* pA = header_str;
	if(NULL == inf->redirection_url)
		return ret;
	
	pA += strlen("HTTP/1.X ");
	switch(*pA)
	{
		case '2':			//OK
			pA = strstr(header_str, HTTP_CONTENT_LENGTH);
			if(NULL == pA)
				inf->content_length = 0;
			else
			{
				pA += (strlen(HTTP_CONTENT_LENGTH) + 1);
				inf->content_length = atoi(pA);
			}
			ret = RESPOND_OK;
			break;
		case '3':			//redirection
			pA = strstr(header_str, HTTP_LOCATION);
			if(NULL == pA)
				ret = RESPOND_ERROR;
			else
			{
				pA += (strlen(HTTP_LOCATION) + 1);
				inf->content_length =(INT32)
									((UINT32)strstr(pA, "\r\n") - (UINT32)pA);
				memcpy(inf->redirection_url, pA, inf->content_length);
				inf->redirection_url[inf->content_length] = 0;
				httpin_debug("redirect:%s\n", inf->redirection_url);
				ret = RESPOND_REDIRECTION;
			}
			break;
		case '4':			//request error
		case '5':			//server error
		default:
			ret = RESPOND_ERROR;
			break;		
	}
	return ret;
}
static INT32 httpin_recv_content(INT32 sock,
								UINT8* buf,
								INT32 bufsize,
								INT32* recvsize,
								UINT8* running_flag)
{
	INT32 ret = 0;
	INT32 bytes_read;
	*recvsize = 0;
	fd_set set;
	struct timeval tv = {30, 0};
	FD_ZERO(&set);
	FD_SET(sock, &set);
	while(bufsize > 0)
	{
		bytes_read = httpin_select(sock + 1, &set, NULL, NULL, &tv);
		if(bytes_read <= 0)
		{
			ret = -1;
			break;
		}
		bytes_read = httpin_recv(sock, buf + *recvsize, bufsize, 0);
		if(bytes_read)
		{
			bufsize -= bytes_read;
			(*recvsize) += bytes_read;
		}
		else
		{
			ret = -1;
			break;
		}
	}
	return ret;
}
static INT32 httpin_sock_create(const UINT8* url, INT32* sock)
{
	struct hostent *hent;
	struct sockaddr_in dest;
	UINT8* pA;
	UINT8 tmpbuf[TMPBUFSIZE];
	if(NULL == sock)
		return -5;
	if(0 != httpin_get_host_from_url(url, tmpbuf, TMPBUFSIZE))
		return -1;
	hent = httpin_gethostbyname(tmpbuf);
	if(NULL == hent)
		return -2;		
	/* create socket */
	*sock = httpin_socket(AF_INET, SOCK_STREAM, 0);
	if(0 > *sock)
		return -3;		
	/* connect */
	memset(&dest, 0, sizeof(dest));
	dest.sin_family = AF_INET;
	pA = strstr(url, tmpbuf) + strlen(tmpbuf);	//port
	if(':' == *pA)
		dest.sin_port = htons(atoi(++pA));
	else
		dest.sin_port = htons(80);
	memcpy(&dest.sin_addr, hent->h_addr, sizeof(struct in_addr));
	if(0 != httpin_connect(*sock, (struct sockaddr*)&dest, sizeof(dest)))
	{
		httpin_close(*sock);
		*sock = -1;
		return -4;
	}else
		return 0;
}


static INT32 httpin_default_cb(void* buf, UINT32 size, void* ctx, UINT32 status)
{
	UINT8 tmpbuf[0x10000];
	switch(status)
	{
		case HTTPIN_HEADER:
			httpin_debug("HTTPIN_HEADER\n");
			break;
		case HTTPIN_START:
			httpin_debug("HTTPIN_START\n");
			break;
		case HTTPIN_PROGRESS:
			memcpy(tmpbuf, buf, size);
			tmpbuf[size] = 0;
			httpin_debug("%s", tmpbuf);
			break;
		case HTTPIN_FINISHED:
			httpin_debug("HTTPIN_FINISHED\n");
			break;
		case HTTPIN_ABORT:
			httpin_debug("HTTPIN_ABORT\n");
			break;
		case HTTPIN_ERROR:
			httpin_debug("HTTPIN_ERROR\n");
			break;
		default:
			break;
	}
	return 0;	
}



