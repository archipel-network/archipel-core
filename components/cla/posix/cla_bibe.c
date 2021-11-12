#include "aap/aap.h"
#include "aap/aap_parser.h"
#include "aap/aap_serializer.h"

#include "cla/cla.h"
#include "cla/cla_contact_tx_task.h"
#include "cla/posix/cla_bibe.h"
#include "cla/bibe_proto.h"
#include "cla/posix/cla_tcp_common.h"
#include "cla/posix/cla_tcp_util.h"

#include "bundle6/parser.h"
#include "bundle7/parser.h"

#include "platform/hal_config.h"
#include "platform/hal_io.h"
#include "platform/hal_semaphore.h"
#include "platform/hal_task.h"
#include "platform/hal_types.h"

#include "ud3tn/bundle_agent_interface.h"
#include "ud3tn/bundle_processor.h"
#include "ud3tn/cmdline.h"
#include "ud3tn/common.h"
#include "ud3tn/config.h"
#include "ud3tn/result.h"
#include "ud3tn/simplehtab.h"
#include "ud3tn/task_tags.h"

#include <sys/socket.h>
#include <unistd.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

struct bibe_config {
	struct cla_tcp_config base;

	struct htab_entrylist *param_htab_elem[CLA_TCP_PARAM_HTAB_SLOT_COUNT];
	struct htab param_htab;
	Semaphore_t param_htab_sem;

    const char *node;
	const char *service;
};

struct bibe_contact_parameters {
	// IMPORTANT: The cla_tcp_link is only initialized iff connected == true
	struct bibe_link link;

	struct bibe_config *config;

	Task_t management_task;

	char *cla_sock_addr;

	bool in_contact;
	bool connected;
	int connect_attempt;

	int socket;
};


static enum ud3tn_result handle_established_connection(
	struct bibe_contact_parameters *const param)
{
	struct bibe_config *const bibe_config = param->config;

	if (cla_tcp_link_init(&param->link.base, param->socket,
			      &bibe_config->base)
			!= UD3TN_OK) {
		LOG("bibe: Error initializing CLA link!");
		return UD3TN_FAIL;
	}

	cla_link_wait_cleanup(&param->link.base.base);

	return UD3TN_OK;
}

static void bibe_link_management_task(void *p)
{
	struct bibe_contact_parameters *const param = p;

	ASSERT(param->cla_sock_addr != NULL);
	do {
		if (param->connected) {
			ASSERT(param->socket > 0);
			handle_established_connection(param);
			param->connected = false;
			param->connect_attempt = 0;
			param->socket = -1;
		} else {
			ASSERT(param->socket < 0);
			param->socket = cla_tcp_connect_to_cla_addr(
				param->cla_sock_addr,
				NULL
			);
			if (param->socket < 0) {
				if (++param->connect_attempt >
						CLA_TCP_MAX_RETRY_ATTEMPTS) {
					LOG("bibe: Final retry failed.");
					break;
				}
				LOGF("bibe: Delayed retry %d of %d in %d ms",
				     param->connect_attempt,
				     CLA_TCP_MAX_RETRY_ATTEMPTS,
				     CLA_TCP_RETRY_INTERVAL_MS);
				hal_task_delay(CLA_TCP_RETRY_INTERVAL_MS);
				continue;
			}
			LOGF("bibe: Connected successfully to \"%s\"",
			     param->cla_sock_addr);
			param->connected = true;
			const struct aap_message register_bibe = {
				.type = AAP_MESSAGE_REGISTER,
				.eid = "bibe",
				.eid_length = 4,
			};
   		 	uint64_t *buffer = malloc(sizeof(struct aap_message));
    		aap_serialize_into(buffer, &register_bibe);

    		tcp_send_all(param->socket, buffer, aap_get_serialized_size(&register_bibe));
		}
	} while (param->in_contact);
	LOGF("bibe: Terminating contact link manager for \"%s\"",
	     param->cla_sock_addr);
	hal_semaphore_take_blocking(param->config->param_htab_sem);
	htab_remove(&param->config->param_htab, param->cla_sock_addr);
	hal_semaphore_release(param->config->param_htab_sem);
	bibe_parser_reset(&param->link.bibe_parser);
	free(param->cla_sock_addr);

	Task_t management_task = param->management_task;

	free(param);
	hal_task_delete(management_task);
}

