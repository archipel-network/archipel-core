#ifndef CLA_MTCP_H
#define CLA_MTCP_H

#include "cla/cla.h"
#include "cla/posix/cla_tcp_common.h"

#include "ud3tn/bundle_agent_interface.h"

#include <stddef.h>

struct cla_config *mtcp_create(
	const char *const options[], const size_t option_count,
	const struct bundle_agent_interface *bundle_agent_interface);

/* INTERNAL API */

struct mtcp_link {
	struct cla_tcp_link base;
	struct parser mtcp_parser;
};

size_t mtcp_mbs_get(struct cla_config *const config);

void mtcp_reset_parsers(struct cla_link *link);

size_t mtcp_forward_to_specific_parser(struct cla_link *link,
				       const uint8_t *buffer, size_t length);

void mtcp_begin_packet(struct cla_link *link, size_t length);

void mtcp_end_packet(struct cla_link *link);

void mtcp_send_packet_data(
	struct cla_link *link, const void *data, const size_t length);

#endif /* CLA_MTCP_H */
