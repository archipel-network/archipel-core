// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
/*
 * hal_store.c
 *
 * Description: contains the POSIX implementation of the hardware
 * abstraction layer interface for bundle persistance
 *
 */

#include "ud3tn/result.h"
#include "platform/hal_store.h"
#include "platform/hal_io.h"
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

struct bundle_store* hal_store_init(const char* identifier) {
    if(mkdir(identifier, S_IRWXG|S_IRWXU) && errno != EEXIST){
        LOGF("Bundle Store : Failed to create folder %s (error %d)", identifier, errno);
        return NULL;
    }

    struct bundle_store* s = malloc(sizeof(struct bundle_store));
    if(s == NULL){
        return NULL;
    }
    s->identifier = strdup(identifier);
    return s;
}

static void eid_to_filename(char* eid){
	for(size_t i = 0; i<strlen(eid); i++){
		if(!((eid[i] >= 'a' && eid[i] <= 'z') ||
			 (eid[i] >= 'A' && eid[i] <= 'Z') ||
			 (eid[i] >= '0' && eid[i] <= '9') ||
			 eid[i] == '_' || eid[i] == '.' || eid[i] == '-'
		 )){
			eid[i] = '-';
		}
	}
}

void write_bundle_to_file(void* file, const void * b, const size_t size){

	FILE* f = (FILE*) file;
	const uint8_t* buffer = (const uint8_t*) b;

	if(fwrite(buffer, 1, size, f) != size) {
		LOG("FileCLA : failed to write file buffer");
	}
}

enum ud3tn_result hal_store_bundle(struct bundle_store* store, struct bundle *bundle) {
    size_t max_len = (25 + 1 + strlen(bundle->source) + strlen(bundle->destination) + 1);
    char* filename = malloc(sizeof(char) * max_len);
    snprintf(filename, max_len, "%ld_%s_%s", bundle->sequence_number, bundle->source, bundle->destination);
    eid_to_filename(filename);

    char* path = malloc(sizeof(char) * (strlen(store->identifier) + 1 + max_len));
    sprintf(path, "%s/%s", store->identifier, filename);
    free(filename);

    enum ud3tn_result return_result = UD3TN_FAIL;

    FILE* fd = fopen(path, "w");
    if(fd){
        return_result = bundle_serialize(bundle, write_bundle_to_file, fd);
        fclose(fd);
    } else {
        LOGF("Bundle Store : Failed to create file %s (error %d)", path, errno);
    }

    free(path);
    return return_result;
}