static void launch_connection_management_task(
	struct bibe_config *const bibe_config,
	const int sock, const char *cla_addr)
{
	ASSERT(cla_addr);
	struct bibe_contact_parameters *contact_params =
		malloc(sizeof(struct bibe_contact_parameters));

	if (!contact_params) {
		LOG("bibe: Failed to allocate memory!");
		return;
	}

	contact_params->config = bibe_config;
	contact_params->connect_attempt = 0;

	if (sock < 0) {
		contact_params->cla_sock_addr = cla_get_connect_addr(
			cla_addr,
			"bibe"
		);
		contact_params->socket = -1;
		contact_params->connected = false;
		contact_params->in_contact = true;
	} else {
		ASSERT(sock != -1);
		contact_params->cla_sock_addr = strdup(cla_addr);
		contact_params->socket = sock;
		contact_params->connected = true;
		contact_params->in_contact = false;
	}

	if (!contact_params->cla_sock_addr) {
		LOG("bibe: Failed to copy CLA address!");
		goto fail;
	}

	bibe_parser_reset(&contact_params->link.bibe_parser);

	struct htab_entrylist *htab_entry = NULL;

	htab_entry = htab_add(
		&bibe_config->param_htab,
		contact_params->cla_sock_addr,
		contact_params
	);
	if (!htab_entry) {
		LOG("bibe: Error creating htab entry!");
		goto fail;
	}

	contact_params->management_task = hal_task_create(
		bibe_link_management_task,
		"bibe_mgmt_t",
		CONTACT_MANAGEMENT_TASK_PRIORITY,
		contact_params,
		CONTACT_MANAGEMENT_TASK_STACK_SIZE,
		(void *)CLA_SPECIFIC_TASK_TAG
	);

	if (!contact_params->management_task) {
		LOG("bibe: Error creating management task!");
		if (htab_entry) {
			ASSERT(contact_params->cla_sock_addr);
			ASSERT(htab_remove(
				&bibe_config->param_htab,
				contact_params->cla_sock_addr
			) == contact_params);
		}
		goto fail;
	}

	return;

fail:
	free(contact_params->cla_sock_addr);
	free(contact_params);
}

static enum ud3tn_result bibe_launch(struct cla_config *const config)
{
	/* Since the BIBE CLA does not need a listener task, the bibe_launch
	   function has pretty much no functionality.
	   It could however be used to establish a "standard connection" 
	   if there's a predefined partner node. */
	return UD3TN_OK;
}

static const char *bibe_name_get(void)
{
	return "bibe";
}

size_t bibe_mbs_get(struct cla_config *const config)
{
	(void)config;
	return SIZE_MAX;
}

void bibe_reset_parsers(struct cla_link *link)
{
	struct bibe_link *const bibe_link = (struct bibe_link *)link;

	rx_task_reset_parsers(&link->rx_task_data);

	bibe_parser_reset(&bibe_link->bibe_parser);
	link->rx_task_data.cur_parser = &bibe_link->bibe_parser;
}


