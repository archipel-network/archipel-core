#include "cla/mtcp_proto.h"
#include "cla/stm32/cla_usbotg.h"

#include "platform/hal_config.h"
#include "platform/hal_io.h"
#include "platform/hal_queue.h"
#include "platform/hal_semaphore.h"

#include "ud3tn/bundle_agent_interface.h"
#include "ud3tn/common.h"
#include "ud3tn/config.h"

#include <FreeRTOS.h>

#include <usbd_cdc_vcp.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


struct usbotg_config {
	struct cla_config base;

	struct cla_link link;

	QueueIdentifier_t rx_queue, tx_queue;
	Semaphore_t comm_semaphore;

	// Note that we use the MTCP data format!
	struct parser mtcp_parser;
};


/*
 * INIT
 */

static const char *usbotg_name_get(void)
{
	return "usbotg";
}

static enum ud3tn_result usbotg_launch(struct cla_config *const config)
{
	struct usbotg_config *const usbotg_config =
		(struct usbotg_config *)config;

	usbotg_config->rx_queue = hal_queue_create(
		COMM_RX_QUEUE_LENGTH,
		sizeof(uint8_t)
	);
	usbotg_config->tx_queue = hal_queue_create(
		COMM_TX_QUEUE_LENGTH,
		sizeof(uint8_t)
	);
	USB_VCP_Init(usbotg_config->rx_queue, usbotg_config->tx_queue);

	usbotg_config->comm_semaphore = hal_semaphore_init_binary();
	if (usbotg_config->comm_semaphore == NULL)
		return UD3TN_FAIL;
	hal_semaphore_release(usbotg_config->comm_semaphore);

	return UD3TN_OK;
}

static size_t usbotg_mbs_get(struct cla_config *const config)
{
	(void)config;
	return SIZE_MAX;
}


/*
 * RX
 */

void usbotg_reset_parsers(struct cla_link *link)
{
	struct usbotg_config *const usbotg_config =
		(struct usbotg_config *)link->config;

	rx_task_reset_parsers(&link->rx_task_data);

	mtcp_parser_reset(&usbotg_config->mtcp_parser);
	link->rx_task_data.cur_parser = &usbotg_config->mtcp_parser;
}

static size_t usbotg_forward_to_specific_parser(struct cla_link *link,
						const uint8_t *buffer,
						size_t length)
{
	struct usbotg_config *const usbotg_config =
		(struct usbotg_config *)link->config;
	struct rx_task_data *const rx_data = &link->rx_task_data;
	size_t result = 0;

	// Decode MTCP CBOR byte string header if not done already
	if (!(usbotg_config->mtcp_parser.flags & PARSER_FLAG_DATA_SUBPARSER))
		return mtcp_parser_parse(&usbotg_config->mtcp_parser,
					 buffer, length);

	// We do not allow to parse more than the stated length...
	if (length > usbotg_config->mtcp_parser.next_bytes)
		length = usbotg_config->mtcp_parser.next_bytes;

	switch (rx_data->payload_type) {
	case PAYLOAD_UNKNOWN:
		result = select_bundle_parser_version(rx_data, buffer, length);
		if (result == 0) {
			LOG("usbotg: Could not select a valid bundle parser.");
			usbotg_reset_parsers(link);
		}
		break;
	case PAYLOAD_BUNDLE6:
		rx_data->cur_parser = rx_data->bundle6_parser.basedata;
		result = bundle6_parser_read(
			&rx_data->bundle6_parser,
			buffer,
			length
		);
		break;
	case PAYLOAD_BUNDLE7:
		rx_data->cur_parser = rx_data->bundle7_parser.basedata;
		result = bundle7_parser_read(
			&rx_data->bundle7_parser,
			buffer,
			length
		);
		break;
	default:
		LOG("usbotg: Invalid payload type detected.");
		usbotg_reset_parsers(link);
		return 0;
	}

	ASSERT(result <= usbotg_config->mtcp_parser.next_bytes);
	usbotg_config->mtcp_parser.next_bytes -= result;

	// All done
	if (!usbotg_config->mtcp_parser.next_bytes)
		usbotg_reset_parsers(link);

	return result;
}

static enum ud3tn_result usbotg_read(struct cla_link *link,
				     uint8_t *buffer, size_t length,
				     size_t *bytes_read)
{
	struct usbotg_config *const usbotg_config =
		(struct usbotg_config *)link->config;
	QueueIdentifier_t rx_queue = usbotg_config->rx_queue;

	// Special case: empty buffer
	if (length == 0)
		return 0;

	// Write-pointer to the current buffer position
	uint8_t *stream = buffer;

	// Receive at least one byte in blocking manner from the RX queue
	while (hal_queue_receive(rx_queue, stream, -1) != UD3TN_OK)
		;
	length--;
	stream++;

	// Emulate the behavior of recv() by reading further bytes with a very
	// small timeout.
	while (length--) {
		if (hal_queue_receive(rx_queue, stream,
				      COMM_RX_TIMEOUT) != UD3TN_OK)
			break;
		stream++;
	}

	*bytes_read = stream - buffer;

	return UD3TN_OK;
}


