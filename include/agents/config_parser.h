#ifndef CONFIGPARSER_H_INCLUDED
#define CONFIGPARSER_H_INCLUDED

#include "ud3tn/parser.h"
#include "ud3tn/result.h"
#include "ud3tn/router_task.h"

#include <stdint.h>

enum config_parser_stage {
	RP_EXPECT_COMMAND_TYPE,
	RP_EXPECT_NODE_CONF_START_DELIMITER,
	RP_EXPECT_NODE_CONF_EID,
	RP_EXPECT_NODE_CONF_RELIABILITY_SEPARATOR,
	RP_EXPECT_NODE_CONF_RELIABILITY,
	RP_EXPECT_NODE_CONF_CLA_ADDR_SEPARATOR,
	RP_EXPECT_CLA_ADDR_START_DELIMITER,
	RP_EXPECT_CLA_ADDR,
	RP_EXPECT_CLA_ADDR_NODES_SEPARATOR,
	RP_EXPECT_NODE_LIST_START_DELIMITER,
	RP_EXPECT_NODE_START_DELIMITER,
	RP_EXPECT_NODE_EID,
	RP_EXPECT_NODE_SEPARATOR,
	RP_EXPECT_NODES_CONTACTS_SEPARATOR,
	RP_EXPECT_CONTACT_LIST_START_DELIMITER,
	RP_EXPECT_CONTACT_START_DELIMITER,
	RP_EXPECT_CONTACT_START_TIME,
	RP_EXPECT_CONTACT_END_TIME,
	RP_EXPECT_CONTACT_BITRATE,
	RP_EXPECT_CONTACT_NODE_LIST_START_DELIMITER,
	RP_EXPECT_CONTACT_NODE_START_DELIMITER,
	RP_EXPECT_CONTACT_NODE_EID,
	RP_EXPECT_CONTACT_NODE_SEPARATOR,
	RP_EXPECT_CONTACT_END_DELIMITER,
	RP_EXPECT_CONTACT_SEPARATOR,
	RP_EXPECT_COMMAND_END_MARKER
};

struct config_parser {
	struct parser *basedata;
	void (*send_callback)(struct router_command *, void *);
	void *send_param;
	enum config_parser_stage stage;
	struct router_command *router_command;
	int current_index;
	char *current_int_data;
	struct endpoint_list *current_eid;
	struct contact_list *current_contact;
};

struct parser *config_parser_init(
	struct config_parser *parser,
	void (*send_callback)(struct router_command *, void *), void *param);
size_t config_parser_read(struct config_parser *parser,
	const uint8_t *buffer, size_t length);
enum ud3tn_result config_parser_reset(struct config_parser *parser);

#endif /* CONFIGPARSER_H_INCLUDED */
