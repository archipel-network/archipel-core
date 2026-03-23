// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
/*
 * hal_store.c
 *
 * Description: contains the POSIX implementation of the hardware
 * abstraction layer interface for bundle persistance
 *
 */

#include "bundle6/parser.h"
#include "bundle7/parser.h"
#include "ud3tn/bundle.h"
#include "ud3tn/result.h"
#include "platform/hal_store.h"
#include "platform/hal_io.h"
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <dirent.h>
#include <inttypes.h>

#ifdef ARCHIPEL_CORE

#define SEQUENCE_NUMBER_KEY "sequence_number"

struct posix_bundle_store {
    struct bundle_store base;
    char* datadir;
};

struct posix_bundle_store_loadall_item {
    char* filepath;
    char protocol_version;
    struct posix_bundle_store_loadall_item* next;
};

struct posix_bundle_store_loadall {
    struct bundle_store_loadall base;
    struct posix_bundle_store_loadall_item* next_item;
};

struct bundle_store* hal_store_init(const char* identifier) {
    if(mkdir(identifier, S_IRWXG|S_IRWXU) && errno != EEXIST){
        LOGF_ERROR("Bundle Store : Failed to create folder %s (error %d)", identifier, errno);
        return NULL;
    }

    char* values_path = malloc(sizeof(char) * (strlen(identifier) + 7 + 1));
    sprintf(values_path, "%s/values", identifier);
    if(mkdir(values_path, S_IRWXG|S_IRWXU) && errno != EEXIST){
        LOGF_ERROR("Bundle Store : Failed to create folder %s (error %d)", values_path, errno);
        return NULL;
    }
    free(values_path);

    char* data_path = malloc(sizeof(char) * (strlen(identifier) + 5 + 1));
    sprintf(data_path, "%s/data", identifier);
    if(mkdir(data_path, S_IRWXG|S_IRWXU) && errno != EEXIST){
        LOGF_ERROR("Bundle Store : Failed to create folder %s (error %d)", data_path, errno);
        return NULL;
    }
    
    struct posix_bundle_store* s = malloc(sizeof(struct posix_bundle_store));
    if(s == NULL){
        return NULL;
    }
    s->base.identifier = strdup(identifier);
    s->datadir = data_path;


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
		LOG_ERROR("FileCLA : failed to write file buffer");
	}
}

const char* BUNDLE_METADATA_BUNDLE_RET_CONSTRAINT_CUSTODY_ACCEPTED = "RET_CONSTRAINT_CUSTODY_ACCEPTED";
const char* BUNDLE_METADATA_BUNDLE_RET_CONSTRAINT_REASSEMBLY_PENDING = "RET_CONSTRAINT_REASSEMBLY_PENDING";
const char* BUNDLE_METADATA_BUNDLE_RET_CONSTRAINT_FORWARD_PENDING = "RET_CONSTRAINT_FORWARD_PENDING";
const char* BUNDLE_METADATA_BUNDLE_RET_CONSTRAINT_DISPATCH_PENDING = "RET_CONSTRAINT_DISPATCH_PENDING";
const char* BUNDLE_METADATA_BUNDLE_RET_CONSTRAINT_FLAG_OWN = "RET_CONSTRAINT_FLAG_OWN";

enum ud3tn_result _hal_store_write_metadata(struct bundle* bundle, FILE* file) {
    if((bundle->ret_constraints & BUNDLE_RET_CONSTRAINT_CUSTODY_ACCEPTED) == BUNDLE_RET_CONSTRAINT_CUSTODY_ACCEPTED){
        fwrite(
            BUNDLE_METADATA_BUNDLE_RET_CONSTRAINT_CUSTODY_ACCEPTED,
            sizeof(char),
            strlen(BUNDLE_METADATA_BUNDLE_RET_CONSTRAINT_CUSTODY_ACCEPTED),
            file
        );
        fwrite("\n", sizeof(char), 1, file);
    }

