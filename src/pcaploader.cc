#include <cstdio>
#include <cstdint>
#include <pcap.h>

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