size_t bibe_forward_to_specific_parser(struct cla_link *link,
				       const uint8_t *buffer, size_t length)
{
	struct rx_task_data *const rx_data = &link->rx_task_data;
	size_t result = 0;

	/* The BIBE CLA only cares about AAP messages. */
	switch (rx_data->payload_type) {
	case PAYLOAD_UNKNOWN:
		result = select_bundle_parser_version(rx_data, buffer, length);
		if (result == 0)
			bibe_reset_parsers(link);
		break;
	case PAYLOAD_AAP:
		rx_data->cur_parser = rx_data->aap_parser.basedata;
		result = aap_parser_read(
			&rx_data->aap_parser, 
			buffer, 
			length
		);
		break;
	default:
		bibe_reset_parsers(link);
		return 0;
	}

	if(rx_data->aap_parser.status == PARSER_STATUS_DONE){
		struct aap_message msg = aap_parser_extract_message(&rx_data->aap_parser);
		
		/* The only relevant message type is RECVBIBE, as the CLA
		   does not need to do anything with WELCOME or ACK messages. */
		if(msg.type == AAP_MESSAGE_RECVBIBE){
			/* Parsing the BPDU */
			struct bibe_protocol_data_unit bpdu;
			size_t err = bibe_parser_parse(
				msg.payload, 
				msg.payload_length, 
				&bpdu
			);
			if (err == 0 && bpdu.payload_length != 0)
			{
				/* Parsing and forwarding the encapsulated bundle. */
            	bundle7_parser_read(
					&rx_data->bundle7_parser, 
					bpdu.encapsulated_bundle, 
					bpdu.payload_length
				);
			}
		}

		aap_message_clear(&msg);
		bibe_reset_parsers(link);
	}
	return result;
}

/*
 * TX
 */

static struct bibe_contact_parameters *get_contact_parameters(
	struct cla_config *config, const char *cla_addr)
{
	struct bibe_config *const bibe_config =
		(struct bibe_config *)config;
	char *const cla_sock_addr = cla_get_connect_addr(cla_addr, "bibe");

	struct bibe_contact_parameters *param = htab_get(
		&bibe_config->param_htab,
		cla_sock_addr
	);
	free(cla_sock_addr);
	return param;
}

static struct cla_tx_queue bibe_get_tx_queue(
	struct cla_config *config, const char *eid, const char *cla_addr)
{
	(void)eid;
	struct bibe_config *const bibe_config = (struct bibe_config *)config;

	hal_semaphore_take_blocking(bibe_config->param_htab_sem);
	struct bibe_contact_parameters *const param = get_contact_parameters(
		config,
		cla_addr
	);

	if (param && param->connected) {
		struct cla_link *const cla_link = &param->link.base.base;

		hal_semaphore_take_blocking(cla_link->tx_queue_sem);
		hal_semaphore_release(bibe_config->param_htab_sem);

		// Freed while trying to obtain it
		if (!cla_link->tx_queue_handle)
			return (struct cla_tx_queue){ NULL, NULL };

		return (struct cla_tx_queue){
			.tx_queue_handle = cla_link->tx_queue_handle,
			.tx_queue_sem = cla_link->tx_queue_sem,
		};
	}

	hal_semaphore_release(bibe_config->param_htab_sem);
	return (struct cla_tx_queue){ NULL, NULL };
}

static enum ud3tn_result bibe_start_scheduled_contact(
	struct cla_config *config, const char *eid, const char *cla_addr)
{
	(void)eid;
	struct bibe_config *const bibe_config = (struct bibe_config *)config;

	hal_semaphore_take_blocking(bibe_config->param_htab_sem);
	struct bibe_contact_parameters *const param = get_contact_parameters(
		config,
		cla_addr
	);

	if (param) {
		LOGF("bibe: Associating open connection with \"%s\" to new contact",
		     cla_addr);
		param->in_contact = true;
		hal_semaphore_release(bibe_config->param_htab_sem);
		return UD3TN_OK;
	}

	launch_connection_management_task(bibe_config, -1, cla_addr);
	hal_semaphore_release(bibe_config->param_htab_sem);

	return UD3TN_OK;
}

static enum ud3tn_result bibe_end_scheduled_contact(
	struct cla_config *config, const char *eid, const char *cla_addr)
{
	(void)eid;
	struct bibe_config *const bibe_config = (struct bibe_config *)config;

	hal_semaphore_take_blocking(bibe_config->param_htab_sem);
	struct bibe_contact_parameters *const param = get_contact_parameters(
		config,
		cla_addr
	);

