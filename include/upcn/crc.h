#ifndef CRC_H_INCLUDED
#define CRC_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum crc_version {
	CRC16_X25,
	CRC16_CCITT_FALSE,

	// CRC-32-C (Castagnoli)
	CRC32,
};

struct crc_stream {
	void (*feed)(struct crc_stream *crc, uint8_t byte);
	void (*feed_eof)(struct crc_stream *crc);
	union {
		uint32_t checksum;
		uint8_t bytes[4];
	};
};


/**
 * @brief X.25 CRC-16 checksum
 *
 * Polynom:         0x1021
 * Initial value:   0xffff
 * Final XOR value: 0xffff
 * Reflect input data:              TRUE
 * Reflect result before final XOR: TRUE
 *
 * @param data  Pointer to byte array to perform CRC on
 * @param len   Number of bytes (8-bit) to CRC
 *
 * @return CRC checksum
 */
uint16_t crc16_x25(const uint8_t *data, size_t len);

/**
 * @brief CRC-16 CCITT FALSE checksum
 *
 * Parameters:
 *   Polynomial 0x1021
 *   Reflect input data: FALSE
 *   Reflect result before final XOR: FALSE
 *   Initial Value: 0xffff
 *   Final XOR Value: 0x0000
 *
 * @param data  Pointer to byte array to perform CRC on
 * @param len   Number of bytes (8-bit) to CRC
 *
 * @return CRC checksum
 */
uint16_t crc16_ccitt_false(const uint8_t *data, size_t len);

/**
 * @brief CRC-32 (ANSI/Ethernet) checksum
 *
 * Polynom:         0x04c11db7
 * Initial value:   0xffffffff
 * Final XOR value: 0xffffffff
 * Reflect input data:              TRUE
 * Reflect result before final XOR: TRUE
 *
 * @param data  Pointer to byte array to perform CRC on
 * @param len   Number of bytes (8-bit) to CRC
 *
 * @return CRC checksum
 */
uint32_t crc32(const uint8_t *data, size_t len);

void crc_init(struct crc_stream *crc, enum crc_version version);

void crc_feed_bytes(struct crc_stream *crc, const uint8_t *data, size_t len);


#endif /* CRC_H_INCLUDED */
