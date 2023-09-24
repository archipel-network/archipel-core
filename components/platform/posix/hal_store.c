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

#define SEQUENCE_NUMBER_KEY "sequence_number"

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

    char* values_path = malloc(sizeof(char) * (strlen(identifier) + 7 + 1));
    sprintf(values_path, "%s/values", identifier);
    if(mkdir(values_path, S_IRWXG|S_IRWXU) && errno != EEXIST){
        LOGF("Bundle Store : Failed to create folder %s (error %d)", values_path, errno);
        return NULL;
    }
    free(values_path);

    char* data_path = malloc(sizeof(char) * (strlen(identifier) + 5 + 1));
    sprintf(data_path, "%s/data", identifier);
    if(mkdir(data_path, S_IRWXG|S_IRWXU) && errno != EEXIST){
        LOGF("Bundle Store : Failed to create folder %s (error %d)", data_path, errno);
        return NULL;
    }
    free(data_path);

    struct posix_bundle_store* s = malloc(sizeof(struct posix_bundle_store));
    if(s == NULL){
        return NULL;
    }
    s->base.identifier = strdup(identifier);

    s->current_sequence_number = 0;
    s->current_sequence_number_sem = hal_semaphore_init_binary();
    s->current_sequence_number = hal_store_get_uint64_value((struct bundle_store*) s, SEQUENCE_NUMBER_KEY, 0);
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

    char* dirpath = malloc(sizeof(char) * (strlen(store->base.identifier) + 5 + 1));
    sprintf(dirpath, "%s/data", store->base.identifier);

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

struct bundle_store_popseq* hal_store_popseq(struct bundle_store* base_store){

    struct posix_bundle_store* store = 
        (struct posix_bundle_store*) base_store;

    hal_semaphore_take_blocking(store->current_sequence_number_sem);
    const uint64_t max_seqnum = store->current_sequence_number;
    store->current_sequence_number += 1;
    hal_store_set_uint64_value(
        base_store,
        SEQUENCE_NUMBER_KEY,
        store->current_sequence_number);
    hal_semaphore_release(store->current_sequence_number_sem);

    struct posix_bundle_store_popseq* popseq = malloc(sizeof(struct posix_bundle_store_popseq));
    popseq->base.store = base_store;
    popseq->max_sequence_number = max_seqnum;

    popseq->folder_path = malloc(sizeof(char) * (
        strlen(store->base.identifier)
        + 5 // /data
        + 1 // \0
    ));
    sprintf(popseq->folder_path, "%s/data", store->base.identifier);

    popseq->dir = opendir(popseq->folder_path);

    return (struct bundle_store_popseq*) popseq;
}

void hal_store_popseq_free(struct bundle_store_popseq* base_popseq){
    struct posix_bundle_store_popseq* popseq = 
        (struct posix_bundle_store_popseq*) base_popseq;

    closedir(popseq->dir);
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

char* _hal_store_get_value_path(struct bundle_store* store, const char* key){
    char* filepath = malloc(sizeof(char) * (
        strlen(store->identifier)
        + 1 // /
        + 6 // values
        + 1 // /
        + strlen(key) // key
        + 1 // \0
    ));
    sprintf(filepath, "%s/values/%s", store->identifier, key);
    return filepath;
}

enum ud3tn_result hal_store_set_uint64_value(
    struct bundle_store* store,
    const char* key,
    const uint64_t value){

    char* filepath = _hal_store_get_value_path(store, key);

    enum ud3tn_result result = UD3TN_OK; 

    FILE* file = fopen(filepath, "w");
    if(file == NULL){
        LOGF("Bundle Store : Failed to write value %s in file %s", key, filepath);
        result = UD3TN_FAIL;
    } else {
        size_t n = fwrite(&value, sizeof(uint64_t), 1, file);
        if(n < 1){
            LOGF("Bundle Store : Failed to write value %s in file %s", key, filepath);
            result = UD3TN_FAIL;
        }
        fclose(file);
    }

    free(filepath);

    return result;
}

uint64_t hal_store_get_uint64_value(
    struct bundle_store* store,
    const char* key,
    uint64_t default_value){
    
    char* filepath = _hal_store_get_value_path(store, key);
    uint64_t value = default_value;

    FILE* file = fopen(filepath, "r");
    if(file != NULL){
        size_t n = fread(&value, sizeof(uint64_t), 1, file);
        if(n < 1){
            LOGF("Bundle Store : Failed to read value %s in file %s", key, filepath);
            value = default_value;
        }
        fclose(file);
    }

    free(filepath);
    return value;
}