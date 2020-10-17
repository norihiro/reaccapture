#include <cstdio>
#include <cstdint>
#include <pcap.h>
#include <signal.h>

static bool is_interrupted = 0;
static pcap_t *p_int;

void interrupted(int)
{
	is_interrupted = 1;
	if (p_int) pcap_breakloop(p_int);
}

int load_pcap_offline(FILE *fp, int (*got_msg)(const uint8_t *, int))
{
	char errbuf[PCAP_ERRBUF_SIZE];

	pcap_t *p = pcap_fopen_offline(fp, errbuf);
	if(!p) {
		fputs(errbuf, stderr);
		return __LINE__;
	}

	struct pcap_pkthdr header;
	while(const uint8_t *payload = pcap_next(p, &header)) {
		got_msg(payload, header.caplen);
	}

	return 0;
}

int load_pcap_device(const char *if_name, int (*got_msg)(const uint8_t *, int))
{
	char errbuf[PCAP_ERRBUF_SIZE];

	pcap_t *p = pcap_create(if_name, errbuf);

	if(!p) {
		fputs(errbuf, stderr);
		return __LINE__;
	}

	pcap_set_promisc(p, 1);
	// TODO: pcap_set_buffer_size(p, TBD);

	pcap_activate(p);
	// TODO: consider to set a filter

	p_int = p;
	signal(SIGINT, interrupted);

	struct pcap_pkthdr header;
	while(const uint8_t *payload = pcap_next(p, &header)) {
		if (is_interrupted) break;
		got_msg(payload, header.caplen);
	}

	return 0;
}