/*
 * TX
 */

static struct cla_tx_queue usbotg_get_tx_queue(
	struct cla_config *config, const char *eid, const char *cla_addr)
{
	// For single-connection CLAs, these parameters are unused...
	(void)eid;
	(void)cla_addr;

	struct usbotg_config *const usbotg_config =
		(struct usbotg_config *)config;

	hal_semaphore_take_blocking(usbotg_config->link.tx_queue_sem);
	// No check whether queue was deleted because the link never drops
	return (struct cla_tx_queue){
		.tx_queue_handle = usbotg_config->link.tx_queue_handle,
		.tx_queue_sem = usbotg_config->link.tx_queue_sem,
	};
}

static enum ud3tn_result usbotg_start_scheduled_contact(
	struct cla_config *config, const char *eid, const char *cla_addr)
{
	// STUB - UNUSED
	(void)config;
	(void)eid;
	(void)cla_addr;

	return UD3TN_OK;
}

static enum ud3tn_result usbotg_end_scheduled_contact(
	struct cla_config *config, const char *eid, const char *cla_addr)
{
	// STUB - UNUSED
	(void)config;
	(void)eid;
	(void)cla_addr;

	return UD3TN_OK;
}

static inline void VCP_Transmit_Bytes(QueueIdentifier_t tx_queue,
				      const uint8_t *_buf, const int _len)
{
	int i;

#ifndef COMM_BLOCK_ON_DISCONNECT
	if (!USB_VCP_Connected()) {
		hal_queue_reset(tx_queue);
		return;
	}
#endif /* COMM_BLOCK_ON_DISCONNECT */
	for (i = 0; i < _len; i++) {
#if !defined(COMM_TX_TIMEOUT) || COMM_TX_TIMEOUT < 0
		hal_queue_push_to_back(tx_queue, &_buf[i]);
#else /* !defined(COMM_TX_TIMEOUT) || COMM_TX_TIMEOUT < 0 */
		if (hal_queue_try_push_to_back(tx_queue, &_buf[i],
				COMM_TX_TIMEOUT) != UD3TN_OK)
			return;
#endif /* !defined(COMM_TX_TIMEOUT) || COMM_TX_TIMEOUT < 0 */
	}
}

static void usbotg_begin_packet(struct cla_link *link, size_t length)
{
	struct usbotg_config *const usbotg_config =
		(struct usbotg_config *)link->config;

	const size_t BUFFER_SIZE = 9; // max. for uint64_t
	uint8_t buffer[BUFFER_SIZE];

	const size_t hdr_len = mtcp_encode_header(buffer, BUFFER_SIZE, length);

	VCP_Transmit_Bytes(usbotg_config->tx_queue, buffer, hdr_len);
}

static void usbotg_end_packet(struct cla_link *link)
{
	// STUB
	(void)link;
}

static void usbotg_send_packet_data(
	struct cla_link *link, const void *data, const size_t length)
{
	struct usbotg_config *const usbotg_config =
		(struct usbotg_config *)link->config;

	VCP_Transmit_Bytes(usbotg_config->tx_queue, data, length);
}

static void usbotg_disconnect_handler(struct cla_link *link)
{
	(void)link;
	ASSERT(0);
}

/*
 * CREATE
 */

const struct cla_vtable usbotg_vtable = {
	.cla_name_get = usbotg_name_get,
	.cla_launch = usbotg_launch,
	.cla_mbs_get = usbotg_mbs_get,

	.cla_get_tx_queue = usbotg_get_tx_queue,
	.cla_start_scheduled_contact = usbotg_start_scheduled_contact,
	.cla_end_scheduled_contact = usbotg_end_scheduled_contact,

	.cla_begin_packet = usbotg_begin_packet,
	.cla_end_packet = usbotg_end_packet,
	.cla_send_packet_data = usbotg_send_packet_data,

	.cla_rx_task_reset_parsers = usbotg_reset_parsers,
	.cla_rx_task_forward_to_specific_parser =
			usbotg_forward_to_specific_parser,

	.cla_read = usbotg_read,

	.cla_disconnect_handler = usbotg_disconnect_handler,
};

struct cla_config *usbotg_create(
	const char *const options[], const size_t option_count,
	const struct bundle_agent_interface *bundle_agent_interface)
{
	(void)options;
	(void)option_count;

	struct usbotg_config *config = malloc(sizeof(struct usbotg_config));

	if (!config) {
		LOG("usbotg: Memory allocation failed!");
		return NULL;
	}
	cla_config_init(&config->base, bundle_agent_interface);
	/* set base_config vtable */
	config->base.vtable = &usbotg_vtable;

	if (cla_link_init(&config->link, &config->base) != UD3TN_OK) {
		LOG("usbotg: Link initialization failed!");
		free(config);
		return NULL;
	}

	return &config->base;
}
