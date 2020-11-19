#include "agents/config_parser.h"

#include "platform/hal_io.h"

#include "ud3tn/node.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>

static char const EID_START_DELIMITER = '(';
static char const EID_END_DELIMITER = ')';
static char const CLA_ADDR_START_DELIMITER = '(';
static char const CLA_ADDR_END_DELIMITER = ')';
static char const NODE_CONF_RELIABILITY_SEPARATOR = ',';
static char const NODE_CONF_CLA_ADDR_SEPARATOR = ':';
static char const CLA_ADDR_NODES_SEPARATOR = ':';
static char const LIST_START_DELIMITER = '[';
static char const LIST_END_DELIMITER = ']';
static char const LIST_ELEMENT_SEPARATOR = ',';
static char const OBJECT_START_DELIMITER = '{';
static char const OBJECT_END_DELIMITER = '}';
static char const OBJECT_ELEMENT_SEPARATOR = ',';
static char const NODES_CONTACTS_SEPARATOR = ':';

static uint8_t const COMMAND_END_MARKER = ';';

static uint8_t const DEFAULT_EID_BUFFER_SIZE = 16;
static uint8_t const DEFAULT_CLA_ADDR_BUFFER_SIZE = 21;
static uint8_t const DEFAULT_INT_BUFFER_SIZE = 16;

static void send_router_command(struct config_parser *parser);

static void begin_read_data_eid(
	struct config_parser *parser, struct endpoint_list **target)
{
	struct endpoint_list *new_entry = malloc(sizeof(struct endpoint_list));
	char *new_eid = malloc(DEFAULT_EID_BUFFER_SIZE * sizeof(char));

	new_eid[0] = '\0';
	new_entry->eid = new_eid;
	new_entry->next = NULL;

	if (parser->current_eid == NULL)
		*target = new_entry;
	else
		parser->current_eid->next = new_entry;
	parser->current_eid = new_entry;
	parser->current_index = 0;
}

static void begin_read_node_conf_eid(struct config_parser *parser)
{
	parser->router_command->data->eid =
		malloc(DEFAULT_EID_BUFFER_SIZE * sizeof(char));
	parser->router_command->data->eid[0] = '\0';
	parser->current_index = 0;
}

static bool read_eid(
	struct config_parser *parser, char **eid_ptr, const char byte)
{
	/* Check for valid chars (URIs) */
	if (
		/* !"#$%&'()*+,-./0123456789:<=>?@ / A-Z / [\]^_ */
		(byte >= 0x21 && byte <= 0x5F) ||
		/* a-z / ~ */
		(byte >= 0x61 && byte <= 0x7A) || byte == 0x7E
	) {
		(*eid_ptr)[parser->current_index++] = byte;
		if (parser->current_index >= DEFAULT_EID_BUFFER_SIZE)
			(*eid_ptr) = realloc(*eid_ptr,
				(parser->current_index + 1) * sizeof(char));
		(*eid_ptr)[parser->current_index] = '\0';
		return true;
	} else {
		return false;
	}
}

static void end_read_eid(struct config_parser *parser, char **eid_ptr)
{
	(*eid_ptr)[parser->current_index] = '\0';
}

static void begin_read_cla_addr(struct config_parser *parser)
{
	parser->router_command->data->cla_addr =
		malloc(DEFAULT_CLA_ADDR_BUFFER_SIZE * sizeof(char));
	parser->router_command->data->cla_addr[0] = '\0';
	parser->current_index = 0;
}

static void end_read_cla_addr(struct config_parser *parser, char **cla_addr_ptr)
{
	(*cla_addr_ptr)[parser->current_index] = '\0';
}

static bool read_cla_addr(
	struct config_parser *parser, char **cla_addr_ptr, const char byte)
{
	if (
		(byte >= '0' && byte <= '9') ||
		(byte >= 'A' && byte <= 'Z') ||
		(byte >= 'a' && byte <= 'z') ||
		(byte == ':') || (byte == '.') ||
		(byte == '[') || (byte == ']')
	) {
		(*cla_addr_ptr)[parser->current_index++] = byte;
		if (parser->current_index >= DEFAULT_CLA_ADDR_BUFFER_SIZE)
			(*cla_addr_ptr) = realloc(*cla_addr_ptr,
				(parser->current_index + 1) * sizeof(char));
		return true;
	} else {
		return false;
	}
}

