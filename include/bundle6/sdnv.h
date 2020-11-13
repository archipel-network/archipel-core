#ifndef SDNV_H_INCLUDED
#define SDNV_H_INCLUDED

#include <stdint.h>

/**
 * Maximum amount of bytes used by a supported SDNV.
 * (We support up to 64 raw data bits.)
 */
#define MAX_SDNV_SIZE 10

enum sdnv_status {
	SDNV_IN_PROGRESS,
	SDNV_DONE,
	SDNV_ERROR
};

enum sdnv_error {
	SDNV_ERROR_NONE,
	SDNV_ERROR_OVERFLOW,
	SDNV_ERROR_ALREADY_DONE
};

struct sdnv_state {
	enum sdnv_status status;
	enum sdnv_error  error;
	uint8_t bytes_parsed;
};

void sdnv_reset(struct sdnv_state *state);

void sdnv_read_u8(struct sdnv_state *state, uint8_t *value, uint8_t byte);
void sdnv_read_u16(struct sdnv_state *state, uint16_t *value, uint8_t byte);
void sdnv_read_u32(struct sdnv_state *state, uint32_t *value, uint8_t byte);
void sdnv_read_u64(struct sdnv_state *state, uint64_t *value, uint8_t byte);

int_fast8_t sdnv_get_size_u8(uint8_t value);
int_fast8_t sdnv_get_size_u16(uint16_t value);
int_fast8_t sdnv_get_size_u32(uint32_t value);
int_fast8_t sdnv_get_size_u64(uint64_t value);

int_fast8_t sdnv_write_u8(uint8_t *buffer, uint8_t value);
int_fast8_t sdnv_write_u16(uint8_t *buffer, uint16_t value);
int_fast8_t sdnv_write_u32(uint8_t *buffer, uint32_t value);
int_fast8_t sdnv_write_u64(uint8_t *buffer, uint64_t value);

#endif /* SDNV_H_INCLUDED */
