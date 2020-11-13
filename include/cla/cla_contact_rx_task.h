#ifndef CLA_CONTACT_RX_TASK_H_INCLUDED
#define CLA_CONTACT_RX_TASK_H_INCLUDED

#include "bundle6/parser.h"
#include "bundle7/parser.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum cla_payload_type {
	PAYLOAD_UNKNOWN = 0,
	PAYLOAD_BUNDLE6 = 6,
	PAYLOAD_BUNDLE7 = 7
};

#define CLA_RX_BUFFER_SIZE 64

struct rx_task_data {
	enum cla_payload_type payload_type;

	struct parser *cur_parser;
	struct bundle6_parser bundle6_parser;
	struct bundle7_parser bundle7_parser;

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

enum upcn_result rx_task_data_init(struct rx_task_data *rx_data,
				   void *cla_config);
void rx_task_data_deinit(struct rx_task_data *rx_data);

// Forward declaration to prevent (circular) inclusion of cla.h here.
struct cla_link;

/**
 * @brief cla_launch_contact_rx_task Creates a new RX handler task.
 * @param link The link associated to the task
 * @return whether the operation was successful
 */
enum upcn_result cla_launch_contact_rx_task(struct cla_link *link);

#endif /* CLA_CONTACT_RX_TASK_H_INCLUDED */
