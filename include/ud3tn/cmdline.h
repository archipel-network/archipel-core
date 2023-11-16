// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#ifndef CMDLINE_H_INCLUDED
#define CMDLINE_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>

// Default local EID.
#ifndef DEFAULT_EID
#define DEFAULT_EID "dtn://ud3tn.dtn/"
#endif // DEFAULT_EID

// Default options string provided to the CLA subsystem.
#ifndef DEFAULT_CLA_OPTIONS
#define DEFAULT_CLA_OPTIONS \
	"tcpclv3:*,4556;tcpspp:*,4223,false,1;smtcp:*,4222,false;mtcp:*,4224"
#endif // DEFAULT_CLA_OPTIONS

// Default TCP IP/port used for the application agent interface.
#ifndef DEFAULT_AAP_NODE
#define DEFAULT_AAP_NODE "0.0.0.0"
#endif // DEFAULT_AAP_NODE
#ifndef DEFAULT_AAP_SERVICE
#define DEFAULT_AAP_SERVICE "4242"
#endif // DEFAULT_AAP_SERVICE

// Default filename used for the AAP domain socket.
#ifndef DEFAULT_AAP_SOCKET_FILENAME
#define DEFAULT_AAP_SOCKET_FILENAME "ud3tn.socket"
#endif // DEFAULT_AAP_SOCKET_FILENAME

// Filename used for the AAP 2.0 domain socket.
#ifndef DEFAULT_AAP2_SOCKET_FILENAME
#define DEFAULT_AAP2_SOCKET_FILENAME "ud3tn.aap2.socket"
#endif // DEFAULT_AAP2_SOCKET_FILENAME

// Default BP version used for generated bundles.
#ifndef DEFAULT_BUNDLE_VERSION
#define DEFAULT_BUNDLE_VERSION 7
#endif // DEFAULT_BUNDLE_VERSION

// Default lifetime, in seconds, of bundles sent via AAP.
#ifndef DEFAULT_BUNDLE_LIFETIME_S
#define DEFAULT_BUNDLE_LIFETIME_S 86400
#endif // DEFAULT_BUNDLE_LIFETIME_S

// Default log level.
// LOG_ERROR = 1, LOG_WARN  = 2, LOG_INFO  = 3, LOG_DEBUG = 4.
#ifndef DEFAULT_LOG_LEVEL
#ifdef DEBUG
#define DEFAULT_LOG_LEVEL 3
#else // DEBUG
#define DEFAULT_LOG_LEVEL 2
#endif // DEBUG
#endif // DEFAULT_LOG_LEVEL

struct ud3tn_cmdline_options {
	char *eid; // e.g.: dtn://ud3tn.dtn/
	char *cla_options; // e.g.: tcpspp:*,3333,false,1;tcpcl:*,4356
	char *aap_socket; // e.g.: /tmp/ud3tn.socket
	char *aap_node; // e.g.: 127.0.0.1
	char *aap_service; // e.g.: 4242
	char *aap2_socket; // e.g.: /tmp/ud3tn.aap2.socket
	uint8_t bundle_version;
	uint8_t log_level;
	bool status_reporting;
	bool allow_remote_configuration;
	bool exit_immediately; // after parsing --help or --usage etc.
	uint64_t mbs; // maximum bundle size
	uint64_t lifetime_s;
};

const struct ud3tn_cmdline_options *parse_cmdline(int argc, char *argv[]);

#endif // CMDLINE_H_INCLUDED
