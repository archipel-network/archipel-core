#include "ud3tn/bundle.h"
#include "ud3tn/common.h"
#include "ud3tn/contact_manager.h"
#include "ud3tn/node.h"
#include "ud3tn/router.h"
#include "ud3tn/router_optimizer.h"
#include "ud3tn/router_task.h"
#include "ud3tn/routing_table.h"
#include "ud3tn/task_tags.h"

#include "platform/hal_config.h"
#include "platform/hal_io.h"
#include "platform/hal_queue.h"
#include "platform/hal_semaphore.h"
#include "platform/hal_task.h"

#include "util/llsort.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static struct router_config RC;

static struct routed_bundle **popt, **preempted;

void router_optimizer_update_config_int(struct router_config conf)
{
	RC = conf;
	if (popt == NULL)
		popt = malloc(RC.opt_max_bundles * sizeof(void *));
	if (preempted == NULL)
		preempted = malloc(RC.opt_max_pre_bundles * sizeof(void *));
	ASSERT(popt != NULL);
	ASSERT(preempted != NULL);
}

struct router_optimizer_task_params {
	QueueIdentifier_t router_queue;
	Semaphore_t clist_semaphore;
	Semaphore_t opt_semaphore;
	struct contact_list **clist_ptr;
};

static void router_optimizer_task(void *param);

Semaphore_t router_start_optimizer_task(
	QueueIdentifier_t router_signaling_queue,
	Semaphore_t clist_semaphore, struct contact_list **clistptr)
{
	struct router_optimizer_task_params *p;

	ASSERT(router_signaling_queue != NULL);
	ASSERT(clist_semaphore != NULL);
	ASSERT(clistptr != NULL);
	p = malloc(sizeof(struct router_optimizer_task_params));
	if (p == NULL)
		return NULL;
	p->router_queue = router_signaling_queue;
	p->clist_semaphore = clist_semaphore;
	p->opt_semaphore = hal_semaphore_init_binary(); /* Locked already */
	if (p->opt_semaphore == NULL) {
		free(p);
		return NULL;
	}
	p->clist_ptr = clistptr;
	router_optimizer_update_config_int(router_get_config());
	if (hal_task_create(router_optimizer_task,
			    "rout_opt_t",
			    ROUTER_OPTIMIZER_TASK_PRIORITY,
			    p,
			    ROUTER_OPTIMIZER_TASK_STACK_SIZE,
			    (void *)ROUTER_OPTIMIZER_TASK_TAG)
			!= NULL) {
		return p->opt_semaphore;
	}
	free(p);
	return NULL;
}

static int router_optimization_affordable(struct contact_list **clistptr);
static uint8_t router_run_optimization(
	struct contact_list *global_clist, QueueIdentifier_t router_queue);

static void router_optimizer_task(void *param)
{
	struct router_optimizer_task_params *p
		= (struct router_optimizer_task_params *)param;
	uint8_t res, opt;

	for (;;) {
		hal_semaphore_take_blocking(p->opt_semaphore);
		hal_semaphore_release(p->opt_semaphore);
		if (router_optimization_affordable(p->clist_ptr)) {
			hal_semaphore_take_blocking(p->clist_semaphore);

			/* TODO: Do nothing if no new routed bundles => CM */
			res = router_run_optimization(
				*(p->clist_ptr), p->router_queue);
			opt = res & 0x7F;
			if ((res & 0x80) != 0)
				LOG("RouterOptimizer: Sorted list.");
			if (opt != 0)
				LOGF("RouterOptimizer: Optimized %d bundle(s).",
				     opt);
			hal_semaphore_release(p->clist_semaphore);
			if (res == 0)
				hal_semaphore_poll(p->opt_semaphore);
		}
		if (ROUTER_OPTIMIZER_DELAY != 0)
			hal_task_delay(ROUTER_OPTIMIZER_DELAY);
	}
}

static int router_optimization_affordable(struct contact_list **clistptr)
{
	int64_t cur_time = hal_time_get_timestamp_s();
	int64_t nxt_time = contact_manager_get_next_contact_time();

	(void)clistptr;
	return !contact_manager_in_contact() &&
		(nxt_time - cur_time) >= RC.opt_min_time;
}

static void try_optimize_decision_preempt(
	struct routed_bundle *rb, QueueIdentifier_t router_queue);

static uint8_t router_run_optimization(
	struct contact_list *global_clist, QueueIdentifier_t router_queue)
{
	struct contact_list *cur_elem;
	struct contact *c;
	enum bundle_routing_priority last_prio;
	struct routed_bundle_list *rbl;
	struct routed_bundle *r;
	uint8_t popt_c, sort, i, opt = 0;

