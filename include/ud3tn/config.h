#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

#include <limits.h>


// We provide a public constant for that to allow compiling (and, thus,
// detecting errors in) debug code regardless of the #define.
#ifdef DEBUG
static const int IS_DEBUG_BUILD = 1;
#else // DEBUG
static const int IS_DEBUG_BUILD;
#endif // DEBUG



/*
 * [CMDLINE] defaults for the provided command line options
 */

/* Default local EID */
#define DEFAULT_EID "dtn://ud3tn.dtn"

/* Default options string provided to the CLA subsystem */
#ifdef PLATFORM_STM32
#define DEFAULT_CLA_OPTIONS "usbotg:"
#else // PLATFORM_STM32
#define DEFAULT_CLA_OPTIONS "tcpclv3:*,4556;tcpspp:*,4223,false,1;smtcp:*,4222,false;mtcp:*,4224"
#endif // PLATFORM_STM32

/* Default TCP IP/port used for the application agent interface */
#define DEFAULT_AAP_NODE "0.0.0.0"
#define DEFAULT_AAP_SERVICE "4242"
/* Filename used for the AAP domain socket */
#define DEFAULT_AAP_SOCKET_FILENAME "ud3tn.socket"
/* BP version used for generated bundles */
#define DEFAULT_BUNDLE_VERSION 7
/* Default lifetime, in seconds, of bundles sent via AAP */
#define DEFAULT_BUNDLE_LIFETIME 86400
/* Default CRC type
 * BUNDLE_CRC_TYPE_NONE = 0,
 * BUNDLE_CRC_TYPE_16   = 1,
 * BUNDLE_CRC_TYPE_32   = 2
 */
#define DEFAULT_CRC_TYPE BUNDLE_CRC_TYPE_16


/*
 * [GENERAL] general configuration options
 */
/* The device we're communicating with should generally be faster */
#define COMM_RX_QUEUE_LENGTH 3072
#define COMM_TX_QUEUE_LENGTH 1024
/* If this is defined, comm_* waits for the PC to fetch outgoing data */
#define COMM_BLOCK_ON_DISCONNECT
/* 01/01/2000 00:00:00 UTC */
#define DTN_TIMESTAMP_OFFSET 946684800
/* default lengths of some individual queues */
#define ROUTER_QUEUE_LENGTH 30
#define BUNDLE_QUEUE_LENGTH 10
/* Contact dropping / failed forwarding policy */
enum failed_forwarding_policy {
	POLICY_DROP,
	POLICY_DROP_IF_NO_CUSTODY,
	POLICY_TRY_RE_SCHEDULE
};
/* Policy for re-scheduling bundles on dropped contacts, etc. */
#define FAILED_FORWARD_POLICY POLICY_DROP_IF_NO_CUSTODY
/* If policy decides to re-schedule, but processing times out => drop */
/* Set this to 0 for an infinite delay - which implies a possible deadlock! */
#define FAILED_FORWARD_TIMEOUT 1000
/* If a status report is pending, but the router is blocked => drop */
/* Set this to 0 for an infinite delay - which implies a possible deadlock! */
#define STATUS_REPORT_TIMEOUT 1000
/* If a custody signal is pending, but the router is blocked => drop */
/* Set this to 0 for an infinite delay - which implies a possible deadlock! */
#define CUSTODY_SIGNAL_TIMEOUT 1000



/*
 * [BUNDLE] configuration of Bundle related
 * processing
 */
/* Bundles requiring more space will be dropped immediately */
#ifdef PLATFORM_STM32
#define BUNDLE_QUOTA 24576
#else
#define BUNDLE_QUOTA 1073741824
#endif

/* The maximum count of bundles for which we have custody at a time */
#define CUSTODY_MAX_BUNDLE_COUNT 16
/* The maximum size of a bundle for which custody will be accepted */
#define CUSTODY_MAX_BUNDLE_SIZE 1024



/*
 * [CLA] convergence layer related configuration
 */
// Length of the outgoing-bundle queue (contact manager to TX task)
#define CONTACT_TX_TASK_QUEUE_LENGTH 3
// Length of the listen backlog for single-connection CLAs
#define CLA_TCP_SINGLE_BACKLOG 1
// Length of the listen backlog for multi-connection CLAs
#define CLA_TCP_MULTI_BACKLOG 64
// On contact start, outgoing connections are attempted. If the first attempt
// fails, it is retried in the given interval up to the given maximum number
// of attempts.
#define CLA_TCP_RETRY_INTERVAL_MS 1000
#define CLA_TCP_MAX_RETRY_ATTEMPTS 10
// The number of slots in the TCP CLA hash tables (e.g. for TCPCLv3 and MTCP)
#define CLA_TCP_PARAM_HTAB_SLOT_COUNT 32
// Whether or not to close active TCP connections after a contact
#define CLA_MTCP_CLOSE_AFTER_CONTACT 0



/*
 * [ROUTING] configuration options related to the routing
 * algorithm
 */
#define ROUTER_MAX_FRAGMENTS 10
#define ROUTER_MAX_CONTACTS 4
/* Default values */
#define ROUTER_GLOBAL_MBS SIZE_MAX
#define FRAGMENT_MIN_PAYLOAD 8
#define MIN_PROBABILITY 0.90f
#define MIN_NODE_CONFIDENCE_OPPORTUNISTIC 0.2f
/* This value has to be greater than or equal to MIN_PROBABILITY */
#define MIN_NODE_CONFIDENCE_DETERMINISTIC 0.90f
#define NODE_TRUSTWORTHINESS_WEIGHT 0.0f
#define NODE_RELIABILITY_WEIGHT 1.0f
/* The minimum available time for an optimization to be triggered */
#define OPTIMIZATION_MIN_TIME 2
#define OPTIMIZATION_MAX_BUNDLES 3
#define OPTIMIZATION_MAX_PRE_BUNDLES 9
#define OPTIMIZATION_MAX_PRE_BUNDLES_CONTACT 3
#define ROUTER_OPTIMIZER_DELAY 50
/* Number of slots in the node hash table */
#define NODE_HTAB_SLOT_COUNT 128
/* Below this, NBFs will be consulted */
#define ROUTER_MIN_CONTACTS_HTAB 10
/* Below this, a default route will be used */
#define ROUTER_MIN_CONTACTS_NBF 2
/* The "reliability" of the default route */
/* This makes sure to use the default route everytime and not fail */
#define ROUTER_DEF_BASE_RELIABILITY MIN_PROBABILITY



/*
 * [CONTACT] contact related configuration
 */
/* Maximum period (in ms) for checking */
#define CONTACT_CHECKING_MAX_PERIOD 600000
/* Maximum number of concurrent contacts that can be handled by CM */
#define MAX_CONCURRENT_CONTACTS 10


#endif /* CONFIG_H_INCLUDED */
