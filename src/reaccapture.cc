#include "reaccapture-config.h"
#include <cstdio>
#include <cstring>
#include <cassert>
#include <cstdlib>
#include <cctype>
#include <unistd.h>
#include <sys/time.h>

// to suppress warning
#include "mac-reacdriver/REACDataStream.h"
#include "sys/kpi_mbuf.h"
#undef ETHER_ADDR_LEN

#include <sys/select.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <net/ethernet.h> /* the L2 protocols */
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/select.h>

#include "reacsocket.h"
#include "ringbuf.h"
#define SETTINGS_INSTANCIATE
#include "settings.h"
#undef SETTINGS_INSTANCIATE

// instances
static int sock;
static int g_ifindex;
uint8_t interfaceAddr[ETHER_ADDR_LEN];
uint8_t master_addr[ETHER_ADDR_LEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
bool connected = 0;
enum HandshakeState {
	HANDSHAKE_NOT_INITIATED = 0,
	HANDSHAKE_GOT_MASTER_ANNOUNCE,
	HANDSHAKE_SENT_FIRST_ANNOUNCE,
	HANDSHAKE_GOT_SECOND_MASTER_ANNOUNCE,
	HANDSHAKE_CONNECTED
};
static HandshakeState g_handshake_state;
static unsigned int recievedPacketCounter = 0;
static int n_master_in_channels=0, n_master_out_channels=0;
static uint8_t splitIdentifier;
FILE *fp_outputs[40] = {0};
int n_skipped_counter = 0;
uint16_t counter_recv_last=0, counter_recv_last1=0;
uint32_t counter_recv_upper=0;
bool counter_recv_second = 0;
uint32_t n_samples = 0;

static int get_hwaddr(int fd)
{
	return 0; // TODO implement me
}

static
int write_packets(const uint8_t *data, int len)
{
	FILE *fp = stdout;
	fwrite(&len, sizeof(int), 1, fp);
	fwrite(data, len, 1, fp);
	return 0;
}

static int got_msg(const uint8_t *data_packet, int len_packet);

static
int load_packets(FILE *fp)
{
	int len;
	while(fread(&(len=0), sizeof(int), 1, fp)==1 && len>0) {
		uint8_t data[len];
		if(fread(data, len, 1, fp)!=1) {
			perror("fread");
			break;
		}
		got_msg(data, len);
	}
	return 0;
}

static
int save_pcm_init()
{
	for(int i=0; i<40; i++) {
		if(settings::skip_channel & (1LL<<i)) continue;
		char name[64]; sprintf(name, "output-%02d.pcm", i+1);
		fp_outputs[i] = (settings::stereo_channel & (1LL<<i) && i&1) ? fp_outputs[i-1] : fopen(name, "wb");
	}
	return 0;
}

static
int save_wav_init()
{
	for(int i=0; i<40; i++) {
		if(settings::skip_channel & (1LL<<i)) continue;
		if(settings::stereo_channel & (1LL<<i) && i&1) {
			fp_outputs[i] = fp_outputs[i-1];
		}
		else {
			char name[64]; sprintf(name, "output-%02d.wav", i+1);
			fp_outputs[i] = fopen(name, "wb");
			uint8_t data[44] = {"RIFF"};
			fwrite(data, 44, 1, fp_outputs[i]);
		}
	}
	return 0;
}

static inline uint8_t* add32n(uint8_t *p, uint32_t n) { *p++=n&0xFF; *p++=(n>>8)&0xFF; *p++=(n>>16)&0xFF; *p++=(n>>24)&0xFF; return p; }
static inline uint8_t* add16n(uint8_t *p, uint16_t n) { *p++=n&0xFF; *p++=(n>>8)&0xFF; return p; }
static inline uint8_t* add32s(uint8_t *p, const char *s) { memcpy(p, s, 4); return p+4; }

static
int save_wav_fin()
{
	uint8_t headers[2][44];
	uint32_t n_byte_sample = settings::save_mode==save_wave24 ? 3 : settings::save_mode==save_wave16 ? 2 : -1;
	uint32_t n_data_byte = n_samples *n_byte_sample;
	for(uint32_t n_channel=1; n_channel<=2; n_channel++) {
		uint8_t *p = headers[n_channel-1];

		// RIFF header
		p = add32s(p, "RIFF");
		p = add32n(p, n_byte_sample+36); // size

		// WAVE header
		p = add32s(p, "WAVE");

		// fmt chunk
		p = add32s(p, "fmt ");
		p = add32n(p, 16); // size of fmt chunk
		p = add16n(p, 1); // format: 1=linear PCM
		p = add16n(p, n_channel); // #channels: 1=mono, 2=stereo
		p = add32n(p, 48000); // sampling rate
		p = add32n(p, n_byte_sample*48000); // data rate [B/s]
		p = add16n(p, n_byte_sample*n_channel); // block size [B/sample], if stereo, make it twice.
		p = add16n(p, n_byte_sample*8); // bit/sample, independent to stereo or mono.

		// data chunk
		p = add32s(p, "data");
		p = add32n(p, n_data_byte*n_channel);
	}

	for(int i=0; i<40; i++) if(fp_outputs[i]) {
		uint32_t n_channel = (settings::stereo_channel & (1LL<<i)) ? 2 : 1;
		if(n_channel==2 && i&1) continue;
		FILE *fp = fp_outputs[i];
		fseek(fp, 0, SEEK_SET);
		fwrite(headers[n_channel-1], 44, 1, fp);
		fclose(fp); fp_outputs[i]=0;
	}
	return 0;
}

static
void save_pcm_data_s24be(const uint8_t *data, int skipped)
{
	if(skipped) {
		int len = skipped * 12 * 3;
		uint8_t x[len]; memset(x, 0, len);
		fprintf(stderr, "padding %d samples\n", len/3);
		for(int i=0; i<40; i++)
			if(fp_outputs[i]) fwrite(x, len, 1, fp_outputs[i]);
	}

	for(int j=0; j<12; j++) {
		for(int i=0; i<40; i+=2) {
			uint8_t x[3] = {data[1], data[0], data[3]};
			uint8_t y[3] = {data[2], data[5], data[4]};
			if(fp_outputs[i+0]) fwrite(x, 3, 1, fp_outputs[i+0]);
			if(fp_outputs[i+1]) fwrite(y, 3, 1, fp_outputs[i+1]);
			data += 6;
		}
	}
}

static
void save_pcm_data_s24le(const uint8_t *data, int skipped)
{
	if(skipped) {
		int len = skipped * 12 * 3;
		uint8_t x[len]; memset(x, 0, len);
		fprintf(stderr, "padding %d samples\n", len/3);
		for(int i=0; i<40; i++)
			if(fp_outputs[i]) fwrite(x, len, 1, fp_outputs[i]);
	}

	for(int j=0; j<12; j++) {
		for(int i=0; i<40; i+=2) {
			uint8_t x[3] = {data[3], data[0], data[1]};
			uint8_t y[3] = {data[4], data[5], data[2]};
			if(fp_outputs[i+0]) fwrite(x, 3, 1, fp_outputs[i+0]);
			if(fp_outputs[i+1]) fwrite(y, 3, 1, fp_outputs[i+1]);
			data += 6;
		}
	}
}

static
void save_pcm_data_s16be(const uint8_t *data, int skipped)
{
	if(skipped) {
		int len = skipped * 12 * 2;
		uint8_t x[len]; memset(x, 0, len);
		fprintf(stderr, "padding %d samples\n", len/3);
		for(int i=0; i<40; i++)
			if(fp_outputs[i]) fwrite(x, len, 1, fp_outputs[i]);
	}

	for(int j=0; j<12; j++) {
		for(int i=0; i<40; i+=2) {
			uint8_t x[2] = {data[1], data[0]};
			uint8_t y[2] = {data[2], data[5]};
			if(fp_outputs[i+0]) fwrite(x, 2, 1, fp_outputs[i+0]);
			if(fp_outputs[i+1]) fwrite(y, 2, 1, fp_outputs[i+1]);
			data += 6;
		}
	}
}

static
void save_pcm_data_s16le(const uint8_t *data, int skipped)
{
	if(skipped) {
		int len = skipped * 12 * 2;
		uint8_t x[len]; memset(x, 0, len);
		fprintf(stderr, "padding %d samples\n", len/3);
		for(int i=0; i<40; i++)
			if(fp_outputs[i]) fwrite(x, len, 1, fp_outputs[i]);
	}

	for(int j=0; j<12; j++) {
		for(int i=0; i<40; i+=2) {
			uint8_t x[2] = {data[0], data[1]};
			uint8_t y[2] = {data[5], data[2]};
			if(fp_outputs[i+0]) fwrite(x, 2, 1, fp_outputs[i+0]);
			if(fp_outputs[i+1]) fwrite(y, 2, 1, fp_outputs[i+1]);
			data += 6;
		}
	}
}

static uint8_t check_checksum(const REACPacketHeader *packet)
{
	uint8_t sum = 0;
	for (int i=0; i<32; i++)
		sum += packet->data[i];
	return sum;
}

static void apply_checksum(REACPacketHeader *packet)
{
	uint8_t sum = 0;
	for (int i=0; i<31; i++)
		sum += packet->data[i];
	packet->data[31] = -sum;
}

static void split_got_packet(const REACPacketHeader *packetHeader);

static
int got_msg(const uint8_t *data_packet, int len_packet)
{
#define ETHER_HEADER_LEN (6*2+2)
	const EthernetHeader *ethernetHeader = (EthernetHeader*)data_packet;

	// REACConnection::filterInputFunc
	if(memcmp(ethernetHeader->type, REACConstants::PROTOCOL, sizeof(ethernetHeader->type))) {
#ifdef DEBUG
		fprintf(stderr, "got_msg: ignoring type=%.2x:%.2x\n", ethernetHeader->type[0], ethernetHeader->type[1]);
#endif // DEBUG
		return 0;
	}

	recievedPacketCounter++;

	const uint8_t *data = data_packet + ETHER_HEADER_LEN;
	int len = len_packet - ETHER_HEADER_LEN;
#ifdef DEBUG
	printf("recvfrom returns %d len=%d:", len_packet, len);
	for(int i=0; i<len_packet && i<len_packet; i++) { printf(" %02X", data_packet[i]); } printf("\n");
#endif // DEBUG
	// TODO: why not check destination address?
	// TODO: see REACConnection::filterCommandGateMsg

	if (len < sizeof(REACPacketHeader)+sizeof(REACConstants::ENDING)) {
		fprintf(stderr, "Error: too short packet %d\n", len);
		return 1;
	}

	// packet ending
	if(memcmp(data+len-sizeof(REACConstants::ENDING), REACConstants::ENDING, sizeof(REACConstants::ENDING))) {
		fprintf(stderr, "ENDING not match: %02X %02X, expected %02X %02X\n",
				data[len-sizeof(REACConstants::ENDING)],
				data[len-sizeof(REACConstants::ENDING)+1],
				REACConstants::ENDING[0],
				REACConstants::ENDING[1] );
		return 1;
	}

	const REACPacketHeader *packetHeader = (const REACPacketHeader*)data;

	// It seems that packet with type=00:00 don't have the check-sum.
	if(packetHeader->type[0] || packetHeader->type[1]) {
		if(uint8_t s = check_checksum(packetHeader)) {
			fprintf(stderr, "Error: checksum failed sum=%02X, counter=%02X:%02X, type=%02X:%02X", s, packetHeader->counter[0], packetHeader->counter[1], packetHeader->type[0], packetHeader->type[1]);
			for(int i=0; i<32+1; i++) { fprintf(stderr, " %02X", packetHeader->data[i]); } putc('\n', stderr);
		}
		else {
			if(settings::mode==reacmode_split) {
				split_got_packet(packetHeader);
			}
		}
	}


	{
		uint16_t c = packetHeader->counter[0] | packetHeader->counter[1]<<8;
		if(counter_recv_second) {
			if(c < counter_recv_last)
				counter_recv_upper += 1;
			uint16_t skipped = (c - counter_recv_last - 1) & 0xFFFF;
			n_skipped_counter += skipped;
			if(skipped) {
				int ss = ((counter_recv_upper<<16 | c) - counter_recv_last1) / 4000;
				int mm = ss/60; ss%=60;
				int hh = mm/60; mm%=60;
				fprintf(
						stderr,
						"Error: missing %d packet(s) at %02d:%02d:%02d\n",
						skipped,
						hh, mm, ss
						);
			}
		}
		else {
			counter_recv_second = 1;
			counter_recv_last1 = c;
		}
		counter_recv_last = c;
	}

	n_samples += 12 * (1+n_skipped_counter);

	if(settings::save_mode == save_packets) {
		write_packets(data_packet, len_packet);
		return 0;
	}

	// TODO: receive audio, see copyAudioFromMbufToBuffer and txt2wavtxt.awk
	// Not only type=0000, all packet types include the audio data.
	if(settings::save_mode==save_pcm) {
		save_pcm_data_s24be(data + sizeof(REACPacketHeader), n_skipped_counter);
		n_skipped_counter = 0;
	}
	else if(settings::save_mode==save_pcm16) {
		save_pcm_data_s16be(data + sizeof(REACPacketHeader), n_skipped_counter);
		n_skipped_counter = 0;
	}
	else if(settings::save_mode==save_wave24) {
		save_pcm_data_s24le(data + sizeof(REACPacketHeader), n_skipped_counter);
		n_skipped_counter = 0;
	}
	else if(settings::save_mode==save_wave16) {
		save_pcm_data_s16le(data + sizeof(REACPacketHeader), n_skipped_counter);
		n_skipped_counter = 0;
	}
	return 0;
}

static
void split_got_packet(const REACPacketHeader *packetHeader)
{
	// REACSplitDataStream::gotPacket

	if(!(packetHeader->type[0]==0xCF && packetHeader->type[1]==0xEA)) // not REAC_STREAM_MASTER_ANNOUNCE
		return;

	switch(g_handshake_state) {
		case HANDSHAKE_NOT_INITIATED:
			if(packetHeader->data[6]==0x0D) {
				memcpy(master_addr, packetHeader->data+9, sizeof(master_addr));
				n_master_in_channels = packetHeader->data[15];
				n_master_out_channels = packetHeader->data[16];
				g_handshake_state = HANDSHAKE_GOT_MASTER_ANNOUNCE;
				fprintf(stderr, "handshake: got master announce: master_addr=%02X:%02X:%02X:%02X:%02X:%02X in-channels=%d out-channels=%d\n", master_addr[0], master_addr[1], master_addr[2], master_addr[3], master_addr[4], master_addr[5], n_master_in_channels, n_master_out_channels);
			}
			break;
		case HANDSHAKE_SENT_FIRST_ANNOUNCE:
			if(packetHeader->data[6]==0x0A && !memcmp(packetHeader->data+9, interfaceAddr, sizeof(interfaceAddr))) {
				splitIdentifier = packetHeader->data[16];
				g_handshake_state = HANDSHAKE_GOT_SECOND_MASTER_ANNOUNCE;
				fprintf(stderr, "handshake: got second announce: split identifier=%d\n", splitIdentifier);
			}
			break;
	}
}

static
void split_send_announcement()
{
	// REACConnection::sendSplitAnnouncementPacket
	static unsigned int counterAtLastSplitAnnounce = 0;
	static uint16_t splitAnnouncementCounter = 0;
	assert(settings::mode==reacmode_split);

	const int fillerSize = 288;
	const int fillerOffset = ETHER_HEADER_LEN+sizeof(REACPacketHeader);
	const int endingOffset = fillerOffset+fillerSize;
	const int packetLen = endingOffset+sizeof(REACConstants::ENDING);

	uint8_t data_packet[packetLen];
	memset(data_packet, 0, packetLen);

	EthernetHeader *eth_header = (EthernetHeader*)(data_packet + 0);
	memcpy(eth_header->shost, interfaceAddr, sizeof(eth_header->shost));
	memcpy(eth_header->dhost, master_addr, sizeof(eth_header->dhost));
	memcpy(eth_header->type, REACConstants::PROTOCOL, sizeof(REACConstants::PROTOCOL));

	// REACSplitDataStream::prepareSplitAnnounce
	REACPacketHeader *reac_header = (REACPacketHeader*)(data_packet + ETHER_HEADER_LEN);
	reac_header->type[0] =  0xCE; // REAC_STREAM_SPLIT_ANNOUNCE
	reac_header->type[1] =  0xEA;
	bool to_send = 0;
	if(g_handshake_state==HANDSHAKE_GOT_MASTER_ANNOUNCE) {
		reac_header->data[0] = 0x01;
		reac_header->data[1] = 0x00;
		reac_header->data[2] = 0x7f;
		reac_header->data[3] = 0x00;
		reac_header->data[4] = 0x01;
		reac_header->data[5] = 0x03;
		reac_header->data[6] = 0x08;
		reac_header->data[7] = 0x43;
		reac_header->data[8] = 0x05;
		memcpy(reac_header->data+9, interfaceAddr, sizeof(interfaceAddr));
		g_handshake_state = HANDSHAKE_SENT_FIRST_ANNOUNCE;
		to_send = 1;
	}
	else if(g_handshake_state==HANDSHAKE_GOT_SECOND_MASTER_ANNOUNCE) {
		reac_header->data[0] = 0x01;
		reac_header->data[1] = 0x00;
		reac_header->data[2] = splitIdentifier;
		reac_header->data[3] = 0x00;
		reac_header->data[4] = 0x01;
		reac_header->data[5] = 0x03;
		reac_header->data[6] = 0x08;
		reac_header->data[7] = 0x42;
		reac_header->data[8] = 0x05;
		memcpy(reac_header->data+9, interfaceAddr, sizeof(interfaceAddr));
		g_handshake_state = HANDSHAKE_CONNECTED;
		to_send = 1;
	}
	else if(g_handshake_state==HANDSHAKE_CONNECTED) {
		reac_header->data[0] = 0x01;
		reac_header->data[1] = 0x00;
		reac_header->data[2] = splitIdentifier;
		reac_header->data[3] = 0x00;
		reac_header->data[4] = 0x01;
		reac_header->data[5] = 0x03;
		reac_header->data[6] = 0x02;
		reac_header->data[7] = 0x41;
		reac_header->data[8] = 0x05;
		to_send = 1;
	}
	if(g_handshake_state!=HANDSHAKE_NOT_INITIATED && recievedPacketCounter==counterAtLastSplitAnnounce) {
		fprintf(stderr, "split_send_announcement: disconnected\n");
		g_handshake_state = HANDSHAKE_NOT_INITIATED;
		memset(master_addr, -1, sizeof(master_addr));
	}
	counterAtLastSplitAnnounce = recievedPacketCounter;
	if(!to_send) return;
	reac_header->setCounter(splitAnnouncementCounter++);
	apply_checksum(reac_header);

	memcpy(data_packet+endingOffset, REACConstants::ENDING, sizeof(REACConstants::ENDING));

#ifdef DEBUG
	fprintf(stderr, "split_send_announcement: sending handshake %d\n", (int)g_handshake_state);
#endif // DEBUG
	struct sockaddr_ll rll = {0};
	rll.sll_family = AF_PACKET;
	rll.sll_ifindex = g_ifindex;
	rll.sll_halen = ETHER_ADDR_LEN;
	memcpy(&rll.sll_protocol, REACConstants::PROTOCOL, sizeof(rll.sll_protocol));
	memcpy(rll.sll_addr, master_addr, ETHER_ADDR_LEN);
	socklen_t rll_size = sizeof(rll);
	sendto(sock, data_packet, packetLen, 0, (sockaddr*)&rll, rll_size);
}

int rr_loop(ringbuf &rb)
{
	for(;;) {
		if(!rb.has_data()) {
			usleep(250);
			continue;
		}
		ringbuf::buf_s &buf = rb.read_nextbuf();

		// work for the buf
		got_msg(buf.buf, buf.len);

		rb.read();
	}
	return 0;
}

int load_pcap_offline(FILE *fp, int (*got_msg)(const uint8_t *, int));

int print_summary()
{
	// TODO: fix REAC_PACKETS_PER_SECOND
	int ms = ((counter_recv_upper<<16 | counter_recv_last) - counter_recv_last1) / 4;
	int ss = ms/1000; ms%=1000;
	int mm = ss/60; ss%=60;
	int hh = mm/60; mm%=60;
	printf("Total time: %02d:%02d:%02d.%03d\n", hh, mm, ss, ms);
	printf("%s: %d packet%s dropped.\n", n_skipped_counter?"Error":"Info", n_skipped_counter, n_skipped_counter>1?"s":"");
	return 0;
}

static char *parse_channels_n_to_n(char *arg, uint64_t *channels)
{
	char *nxt = 0;
	fprintf(stderr, "parse_channels_n_to_n: strtol(%s)\n", arg);
	int x = strtol(arg, &nxt, 10);
	if(x<0 || 40<x) return NULL;
	if(nxt==arg) return NULL;
	if(*nxt=='-') {
		int y = strtol(nxt+1, &nxt, 10);
		if(y<x || 40<y) return NULL;
		if(nxt==arg) return NULL;
		for(int i=x; i<=y; i++) *channels |= 1LL << i-1;
	}
	else
		*channels |= 1LL << x-1;
	if(!*nxt || *nxt==',') return nxt;
	return 0;
}

static int parse_channels(char *arg, uint64_t *channels)
{
	fprintf(stderr, "parse_channels: arg=%s\n", arg);
	*channels = 0;
	while(*arg) {
		char *nxt = parse_channels_n_to_n(arg, channels);
		if(!nxt) return __LINE__;
		fprintf(stderr, "parse_channels: nxt=%s\n", nxt);
		if(nxt != arg) arg = nxt; else break;
		while(*arg && !isdigit(*arg)) arg++;
	}
	return 0;
}

static const char *help_message = "\
reaccapture - capture/postprocess REAC packets\n\
\n\
Input options:\n\
	--load-pcap-file\n\
		Load pcap format file. The file can be specified in argument.\n\
		If no file is specified, it will read from stdin.\n\
Output options:\n\
	--save-wav-{16,24}\n\
		Save audio data as 16-bit or 24-bit WAVE format.\n\
	--save-pcm\n\
		Save audio data as 24-bit PCM format.\n\
	--save-pcm-16\n\
		Save audio data as 16-bit PCM format.\n\
	--save-channels arg\n\
		Limit to save channels specified by arg.\n\
Other options:\n\
	-v\n\
		Increase verbose level.\n\
\n\
Version: " PACKAGE_STRING;

int main(int argc, char **argv)
{
	FILE *fp_inp = NULL;
	// parse arguments
	for(int i=1; i<argc; i++) {
		char *ai = argv[i];
		if(ai[0]=='-' && ai[1]=='-') {
			if(!strcmp(ai, "--save-packets"))
				settings::save_mode = save_packets;
			else if(!strcmp(ai, "--save-pcm"))
				settings::save_mode = save_pcm;
			else if(!strcmp(ai, "--save-pcm-16"))
				settings::save_mode = save_pcm16;
			else if(!strcmp(ai, "--save-wav-24"))
				settings::save_mode = save_wave24;
			else if(!strcmp(ai, "--save-wav-16"))
				settings::save_mode = save_wave16;
			else if(!strcmp(ai, "--save-channels")) {
				if(i+1>=argc || parse_channels(argv[++i], &settings::skip_channel)) {
					fprintf(stderr, "Error: option %s\n", ai);
					return __LINE__;
				}
				settings::skip_channel ^= (1LL<<40)-1;
			}
			else if(!strcmp(ai, "--save-channels-8")) {
				if(i+1>=argc || parse_channels(argv[++i], &settings::skip_channel)) {
					fprintf(stderr, "Error: option %s\n", ai);
					return __LINE__;
				}
				settings::skip_channel <<= 8;
				settings::skip_channel ^= (1LL<<40)-1;
			}
			else if(!strcmp(ai, "--stereo-channels")) {
				using settings::stereo_channel;
				if(i+1>=argc || parse_channels(argv[++i], &stereo_channel)) {
					fprintf(stderr, "Error: option %s\n", ai);
					return __LINE__;
				}
				stereo_channel |= (stereo_channel<<1 & 0xAAAAAAAAAALL) | (stereo_channel>>1 & 0x5555555555LL);
			}
			else if(!strcmp(ai, "--stereo-channels-8")) {
				using settings::stereo_channel;
				if(i+1>=argc || parse_channels(argv[++i], &stereo_channel)) {
					fprintf(stderr, "Error: option %s\n", ai);
					return __LINE__;
				}
				stereo_channel <<= 8;
				stereo_channel |= (stereo_channel<<1 & 0xAAAAAAAAAALL) | (stereo_channel>>1 & 0x5555555555LL);
			}
			else if(!strcmp(ai, "--load-file"))
				settings::mode = reacmode_file;
			else if(!strcmp(ai, "--load-pcap-file"))
				settings::mode = reacmode_pcap_file;
			else if(!strcmp(ai, "--split-handshake"))
				settings::mode = reacmode_split;
			else if(!strcmp(ai, "--help")) {
				puts(help_message);
				return 0;
			}
			else {
				fprintf(stderr, "Error: unknown option %s\n", ai);
				return __LINE__;
			}
		}
		else if(*ai=='-') while(char c=*++ai) switch(c) {
			case 'd':
				settings::if_name = argv[++i];
				break;
			case 'v':
				settings::verbose ++;
			default:
				fprintf(stderr, "Error: unknown option %c\n", c);
				return __LINE__;
		}
		else if(!fp_inp) {
			fp_inp = fopen(ai, "rb");
			if(!fp_inp) {
				fprintf(stderr, "Error: cannot open %s\n", ai);
				return __LINE__;
			}
		}
		else {
			fprintf(stderr, "Error: unknown argument %s\n", ai);
		}
	}

	if(!fp_inp) fp_inp = stdin;

	settings::stereo_channel &= ~(settings::skip_channel | (settings::skip_channel<<1 & 0xAAAAAAAAAALL) | (settings::skip_channel>>1 & 0x5555555555LL));

	using settings::verbose;

	if(settings::save_mode==save_pcm || settings::save_mode==save_pcm16)
		save_pcm_init();

	if(settings::save_mode==save_wave24 || settings::save_mode==save_wave16)
		save_wav_init();

	int ret = 0;
	// Won't use NIC but load from a file.
	if(settings::mode==reacmode_file) {
		ret = load_packets(fp_inp) | print_summary();
	}
	else if(settings::mode==reacmode_pcap_file) {
		ret = load_pcap_offline(fp_inp, got_msg) | print_summary();
	}
	else {

		g_handshake_state = HANDSHAKE_NOT_INITIATED;
		reacsocket *rs = new reacsocket();

		ringbuf *rb = new ringbuf;

		if(verbose>1) fprintf(stderr, "initializing interface\n");
		if(int r = rs->init_socket()) return r;

		// start socket thread
		if(verbose>1) fprintf(stderr, "starting socket thread\n");
		rs->start_thread(*rb);

		// loop
		ret = rr_loop(*rb);
	}

	if(settings::save_mode==save_wave24 || settings::save_mode==save_wave16)
		save_wav_fin();

	return ret;
}
