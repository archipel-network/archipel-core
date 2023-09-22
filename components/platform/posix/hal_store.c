// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
/*
 * hal_store.c
 *
 * Description: contains the POSIX implementation of the hardware
 * abstraction layer interface for bundle persistance
 *
 */

#include "bundle7/parser.h"
#include "bundle6/parser.h"
#include "ud3tn/result.h"
#include "ud3tn/config.h"
#include "platform/hal_store.h"
#include "platform/hal_io.h"
#include "platform/hal_semaphore.h"
#include <sys/stat.h>
#include "ud3tn/eid.h"
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <dirent.h>
#include <inttypes.h>

struct posix_bundle_store {
    struct bundle_store base;

    Semaphore_t current_sequence_number_sem;
    uint64_t current_sequence_number;
};

struct posix_bundle_store_popseq {
    struct bundle_store_popseq base;
    uint64_t max_sequence_number;
    char* folder_path;
    DIR* dir;
};

struct bundle_store* hal_store_init(const char* identifier) {
    if(mkdir(identifier, S_IRWXG|S_IRWXU) && errno != EEXIST){
        LOGF("Bundle Store : Failed to create folder %s (error %d)", identifier, errno);
        return NULL;
    }

    struct posix_bundle_store* s = malloc(sizeof(struct posix_bundle_store));
    if(s == NULL){
        return NULL;
    }
    s->base.identifier = strdup(identifier);

    s->current_sequence_number = 0; // BUG Should restore sequence number after restart
    s->current_sequence_number_sem = hal_semaphore_init_binary();
    hal_semaphore_release(s->current_sequence_number_sem);

    return ((struct bundle_store*) s);
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

enum ud3tn_result hal_store_bundle(struct bundle_store* base_store, struct bundle *bundle) {
    struct posix_bundle_store* store = 
        (struct posix_bundle_store*) base_store;

    hal_semaphore_take_blocking(store->current_sequence_number_sem);
    uint64_t current_seqnum = store->current_sequence_number;
    hal_semaphore_release(store->current_sequence_number_sem);

    // Create index folder
    char* node_id = get_node_id(bundle->destination);
    eid_to_filename(node_id);
    char* dirpath = malloc(sizeof(char) * (strlen(store->base.identifier) + 1 + strlen(node_id)));
    sprintf(dirpath, "%s/%s", store->base.identifier, node_id);
    free(node_id);
    if(mkdir(dirpath, S_IRWXG|S_IRWXU) && errno != EEXIST){
        LOGF("Bundle Store : Failed to create folder %s (error %d)", dirpath, errno);
        free(dirpath);
        return UD3TN_FAIL;
    }

    // prepare filename
    struct bundle_unique_identifier bundle_id = bundle_get_unique_identifier(bundle);
    size_t max_len = (
        24 // store sequence number
        + 1 // -
        + 4 // protocol version
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
    char* filename = malloc(sizeof(char) * (max_len + 1));
    snprintf(filename, max_len, "%ld-%d_%s_%ld_%ld_%d_%d",
        current_seqnum,
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

struct bundle_store_popseq* hal_store_popseq(struct bundle_store* base_store, const char* destination){

    struct posix_bundle_store* store = 
        (struct posix_bundle_store*) base_store;

    hal_semaphore_take_blocking(store->current_sequence_number_sem);
    const uint64_t max_seqnum = store->current_sequence_number;
    hal_semaphore_release(store->current_sequence_number_sem);

    struct posix_bundle_store_popseq* popseq = malloc(sizeof(struct posix_bundle_store_popseq));
    popseq->base.store = base_store;
    popseq->base.destination = get_node_id(destination);
    popseq->max_sequence_number = max_seqnum;

    char* filename = strdup(popseq->base.destination);
    eid_to_filename(filename);

    popseq->folder_path = malloc(sizeof(char) * (
        strlen(store->base.identifier)
        + 1
        + strlen(filename)
        + 1
    ));
    sprintf(popseq->folder_path, "%s/%s", store->base.identifier, filename);
    free(filename);

    popseq->dir = opendir(popseq->folder_path);

    return (struct bundle_store_popseq*) popseq;
}

void hal_store_popseq_free(struct bundle_store_popseq* base_popseq){
    struct posix_bundle_store_popseq* popseq = 
        (struct posix_bundle_store_popseq*) base_popseq;

    closedir(popseq->dir);
    free(popseq->base.destination);
    free(popseq->folder_path);
    free(popseq);
}


static void _hal_store_get_bundle(struct bundle *bundle, void* p){
    struct bundle** bundle_box = (struct bundle**) p;
    (*bundle_box) = bundle;
}

struct bundle* hal_store_popseq_next(struct bundle_store_popseq* base_popseq){
    struct posix_bundle_store_popseq* popseq = 
        (struct posix_bundle_store_popseq*) base_popseq;

    if(popseq->dir == NULL){
        return NULL;
    }

    struct bundle* next_bundle = NULL;

	struct bundle7_parser b7_parser;
	bundle7_parser_init(&b7_parser, &_hal_store_get_bundle, &next_bundle);
	b7_parser.bundle_quota = BUNDLE_MAX_SIZE;

	struct bundle6_parser b6_parser;
	bundle6_parser_init(&b6_parser, &_hal_store_get_bundle, &next_bundle);

    char seqnum_buf[256];

    struct dirent* entry;
    uintmax_t seqnum;
    char protocol_version;
    size_t len = 0;
    uint8_t buffer[HAL_STORE_READ_BUFFER_SIZE];

    while ((entry = readdir(popseq->dir)) != NULL)
    {
        if(entry->d_type != 8 /* DT_REG */){
            continue; // Not a file
        }

        for(size_t i = 0; i<256; i++){
            if(entry->d_name[i] == '-'){
                if(i+1<256){
                    if(entry->d_name[i+1] == '7' || entry->d_name[i+1] == '6'){
                        protocol_version = entry->d_name[i+1];
                    } else {
                        continue; // No protocol version in filename
                    }
                }
                break;
            }
            if(entry->d_name[i] == '\0'){
                break;
            }
            seqnum_buf[i] = entry->d_name[i];
        }
        seqnum = strtoumax(&seqnum_buf[0], NULL, 10);

        if(seqnum <= popseq->max_sequence_number){

            char* filename = malloc(sizeof(char) * (
                strlen(popseq->folder_path)
                + 1
                + strlen(entry->d_name)
                + 1
            ));
            sprintf(filename, "%s/%s", popseq->folder_path, entry->d_name);
            FILE* file = fopen( filename,"r");

            if(file == NULL){
                free(filename);
                continue; // Error reading file
            }

            while((len = fread(&buffer, sizeof(char), FILECLA_READ_BUFFER_SIZE, file)) > 0){
                
				struct parser *basedata;
                
                if(protocol_version == '7'){
                    bundle7_parser_read(&b7_parser, buffer, len);
                    basedata = b7_parser.basedata;
                } else if(protocol_version == '6'){
                    bundle6_parser_read(&b6_parser, buffer, len);
                    basedata = b6_parser.basedata;
                }

                if(basedata->status == PARSER_STATUS_ERROR){
                    break; // Parsing error
                }
            }

            fclose(file);

            if(next_bundle != NULL){
                if(remove(filename)){
                    LOGF("Bundle Store : Error removeing file %s", filename);
                }
                break;
            }

            free(filename);
        }
    }

    return next_bundle;
}