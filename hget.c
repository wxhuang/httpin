#include "config.h"
#include "httpin.h"

typedef struct _hget_ctx
{
	unsigned char* buffer;
	unsigned int bufsize;
	unsigned int bytes_read;
}hget_ctx;
int hget_cb(void* buf, unsigned int size, void* pctx, unsigned int status)
{
	int ret = 0;
	unsigned int tmp;
	hget_ctx* ctx = pctx;
	switch(status)
	{
		case HTTPIN_HEADER:
			break;
		case HTTPIN_START:
			break;
		case HTTPIN_PROGRESS:
			tmp = ctx->bufsize - ctx->bytes_read;
			if(size > tmp)
			{
				size = tmp;
				ret = -1;
			}
			memcpy(ctx->buffer + ctx->bytes_read, buf, size);
			ctx->bytes_read += size;
			break;
		case HTTPIN_FINISHED:
			ctx->buffer[ctx->bytes_read] = 0;
		case HTTPIN_ABORT:
		case HTTPIN_ERROR:
		default:
			break;
	}
	return ret;
}

int hget(unsigned char* url, unsigned char* buffer, int bufsize)
{
	int ret;
	char urlbuf[1024];
	strcpy(urlbuf, url);
	httpin_param param;
	memset(&param, 0, sizeof(param));
	param.url = urlbuf;
	param.running_flag = 1;

	hget_ctx ctx;
	ctx.buffer = buffer;
	ctx.bufsize = (unsigned int)bufsize;
	ctx.bytes_read = 0;
	
	ret = httpin_get(&param, hget_cb, &ctx);
	printf("%s\n", ctx.buffer);
	printf("recvsize = %d\n", ctx.bytes_read);
	return ret;
}

int main()
{
	INT32 ret;
	UINT8* buf = malloc(0x40000);
	ret = hget("http://gdata.youtube.com/feeds/api/videos", buf, 0x40000);
	printf("%d\n", ret);
	free(buf);
	return 0;
}


