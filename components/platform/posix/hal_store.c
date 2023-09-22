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
#include "ud3tn/eid.h"
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
    // Create index folder
    char* node_id = get_node_id(bundle->destination);
    eid_to_filename(node_id);
    char* dirpath = malloc(sizeof(char) * (strlen(store->identifier) + 1 + strlen(node_id)));
    sprintf(dirpath, "%s/%s", store->identifier, node_id);
    free(node_id);
    if(mkdir(dirpath, S_IRWXG|S_IRWXU) && errno != EEXIST){
        LOGF("Bundle Store : Failed to create folder %s (error %d)", dirpath, errno);
        free(dirpath);
        return UD3TN_FAIL;
    }

    // prepare filename
    struct bundle_unique_identifier bundle_id = bundle_get_unique_identifier(bundle);
    size_t max_len = (
        4 // protocol version
        + 1 // _
        + strlen(bundle->source)
        + 1 // _
        + 24 // creation timestamp
        + 1 // _
        + 24 // sequence number
        + 1 // _
        + 10 // fragment offset
        + 1 // _
        + 10 // Payload length
    );
    char* filename = malloc(sizeof(char) * max_len);
    snprintf(filename, max_len, "%d_%s_%ld_%ld_%d_%d",
        bundle_id.protocol_version,
        bundle_id.source,
        bundle_id.creation_timestamp_ms,
        bundle_id.sequence_number,
        bundle_id.fragment_offset,
        bundle_id.payload_length
    );
    bundle_free_unique_identifier(&bundle_id);
    eid_to_filename(filename);

    // create path
    char* path = malloc(sizeof(char) * (strlen(dirpath) + 1 + max_len));
    sprintf(path, "%s/%s", dirpath, filename);
    free(filename);
    free(dirpath);

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