# Linux REAC capture

The reaccapture is a user-level software to capture audio data from REAC
equipment.

# Usage
1. capture packets with tcpdump
   `tcpdump -i eth0 -w a.pcap`
2. use reaccapture to convert to wave files.
   `reaccapture --load-pcap-file a.pcap --save-wav-24`

# TODO
- More organized arguments.
- Use libpcap to capture packets and convert to wave file directly.
  - When installing, set these capabilities.
    - `cap\_net\_admin,cap\_net\_raw+eip` for users to capture and send packets.
    - `cap\_sys\_nice` to increase self nice.

# License
This code is released under the General Public License version 3. See the file
'COPYING' for the full license.

# Thanks
This code is based on another software 'reacdriver', which is a kernel driver
to communicate with a REAC equipment on OS X. The author would like to thank
Per Grön for his release of the ['reacdriver' on GitHub](https://github.com/per-gron/reacdriver)
