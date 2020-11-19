#ifndef CLA_TCP_COMMON_H_INCLUDED
#define CLA_TCP_COMMON_H_INCLUDED

#include "cla/cla.h"

#include "ud3tn/bundle_agent_interface.h"
#include "ud3tn/result.h"

#include "platform/hal_types.h"

#include <stdint.h>
#include <stdbool.h>

#include <netinet/in.h>
#include <sys/socket.h>

#define CLA_OPTION_TCP_ACTIVE "true"
#define CLA_OPTION_TCP_PASSIVE "false"

struct cla_tcp_link {
	struct cla_link base;

	/* The handle for the connected socket */
	int connection_socket;
};

struct cla_tcp_config {
	struct cla_config base;

	/* The handle for the passive or active socket */
	int socket;

	/* Task handle for the listener - required to support concurrent CLAs */
	Task_t listen_task;
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
	struct cla_tcp_config *config);

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
