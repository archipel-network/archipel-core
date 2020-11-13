#include "cla/cla.h"
#include "cla/cla_contact_tx_task.h"

#include "bundle6/parser.h"
#include "bundle7/parser.h"

#include "platform/hal_config.h"
#include "platform/hal_io.h"
#include "platform/hal_semaphore.h"
#include "platform/hal_task.h"

#include "upcn/bundle_storage_manager.h"
#include "upcn/common.h"
#include "upcn/router_task.h"
#include "upcn/task_tags.h"

#include <stdlib.h>


static inline void report_bundle(QueueIdentifier_t signaling_queue,
				 struct routed_bundle *bundle,
				 enum router_signal_type type)
{
	struct router_signal signal = {
		.type = type,
		.data = bundle
	};

	hal_queue_push_to_back(signaling_queue, &signal);
}

static void cla_contact_tx_task(void *param)
{
	struct cla_link *link = param;
	struct cla_contact_tx_task_command cmd;
	struct bundle *b;
	struct routed_bundle_list *cur;
	enum upcn_result s;
	void const *cla_send_packet_data =
		link->config->vtable->cla_send_packet_data;
	QueueIdentifier_t router_signaling_queue =
		link->config->bundle_agent_interface->router_signaling_queue;

	while (link->active) {
		if (hal_queue_receive(link->tx_queue_handle,
				      &cmd, -1) == UPCN_FAIL)
			continue;
		else if (cmd.type == TX_COMMAND_FINALIZE)
			break;
		/* TX_COMMAND_BUNDLES received */
		while (cmd.bundles != NULL) {
			cur = cmd.bundles;
			cmd.bundles = cmd.bundles->next;
			cur->data->serialized++;
			b = bundle_storage_get(cur->data->id);
			if (b != NULL) {
				LOGF(
					"TX: Sending bundle #%d via CLA %s",
					b->id,
					link->config->vtable->cla_name_get()
				);
				link->config->vtable->cla_begin_packet(
					link,
					bundle_get_serialized_size(b)
				);
				s = bundle_serialize(
					b,
					cla_send_packet_data,
					(void *)link
				);
				link->config->vtable->cla_end_packet(link);
			} else {
				LOGF("TX: Bundle #%d not found!",
				     cur->data->id);
				s = UPCN_FAIL;
			}

			if (s == UPCN_OK) {
				cur->data->transmitted++;
				report_bundle(
					router_signaling_queue,
					cur->data,
					ROUTER_SIGNAL_TRANSMISSION_SUCCESS
				);
			} else {
				report_bundle(
					router_signaling_queue,
					cur->data,
					ROUTER_SIGNAL_TRANSMISSION_FAILURE
				);
			}
			/* Free only the RB list, the RB is reported */
			free(cur);
		}
	}

	// Lock the queue before we start to free it
	hal_semaphore_take_blocking(link->tx_queue_sem);

	// Consume the rest of the queue
	while (hal_queue_receive(link->tx_queue_handle, &cmd, 0) != UPCN_FAIL) {
		if (cmd.type == TX_COMMAND_BUNDLES) {
			while (cmd.bundles != NULL) {
				cur = cmd.bundles;
				cmd.bundles = cmd.bundles->next;
				cur->data->serialized++;
				report_bundle(
					router_signaling_queue,
					cur->data,
					ROUTER_SIGNAL_TRANSMISSION_FAILURE
				);
				/* Free only the RB list, the RB is reported */
				free(cur);
			}
		}
	}

	Task_t tx_task_handle = link->tx_task_handle;

	// After releasing the semaphore, link may become invalid.
	hal_semaphore_release(link->tx_task_sem);
	hal_task_delete(tx_task_handle);
}

enum upcn_result cla_launch_contact_tx_task(struct cla_link *link)
{
	static uint8_t ctr = 1;
	static char tname_buf[6];

	tname_buf[0] = 't';
	tname_buf[1] = 'x';
	snprintf(tname_buf + 2, sizeof(tname_buf) - 2, "%hhu", ctr++);

	hal_semaphore_take_blocking(link->tx_task_sem);
	link->tx_task_handle = hal_task_create(
		cla_contact_tx_task,
		tname_buf,
		CONTACT_TX_TASK_PRIORITY,
		link,
		CONTACT_TX_TASK_STACK_SIZE,
		(void *)CONTACT_TX_TASK_TAG
	);

	return link->tx_task_handle ? UPCN_OK : UPCN_FAIL;
}

void cla_contact_tx_task_request_exit(QueueIdentifier_t queue)
{
	struct cla_contact_tx_task_command command = {
		.type = TX_COMMAND_FINALIZE,
		.bundles = NULL,
	};

	ASSERT(queue != NULL);
	hal_queue_push_to_back(queue, &command);
}
