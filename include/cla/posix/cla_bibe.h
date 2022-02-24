// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#ifndef CLA_bibe_H
#define CLA_bibe_H

#include "aap/aap_parser.h"

#include "cla/cla.h"
#include "cla/posix/cla_tcp_common.h"
#include "cla/bibe_proto.h"

#include "ud3tn/bundle_agent_interface.h"

#include <stddef.h>

struct cla_config *bibe_create(
	const char *const options[], const size_t option_count,
	const struct bundle_agent_interface *bundle_agent_interface);

/* INTERNAL API */

struct bibe_link {
	struct cla_tcp_link base;
	struct aap_parser aap_parser;
};

size_t bibe_mbs_get(struct cla_config *const config);

void bibe_reset_parsers(struct cla_link *link);

size_t bibe_forward_to_specific_parser(struct cla_link *link,
				       const uint8_t *buffer, size_t length);

void bibe_begin_packet(struct cla_link *link, size_t length, char *cla_addr);

void bibe_end_packet(struct cla_link *link);

void bibe_send_packet_data(
	struct cla_link *link, const void *data, const size_t length);

#endif /* CLA_bibe_H */
