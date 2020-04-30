
#include "ringbuf.h"

ringbuf::ringbuf()
{
	start = 0;
	end = 0;
	mutex = PTHREAD_MUTEX_INITIALIZER;
}

void ringbuf::wrote()
{
	pthread_mutex_lock(&mutex);
	if(++end==BUF_SIZE) end=0;
	// TODO: check overrun
	pthread_mutex_unlock(&mutex);
}

bool ringbuf::has_data()
{
	pthread_mutex_lock(&mutex);
	bool b = start!=end;
	pthread_mutex_unlock(&mutex);
	return b;
}
