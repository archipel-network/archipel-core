#ifndef ROUTER_H_INCLUDED
#define ROUTER_H_INCLUDED

#include "ud3tn/bundle.h"
#include "ud3tn/config.h"
#include "ud3tn/node.h"
#include "ud3tn/routing_table.h"

#include <stddef.h>
#include <stdint.h>

struct router_config {
	size_t global_mbs;
	uint16_t fragment_min_payload;
	float min_probability;
	float min_node_confidence_opportunistic;
	float min_node_confidence_deterministic;
	float node_trustworthiness_weight;
	float node_reliability_weight;
	uint32_t opt_min_time;
	uint8_t opt_max_bundles;
	uint8_t opt_max_pre_bundles;
	uint8_t opt_max_pre_bundles_contact;
	uint8_t router_min_contacts_htab;
	uint8_t router_min_contacts_nbf;
	float router_nbf_base_reliability;
	float router_def_base_reliability;
};

struct fragment_route {
	uint32_t payload_size;
	float probability;
	struct contact *contacts[ROUTER_MAX_CONTACTS];
	uint8_t contact_count;
	uint8_t preemption_improved;
};

/* With MAX_FRAGMENTS = 3 and MAX_CONTACTS = 5: 92 bytes on stack */
struct router_result {
	float probability;
	struct fragment_route fragment_results[ROUTER_MAX_FRAGMENTS];
	int32_t fragments;
	uint8_t preemption_improved;
};

#define TRUSTWORTHINESS(node) (node->trustworthiness)
#define RELIABILITY(node) (node->reliability)
#define ROUTER_NODE_CONFIDENCE(node) \
	(RC.node_trustworthiness_weight * TRUSTWORTHINESS(node) \
	+ RC.node_reliability_weight * RELIABILITY(node))
#define ROUTER_CONTACT_CONFIDENCE(contact, association_prob) \
	(ROUTER_NODE_CONFIDENCE(contact->node) * association_prob)

#define ROUTER_BUNDLE_PRIORITY(bundle) (bundle_get_routing_priority(bundle))
#define ROUTER_CONTACT_CAPACITY(contact, prio) \
	(contact_get_cur_remaining_capacity(contact, prio))

struct router_config router_get_config(void);
enum ud3tn_result router_update_config(struct router_config config);

struct associated_contact_list *router_lookup_destination(char *dest);
uint8_t router_calculate_fragment_route(
	struct fragment_route *res, uint32_t size,
	struct associated_contact_list *contacts, uint32_t preprocessed_size,
	enum bundle_routing_priority priority, uint64_t exp_time,
	struct contact **excluded_contacts, uint8_t excluded_contacts_count);

struct router_result router_get_first_route(struct bundle *bundle);
struct router_result router_try_reuse(
	struct router_result route, struct bundle *bundle);

enum ud3tn_result router_add_bundle_to_contact(
	struct contact *contact, struct routed_bundle *rb);
struct routed_bundle *router_remove_bundle_from_contact(
	struct contact *contact, bundleid_t id);
uint8_t router_add_bundle_to_route(struct fragment_route *r, struct bundle *b);
uint8_t router_update_routed_bundle(
	struct fragment_route *r, struct routed_bundle *rb);
void router_remove_bundle_from_route(
	struct fragment_route *r, bundleid_t id, uint8_t free_rb);

#endif /* ROUTER_H_INCLUDED */