	ASSERT(popt != NULL);
	cur_elem = global_clist;
	while (cur_elem != NULL) {
		c = cur_elem->data;
		ASSERT(c != NULL);
		rbl = c->contact_bundles;
		last_prio = BUNDLE_RPRIO_MAX;
		sort = 0;
		popt_c = 0;
		while (rbl != NULL) {
			r = rbl->data;
			ASSERT(r != NULL);
			if (r->prio > last_prio)
				sort = 1;
			/* Maybe sort by that value? */
			if (r->preemption_improvement != 0 && !r->serialized) {
				if (popt_c != RC.opt_max_bundles)
					popt[popt_c++] = r;
			}
			last_prio = r->prio;
			rbl = rbl->next;
		}
		if (popt_c != 0) {
			for (i = 0; i < popt_c; i++)
				try_optimize_decision_preempt(
					popt[i], router_queue);
			opt += popt_c;
		}
		if (sort) {
			opt |= 0x80;
			LLSORT_DESC(struct routed_bundle_list, data->prio,
				c->contact_bundles);
		}
		if (popt_c != 0)
			break;
		cur_elem = cur_elem->next;
	}
	return opt;
}

static inline struct routed_bundle_list *get_less_prio_bundle_list(
	struct routed_bundle_list *cur_list, enum bundle_routing_priority prio)
{
	struct routed_bundle_list *all_list = NULL;
	struct routed_bundle_list **all_list_e = &all_list;
	struct routed_bundle_list *next;

	while (cur_list != NULL) {
		if (cur_list->data->prio < prio) {
			*all_list_e = malloc(sizeof(struct routed_bundle_list));
			if (*all_list_e == NULL) {
				while (all_list != NULL) {
					next = all_list->next;
					free(all_list);
					all_list = next;
				}
				ASSERT(all_list != NULL);
				continue;
			}
			(*all_list_e)->data = cur_list->data;
			(*all_list_e)->next = NULL;
			all_list_e = &(*all_list_e)->next;
		}
		cur_list = cur_list->next;
	}
	return all_list;
}

static float try_calculate_new_decision(
	struct associated_contact_list *contacts,
	enum bundle_routing_priority prio, uint32_t size,
	struct contact **newc, uint8_t *newc_c,
	struct routed_bundle **preempted, uint8_t *preempted_count);
static uint8_t do_preemption(
	struct contact *contact,
	enum bundle_routing_priority prio, uint32_t size,
	struct routed_bundle **preempted, uint8_t *preempted_count,
	uint8_t *preempted_contact_count);
static uint8_t re_route_preempted_fragments(
	struct routed_bundle **preempted, uint8_t preempted_count,
	struct contact **ignored, uint8_t ignored_count,
	QueueIdentifier_t router_queue);

/*
 * As the bundle was removed from its original contact and the list is ordered,
 * we won't get worse results from this algorithm. Thus, we don't need to check
 * against expiration times.
 */
static void try_optimize_decision_preempt(
	struct routed_bundle *rb, QueueIdentifier_t router_queue)
{
	static struct contact *newc[ROUTER_MAX_CONTACTS];
	struct associated_contact_list *dest_contacts;
	uint8_t i, newc_c = 0;
	uint8_t preempted_count = 0;

	ASSERT(rb != NULL);
	if (preempted == NULL) /* If buffer failed to initialize */
		return;
	for (i = 0; i < rb->contact_count; i++)
		router_remove_bundle_from_contact(rb->contacts[i], rb->id);
	dest_contacts = router_lookup_destination(rb->destination);
	if (dest_contacts == NULL)
		goto finalize;
	/* Disable optimizing this bundle again */
	rb->preemption_improvement = 0;
	/* Calculate a new decision, check decision and try to assign bundles */
	if (
		try_calculate_new_decision(dest_contacts, rb->prio, rb->size,
			newc, &newc_c, preempted, &preempted_count)
			>= RC.min_probability &&
		re_route_preempted_fragments(preempted, preempted_count,
			newc, newc_c, router_queue)
	) {
		/* Apply */
		ASSERT(newc_c > 0);
		rb->contacts = realloc(rb->contacts, sizeof(void *) * newc_c);
		memcpy(rb->contacts, newc, sizeof(void *) * newc_c);
		rb->contact_count = newc_c;
	}
	list_free(dest_contacts);
finalize:
	for (i = 0; i < rb->contact_count; i++)
		router_add_bundle_to_contact(rb->contacts[i], rb);
}

static float try_calculate_new_decision(
	struct associated_contact_list *contacts,
	enum bundle_routing_priority prio, uint32_t size,
	struct contact **newc, uint8_t *newc_c,
	struct routed_bundle **preempted, uint8_t *preempted_count)
{
	uint8_t preempted_contact_count;
	struct contact *c;
	float probability = 0.0f, conf, assoc_node_prob;
	uint64_t time = hal_time_get_timestamp_s();

