#include "bundle7/parser.h"

#include "cla/cla.h"
#include "cla/cla_contact_rx_task.h"

#include "ud3tn/bundle.h"
#include "ud3tn/common.h"
#include "ud3tn/parser.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// GENERIC CLA EMULATION

typedef size_t (*parse_func_t)(void *, const uint8_t *, size_t);

struct file_link {
	struct cla_link base;
	FILE *fp;
	parse_func_t parse_func;
	void *parse_param;
	bool error;
};

static void reset_parsers(struct cla_link *const link)
{
	struct file_link *const file_link = (struct file_link *)link;

	if (link->rx_task_data.cur_parser->status != PARSER_STATUS_DONE)
		file_link->error = true;
}

static size_t fwd_to_parser(
	struct cla_link *const link,
	const uint8_t *const buffer,
	const size_t length)
{
	struct file_link *const file_link = (struct file_link *)link;

	return file_link->parse_func(file_link->parse_param, buffer, length);
}

static enum ud3tn_result file_read(
	struct cla_link *const link,
	uint8_t *const buffer,
	const size_t length,
	size_t *const bytes_read)
{
	struct file_link *const file_link = (struct file_link *)link;

	if (feof(file_link->fp))
		return UD3TN_FAIL;

	size_t ret = fread(
		buffer,
		1,
		length,
		file_link->fp
	);

	if (ret != length) {
		if (ferror(file_link->fp)) {
			perror("fread()");
			file_link->error = true;
			return UD3TN_FAIL;
		} else if (ret == 0) {
			return UD3TN_FAIL;
		}
	}

	if (bytes_read)
		*bytes_read = ret;

	return UD3TN_OK;
}

static enum ud3tn_result parse_until_done(
	FILE *const fp,
	struct parser *const parser_base,
	const parse_func_t parse_func,
	void *const parse_param)
{
	struct cla_vtable vtable = {
		.cla_rx_task_reset_parsers = reset_parsers,
		.cla_rx_task_forward_to_specific_parser = fwd_to_parser,
		.cla_read = file_read,
	};
	struct cla_config config = {
		.vtable = &vtable,
	};
	struct file_link link = {
		.base = {
			.config = &config,
			.rx_task_data = {
				.cur_parser = parser_base,
			},
		},
		.fp = fp,
		.parse_func = parse_func,
		.parse_param = parse_param,
		.error = false,
	};
	link.base.rx_task_data.input_buffer.end = (
		&link.base.rx_task_data.input_buffer.start[0]
	);

	// See cla_contact_rx_task in cla_contact_rx_task.c
	struct rx_task_data *rx_data = &link.base.rx_task_data;
	uint8_t *parsed;

	for (;;) {
		if (HAS_FLAG(rx_data->cur_parser->flags, PARSER_FLAG_BULK_READ))
			parsed = rx_bulk_read(&link.base);
		else
			parsed = rx_chunk_read(&link.base);

		if (rx_data->cur_parser->status == PARSER_STATUS_ERROR) {
			fprintf(stderr, "Parser error reported, aborting.\n");
			link.error = true;
			break;
		}

		if (rx_data->cur_parser->status == PARSER_STATUS_DONE)
			break;

		if (link.error) {
			fprintf(stderr, "Parser reset detected, aborting.\n");
			break;
		}

		/* The whole input buffer was consumed, reset it. */
		if (parsed == rx_data->input_buffer.end) {
			rx_data->input_buffer.end = rx_data->input_buffer.start;
		} else if (parsed != rx_data->input_buffer.start) {
			ASSERT(parsed > rx_data->input_buffer.start);
			memmove(rx_data->input_buffer.start,
				parsed,
				rx_data->input_buffer.end - parsed);
			rx_data->input_buffer.end -=
				parsed - rx_data->input_buffer.start;
		} else if (rx_data->input_buffer.end ==
			 rx_data->input_buffer.start + CLA_RX_BUFFER_SIZE) {
			fprintf(stderr, "Error: Parsing buffer full, reset.\n");
			link.error = true;
			rx_data->input_buffer.end = rx_data->input_buffer.start;
			break;
		}
	}

	return link.error ? UD3TN_FAIL : UD3TN_OK;
}

// GENERIC BUNDLE HELPERS

