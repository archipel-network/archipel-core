#include "cla/blackhole_parser.h"
#include "cla/cla.h"
#include "cla/posix/cla_tcpspp.h"
#include "cla/posix/cla_tcp_common.h"
#include "cla/posix/cla_tcp_util.h"

#include "bundle6/parser.h"
#include "bundle7/parser.h"

#include "platform/hal_config.h"
#include "platform/hal_io.h"
#include "platform/hal_task.h"

#include "spp/spp.h"
#include "spp/spp_parser.h"
#include "spp/spp_timecodes.h"

#include "ud3tn/bundle_agent_interface.h"
#include "ud3tn/cmdline.h"
#include "ud3tn/common.h"
#include "ud3tn/config.h"
#include "ud3tn/parser.h"
#include "ud3tn/result.h"
#include "ud3tn/task_tags.h"

#include <sys/socket.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

// Buffer size for serialization
#define MAX_SPP_HEADER_SIZE 32

enum cla_tcpspp_payload_type {
	PAYLOAD_DATA = 0,
	PAYLOAD_CRC16 = 1,
};

struct crc16_parser {
	struct parser basedata;
	unsigned int ctr;
	uint8_t crc[2];
};

struct tcpspp_config {
	// Inherits from: struct cla_tcp_single_config
	struct cla_tcp_single_config base;

	uint16_t apid;

	// NOTE: Most of the following variables are link-specific.
	// Though, we ensure that there is only one link at a time and, thus,
	// we can store them in the CLA config. Other CLAs might have to extend
	// the cla_link struct.

	struct spp_tc_context_t spp_timecode;
	struct spp_context_t *spp_ctx;
	struct crc_stream crc16;

	enum cla_tcpspp_payload_type tcpspp_payload_type;
	struct spp_parser spp_parser;
	struct crc16_parser crc_parser;
};

static void tcpspp_link_creation_task(void *param)
{
	struct tcpspp_config *tcpspp_config = (struct tcpspp_config *)param;

	LOGF("tcpspp: Active for APID %d, using %s mode",
	     tcpspp_config->apid,
	     tcpspp_config->base.tcp_active ? "active" : "passive");

	cla_tcp_single_link_creation_task(
		&tcpspp_config->base,
		sizeof(struct cla_tcp_link)
	);
	ASSERT(0);
}

static enum ud3tn_result tcpspp_launch(struct cla_config *const config)
{
	struct tcpspp_config *tcpspp_config = (struct tcpspp_config *)config;

	tcpspp_config->base.base.listen_task = hal_task_create(
		tcpspp_link_creation_task,
		"spp_listen_t",
		CONTACT_LISTEN_TASK_PRIORITY,
		config,
		CONTACT_LISTEN_TASK_STACK_SIZE,
		(void *)CLA_SPECIFIC_TASK_TAG
	);

	if (!tcpspp_config->base.base.listen_task)
		return UD3TN_FAIL;
	return UD3TN_OK;
}

static const char *tcpspp_get_name(void)
{
	return "tcpspp";
}


static size_t tcpspp_mbs_get(struct cla_config *const config)
{
	(void)config;
	return (1 << 16) - MAX_SPP_HEADER_SIZE; // conservative estimation
}

/*
 * RX
 */

/* this is a super fugly hack to skip the two bytes of CRC16 */
static struct parser *crc16_parser_init(struct crc16_parser *parser)
{
	parser->basedata.flags = PARSER_FLAG_NONE;
	parser->basedata.status = PARSER_STATUS_GOOD;
	parser->basedata.next_bytes = 0;
	parser->ctr = 0;
	return &parser->basedata;
}

static size_t crc16_parser_read(struct crc16_parser *parser,
				const uint8_t *buffer,
				size_t length)
{
	const size_t remaining = 2 - parser->ctr;
	size_t consumed = 0;

	if (length <= remaining)
		consumed = length;
	else
		consumed = remaining;

	for (size_t i = 0; i < consumed; i++)
		parser->crc[parser->ctr + i] = buffer[i];

	parser->ctr += consumed;
	if (parser->ctr >= 2)
		parser->basedata.status = PARSER_STATUS_DONE;

	return consumed;
}

