#pragma once

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

struct fileio_callback_s
{
	size_t (*write)(void *, const void *buffer, size_t size);
	int (*seek)(void *, size_t offset, int whence);
	void (*flush)(void *);
	int (*close)(void *);
};

struct fileio_s
{
	struct fileio_callback_s cb;
	void *priv;
};

typedef struct fileio_s fileio_t;

fileio_t *create_fastwrite(const char *name);

static inline size_t fio_write(fileio_t *fio, const void *buffer, size_t size)
{
	return fio->cb.write(fio->priv, buffer, size);
}

static inline int fio_seek(fileio_t *fio, size_t offset, int whence)
{
	return fio->cb.seek(fio->priv, offset, whence);
}

static inline void fio_flush(fileio_t *fio)
{
	fio->cb.flush(fio->priv);
}

static int fio_close(fileio_t *fio)
{
	int ret = fio->cb.close(fio->priv);
	free(fio);
	return ret;
}

#ifdef __cplusplus
}
#endif
