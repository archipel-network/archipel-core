#include "cla/blackhole_parser.h"
#include "cla/cla.h"
#include "cla/cla_contact_rx_task.h"

#include "bundle6/parser.h"
#include "bundle7/parser.h"

#include "platform/hal_config.h"
#include "platform/hal_io.h"
#include "platform/hal_semaphore.h"
#include "platform/hal_task.h"
#include "platform/hal_time.h"

#include "ud3tn/bundle_processor.h"
#include "ud3tn/bundle_storage_manager.h"
#include "ud3tn/common.h"
#include "ud3tn/config.h"
#include "ud3tn/task_tags.h"

#include <signal.h>
#include <errno.h>

#ifdef CLA_RX_READ_TIMEOUT
static const unsigned long CLA_RX_READ_TIMEOUT_MS = CLA_RX_READ_TIMEOUT;
#else // CLA_RX_READ_TIMEOUT
static const unsigned long CLA_RX_READ_TIMEOUT_MS;
#endif // CLA_RX_READ_TIMEOUT

static void bundle_send(struct bundle *bundle, void *param)
{
	struct cla_config *const config = param;
	bundleid_t new_id;

	ASSERT(bundle != NULL);

	if (strncmp(config->bundle_agent_interface->local_eid,
		    bundle->source,
		    strlen(config->bundle_agent_interface->local_eid)) == 0) {
		LOGF("CLA: Dropping bundle from \"%s\" (EID spoofing detected)",
		     bundle->source);
		bundle_free(bundle);
		return;
	}

	new_id = bundle_storage_add(bundle);
	if (new_id != BUNDLE_INVALID_ID) {
		LOGF("CLA: Received new bundle #%d from \"%s\" to \"%s\" via CLA %s",
		     new_id, bundle->source, bundle->destination,
		     config->vtable->cla_name_get());
		bundle_processor_inform(
			config->bundle_agent_interface->bundle_signaling_queue,
			new_id,
			BP_SIGNAL_BUNDLE_INCOMING,
			BUNDLE_SR_REASON_NO_INFO
		);
	} else {
		LOGF("CLA: Dropping bundle from \"%s\" (OOM?)", bundle->source);
		bundle_free(bundle);
	}
}

enum ud3tn_result rx_task_data_init(struct rx_task_data *rx_data,
				    void *cla_config)
{
	rx_data->payload_type = PAYLOAD_UNKNOWN;
	rx_data->timeout_occured = false;
	rx_data->input_buffer.end = &rx_data->input_buffer.start[0];

	if (!bundle6_parser_init(&rx_data->bundle6_parser,
				 &bundle_send, cla_config))
		return UD3TN_FAIL;
	if (!bundle7_parser_init(&rx_data->bundle7_parser,
				 &bundle_send, cla_config))
		return UD3TN_FAIL;
	rx_data->bundle7_parser.bundle_quota = BUNDLE_QUOTA;
	if (!blackhole_parser_init(&rx_data->blackhole_parser))
		return UD3TN_FAIL;

	return UD3TN_OK;
}

void rx_task_reset_parsers(struct rx_task_data *rx_data)
{
	rx_data->payload_type = PAYLOAD_UNKNOWN;

	ASSERT(bundle6_parser_reset(&rx_data->bundle6_parser) == UD3TN_OK);
	ASSERT(bundle7_parser_reset(&rx_data->bundle7_parser) == UD3TN_OK);
	ASSERT(blackhole_parser_reset(&rx_data->blackhole_parser) == UD3TN_OK);
}

void rx_task_data_deinit(struct rx_task_data *rx_data)
{
	rx_data->payload_type = PAYLOAD_UNKNOWN;

	ASSERT(bundle6_parser_deinit(&rx_data->bundle6_parser) == UD3TN_OK);
	ASSERT(bundle7_parser_deinit(&rx_data->bundle7_parser) == UD3TN_OK);
	ASSERT(blackhole_parser_deinit(&rx_data->blackhole_parser) == UD3TN_OK);
}

