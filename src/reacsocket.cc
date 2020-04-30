#include "reacsocket.h"
#include "ringbuf.h"
#include "settings.h"
#include "constants.h"

#include <cstdio>
#include <cstring>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
// #include <sys/select.h>
// #include <sys/socket.h>
#include <linux/if_packet.h>
#include <pthread.h>

static inline
int bind_to_if(int fd, int ifindex)
{
	struct sockaddr_ll sa = {0};
	sa.sll_family = AF_PACKET;
	sa.sll_protocol = htons(ETH_P_ALL);
	sa.sll_ifindex = ifindex;
	if(bind(fd, (sockaddr *)&sa, sizeof(sa))<0) {
		perror("bind");
		return __LINE__;
	}
	return 0;
}

static inline
int set_promiscuous(int fd, int ifindex)
{
	struct packet_mreq mreq = {0};
	mreq.mr_type = PACKET_MR_PROMISC;
	mreq.mr_ifindex = ifindex;
	mreq.mr_alen = 0;
	mreq.mr_address[0] ='\0';
	if(setsockopt(fd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, (void *)&mreq, sizeof(mreq))<0) {
		perror("setsockopt PACKET_ADD_MEMBERSHIP");
		return __LINE__;
	}
	return 0;
}

int reacsocket::init_socket()
{
	struct ifreq ifr = {0};

	strncpy(ifr.ifr_name, settings::if_name, IFNAMSIZ-1);

	// create socket
	sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if(sock<0) {
		perror("socket");
		return __LINE__;
	}

	// TODO: Get interface index earlier than getting hwaddr.
	// TODO: Use interface index to get hwaddr.
	// SIOCGIFNAME: ifr_ifindex -> ifr_name

	// get hwaddr
	ifr.ifr_addr.sa_family = AF_INET;
	if(ioctl(sock, SIOCGIFHWADDR, &ifr)<0) {
		perror("ioctl(SIOCGIFHWADDR)");
		return __LINE__;
	}
	memcpy(hwaddr, ifr.ifr_hwaddr.sa_data, ETHER_ADDR_LEN);
#ifdef DEBUG
	if(settings::verbose>2) fprintf(stderr, "interface address is %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",
			hwaddr[0], hwaddr[1], hwaddr[2],
			hwaddr[3], hwaddr[4], hwaddr[5] );
#endif // DEBUG
	uint8_t nul[ETHER_ADDR_LEN] = {0};
	if(!memcmp(hwaddr, nul, ETHER_ADDR_LEN)) {
		fprintf(stderr, "cannot get HW address\n");
		return __LINE__;
	}

	// get interface index
	if(ioctl(sock, SIOCGIFINDEX, &ifr)<0) {
		perror("ioctl(SIOCGIFINDEX)");
		return __LINE__;
	}
	ifindex = ifr.ifr_ifindex;
#ifdef DEBUG
	if(settings::verbose>2) fprintf(stderr, "reacsocket[%p] interface index is %d\n", this, ifindex);
#endif // DEBUG

	if(int r=bind_to_if(sock, ifindex)) return r;

	if(int r=set_promiscuous(sock, ifindex)) return r;

	// for loop
	FD_ZERO(&fds);
	FD_SET(sock, &fds);
	max_fds = sock+1;
	return 0;
}

int reacsocket::next_packet(uint8_t *buf, size_t buflen)
{
	struct sockaddr_ll rll = {0};
	socklen_t rll_size = sizeof(rll);
	int len = recvfrom(sock, buf, buflen, 0, (sockaddr*)&rll, &rll_size);
	return len;
}

struct routine_s
{
	ringbuf &rb;
	reacsocket &rs;
	routine_s(ringbuf &rb_, reacsocket &rs_) :rb(rb_), rs(rs_) {}
};

static
void *routine(void *arg)
{
	ringbuf &rb = ((routine_s*)arg)->rb;
	reacsocket &rs = ((routine_s*)arg)->rs;
	for(;;) {
		ringbuf::buf_s &b = rb.write_nextbuf();
		int ret = rs.next_packet(b.buf, RCV_MAX);
		if(ret>0) {
			b.len = ret;
			if(constants::check_protocol(b.buf)) {
				rb.wrote();
			}
		}
	}
	return NULL;
}

void reacsocket::start_thread(class ringbuf &rb)
{
	pthread_t p;
	routine_s *arg = new routine_s(rb, *this);
	pthread_create(&p, 0, routine, arg);
}
