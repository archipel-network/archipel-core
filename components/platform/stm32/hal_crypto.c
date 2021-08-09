/*
 * hal_crypto.c
 *
 * Description: contains the stm32-implementation of the hardware
 * abstraction layer interface for crypto-related functionality
 * (i.e. hashing, CRC, SHA)
 *
 */

#include "platform/hal_crypto.h"

#include "util/sha2.h"
#include "util/hmac_sha2.h"

#include <stm32f4xx.h>


void hal_hash(uint8_t *message, uint32_t message_length,
	      uint8_t hash[UD3TN_HASH_LENGTH])
{
	sha256(message, message_length, hash);
}


void hal_hash_hmac(uint8_t *key, uint32_t key_length, uint8_t *message,
		   uint32_t message_length, uint8_t hash[UD3TN_HASH_LENGTH])
{
	hmac_sha256(key, key_length, message, message_length,
		hash, UD3TN_HASH_LENGTH);
}


void hal_hash_init(void)
{
	RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_HASH, ENABLE);
}


void hal_crc_init(void)
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_CRC, ENABLE);
}