    if((bundle->ret_constraints & BUNDLE_RET_CONSTRAINT_REASSEMBLY_PENDING) == BUNDLE_RET_CONSTRAINT_REASSEMBLY_PENDING){
        fwrite(
            BUNDLE_METADATA_BUNDLE_RET_CONSTRAINT_REASSEMBLY_PENDING,
            sizeof(char),
            strlen(BUNDLE_METADATA_BUNDLE_RET_CONSTRAINT_REASSEMBLY_PENDING),
            file
        );
        fwrite("\n", sizeof(char), 1, file);
    }

    if((bundle->ret_constraints & BUNDLE_RET_CONSTRAINT_FORWARD_PENDING) == BUNDLE_RET_CONSTRAINT_FORWARD_PENDING){
        fwrite(
            BUNDLE_METADATA_BUNDLE_RET_CONSTRAINT_FORWARD_PENDING,
            sizeof(char),
            strlen(BUNDLE_METADATA_BUNDLE_RET_CONSTRAINT_FORWARD_PENDING),
            file
        );
        fwrite("\n", sizeof(char), 1, file);
    }

    if((bundle->ret_constraints & BUNDLE_RET_CONSTRAINT_DISPATCH_PENDING) == BUNDLE_RET_CONSTRAINT_DISPATCH_PENDING){
        fwrite(
            BUNDLE_METADATA_BUNDLE_RET_CONSTRAINT_DISPATCH_PENDING,
            sizeof(char),
            strlen(BUNDLE_METADATA_BUNDLE_RET_CONSTRAINT_DISPATCH_PENDING),
            file
        );
        fwrite("\n", sizeof(char), 1, file);
    }

    if((bundle->ret_constraints & BUNDLE_RET_CONSTRAINT_FLAG_OWN) == BUNDLE_RET_CONSTRAINT_FLAG_OWN){
        fwrite(
            BUNDLE_METADATA_BUNDLE_RET_CONSTRAINT_FLAG_OWN,
            sizeof(char),
            strlen(BUNDLE_METADATA_BUNDLE_RET_CONSTRAINT_FLAG_OWN),
            file
        );
        fwrite("\n", sizeof(char), 1, file);
    }

    return UD3TN_OK;
}

const size_t STORE_READ_METADATA_BUFFER = 255;

enum ud3tn_result _hal_store_read_metadata(struct bundle* bundle, FILE* file) {
    char buffer[STORE_READ_METADATA_BUFFER];
    size_t buffer_offset = 0;
    size_t buffer_length = 0;

    do {
        size_t bytes_red = fread(
            &buffer[buffer_offset], 
            sizeof(char), STORE_READ_METADATA_BUFFER - buffer_offset,
            file);

        buffer_length += bytes_red;
        
        size_t line_length = 0;
        while(line_length < buffer_length){
            if(buffer[line_length] == '\n'){
                break;
            }
            line_length++;
        }

        if(line_length == 0 && bytes_red == 0){
            break;
        }

        char* line_content = malloc(sizeof(char) * (line_length + 1));
        memcpy(line_content, &buffer, line_length);
        line_content[line_length] = '\0';
        
        if(strcmp(line_content, BUNDLE_METADATA_BUNDLE_RET_CONSTRAINT_FORWARD_PENDING) == 0){
            bundle->ret_constraints |= BUNDLE_RET_CONSTRAINT_FORWARD_PENDING;

        } else if(strcmp(line_content, BUNDLE_METADATA_BUNDLE_RET_CONSTRAINT_DISPATCH_PENDING) == 0) {
            bundle->ret_constraints |= BUNDLE_RET_CONSTRAINT_DISPATCH_PENDING;

        } else if(strcmp(line_content, BUNDLE_METADATA_BUNDLE_RET_CONSTRAINT_REASSEMBLY_PENDING) == 0) {
            bundle->ret_constraints |= BUNDLE_RET_CONSTRAINT_REASSEMBLY_PENDING;

        } else if(strcmp(line_content, BUNDLE_METADATA_BUNDLE_RET_CONSTRAINT_CUSTODY_ACCEPTED) == 0) {
            bundle->ret_constraints |= BUNDLE_RET_CONSTRAINT_CUSTODY_ACCEPTED;

        } else if(strcmp(line_content, BUNDLE_METADATA_BUNDLE_RET_CONSTRAINT_FLAG_OWN) == 0) {
            bundle->ret_constraints |= BUNDLE_RET_CONSTRAINT_FLAG_OWN;
            
        } else {
            LOGF_WARN("HALStore: Discarded unknown metadata %s", line_content);
        }
        
        free(line_content);

        memcpy(buffer, &buffer[line_length], buffer_length - line_length);
        buffer_offset = buffer_length - line_length;

    } while(buffer_length > 0);

