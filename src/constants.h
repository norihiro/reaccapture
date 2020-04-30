#ifndef REAC_CONSTANTS_H
#define REAC_CONSTANTS_H

namespace constants
{
	inline
	bool check_protocol(const uint8_t *mac_packet)
	{
		const uint8_t *type = mac_packet + ETHER_ADDR_LEN*2;
		return type[0]==0x88 && type[1]==0x19;
	}
}

#endif // REAC_CONSTANTS_H
