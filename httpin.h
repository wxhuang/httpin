#ifndef _HTTPIN_H_
#define _HTTPIN_H_

#include "config.h"

typedef enum
{
	HTTPIN_HEADER,
	HTTPIN_START,
	HTTPIN_PROGRESS,
	HTTPIN_FINISHED,
	HTTPIN_ABORT,
	HTTPIN_ERROR,
}httpin_status;

typedef enum
{
	HMETHOD_GET,
	HMETHOD_POST,
}httpin_method;

typedef enum
{
	HERROR_REDIRECT		= 1,
	HERROR_OK			= 0,
	HERROR_MEM			= -1,
	HERROR_HEADER		= -2,
	HERROR_RESPOND		= -3,
	HERROR_SOCK			= -4,
	HERROR_CALLBACK		= -5,
	HERROR_UNKNOWN		= -6,
	HERROR_PARAM		= -7,
	HERROR_STACK		= -8,
}httpin_error;

/****************************************************/

typedef struct _httpin_param
{
	UINT8* url;
	UINT8* ex_header;
	UINT8 redirect_count;
	UINT8 running_flag; 
}httpin_param;

typedef INT32 (*httpin_cb)(void* buf, UINT32 size, void* ctx, UINT32 status);

/****************************************************/

httpin_error httpin_get(httpin_param* param,
						httpin_cb cb,
						void* ctx);
#endif