    return UD3TN_OK;
}

char* _hal_store_bundle_path(struct posix_bundle_store* store, struct bundle *bundle) {
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

    char* path = malloc(sizeof(char) * (strlen(store->datadir) + 1 + max_len));
    sprintf(path, "%s/%s", store->datadir, filename);

    free(filename);
    
    return path;
}

enum ud3tn_result hal_store_bundle_metadata(struct bundle_store* base_store, struct bundle *bundle) {
    struct posix_bundle_store* store = (struct posix_bundle_store*) base_store;

    // create path
    char* path = _hal_store_bundle_path(store, bundle);
    char* metadata_path = malloc(sizeof(char) * strlen(path) + 1 + 5);
    sprintf(metadata_path, "%s.meta", path);

    enum ud3tn_result return_result = UD3TN_FAIL;

    FILE* metadata_fd = fopen(metadata_path, "w");
    if(metadata_fd){
        return_result = _hal_store_write_metadata(bundle, metadata_fd);
        fclose(metadata_fd);
    } else {
        LOGF_ERROR("Bundle Store : Failed to create file %s (error %d)", metadata_path, errno);
    }

    free(path);
    free(metadata_path);
    return return_result;
}

enum ud3tn_result hal_store_bundle(struct bundle_store* base_store, struct bundle *bundle) {
    struct posix_bundle_store* store = 
        (struct posix_bundle_store*) base_store;

    // create path
    char* path = _hal_store_bundle_path(store, bundle);
    char* metadata_path = malloc(sizeof(char) * strlen(path) + 1 + 5);
    sprintf(metadata_path, "%s.meta", path);

    enum ud3tn_result return_result = UD3TN_FAIL;

    FILE* fd = fopen(path, "w");
    if(fd){
        return_result = bundle_serialize(bundle, write_bundle_to_file, fd);
        fclose(fd);
    } else {
        LOGF_ERROR("Bundle Store : Failed to create file %s (error %d)", path, errno);
    }

    FILE* metadata_fd = fopen(metadata_path, "w");
    if(metadata_fd){
        _hal_store_write_metadata(bundle, metadata_fd);
        fclose(metadata_fd);
    } else {
        LOGF_ERROR("Bundle Store : Failed to create file %s (error %d)", metadata_path, errno);
    }

    free(path);
    free(metadata_path);
    return return_result;
}

enum ud3tn_result hal_store_bundle_delete(struct bundle_store* base_store, struct bundle *bundle) {
    struct posix_bundle_store* store = 
        (struct posix_bundle_store*) base_store;

    // create path
    char* path = _hal_store_bundle_path(store, bundle);
    char* metadata_path = malloc(sizeof(char) * strlen(path) + 1 + 5);
    sprintf(metadata_path, "%s.meta", path);

    enum ud3tn_result return_result = UD3TN_FAIL;

    if(remove(path) == 0 || errno == ENOENT){
        return_result = UD3TN_OK;
    } else {
        LOG_ERRNO("HALStore", "Failed to remove bundle", errno);
    };
    remove(metadata_path);

