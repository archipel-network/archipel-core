#ifndef BUNDLE7_CREATE_H_INCLUDED
#define BUNDLE7_CREATE_H_INCLUDED

#include "ud3tn/bundle.h"

#include <stddef.h>
#include <stdint.h>

/**
 * Creates a local BPv7-bis bundle using the specified parameters.
 *
 * @param payload The binary payload data which should be encapsulated.
 *                Memory management will be taken over by the bundle handling
 *                functions. In case of errors, the memory is freed.
 * @param payload_length The length of the payload data.
 * @param source The source EID the created bundle originated from.
 * @param destination The destination EID the bundle is addressed to.
 * @param creation_time_s The bundle creation timestamp (a DTN timestamp in seconds).
 * @param sequence_number The bundle sequence number.
 * @param lifetime The bundle lifetime, in seconds.
 * @param proc_flags Additional processing flags to be set for the bundle.
 */
struct bundle *bundle7_create_local(
	void *payload, size_t payload_length,
	const char *source, const char *destination,
	uint64_t creation_time_s, uint64_t sequence_number, uint64_t lifetime_s,
	enum bundle_proc_flags proc_flags);

#endif // BUNDLE7_CREATE_H_INCLUDED