	*preempted_count = 0;
	*newc_c = 0;
	while (
		contacts != NULL && probability < RC.min_probability &&
		*newc_c != ROUTER_MAX_CONTACTS
	) {
		c = contacts->data;
		assoc_node_prob = contacts->p;
		ASSERT(c != NULL);
		contacts = contacts->next;
		preempted_contact_count = 0;
		if (ROUTER_CONTACT_CAPACITY(c, prio) < (int32_t)size
				|| c->to <= time) {
			continue;
		} else if (ROUTER_CONTACT_CAPACITY(c, 0) < (int32_t)size) {
			/* Preemption case */
			if (!do_preemption(c, prio, size, preempted,
					preempted_count,
					&preempted_contact_count))
				continue;
		}
		conf = ROUTER_CONTACT_CONFIDENCE(c, assoc_node_prob);
		if (conf >= RC.min_node_confidence_deterministic) {
			/* Deterministic routing */
			*newc_c = 0;
			newc[(*newc_c)++] = c;
			probability = conf;
			break;
		} else if (conf >= RC.min_node_confidence_opportunistic) {
			/* Opportunistic routing */
			newc[(*newc_c)++] = c;
			/* P(A u B) = P(A) + P(B) - P(A n B) */
			probability = probability + conf
				- (probability * conf);
		}
	}
	return probability;
}

static uint8_t do_preemption(
	struct contact *contact,
	enum bundle_routing_priority prio, uint32_t size,
	struct routed_bundle **preempted, uint8_t *preempted_count,
	uint8_t *preempted_contact_count)
{
	int32_t target_size;
	struct routed_bundle_list *cur_list, *all_list, *next;

	/* 1. Get list of all bundles with less prio */
	cur_list = contact->contact_bundles;
	all_list = get_less_prio_bundle_list(
		cur_list, prio);
	/* 2. Sort by size desc and the prio (mergesort is stable) */
	LLSORT_DESC(struct routed_bundle_list, data->size, all_list);
	LLSORT(struct routed_bundle_list, data->prio, all_list);
	/* 3. Get bundles to be replaced */
	target_size = size - ROUTER_CONTACT_CAPACITY(contact, 0);
	while (
		target_size > 0 &&
		*preempted_contact_count < RC.opt_max_pre_bundles_contact &&
		*preempted_count < RC.opt_max_pre_bundles &&
		all_list != NULL
	) {
		target_size -= all_list->data->size;
		preempted[(*preempted_count)++] = all_list->data;
		(*preempted_contact_count)++;
		next = all_list->next;
		free(all_list);
		all_list = next;
	}
	while (all_list != NULL) {
		next = all_list->next;
		free(all_list);
		all_list = next;
	}
	/* 4.a If failed, continue */
	if (target_size > 0) {
		*preempted_count -= *preempted_contact_count;
		return 0;
	}
	return 1;
}

static uint8_t re_route_preempted_fragments(
	struct routed_bundle **preempted, uint8_t preempted_count,
	struct contact **ignored, uint8_t ignored_count,
	QueueIdentifier_t router_queue)
{
	uint8_t i, j, success = 1;
	struct fragment_route *froutes;
	struct associated_contact_list *pre_contacts;

	ASSERT(preempted_count > 0);

	/* Try calc new routes for replaced bundles */
	froutes = malloc(preempted_count * sizeof(struct fragment_route));
	if (froutes == NULL)
		return 0;
	for (i = 0; i < preempted_count; i++) {
		froutes[i].payload_size = 0; /* Not needed here */
		pre_contacts = router_lookup_destination(
			preempted[i]->destination);
		if (pre_contacts == NULL) {
			success = 0;
			goto finalize;
		}
		if (!router_calculate_fragment_route(&froutes[i],
				preempted[i]->size, pre_contacts, 0,
				preempted[i]->prio, preempted[i]->exp_time,
				ignored, ignored_count))
			success = 0;
		list_free(pre_contacts)
		if (success == 0)
			goto finalize;
	}
	/* Apply pre. routes */
	for (i = 0; i < preempted_count; i++) {
		for (j = 0; j < preempted[i]->contact_count; j++)
			router_remove_bundle_from_contact(
				preempted[i]->contacts[j],
				preempted[i]->id);
		free(preempted[i]->contacts);
		preempted[i]->contacts = NULL;
		if (router_update_routed_bundle(&froutes[i], preempted[i])
			== 0
		) {
			/* Fragment could not be routed... */
			/* TODO: Maybe add to waiting list */
			struct router_signal rs = {
				.type = ROUTER_SIGNAL_OPTIMIZATION_DROP,
				.data = preempted[i]
			};
			hal_queue_push_to_back(router_queue, &rs);
		}
	}
finalize:
	free(froutes);
	return success;
}
