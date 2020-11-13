#include <stdint.h>
#include <stdlib.h>
#include <string.h>

char **eidlist_decode(const uint8_t *const ptr, const int length)
{
	int i, j, eid_length;
	uint8_t eid_index;
	char **result;

	if (length <= 1)
		return NULL;
	result = malloc(sizeof(char *) * ptr[0]);
	if (result == NULL)
		return NULL;
	eid_index = 0;
	i = 1;
	while (i < length) {
		eid_length = -1;
		for (j = i; j < length; j++) {
			if (ptr[j] == 0) {
				eid_length = j - i;
				break;
			}
		}
		if (eid_length == -1) {
			for (j = 0; j < eid_index; j++)
				free(result[j]);
			free(result);
			return NULL;
		}
		eid_length++;
		result[eid_index] = malloc(eid_length);
		if (result[eid_index] == NULL) {
			for (j = 0; j < eid_index; j++)
				free(result[j]);
			free(result);
			return NULL;
		}
		for (j = 0; j < eid_length; i++, j++)
			result[eid_index][j] = *(char *)(&ptr[i]);
		eid_index++;
	}
	if (eid_index != ptr[0]) {
		for (j = 0; j < eid_index; j++)
			free(result[j]);
		free(result);
		return NULL;
	}
	return result;
}

uint8_t *eidlist_encode(
	const char *const *const eids, const uint8_t count, int *const res_len)
{
	uint8_t i, j;
	int out_index;
	uint8_t *res;

	*res_len = 1;
	for (i = 0; i < count; i++)
		*res_len += strlen(eids[i]) + 1;
	res = malloc(*res_len);
	if (res == NULL) {
		*res_len = -1;
		return NULL;
	}
	res[0] = count;
	out_index = 1;
	for (i = 0; i < count; i++) {
		j = 0;
		do {
			res[out_index++] = *(uint8_t *)(&eids[i][j]);
		} while (eids[i][j++] != 0);
	}
	return res;
}