size_t select_bundle_parser_version(struct rx_task_data *rx_data,
				    const uint8_t *buffer,
				    size_t length)
{
	/* Empty buffers cannot be parsed */
	if (length == 0)
		return 0;

	switch (buffer[0]) {
	/* Bundle Protocol v6 (RFC 5050) */
	case 6:
		/* rx_data->spp_parser->type = INPUT_TYPE_BUNDLE_V6; */
		rx_data->payload_type = PAYLOAD_BUNDLE6;
		rx_data->cur_parser = rx_data->bundle6_parser.basedata;
		return bundle6_parser_read(&rx_data->bundle6_parser,
				buffer, length);
	/* CBOR indefinite array -> Bundle Protocol v7 */
	case 0x9f:
		/* rx_data->input_parser->type = INPUT_TYPE_BUNDLE_V7; */
		rx_data->payload_type = PAYLOAD_BUNDLE7;
		rx_data->cur_parser = rx_data->bundle7_parser.basedata;
		return bundle7_parser_read(&rx_data->bundle7_parser,
				buffer, length);
	/* Unknown Bundle Protocol version, keep buffer */
	default:
		return 0;
	}
}

/**
 * If a "bulk read" operation is requested, this gets handled by the input
 * processor directly. A preallocated byte buffer and the requested length have
 * to be passed in the parser base data structure. After successful completion
 * of the bulk read operation, the parser gets called again with an empty input
 * buffer (NULL pointer) and the bulk read flag cleared to trigger further
 * processing.
 *
 * Bytes in the current input buffer are considered and copied appropriatly --
 * meaning that non-parsed input bytes are copied into the bulk read buffer and
 * the remaining bytes are read directly from the input stream.
 * @return Pointer to the position up to the input buffer is consumed after the
 *         operation.
 */
static uint8_t *bulk_read(struct cla_link *link)
{
	struct rx_task_data *const rx_data = &link->rx_task_data;
	uint8_t *parsed = rx_data->input_buffer.start +
			  rx_data->cur_parser->next_bytes;

	/*
	 * Bulk read operation requested that is smaller than the input buffer.
	 *
	 * -------------------------------------
	 * |   |   |   |   |   |   |   |   |   | input buffer
	 * -------------------------------------
	 *
	 * |_______________________|___________|
	 *
	 *   bulk read operation     remaining
	 *
	 *
	 */
	if (parsed <= rx_data->input_buffer.end) {
		/* Fill bulk read buffer from input buffer. */
		memcpy(
			rx_data->cur_parser->next_buffer,
			rx_data->input_buffer.start,
			rx_data->cur_parser->next_bytes
		);
	}

	/*
	 *
	 * -----------------------------
	 * |   |   |   |   |   |   |   |  input buffer
	 * -----------------------------
	 *
	 * |___________________________________________|
	 *
	 *              bulk read operation
	 *
	 * ---------------------------------------------
	 * |   |   |   |   |   |   |   |   |   |   |   |  bulk buffer
	 * ---------------------------------------------
	 *                               ^
	 *                               |
	 *                               |
	 *                    pointer for HAL read operation
	 */
	else {
		size_t filled = rx_data->input_buffer.end -
				rx_data->input_buffer.start;

		/* Copy the whole input buffer to bulk read buffer. */
		if (filled)
			memcpy(
				rx_data->cur_parser->next_buffer,
				rx_data->input_buffer.start,
				filled
			);

		size_t to_read = rx_data->cur_parser->next_bytes - filled;
		uint8_t *pos = rx_data->cur_parser->next_buffer + filled;
		size_t read;

		while (to_read) {
			/* Read the remaining bytes directly from the HAL. */
			enum ud3tn_result result =
				link->config->vtable->cla_read(
					link,
					pos,
					to_read,
					&read
				);

			/* We could not read from input, reset all parsers. */
			if (result != UD3TN_OK) {
				link->config->vtable->cla_rx_task_reset_parsers(
					link
				);
				return rx_data->input_buffer.end;
			}

			const uint64_t cur_time = hal_time_get_timestamp_ms();

			if (CLA_RX_READ_TIMEOUT_MS) {
				// Timeout check - if we waited for too long
				// since the last bytes parsed, reset.
				const uint64_t time_since_last_rx = (
					cur_time - link->last_rx_time_ms
				);

				if (time_since_last_rx >
						CLA_RX_READ_TIMEOUT_MS) {
					LOGF("RX: Timeout after %llu ms in bulk read mode, reset.",
					     time_since_last_rx);
					link->config->vtable
						->cla_rx_task_reset_parsers(
							link
						);
					return rx_data->input_buffer.end;
				}
			}

			link->last_rx_time_ms = cur_time;

			ASSERT(read <= to_read);
			to_read -= read;
			pos += read;
		}

		// We have read everything that was in the buffer (+ more,
		// but that is not relevant to the caller).
		parsed = rx_data->input_buffer.end;
	}

	/* Disable bulk read mode. */
	rx_data->cur_parser->flags &= ~PARSER_FLAG_BULK_READ;

	/*
	 * Feed parser with an empty buffer, indicating that the bulk read
	 * operation was performed.
	 */
	link->config->vtable->cla_rx_task_forward_to_specific_parser(
		link,
		NULL,
		0
	);

	if (rx_data->cur_parser->status != PARSER_STATUS_GOOD) {
		if (rx_data->cur_parser->status == PARSER_STATUS_ERROR)
			LOG("RX: Parser failed after bulk read, reset.");
		link->config->vtable->cla_rx_task_reset_parsers(link);
	}

	return parsed;
}