	if (param && param->in_contact) {
		LOGF("bibe: Marking open connection with \"%s\" as opportunistic",
		     cla_addr);
		param->in_contact = false;
		if (param->socket >= 0) {
			LOGF("bibe: Terminating connection with \"%s\"",
			     cla_addr);
			// Shutting down the socket to force the lower layers
			// Application Agent to deregister the "bibe" sink.
			shutdown(param->socket, SHUT_RDWR);
			close(param->socket);
		}
	}

	hal_semaphore_release(bibe_config->param_htab_sem);

	return UD3TN_OK;
}

void bibe_begin_packet(struct cla_link *link, size_t length)
{
	struct cla_tcp_link *const tcp_link = (struct cla_tcp_link *)link;

	// A previous operation may have canceled the sending process.
	if (!link->active)
		return;

	const size_t BUFFER_SIZE = 9; // max. for uint64_t
	uint8_t buffer[BUFFER_SIZE];

	const size_t hdr_len = 0; //bibe_encode_header(buffer, BUFFER_SIZE, length);

	if (tcp_send_all(tcp_link->connection_socket, buffer, hdr_len) == -1) {
		LOG("bibe: Error during sending. Data discarded.");
		link->config->vtable->cla_disconnect_handler(link);
	}
}

void bibe_end_packet(struct cla_link *link)
{
	// STUB
	(void)link;
}

void bibe_send_packet_data(
	struct cla_link *link, const void *data, const size_t length)
{
	struct cla_tcp_link *const tcp_link = (struct cla_tcp_link *)link;

	// A previous operation may have canceled the sending process.
	if (!link->active)
		return;

	if (tcp_send_all(tcp_link->connection_socket, data, length) == -1) {
		LOG("bibe: Error during sending. Data discarded.");
		link->config->vtable->cla_disconnect_handler(link);
	}
}

const struct cla_vtable bibe_vtable = {
	.cla_name_get = bibe_name_get,
	.cla_launch = bibe_launch,
	.cla_mbs_get = bibe_mbs_get,

	.cla_get_tx_queue = bibe_get_tx_queue,
	.cla_start_scheduled_contact = bibe_start_scheduled_contact,
	.cla_end_scheduled_contact = bibe_end_scheduled_contact,

	.cla_begin_packet = bibe_begin_packet,
	.cla_end_packet = bibe_end_packet,
	.cla_send_packet_data = bibe_send_packet_data,

	.cla_rx_task_reset_parsers = bibe_reset_parsers,
	.cla_rx_task_forward_to_specific_parser =
		bibe_forward_to_specific_parser,

	.cla_read = cla_tcp_read,

	.cla_disconnect_handler = cla_tcp_disconnect_handler,
};

static enum ud3tn_result bibe_init(
	struct bibe_config *config,
	const char *node, const char *service,
	const struct bundle_agent_interface *bundle_agent_interface)
{
	/* Initialize base_config */
	if (cla_tcp_config_init(&config->base,
				bundle_agent_interface) != UD3TN_OK)
		return UD3TN_FAIL;

	/* set base_config vtable */
	config->base.base.vtable = &bibe_vtable;

	htab_init(&config->param_htab, CLA_TCP_PARAM_HTAB_SLOT_COUNT,
		  config->param_htab_elem);

	config->param_htab_sem = hal_semaphore_init_binary();
	hal_semaphore_release(config->param_htab_sem);

    config->node = node;
    config->service = service;
    
	return UD3TN_OK;
}

struct cla_config *bibe_create(
	const char *const options[], const size_t option_count,
	const struct bundle_agent_interface *bundle_agent_interface)
{
	struct bibe_config *config = malloc(sizeof(struct bibe_config));

	if (!config) {
		LOG("bibe: Memory allocation failed!");
		return NULL;
	}
	// TODO: Allow for passing of options indicating
	// which lower layer to connect to without scheduling
	// a contact? E.g. localhost:4242
	if (bibe_init(config, options[0], options[1],
		      bundle_agent_interface) != UD3TN_OK) {
		free(config);
		LOG("bibe: Initialization failed!");
		return NULL;
	}

	return &config->base.base;
}
