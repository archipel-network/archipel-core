#ifndef CLA_H_INCLUDED
#define CLA_H_INCLUDED

#include "cla/cla_contact_rx_task.h"

#include "ud3tn/bundle_agent_interface.h"
#include "ud3tn/crc.h"
#include "ud3tn/node.h"
#include "ud3tn/result.h"

#include "spp/spp_timecodes.h"

#include "platform/hal_types.h"

#include <stddef.h>
#include <stdint.h>

#define CLA_MAX_OPTION_COUNT 10

struct cla_config {
	const struct cla_vtable *vtable;

	const struct bundle_agent_interface *bundle_agent_interface;
};

struct cla_link {
	struct cla_config *config;

	// Flag to determine whether the link is still usable
	bool active;

	// Semaphore for waiting until the RX task is finished
	Semaphore_t rx_task_sem;
	// Semaphore for waiting until the TX task is finished
	Semaphore_t tx_task_sem;


	Task_t rx_task_handle;
	struct rx_task_data rx_task_data;

	// Timestamp of last received byte for implementing the read timeout
	uint64_t last_rx_time_ms;

	Task_t tx_task_handle;
	// Queue handing over bundles to the TX task
	QueueIdentifier_t tx_queue_handle;
	// Semaphore blocking the TX queue while bundles are being added
	Semaphore_t tx_queue_sem;
};

struct cla_tx_queue {
	QueueIdentifier_t tx_queue_handle;
	Semaphore_t tx_queue_sem;
};

/*
 * Global CLA Instance Management
 */

enum ud3tn_result cla_initialize_all(
	const char *cla_config_str,
	const struct bundle_agent_interface *bundle_agent_interface);

struct cla_config *cla_config_get(const char *cla_addr);

/*
 * Private API
 */

enum ud3tn_result cla_config_init(
	struct cla_config *config,
	const struct bundle_agent_interface *bundle_agent_interface);

enum ud3tn_result cla_link_init(struct cla_link *link,
				struct cla_config *config);

void cla_link_wait_cleanup(struct cla_link *link);

char *cla_get_connect_addr(const char *cla_addr, const char *cla_name);

void cla_generic_disconnect_handler(struct cla_link *link);

/*
 * Virtual Functions
 */

struct cla_vtable {
	// Public API

	/* Returns a unique identifier of the CLA as part of the CLA address */
	const char *(*cla_name_get)(void);
	/* Starts the TX/RX tasks and, e.g., the socket listener */
	enum ud3tn_result (*cla_launch)(struct cla_config *);
	/* Frees up memory and resources used by the CLA. - TODO */
	// enum ud3tn_result (*cla_destroy)(struct cla_config *);
	/* Obtains the max. serialized size of outgoing bundles for this CLA. */
	size_t (*cla_mbs_get)(struct cla_config *);


	/* Returns the transmission queue for the given node EID and address */
	struct cla_tx_queue (*cla_get_tx_queue)(struct cla_config *,
						const char *, const char *);
	/* Initiates a scheduled contact for a given EID and CLA address */
	enum ud3tn_result (*cla_start_scheduled_contact)(struct cla_config *,
							 const char *,
							 const char *);
	/* Ends a scheduled contact for a given EID and CLA address */
	enum ud3tn_result (*cla_end_scheduled_contact)(struct cla_config *,
						       const char *,
						       const char *);

	// TX Task API

	/* Initiates bundle transmission for a single bundle */
	void (*cla_begin_packet)(struct cla_link *,
				 size_t);
	/* Terminates bundle transmission for a single bundle */
	void (*cla_end_packet)(struct cla_link *);
	/* Sends part of the serialized bundle. Can be called multiple times. */
	void (*cla_send_packet_data)(struct cla_link *,
				     const void *,
				     const size_t);

	// RX Task API

	void (*cla_rx_task_reset_parsers)(struct cla_link *);
	size_t (*cla_rx_task_forward_to_specific_parser)(struct cla_link *,
							 const uint8_t *,
							 size_t);

	/* Reads a chunk of data */
	enum ud3tn_result (*cla_read)(struct cla_link *, uint8_t *buffer,
				      size_t length, size_t *bytes_read);

	/* Cleans up resources after a link broke */
	void (*cla_disconnect_handler)(struct cla_link *);
};

#endif /* CLA_H_INCLUDED */