/**
 * Reads a chunk of bytes into the input buffer and forwards the input buffer
 * to the specific parser.
 *
 * @return Number of bytes remaining in the input buffer after the operation.
 */
static uint8_t *chunk_read(struct cla_link *link)
{
	struct rx_task_data *const rx_data = &link->rx_task_data;
	// Receive Step - Receive data from I/O system into buffer
	size_t read = 0;

	ASSERT(rx_data->input_buffer.start + CLA_RX_BUFFER_SIZE >=
	       rx_data->input_buffer.end);

	enum ud3tn_result result = link->config->vtable->cla_read(
		link,
		rx_data->input_buffer.end,
		(rx_data->input_buffer.start + CLA_RX_BUFFER_SIZE)
			- rx_data->input_buffer.end,
		&read
	);

	ASSERT(rx_data->input_buffer.start + CLA_RX_BUFFER_SIZE >=
	       rx_data->input_buffer.end + read);

	/* We could not read from input, thus, reset all parsers. */
	if (result != UD3TN_OK) {
		link->config->vtable->cla_rx_task_reset_parsers(link);
		return rx_data->input_buffer.end;
	}

	rx_data->input_buffer.end += read;

	const uint64_t cur_time = hal_time_get_timestamp_ms();

	// Timeout check - if we waited for too long since the last
	// bytes parsed, reset, and parse this chunk as "something new".
	if (CLA_RX_READ_TIMEOUT_MS) {
		const uint64_t time_since_last_rx = (
			cur_time - link->last_rx_time_ms
		);

		if (time_since_last_rx > CLA_RX_READ_TIMEOUT_MS) {
			LOGF("RX: New data received after %llu seconds, start parsing.",
			     time_since_last_rx);
			link->config->vtable->cla_rx_task_reset_parsers(link);
		}
	}

	link->last_rx_time_ms = cur_time;

	// Parsing Step - read back buffer contents and return start pointer
	uint8_t *stream = rx_data->input_buffer.start;

	while (stream < rx_data->input_buffer.end) {
		size_t parsed = link->config->vtable
			->cla_rx_task_forward_to_specific_parser(
				link,
				stream,
				rx_data->input_buffer.end - stream
			);

		/* Advance stream pointer. */
		stream += parsed;

		if (rx_data->cur_parser->status != PARSER_STATUS_GOOD) {
			if (rx_data->cur_parser->status == PARSER_STATUS_ERROR)
				LOG("RX: Parser failed, reset.");
			link->config->vtable->cla_rx_task_reset_parsers(
				link
			);
		} else if (HAS_FLAG(rx_data->cur_parser->flags,
				    PARSER_FLAG_BULK_READ)) {
			/* Bulk read requested - not handled by us. */
			break;
		} else if (parsed == 0) {
			/*
			 * No bytes were parsed -- meaning that there is not
			 * enough data in the buffer. Stop parsing and wait
			 * for the buffer to be filled with sufficient data in
			 * next read iteration.
			 */
			break;
		}
	}

	return stream;
}

