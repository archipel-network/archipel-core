#ifndef CMDLINE_H_INCLUDED
#define CMDLINE_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>

struct ud3tn_cmdline_options {
	char *eid; // e.g.: dtn://ops-sat.dtn
	char *cla_options; // e.g.: tcpspp:*,3333,false,1;tcpcl:*,4356
	char *aap_socket; // e.g.: /tmp/ud3tn.socket
	char *aap_node; // e.g.: 127.0.0.1
	char *aap_service; // e.g.: 4242
	uint8_t bundle_version;
	bool status_reporting;
	bool exit_immediately; // after parsing --help or --usage etc.
	uint64_t mbs; // maximum bundle size
	uint64_t lifetime;
};

const struct ud3tn_cmdline_options *parse_cmdline(int argc, char *argv[]);

#endif // CMDLINE_H_INCLUDED