static void begin_read_contact(struct config_parser *parser)
{
	struct contact_list *new_entry = malloc(sizeof(struct contact_list));

	new_entry->next = NULL;
	new_entry->data = contact_create(parser->router_command->data);
	if (parser->current_contact == NULL)
		parser->router_command->data->contacts = new_entry;
	else
		parser->current_contact->next = new_entry;
	parser->current_contact = new_entry;
	/* Reset for contact eids, misc eid reading has finished here */
	parser->current_eid = NULL;
}

static void begin_read_integer(struct config_parser *parser)
{
	parser->current_int_data
		= malloc(DEFAULT_INT_BUFFER_SIZE * sizeof(char));
	parser->current_index = 0;
}

static bool read_integer(struct config_parser *parser, const char byte)
{
	if (byte >= 0x30 && byte <= 0x39) { /* 0..9 */
		parser->current_int_data[parser->current_index++] = byte;
		if (parser->current_index >= DEFAULT_INT_BUFFER_SIZE)
			parser->current_int_data = realloc(
				parser->current_int_data,
				(parser->current_index + 1) * sizeof(char));
		return true;
	} else {
		return false;
	}
}

static void end_read_uint64(struct config_parser *parser, uint64_t *out)
{
	parser->current_int_data[parser->current_index] = '\0';
	/* No further checks because read_integer already checks for digits */
	*out = strtoull(parser->current_int_data, NULL, 10);
	free(parser->current_int_data);
	parser->current_int_data = NULL;
}

static void end_read_uint32(struct config_parser *parser, uint32_t *out)
{
	parser->current_int_data[parser->current_index] = '\0';
	/* No further checks because read_integer already checks for digits */
	*out = strtoul(parser->current_int_data, NULL, 10);
	free(parser->current_int_data);
	parser->current_int_data = NULL;
}

static void end_read_uint16(struct config_parser *parser, uint16_t *out)
{
	parser->current_int_data[parser->current_index] = '\0';
	/* No further checks because read_integer already checks for digits */
	/* XXX: Overflow might be possible here */
	*out = (uint16_t)atoi(parser->current_int_data);
	free(parser->current_int_data);
	parser->current_int_data = NULL;
}

