#include <stdio.h>
#include <string.h>
#include "fastwrite.h"

#define N_BUFFER (4*1024*1024)

struct priv_s
{
	FILE *fp;
	char buffer[N_BUFFER];
	size_t i_bufuer;
};

static void fw_flush(void *priv)
{
	struct priv_s *p = priv;

	if (p->i_bufuer > 0)
		fwrite(p->buffer, p->i_bufuer, 1, p->fp);
	p->i_bufuer = 0;
}

static size_t fw_write(void *priv, const void *buffer, size_t size)
{
	struct priv_s *p = priv;

	if (p->i_bufuer + size > N_BUFFER)
		fw_flush(priv);

	memcpy(p->buffer + p->i_bufuer, buffer, size);
	p->i_bufuer += size;
	return size;
}

static int fw_seek(void *priv, size_t offset, int whence)
{
	struct priv_s *p = priv;

	fw_flush(priv);

	return fseek(p->fp, offset, whence);
}

static int fw_close(void *priv)
{
	struct priv_s *p = priv;

	fw_flush(priv);
	fclose(p->fp);
	free(priv);
	return 0;
}

fileio_t *create_fastwrite(const char *name)
{
	FILE *fp = fopen(name, "wb");
	if (!fp) {
		return NULL;
	}

	fileio_t *ret = calloc(sizeof(fileio_t), 1);
	struct priv_s *p = calloc(sizeof(struct priv_s), 1);

	p->fp = fp;

	ret->cb.write = fw_write;
	ret->cb.seek = fw_seek;
	ret->cb.flush = fw_flush;
	ret->cb.close = fw_close;
	ret->priv = p;

	return ret;
}