static void cla_contact_rx_task(void *const param)
{
	struct cla_link *link = param;
	struct rx_task_data *const rx_data = &link->rx_task_data;

	uint8_t *parsed;

	while (link->active) {
		if (HAS_FLAG(rx_data->cur_parser->flags, PARSER_FLAG_BULK_READ))
			parsed = bulk_read(link);
		else
			parsed = chunk_read(link);

		/* The whole input buffer was consumed, reset it. */
		if (parsed == rx_data->input_buffer.end) {
			rx_data->input_buffer.end = rx_data->input_buffer.start;
		/*
		 * Remove parsed bytes from input buffer by shifting
		 * the parsed buffer bytes to the left.
		 *
		 *               remaining = 4
		 * ---------------------------------------
		 * | / | / | / | x | x | x | x |   |   |   ...
		 * ---------------------------------------
		 *               ^               ^
		 *               |               |
		 *               |               |
		 *             parsed           end
		 *
		 * memmove:
		 *     Copying takes place as if an intermediate buffer
		 *     were used, allowing the destination and source to
		 *     overlap.
		 *
		 * ---------------------------------------
		 * | x | x | x | x |   |   |   |   |   |   ...
		 * ---------------------------------------
		 *                   ^
		 *                   |
		 *                   |
		 *                  end
		 */
		} else if (parsed != rx_data->input_buffer.start) {
			ASSERT(parsed > rx_data->input_buffer.start);

			memmove(rx_data->input_buffer.start,
				parsed,
				rx_data->input_buffer.end - parsed);

			/*
			 * Move end pointer backwards for that amount of bytes
			 * that were parsed.
			 */
			rx_data->input_buffer.end -=
				parsed - rx_data->input_buffer.start;
		/*
		 * No bytes were parsed but the input buffer is full. We assume
		 * that there was an attempt to send a too large value not
		 * fitting into the input buffer.
		 *
		 * We discard the current buffer content and reset all parsers.
		 */
		} else if (rx_data->input_buffer.end ==
			 rx_data->input_buffer.start + CLA_RX_BUFFER_SIZE) {
			LOG("RX: WARNING, RX buffer is full.");
			link->config->vtable->cla_rx_task_reset_parsers(
				link
			);
			rx_data->input_buffer.end = rx_data->input_buffer.start;
		}
	}

	Task_t rx_task_handle = link->rx_task_handle;

	// After releasing the semaphore, link may become invalid.
	hal_semaphore_release(link->rx_task_sem);
	hal_task_delete(rx_task_handle);
}

enum ud3tn_result cla_launch_contact_rx_task(struct cla_link *link)
{
	static uint8_t ctr = 1;
	static char tname_buf[6];

	tname_buf[0] = 'r';
	tname_buf[1] = 'x';
	snprintf(tname_buf + 2, sizeof(tname_buf) - 2, "%hhu", ctr++);

	hal_semaphore_take_blocking(link->rx_task_sem);
	link->rx_task_handle = hal_task_create(
		cla_contact_rx_task,
		tname_buf,
		CONTACT_RX_TASK_PRIORITY,
		link,
		CONTACT_RX_TASK_STACK_SIZE,
		(void *)CONTACT_RX_TASK_TAG
	);
	return link->rx_task_handle ? UD3TN_OK : UD3TN_FAIL;
}