static void read_command(struct config_parser *parser, const uint8_t byte)
{
	uint16_t tmp;
	struct node *cur_gs = parser->router_command->data;

	switch (parser->stage) {
	default:
		parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	case RP_EXPECT_NODE_CONF_START_DELIMITER:
		if (byte == EID_START_DELIMITER) {
			begin_read_node_conf_eid(parser);
			parser->stage = RP_EXPECT_NODE_CONF_EID;
		} else {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
		break;
	case RP_EXPECT_NODE_CONF_EID:
		if (byte == EID_END_DELIMITER) {
			end_read_eid(parser, &(cur_gs->eid));
			parser->stage =
				RP_EXPECT_NODE_CONF_RELIABILITY_SEPARATOR;
		} else if (!read_eid(parser, &(cur_gs->eid), byte)) {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
		break;
	case RP_EXPECT_NODE_CONF_RELIABILITY_SEPARATOR:
		if (byte == NODE_CONF_RELIABILITY_SEPARATOR) {
			begin_read_integer(parser);
			parser->stage = RP_EXPECT_NODE_CONF_RELIABILITY;
		} else if (byte == NODE_CONF_CLA_ADDR_SEPARATOR) {
			parser->stage = RP_EXPECT_CLA_ADDR_START_DELIMITER;
		} else if (byte == COMMAND_END_MARKER) {
			parser->basedata->status = PARSER_STATUS_DONE;
		} else {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
		break;
	case RP_EXPECT_NODE_CONF_RELIABILITY:
		if (byte == NODE_CONF_CLA_ADDR_SEPARATOR
			|| byte == COMMAND_END_MARKER
		) {
			end_read_uint16(parser, &tmp);
			if (tmp < 100 || tmp > 1000) {
				parser->basedata->status = PARSER_STATUS_ERROR;
				break;
			}
			cur_gs->reliability
				= (float)tmp / 1000.0f;
			parser->stage = RP_EXPECT_CLA_ADDR_START_DELIMITER;
			if (byte == COMMAND_END_MARKER)
				parser->basedata->status = PARSER_STATUS_DONE;
		} else if (!read_integer(parser, byte)) {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
		break;
	case RP_EXPECT_NODE_CONF_CLA_ADDR_SEPARATOR:
		if (byte == NODE_CONF_CLA_ADDR_SEPARATOR)
			parser->stage = RP_EXPECT_CLA_ADDR_START_DELIMITER;
		else if (byte == COMMAND_END_MARKER)
			parser->basedata->status = PARSER_STATUS_DONE;
		else
			parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	case RP_EXPECT_CLA_ADDR_START_DELIMITER:
		if (byte == CLA_ADDR_START_DELIMITER) {
			begin_read_cla_addr(parser);
			parser->stage = RP_EXPECT_CLA_ADDR;
		} else if (byte == CLA_ADDR_NODES_SEPARATOR) {
			parser->stage = RP_EXPECT_NODE_LIST_START_DELIMITER;
		} else if (byte == COMMAND_END_MARKER) {
			parser->basedata->status = PARSER_STATUS_DONE;
		} else {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
		break;
	case RP_EXPECT_CLA_ADDR:
		if (byte == CLA_ADDR_END_DELIMITER) {
			end_read_cla_addr(parser, &(cur_gs->cla_addr));
			parser->stage = RP_EXPECT_CLA_ADDR_NODES_SEPARATOR;
		} else if (!read_cla_addr(parser, &(cur_gs->cla_addr), byte)) {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
		break;
	case RP_EXPECT_CLA_ADDR_NODES_SEPARATOR:
		if (byte == CLA_ADDR_NODES_SEPARATOR)
			parser->stage = RP_EXPECT_NODE_LIST_START_DELIMITER;
		else if (byte == COMMAND_END_MARKER)
			parser->basedata->status = PARSER_STATUS_DONE;
		else
			parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	case RP_EXPECT_NODE_LIST_START_DELIMITER:
		if (byte == LIST_START_DELIMITER)
			parser->stage = RP_EXPECT_NODE_START_DELIMITER;
		else if (byte == NODES_CONTACTS_SEPARATOR)
			parser->stage = RP_EXPECT_CONTACT_LIST_START_DELIMITER;
		else if (byte == COMMAND_END_MARKER)
			parser->basedata->status = PARSER_STATUS_DONE;
		else
			parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	case RP_EXPECT_NODE_START_DELIMITER:
		if (byte == EID_START_DELIMITER) {
			begin_read_data_eid(parser, &(cur_gs->endpoints));
			parser->stage = RP_EXPECT_NODE_EID;
		} else if (byte == LIST_END_DELIMITER) {
			parser->stage = RP_EXPECT_NODES_CONTACTS_SEPARATOR;
		} else {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
		break;
	case RP_EXPECT_NODE_EID:
		if (byte == EID_END_DELIMITER) {
			end_read_eid(parser,
				&(parser->current_eid->eid));
			parser->stage = RP_EXPECT_NODE_SEPARATOR;
		} else if (!read_eid(parser,
			&(parser->current_eid->eid), byte)
		) {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
		break;
	case RP_EXPECT_NODE_SEPARATOR:
		if (byte == LIST_ELEMENT_SEPARATOR)
			parser->stage = RP_EXPECT_NODE_START_DELIMITER;
		else if (byte == LIST_END_DELIMITER)
			parser->stage = RP_EXPECT_NODES_CONTACTS_SEPARATOR;
		else
			parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	case RP_EXPECT_NODES_CONTACTS_SEPARATOR:
		if (byte == NODES_CONTACTS_SEPARATOR)
			parser->stage = RP_EXPECT_CONTACT_LIST_START_DELIMITER;
		else if (byte == COMMAND_END_MARKER)
			parser->basedata->status = PARSER_STATUS_DONE;
		else
			parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	case RP_EXPECT_CONTACT_LIST_START_DELIMITER:
		if (byte == LIST_START_DELIMITER)
			parser->stage = RP_EXPECT_CONTACT_START_DELIMITER;
		else if (byte == COMMAND_END_MARKER)
			parser->basedata->status = PARSER_STATUS_DONE;
		else
			parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	case RP_EXPECT_CONTACT_START_DELIMITER:
		if (byte == OBJECT_START_DELIMITER) {
			begin_read_contact(parser);
			begin_read_integer(parser);
			parser->stage = RP_EXPECT_CONTACT_START_TIME;
		} else if (byte == LIST_END_DELIMITER) {
			parser->stage = RP_EXPECT_COMMAND_END_MARKER;
		} else {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
		break;
	case RP_EXPECT_CONTACT_START_TIME:
		if (byte == OBJECT_ELEMENT_SEPARATOR) {
			end_read_uint64(parser,
				&(parser->current_contact->data->from));
			begin_read_integer(parser);
			parser->stage = RP_EXPECT_CONTACT_END_TIME;
		} else if (!read_integer(parser, byte)) {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
		break;
	case RP_EXPECT_CONTACT_END_TIME:
		if (byte == OBJECT_ELEMENT_SEPARATOR) {
			end_read_uint64(parser,
				&(parser->current_contact->data->to));
			if (parser->current_contact->data->to >
				parser->current_contact->data->from
				&& parser->current_contact->data->to >
				hal_time_get_timestamp_s()
			) {
				begin_read_integer(parser);
				parser->stage = RP_EXPECT_CONTACT_BITRATE;
			} else {
				parser->basedata->status = PARSER_STATUS_ERROR;
			}
		} else if (!read_integer(parser, byte)) {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
		break;
	case RP_EXPECT_CONTACT_BITRATE:
		if (byte == OBJECT_ELEMENT_SEPARATOR) {
			end_read_uint32(parser,
				&(parser->current_contact->data->bitrate));
			parser->stage
				= RP_EXPECT_CONTACT_NODE_LIST_START_DELIMITER;
		} else if (byte == OBJECT_END_DELIMITER) {
			end_read_uint32(parser,
				&(parser->current_contact->data->bitrate));
			parser->stage = RP_EXPECT_CONTACT_SEPARATOR;
		} else if (!read_integer(parser, byte)) {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
		break;
	case RP_EXPECT_CONTACT_NODE_LIST_START_DELIMITER:
		if (byte == LIST_START_DELIMITER)
			parser->stage = RP_EXPECT_CONTACT_NODE_START_DELIMITER;
		else
			parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	case RP_EXPECT_CONTACT_NODE_START_DELIMITER:
		if (byte == EID_START_DELIMITER) {
			begin_read_data_eid(parser, &(parser->current_contact
				->data->contact_endpoints));
			parser->stage = RP_EXPECT_CONTACT_NODE_EID;
		} else if (byte == LIST_END_DELIMITER) {
			parser->stage = RP_EXPECT_CONTACT_END_DELIMITER;
		} else {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
		break;
	case RP_EXPECT_CONTACT_NODE_EID:
		if (byte == EID_END_DELIMITER) {
			end_read_eid(parser, &(parser->current_eid->eid));
			parser->stage = RP_EXPECT_CONTACT_NODE_SEPARATOR;
		} else if (!read_eid(parser,
			&(parser->current_eid->eid), byte)
		) {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
		break;
	case RP_EXPECT_CONTACT_NODE_SEPARATOR:
		if (byte == LIST_ELEMENT_SEPARATOR)
			parser->stage = RP_EXPECT_CONTACT_NODE_START_DELIMITER;
		else if (byte == LIST_END_DELIMITER)
			parser->stage = RP_EXPECT_CONTACT_END_DELIMITER;
		else
			parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	case RP_EXPECT_CONTACT_END_DELIMITER:
		if (byte == OBJECT_END_DELIMITER)
			parser->stage = RP_EXPECT_CONTACT_SEPARATOR;
		else
			parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	case RP_EXPECT_CONTACT_SEPARATOR:
		if (byte == LIST_ELEMENT_SEPARATOR)
			parser->stage = RP_EXPECT_CONTACT_START_DELIMITER;
		else if (byte == LIST_END_DELIMITER)
			parser->stage = RP_EXPECT_COMMAND_END_MARKER;
		else
			parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	case RP_EXPECT_COMMAND_END_MARKER:
		if (byte == COMMAND_END_MARKER)
			parser->basedata->status = PARSER_STATUS_DONE;
		else
			parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	}
}

static void config_parser_read_byte(struct config_parser *parser, uint8_t byte)
{
	if (parser->basedata->status != PARSER_STATUS_GOOD)
		return;

	if (parser->stage == RP_EXPECT_COMMAND_TYPE) {
		parser->stage = RP_EXPECT_NODE_CONF_START_DELIMITER;
		if (byte >= (uint8_t)ROUTER_COMMAND_ADD
			&& byte <= (uint8_t)ROUTER_COMMAND_QUERY
		) {
			parser->router_command->type
				= (enum router_command_type)byte;
		} else {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
	} else {
		read_command(parser, *(char *)(&byte));
	}

	if (parser->basedata->status == PARSER_STATUS_DONE)
		send_router_command(parser);
}

size_t config_parser_read(struct config_parser *parser,
	const uint8_t *buffer, size_t length)
{
	size_t i = 0;

	while (i < length) {
		config_parser_read_byte(parser, buffer[i]);
		if (parser->basedata->status != PARSER_STATUS_GOOD
		    && parser->basedata->status != PARSER_STATUS_DONE) {
			LOGF("ConfigAgentParser: parser status was not good at %d ('%c') -> reset parser (cmd was: \"%s\")",
			     i, buffer[i], buffer);
			config_parser_reset(parser);
			return length;
		}
		i++;
	}

	return i;
}

enum ud3tn_result config_parser_reset(struct config_parser *parser)
{
	if (parser->basedata->status == PARSER_STATUS_GOOD &&
			parser->stage == RP_EXPECT_COMMAND_TYPE)
		return UD3TN_OK;
	else if (parser->basedata->status == PARSER_STATUS_ERROR)
		(void)parser;
	parser->basedata->status = PARSER_STATUS_GOOD;
	parser->basedata->flags = PARSER_FLAG_NONE;
	parser->stage = RP_EXPECT_COMMAND_TYPE;
	parser->current_eid = NULL;
	parser->current_contact = NULL;
	if (parser->router_command != NULL) {
		if (parser->router_command->data != NULL)
			free_node(parser->router_command->data);
		free(parser->router_command);
		parser->router_command = NULL;
	}
	if (parser->current_int_data != NULL) {
		free(parser->current_int_data);
		parser->current_int_data = NULL;
	}
	parser->router_command = malloc(sizeof(struct router_command));
	if (parser->router_command == NULL)
		return UD3TN_FAIL;
	parser->router_command->type = ROUTER_COMMAND_UNDEFINED;
	parser->router_command->data = node_create(NULL);
	if (parser->router_command->data == NULL)
		return UD3TN_FAIL;
	return UD3TN_OK;
}

struct parser *config_parser_init(
	struct config_parser *parser,
	void (*send_callback)(struct router_command *, void *), void *param)
{
	parser->basedata = malloc(sizeof(struct parser));
	if (parser->basedata == NULL)
		return NULL;
	parser->send_callback = send_callback;
	parser->send_param = param;
	parser->basedata->status = PARSER_STATUS_ERROR;
	parser->router_command = NULL;
	if (config_parser_reset(parser) != UD3TN_OK)
		return NULL;
	return parser->basedata;
}

static void send_router_command(struct config_parser *parser)
{
	struct router_command *ptr;

	if (parser->send_callback == NULL)
		return;
	ptr = parser->router_command;
	/* Unset router cmd, the recipient has to take care of it... */
	parser->router_command = NULL;
	parser->send_callback(ptr, parser->send_param);
	/* Don't reset the parser here as the input task must know the state */
}