    free(path);
    free(metadata_path);
    return return_result;
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
        LOGF_ERROR("Bundle Store : Failed to write value %s in file %s", key, filepath);
        result = UD3TN_FAIL;
    } else {
        size_t n = fwrite(&value, sizeof(uint64_t), 1, file);
        if(n < 1){
            LOGF_ERROR("Bundle Store : Failed to write value %s in file %s", key, filepath);
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
            LOGF_ERROR("Bundle Store : Failed to read value %s in file %s", key, filepath);
            value = default_value;
        }
        fclose(file);
    }

    free(filepath);
    return value;
}

struct bundle_store_loadall* hal_store_loadall(struct bundle_store* base_store){
    struct posix_bundle_store* store = (struct posix_bundle_store*) base_store;

    DIR* dir = opendir(store->datadir);
    if(dir == NULL){
        return NULL;
    }

    struct posix_bundle_store_loadall* loader = malloc(sizeof(struct posix_bundle_store_loadall));
    loader->base.store = (struct bundle_store*) store;
    loader->next_item = NULL;

    struct posix_bundle_store_loadall_item** item_container = &loader->next_item;
    struct dirent* dirent;
    while((dirent = readdir(dir)) != NULL){
        if(dirent->d_type != DT_REG){
            continue;
        }

        char* ext = dirent->d_name + sizeof(char) * (strlen(dirent->d_name) - 5);
        if(strcmp(ext, ".meta") == 0){
            continue;
        }

        char protocol_version = dirent->d_name[0];
        if(protocol_version != '7' && protocol_version != '6'){
            continue;
        }

        struct posix_bundle_store_loadall_item* item = malloc(sizeof(struct posix_bundle_store_loadall_item));
        item->filepath = malloc(sizeof(char) * (strlen(store->datadir) + strlen(dirent->d_name) + 1 /* '/' */ + 1 /* \0 */ ));
        sprintf(item->filepath, "%s/%s", store->datadir, dirent->d_name);
        item->protocol_version = protocol_version;
        item->next = NULL;
        *item_container = item;
        item_container = &item->next;
    }

    return (struct bundle_store_loadall*) loader;
}

void _hal_store_loadall_item_free(struct posix_bundle_store_loadall_item* item){
    free(item->filepath);
    if(item->next != NULL)
        return _hal_store_loadall_item_free(item->next);
}

void hal_store_loadall_free(struct bundle_store_loadall* loader_base) {
    struct posix_bundle_store_loadall* loader = (struct posix_bundle_store_loadall*) loader_base;

    if(loader->next_item != NULL)
        return _hal_store_loadall_item_free(loader->next_item);
}

static void _hal_store_get_bundle(
    struct bundle *bundle,
    void * out
){
    *((struct bundle**) out) = bundle;
}

struct bundle* hal_store_loadall_next(struct bundle_store_loadall* loader_base) {
    struct posix_bundle_store_loadall* loader = (struct posix_bundle_store_loadall*) loader_base;
    
    if(loader->next_item == NULL){
        return NULL;
    }

    struct posix_bundle_store_loadall_item* item = loader->next_item;
    loader->next_item = item->next;
    item->next = NULL;

    // Bundle parsing

    struct bundle* next_bundle = NULL;

	struct bundle7_parser b7_parser;
	bundle7_parser_init(&b7_parser, &_hal_store_get_bundle, &next_bundle);
	b7_parser.bundle_quota = BUNDLE_MAX_SIZE;

	struct bundle6_parser b6_parser;
	bundle6_parser_init(&b6_parser, &_hal_store_get_bundle, &next_bundle);

    FILE* file = fopen( item->filepath, "r");
    if(file == NULL){
        LOGF_ERROR("Storage: Error opening file %s: %d (%s)", item->filepath, errno, strerror(errno));
        goto jump_next;
    }

