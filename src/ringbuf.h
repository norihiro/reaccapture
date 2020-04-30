#ifndef RINGBUF_H
#define RINGBUF_H

#include <pthread.h>
#include <cstdint>

// constants
#define RCV_MAX 1664
#define BUF_SIZE 1200 // usually 600 packets are lost, TODO: need to adjust

class ringbuf
{
	public:
	struct buf_s
	{
		size_t len;
		uint8_t buf[RCV_MAX];
	};
	private:

	volatile size_t start, end;
	pthread_mutex_t mutex;
	buf_s ring[BUF_SIZE];

	public:
	ringbuf();

	// write side
	buf_s &write_nextbuf() { return ring[end]; }
	void wrote();

	// read side
	bool has_data();
	buf_s &read_nextbuf() { return ring[start]; }
	void read() { if(++start==BUF_SIZE) start=0; }
};

#endif // RINGBUF_H