void tcpspp_reset_parsers(struct cla_link *link)
{
	struct tcpspp_config *const tcpspp_config =
		(struct tcpspp_config *)link->config;
	struct rx_task_data *const rx_data = &link->rx_task_data;

	rx_task_reset_parsers(rx_data);

	tcpspp_config->tcpspp_payload_type = PAYLOAD_DATA;
	rx_data->cur_parser = spp_parser_init(
		&tcpspp_config->spp_parser,
		tcpspp_config->spp_ctx
	);
	ASSERT(rx_data->cur_parser);
}

size_t tcpspp_forward_to_specific_parser(struct cla_link *link,
					 const uint8_t *buffer,
					 size_t length)
{
	struct tcpspp_config *const config =
		(struct tcpspp_config *)link->config;
	struct rx_task_data *const rx_data = &link->rx_task_data;

	/* Current parser is input_parser */
	if (config->spp_parser.state != SPP_PARSER_STATE_DATA_SUBPARSER) {
		if (config->spp_parser.state ==
				SPP_PARSER_STATE_SH_ANCILLARY_SUBPARSER) {
			LOG("tcpspp: Ancillary data is not supported at the moment, resetting parsers.");
			tcpspp_reset_parsers(link);
			return 0;
		}
		return spp_parser_read(
			&config->spp_parser,
			buffer,
			length
		);
	}

	size_t result = 0;

	switch (rx_data->payload_type) {
	case PAYLOAD_UNKNOWN:
		// Handle CLA specific payload
		switch (config->tcpspp_payload_type) {
		case PAYLOAD_DATA:
			if (config->spp_parser.header.apid != config->apid) {
				LOGF("tcpspp: Forwarding payload for APID %hu (length = %zu) to black hole.",
				     config->spp_parser.header.apid,
				     config->spp_parser.data_length);
				rx_data->payload_type = PAYLOAD_IRRELEVANT;
				rx_data->cur_parser = (
					rx_data->blackhole_parser.basedata
				);
				rx_data->blackhole_parser.to_read = (
					config->spp_parser.data_length
				);
				// CRC length is included in the length.
				// We must not digest this, otherwise the next
				// parsing step will be off.
				if (CLA_TCPSPP_USE_CRC)
					rx_data->blackhole_parser.to_read -= 2;
				result = blackhole_parser_read(
					&rx_data->blackhole_parser,
					buffer,
					length
				);
				break;
			}
			result = select_bundle_parser_version(
				rx_data,
				buffer,
				length
			);
			if (result == 0) {
				LOG("tcpspp: Cannot determine bundle version, resetting parsers.");
				tcpspp_reset_parsers(link);
			}
			break;
		case PAYLOAD_CRC16:
			result = crc16_parser_read(
				&config->crc_parser,
				buffer,
				length
			);
			if (rx_data->cur_parser->status == PARSER_STATUS_DONE)
				config->tcpspp_payload_type = PAYLOAD_DATA;
			// TODO: Check CRC
			break;
		default:
			tcpspp_reset_parsers(link);
			return 0;
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
	case PAYLOAD_IRRELEVANT:
		rx_data->cur_parser = rx_data->blackhole_parser.basedata;
		result = blackhole_parser_read(
			&rx_data->blackhole_parser,
			buffer,
			length
		);
		break;
	default:
		LOG("tcpspp: Invalid payload type detected, resetting parsers.");
		tcpspp_reset_parsers(link);
		return 0;
	}

	if (rx_data->cur_parser->status == PARSER_STATUS_DONE &&
	    rx_data->cur_parser != &config->crc_parser.basedata &&
	    rx_data->cur_parser != &config->spp_parser.base) {
		if (CLA_TCPSPP_USE_CRC) {
			// we need to read two bytes CRC :<
			config->tcpspp_payload_type = PAYLOAD_CRC16;
			rx_data->payload_type = PAYLOAD_UNKNOWN;
			rx_data->cur_parser =
				crc16_parser_init(&config->crc_parser);
		}
	}

	return result;
}

/*
 * TX
 */

static void tcpspp_begin_packet(struct cla_link *link, size_t length)
{
	struct cla_tcp_link *const tcp_link = (struct cla_tcp_link *)link;
	struct tcpspp_config *const tcpspp_config_ =
		(struct tcpspp_config *)link->config;

	// A previous operation may have canceled the sending process.
	if (!link->active)
		return;

	if (CLA_TCPSPP_USE_CRC)
		length += 2;

	size_t spp_length = spp_get_size(tcpspp_config_->spp_ctx, length);
	uint8_t header_buf[MAX_SPP_HEADER_SIZE];
	uint8_t *header_end = &header_buf[0];

	ASSERT(spp_length - length <= MAX_SPP_HEADER_SIZE);

	struct spp_meta_t metadata;

	metadata.apid = tcpspp_config_->apid;
	metadata.is_request = 0;
	metadata.segment_number = 0;
	metadata.segment_status = SPP_SEGMENT_UNSEGMENTED;
	metadata.dtn_timestamp = hal_time_get_timestamp_s();
	metadata.dtn_counter = 0;

	spp_serialize_header(
		tcpspp_config_->spp_ctx,
		&metadata,
		length,
		&header_end
	);

	if (CLA_TCPSPP_USE_CRC) {
		crc_init(&tcpspp_config_->crc16, CRC16_CCITT_FALSE);
		crc_feed_bytes(
			&tcpspp_config_->crc16,
			&header_buf[0],
			header_end - &header_buf[0]
		);
	}

	if (tcp_send_all(tcp_link->connection_socket, header_buf,
			 header_end - &header_buf[0]) == -1) {
		LOG("tcpspp: Error during sending. Data discarded.");
		link->config->vtable->cla_disconnect_handler(link);
	}
}

static void tcpspp_end_packet(struct cla_link *link)
{
	struct cla_tcp_link *tcp_link = (struct cla_tcp_link *)link;
	struct tcpspp_config *tcpspp_config_ =
		(struct tcpspp_config *)link->config;

	// A previous operation may have canceled the sending process.
	if (!link->active)
		return;

	if (CLA_TCPSPP_USE_CRC) {
		tcpspp_config_->crc16.feed_eof(&tcpspp_config_->crc16);
		const uint8_t *crc16 = tcpspp_config_->crc16.bytes;
		// Big Endian (Network Byte Order) is necessary
		const uint8_t crc16_be[2] = { crc16[1], crc16[0] };

		if (tcp_send_all(tcp_link->connection_socket, crc16_be, 2)
				== -1) {
			LOG("tcpspp: Error during sending. Data discarded.");
			link->config->vtable->cla_disconnect_handler(link);
		}
	}
}

static void tcpspp_send_packet_data(
	struct cla_link *link, const void *data, const size_t length)
{
	struct cla_tcp_link *const tcp_link = (struct cla_tcp_link *)link;
	struct tcpspp_config *tcpspp_config_
		= (struct tcpspp_config *)link->config;

	// A previous operation may have canceled the sending process.
	if (!link->active)
		return;

	if (tcp_send_all(tcp_link->connection_socket, data, length) == -1) {
		LOG("tcpspp: Error during sending. Data discarded.");
		link->config->vtable->cla_disconnect_handler(link);
	}

	if (CLA_TCPSPP_USE_CRC)
		crc_feed_bytes(&tcpspp_config_->crc16, data, length);
}

// Public API for Initialization

const struct cla_vtable tcpspp_vtable = {
	.cla_name_get = tcpspp_get_name,
	.cla_launch = tcpspp_launch,
	.cla_mbs_get = tcpspp_mbs_get,

	.cla_get_tx_queue = cla_tcp_single_get_tx_queue,
	.cla_start_scheduled_contact = cla_tcp_single_start_scheduled_contact,
	.cla_end_scheduled_contact = cla_tcp_single_end_scheduled_contact,

	.cla_begin_packet = tcpspp_begin_packet,
	.cla_end_packet = tcpspp_end_packet,
	.cla_send_packet_data = tcpspp_send_packet_data,

	.cla_rx_task_reset_parsers = tcpspp_reset_parsers,
	.cla_rx_task_forward_to_specific_parser =
			&tcpspp_forward_to_specific_parser,

	.cla_read = cla_tcp_read,

	.cla_disconnect_handler = cla_tcp_single_disconnect_handler,
};

static enum ud3tn_result tcpspp_init(
	struct tcpspp_config *const config,
	const char *node, const char *service,
	const bool tcp_active, const uint16_t apid,
	const struct bundle_agent_interface *bundle_agent_interface)
{
	/* Initialize base_config */
	if (cla_tcp_single_config_init(&config->base,
				       bundle_agent_interface) != UD3TN_OK)
		return UD3TN_FAIL;

	config->apid = apid;
	config->base.tcp_active = tcp_active;
	config->base.node = strdup(node);
	config->base.service = strdup(service);

	/* Set cla_config vtable */
	config->base.base.base.vtable = &tcpspp_vtable;

	/* Initialize SPP context */
	static const uint8_t preamble[] = {
		CLA_TCPSPP_TIMESTAMP_FORMAT_PREAMBLE
	};
	const bool have_timecode = sizeof(preamble) > 0;

	config->spp_ctx = spp_new_context();
	if (!spp_configure_ancillary_data(config->spp_ctx, 0))
		LOG("tcpspp: Failed to configure ancillary data size!");

	if (CLA_TCPSPP_USE_CRC)
		LOG("tcpspp: Using CRC16-CCITT-FALSE.");
	else
		LOG("tcpspp: Not using CRC.");

	if (have_timecode) {
		config->spp_timecode.with_p_field =
				CLA_TCPSPP_TIMESTAMP_USE_P_FIELD;
		if (spp_tc_configure_from_preamble(
			&config->spp_timecode,
			&preamble[0], sizeof(preamble)) != 0) {
			/* preamble uses unknown format */
			LOG("tcpspp: Failed to configure timecode!");
		} else if (!spp_configure_timecode(
			config->spp_ctx,
			&config->spp_timecode)) {
			/* this can’t actually fail atm, but may in the future
			 */
			LOG("tcpspp: Failed to apply timecode!");
		}
	} else {
		LOG("tcpspp: Not using timecode.");
	}

	return UD3TN_OK;
}

// Note that this is basically the same as parsing a TCP port number.
static enum ud3tn_result parse_apid(const char *str, uint16_t *result)
{
	char *end;
	long val;

	if (!str)
		return UD3TN_FAIL;
	errno = 0;
	val = strtol(str, &end, 10);
	if (errno == ERANGE || val <= 0  || val > UINT16_MAX ||
	    end == str || *end != 0)
		return UD3TN_FAIL;
	*result = (uint16_t)val;
	return UD3TN_OK;
}

struct cla_config *tcpspp_create(
	const char *const options[], const size_t option_count,
	const struct bundle_agent_interface *bundle_agent_interface)
{
	if (option_count < 2 || option_count > 4) {
		LOG("tcpspp: Options format is: <IP>,<PORT>,[<TCP_ACTIVE>[,<APID>]]");
		return NULL;
	}

	bool tcp_active = false;

	if (option_count > 2) {
		if (parse_tcp_active(options[2], &tcp_active) != UD3TN_OK) {
			LOGF("tcpspp: Could not parse TCP active flag: %s",
			     options[2]);
			return NULL;
		}
	}

	uint16_t apid = 1;

	if (option_count > 3) {
		if (parse_apid(options[3], &apid) != UD3TN_OK) {
			LOGF("tcpspp: Could not parse APID: %s", options[3]);
			return NULL;
		}
	}

	struct tcpspp_config *config = malloc(sizeof(struct tcpspp_config));

	if (config == NULL) {
		LOG("tcpspp: Memory allocation failed!");
		return NULL;
	}

	if (tcpspp_init(config, options[0], options[1], tcp_active, apid,
			bundle_agent_interface) < 0) {
		free(config);
		LOG("tcpspp: Initialization failed!");
		return NULL;
	}

	return (struct cla_config *)config;
}