    size_t len = 0;
    size_t buffer_offset = 0;
    size_t parsed_bytes = 0;
    uint8_t buffer[HAL_STORE_READ_BUFFER_SIZE];

    struct parser *basedata = NULL;
    do {

        if(basedata != NULL && HAS_FLAG(basedata->flags, PARSER_FLAG_BULK_READ)){

            LOGF_DEBUG("BULK READ REMANING of %d bytes", basedata->next_bytes);

            len = fread(basedata->next_buffer, sizeof(char), basedata->next_bytes, file);
            if(len == 0)
                break; // Unexpected end

            basedata->next_buffer += len;
            basedata->next_bytes -= len;

            // We done filled the buffer
            if(basedata->next_bytes == 0){
                // Disable bulk read
                basedata->flags &= ~PARSER_FLAG_BULK_READ;
                // Feed with empty buffer
                if(item->protocol_version == '7'){
                    bundle7_parser_read(&b7_parser, NULL, 0);
                } else if(item->protocol_version == '6'){
                    bundle6_parser_read(&b6_parser, NULL, 0);
                }

                len = 0;
                buffer_offset = 0;
            }

        } else {
            if((len-buffer_offset) == 0){
                /// All data was red by parser, refill from stream
                len = fread(&buffer, sizeof(char), HAL_STORE_READ_BUFFER_SIZE, file);
                buffer_offset = 0;
                if(len == 0)
                    break;
            }

            if(item->protocol_version == '7'){
                parsed_bytes = bundle7_parser_read(&b7_parser, buffer+buffer_offset, len-buffer_offset);
                basedata = b7_parser.basedata;
            } else if(item->protocol_version == '6'){
                parsed_bytes = bundle6_parser_read(&b6_parser, buffer+buffer_offset, len-buffer_offset);
                basedata = b6_parser.basedata;
            }

            buffer_offset += parsed_bytes;

            LOGF_DEBUG("PARSED %d bytes of data", parsed_bytes);

            if(basedata != NULL && HAS_FLAG(basedata->flags, PARSER_FLAG_BULK_READ)){
                // Bulk read requested (data needs to be red in a pre_allocated buffer)
                LOGF_DEBUG("BULK READ of %d bytes requested", basedata->next_bytes);

                if(len-buffer_offset > 0){
                    size_t bytes_to_copy = MIN(len-buffer_offset, basedata->next_bytes);
                    memcpy(basedata->next_buffer, buffer+buffer_offset, bytes_to_copy);
                    basedata->next_buffer += bytes_to_copy;
                    basedata->next_bytes -= bytes_to_copy;
                    buffer_offset -= bytes_to_copy;
                    LOGF_DEBUG("MOVED %d bytes from local buffer", bytes_to_copy);
                }

            }
        }

    } while(basedata == NULL || basedata->status == PARSER_STATUS_GOOD);

    fclose(file);

    if(next_bundle == NULL){
        LOGF_ERROR("HALStore: No bundle found in %s", item->filepath);
        goto jump_next;
    }
    
    char* metadata_path = malloc(sizeof(char) * strlen(item->filepath) + 5 + 1);
    sprintf(metadata_path, "%s.meta", item->filepath);

    FILE* metadata_file = fopen( metadata_path, "r");
    if(metadata_file != NULL){
        _hal_store_read_metadata(next_bundle, metadata_file);
        fclose(metadata_file);
    } else {
        LOGF_ERROR("SHALStore: Failed to read metadata from %s: %d (%s)", metadata_path, errno, strerror(errno));
    }
    free(metadata_path);


    jump_next:
    if(next_bundle != NULL)
        LOGF_DEBUG("Store loaded %s", item->filepath);
    _hal_store_loadall_item_free(item);
    if(next_bundle == NULL)
        return hal_store_loadall_next(loader_base);

    return next_bundle;
}

#endif