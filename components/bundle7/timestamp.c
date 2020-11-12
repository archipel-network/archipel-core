#include "bundle7/timestamp.h"

CborError bundle7_timestamp_parse(CborValue *it,
	uint64_t *creation_timestamp_ms,
	uint64_t *sequence_number)
{
	CborValue recursed;
	CborError err;
	uint64_t value;

	if (!cbor_value_is_array(it)) {
		printf("Errno: no array, type = %d\n", it->type);
		return CborErrorIllegalType;
	}

	err = cbor_value_enter_container(it, &recursed);
	if (err)
		return err;

	if (!cbor_value_is_unsigned_integer(&recursed))
		return CborErrorIllegalType;

	// Extract creation timestamp
	cbor_value_get_uint64(&recursed, &value);
	*creation_timestamp_ms = value;

	err = cbor_value_advance_fixed(&recursed);
	if (err)
		return err;

	if (!cbor_value_is_unsigned_integer(&recursed))
		return CborErrorIllegalType;

	// Extract sequence number
	cbor_value_get_uint64(&recursed, &value);
	*sequence_number = value;

	err = cbor_value_advance_fixed(&recursed);
	if (err)
		return err;

	if (!cbor_value_at_end(&recursed))
		return CborErrorIllegalType;

	return cbor_value_leave_container(it, &recursed);
}
