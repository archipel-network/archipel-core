// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#ifndef CLA_CONTACT_RX_TASK_H_INCLUDED
#define CLA_CONTACT_RX_TASK_H_INCLUDED

#include "cla/blackhole_parser.h"

#include "bundle6/parser.h"
#include "bundle7/parser.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum cla_payload_type {
	PAYLOAD_UNKNOWN = 0,
	PAYLOAD_BUNDLE6 = 6,
	PAYLOAD_BUNDLE7 = 7,
	PAYLOAD_IRRELEVANT = 127,
};

// Size of the input buffer in bytes == size of the chunks used for reading.
#ifndef CLA_RX_BUFFER_SIZE
#define CLA_RX_BUFFER_SIZE 64
#endif // CLA_RX_BUFFER_SIZE

struct rx_task_data {
	enum cla_payload_type payload_type;

	struct parser *cur_parser;
	struct bundle6_parser bundle6_parser;
	struct bundle7_parser bundle7_parser;
	struct blackhole_parser blackhole_parser;

	/**
	 * Size of the input buffer in bytes.
	 */
	struct {
		uint8_t start[CLA_RX_BUFFER_SIZE];
		uint8_t *end;
	} input_buffer;

	bool timeout_occured;
};

void rx_task_reset_parsers(struct rx_task_data *rx_data);

size_t select_bundle_parser_version(struct rx_task_data *rx_data,
				    const uint8_t *buffer,
				    size_t length);

enum ud3tn_result rx_task_data_init(struct rx_task_data *rx_data,
				    void *cla_config);
void rx_task_data_deinit(struct rx_task_data *rx_data);

// Forward declaration to prevent (circular) inclusion of cla.h here.
struct cla_link;

// Export read operations for the data decoder binary
uint8_t *rx_bulk_read(struct cla_link *link);
uint8_t *rx_chunk_read(struct cla_link *link);

/**
 * @brief cla_launch_contact_rx_task Creates a new RX handler task.
 * @param link The link associated to the task
 * @return whether the operation was successful
 */
enum ud3tn_result cla_launch_contact_rx_task(struct cla_link *link);

#endif /* CLA_CONTACT_RX_TASK_H_INCLUDED */
