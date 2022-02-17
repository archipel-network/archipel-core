// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#ifndef BUNDLE7_TIMESTAMP_H_INCLUDED
#define BUNDLE7_TIMESTAMP_H_INCLUDED

#include "cbor.h"

#include <stdint.h>

CborError bundle7_timestamp_parse(CborValue *it,
	uint64_t *creation_timestamp_ms,
	uint64_t *sequence_number);

#endif // BUNDLE7_TIMESTAMP_H_INCLUDED
