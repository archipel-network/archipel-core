// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#ifndef CLA_TCP_COMMON_H_INCLUDED
#define CLA_TCP_COMMON_H_INCLUDED

#include "cla/cla.h"

#include "ud3tn/bundle_processor.h"
#include "ud3tn/result.h"

#include "platform/hal_types.h"

#include <stdbool.h>
#include <stdint.h>

#include <netinet/in.h>
#include <sys/socket.h>

#define CLA_OPTION_TCP_ACTIVE "true"
#define CLA_OPTION_TCP_PASSIVE "false"

// Length of the listen backlog for single-connection CLAs
#ifndef CLA_TCP_SINGLE_BACKLOG
#define CLA_TCP_SINGLE_BACKLOG 1
#endif // CLA_TCP_SINGLE_BACKLOG

// Length of the listen backlog for multi-connection CLAs
#ifndef CLA_TCP_MULTI_BACKLOG
#define CLA_TCP_MULTI_BACKLOG 64
#endif // CLA_TCP_MULTI_BACKLOG

// On contact start, outgoing connections are attempted. If the first attempt
// fails, it is retried in the given interval up to the given maximum number
// of attempts.
#ifndef CLA_TCP_RETRY_INTERVAL_MS
#define CLA_TCP_RETRY_INTERVAL_MS 1000
#endif // CLA_TCP_RETRY_INTERVAL_MS
#ifndef CLA_TCP_MAX_RETRY_ATTEMPTS
#define CLA_TCP_MAX_RETRY_ATTEMPTS 10
#endif // CLA_TCP_MAX_RETRY_ATTEMPTS

// The number of slots in the TCP CLA hash tables (e.g. for TCPCLv3 and MTCP)
#ifndef CLA_TCP_PARAM_HTAB_SLOT_COUNT
#define CLA_TCP_PARAM_HTAB_SLOT_COUNT 32
#endif // CLA_TCP_PARAM_HTAB_SLOT_COUNT

struct cla_tcp_link {
	struct cla_link base;

	/* The handle for the connected socket */
	int connection_socket;
};

struct cla_tcp_config {
	struct cla_config base;

	/* The handle for the passive or active socket */
	int socket;
	/* The last time a connection attempt was made, for rate limiting */
	uint64_t last_connection_attempt_ms;
};

struct cla_tcp_single_config {
	struct cla_tcp_config base;

	/* The active link, if there is any, else NULL */
	struct cla_tcp_link *link;

	/* Whether or not to connect (pro)actively. If false, listen. */
	bool tcp_active;

	/* The number of contacts currently handled via this CLA. */
	int num_active_contacts;

	/* Semaphore for waiting until (some) contact is active. */
	Semaphore_t contact_activity_sem;

	/* The address/port to bind/connect to. */
	const char *node;
	const char *service;
};

/*
 * Private API
 */

enum ud3tn_result cla_tcp_config_init(
	struct cla_tcp_config *config,
	const struct bundle_agent_interface *bundle_agent_interface);

enum ud3tn_result cla_tcp_single_config_init(
	struct cla_tcp_single_config *config,
	const struct bundle_agent_interface *bundle_agent_interface);

enum ud3tn_result cla_tcp_link_init(
	struct cla_tcp_link *link, int connected_socket,
	struct cla_tcp_config *config,
	char *const cla_addr,
	bool is_tx);

enum ud3tn_result cla_tcp_listen(struct cla_tcp_config *config,
				 const char *node, const char *service,
				 int backlog);

int cla_tcp_accept_from_socket(struct cla_tcp_config *config,
			       int listener_socket,
			       char **addr);

enum ud3tn_result cla_tcp_connect(struct cla_tcp_config *config,
				  const char *node, const char *service);

void cla_tcp_single_connect_task(struct cla_tcp_single_config *config,
				 const size_t struct_size);

void cla_tcp_single_listen_task(struct cla_tcp_single_config *config,
				const size_t struct_size);

void cla_tcp_single_link_creation_task(struct cla_tcp_single_config *config,
				       const size_t struct_size);

void cla_tcp_rate_limit_connection_attempts(struct cla_tcp_config *config);

// For the config vtable...

struct cla_tx_queue cla_tcp_single_get_tx_queue(
	struct cla_config *config, const char *eid, const char *cla_addr);

enum ud3tn_result cla_tcp_single_start_scheduled_contact(
	struct cla_config *config, const char *eid, const char *cla_addr);

enum ud3tn_result cla_tcp_single_end_scheduled_contact(
	struct cla_config *config, const char *eid, const char *cla_addr);

void cla_tcp_disconnect_handler(struct cla_link *link);

void cla_tcp_single_disconnect_handler(struct cla_link *link);

/**
 * @brief Read at most "length" bytes from the interface into a buffer.
 *
 * The user must assert that the current buffer is large enough to contain
 * "length" bytes.
 *
 * @param buffer The target buffer to be read to.
 * @param length Size of the buffer in bytes.
 * @param bytes_read Number of bytes read into the buffer.
 * @return Specifies if the read was successful.
 */
enum ud3tn_result cla_tcp_read(struct cla_link *link,
			       uint8_t *buffer, size_t length,
			       size_t *bytes_read);

/**
 * @brief Parse the "TCP active" command line option.
 *
 * @param str The command line option string.
 * @param tcp_active Returns the "TCP active" flag.
 * @return A value indicating whether the operation was successful.
 */
enum ud3tn_result parse_tcp_active(const char *str, bool *tcp_active);

#endif // CLA_TCP_COMMON_H_INCLUDED
