#ifndef SETTINGS_H
#define SETTINGS_H

#ifdef SETTINGS_INSTANCIATE
# define E
# define V(v) = v
#else
# define E extern
# define V(v)
#endif

enum reac_mode_e {
	reacmode_master,
	reacmode_slave,
	reacmode_split,
	reacmode_promiscuous,
	reacmode_file,
	reacmode_pcap_file,
	reacmode_pcap_dev,
};
enum save_mode_e {
	save_none=0,
	save_packets,
	save_pcm, save_pcm16,
	save_wave24, save_wave16,
};

namespace settings
{
	E const char *if_name V("eth0");
	E enum reac_mode_e mode V(reacmode_promiscuous);
	E enum save_mode_e save_mode V(save_none);
	E int verbose V(0);

	E uint64_t skip_channel V(0);
	E uint64_t stereo_channel V(0);

	E const char *of_name[40] V({0});
	E bool print_control;
}

#undef E
#undef V
#endif // SETTINGS_H
