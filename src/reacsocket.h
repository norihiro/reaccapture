#ifndef REACSOCKET_H
#define REACSOCKET_H

#include <cstdint>
#include <net/ethernet.h> /* the L2 protocols */

class reacsocket
{
	int sock;
	int ifindex;
	uint8_t hwaddr[ETHER_ADDR_LEN];

	// for loop
	fd_set fds;
	int max_fds;

	public:
	int init_socket();
	int next_packet(uint8_t *buf, size_t buflen);
	void start_thread(class ringbuf &rb);

	int get_socket() const { return sock; }
	const uint8_t *get_hwaddr() const { return hwaddr; }
};

#endif // REACSOCKET_H