static const char *block_type_to_string(enum bundle_block_type type)
{
	switch (type) {
	default:
		return "unknown block";
	case BUNDLE_BLOCK_TYPE_PAYLOAD:
		return "payload block";
	case BUNDLE_BLOCK_TYPE_PREVIOUS_NODE:
		return "previous node block";
	case BUNDLE_BLOCK_TYPE_BUNDLE_AGE:
		return "bundle age block";
	case BUNDLE_BLOCK_TYPE_HOP_COUNT:
		return "hop count block";
	}
}

static void print_bundle(const struct bundle *const bundle)
{
	printf((
			"BPv%d bundle\n"
			"  - source:       %s\n"
			"  - destination:  %s\n"
			"  - report to:    %s\n"
			"  - creation ts.: %" PRIu64 "\n"
			"  - sequence no.: %" PRIu64 "\n"
			"  - expires at:   %" PRIu64 "\n"
			"  - payload len.: %" PRIu32 "\n"
			"  - proc. flags:  0x%04x\n"
		),
		bundle->protocol_version,
		bundle->source ? bundle->source : "<null>",
		bundle->destination ? bundle->destination : "<null>",
		bundle->report_to ? bundle->report_to : "<null>",
		bundle->creation_timestamp_ms,
		bundle->sequence_number,
		bundle_get_expiration_time_s(bundle),
		bundle->payload_block ? bundle->payload_block->length : 0,
		bundle->proc_flags
	);

	const struct bundle_block_list *blocks = bundle->blocks;

	while (blocks) {
		printf((
				"  - block no. %d of type = %d (%s)\n"
				"    - flags:  0x%04x\n"
				"    - length: %" PRIu32 "\n"
			),
			blocks->data->number,
			blocks->data->type,
			block_type_to_string(blocks->data->type),
			blocks->data->flags,
			blocks->data->length
		);

		blocks = blocks->next;
	}
}

static void send_bundle(struct bundle *bundle, void *param)
{
	struct bundle **dest = param;

	*dest = bundle;
}

// BPv7

static size_t invoke_bpv7_parser(
	void *const parser,
	const uint8_t *const buffer, const size_t length)
{
	struct bundle7_parser *bpv7_parser = (struct bundle7_parser *)parser;

	return bundle7_parser_read(bpv7_parser, buffer, length);
}

static int parse_bpv7(FILE *const fp)
{
	struct bundle7_parser bpv7_parser = {};
	struct bundle *result;

	if (!bundle7_parser_init(&bpv7_parser, send_bundle, &result)) {
		fprintf(stderr, "Failed to initialize parser.\n");
		return 1;
	}

	enum ud3tn_result rc = parse_until_done(
		fp,
		bpv7_parser.basedata,
		invoke_bpv7_parser,
		&bpv7_parser
	);

	bundle7_parser_deinit(&bpv7_parser);

	if (rc != UD3TN_OK) {
		fprintf(stderr, "Failed parsing file as BPv7 bundle.\n");
		return 1;
	}

	print_bundle(result);
	bundle_free(result);

	return 0;
}

// MAIN LOGIC

static void usage(void)
{
	const char *usage_text = "Usage: ud3tndecode <datatype> <file>\n\n"
		"<datatype> may be one of the following:\n"
		"    -6 - parse the input file as BPv6 (RFC 5050) bundle\n"
		"    -7 - parse the input file as BPv7 (RFC 9171) bundle\n"
		"    -a - parse the input file as AAP packet\n"
		"    -c - parse the input file as uD3TN configuration message\n"
		"    -s - parse the input file as SPP packet\n";

	fprintf(stderr, "%s", usage_text);
}

int main(const int argc, const char *const argv[])
{
	if (argc >= 2 && strcmp(argv[1], "-h") == 0) {
		usage();
		return 0;
	}

	if (argc < 3 || strlen(argv[1]) != 2 || argv[1][0] != '-') {
		usage();
		return 1;
	}

	FILE *fp = fopen(argv[2], "rb");

	if (!fp) {
		perror("fopen()");
		return 1;
	}

	int rc = 0;

	switch (argv[1][1]) {
	default:
		usage();
		rc = 1;
		break;
	case '7':
		rc = parse_bpv7(fp);
		break;
	// TODO: All other data types listed in `usage()`
	}

	fclose(fp);

	return rc;
}
