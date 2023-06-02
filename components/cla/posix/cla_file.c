#include "cla/mtcp_proto.h"
#include "cla/posix/cla_file.h"

#include "platform/hal_config.h"
#include "platform/hal_io.h"
#include "platform/hal_queue.h"
#include "platform/hal_semaphore.h"
#include "platform/hal_task.h"

#include "ud3tn/bundle_processor.h"
#include "ud3tn/common.h"
#include "ud3tn/config.h"
#include "ud3tn/task_tags.h"
#include "ud3tn/simplehtab.h"
#include "cla/cla_contact_tx_task.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>

// Configuration of the file CLA
struct filecla_config {
    struct cla_config base;
	Task_t folder_watching_task;
	
	char *local_eid;
	QueueIdentifier_t signaling_queue;

	struct htab_entrylist *contacts_elem[FILECLA_MAX_CONTACTS];
	struct htab contacts;
	Semaphore_t contacts_semaphore;
};

struct filecla_contact {
	const char *eid;
	const char *folder;
	struct cla_tx_queue queue;
};


/* Returns a unique identifier of the CLA as part of the CLA address */
static const char *filecla_name_get(void)
{
	return "file";
}

struct bundle_injection_params {
	char* file_path;
	char* local_eid;
	QueueIdentifier_t signaling_queue;
};

static void inject_bundle(struct bundle *bundle, void* p)
{
	struct bundle_injection_params* params = (struct bundle_injection_params*) p;

	// Check if bundle isn't coming fromp ourself
	struct bundle_block_list **blocks = &bundle->blocks;
	while (*blocks != NULL) {
		if ((*blocks)->data->type == BUNDLE_BLOCK_TYPE_PREVIOUS_NODE) {

			size_t data_len = (*blocks)->data->length;
			char* previous_eid = malloc( data_len + sizeof(char));
			memcpy(previous_eid, (*blocks)->data->data, data_len);
			previous_eid[data_len] = '\0';

			if(strcmp(previous_eid, params->local_eid + 4) == 0){
				free(previous_eid);
				bundle_free(bundle);
				return; // Do nothing if coming from our local eid
			}

			free(previous_eid);
			break;
		}
		blocks = &(*blocks)->next;
	}

	LOGF("Bundle red from %s (source: %s)", params->file_path, bundle->source);
	if(remove(params->file_path) != 0){
		LOGF("FileCLA : Unable to remove file %s", params->file_path);
	};
	bundle_processor_inform(
		params->signaling_queue,
		bundle,
		BP_SIGNAL_BUNDLE_INCOMING,
		NULL,
		NULL,
		NULL,
		NULL
	);
}

static void filecla_folder_watching_task(void *p){
	struct filecla_config *const config = (
		(struct filecla_config *)p
	);

	struct bundle_injection_params parser_params = {
		.local_eid = config->local_eid,
		.signaling_queue = config->signaling_queue,
	};

	struct bundle7_parser b7_parser;
	bundle7_parser_init(&b7_parser, &inject_bundle, &parser_params);
	b7_parser.bundle_quota = BUNDLE_MAX_SIZE;

	struct bundle6_parser b6_parser;
	bundle6_parser_init(&b6_parser, &inject_bundle, &parser_params);

	uint8_t buffer[FILECLA_READ_BUFFER_SIZE];

	LOG("FileCLA : folder watching task started");

	for (;;) {


		char* folders[FILECLA_MAX_CONTACTS];
		size_t n_folders = 0;

		hal_semaphore_take_blocking(config->contacts_semaphore);
		for (size_t i = 0; i < FILECLA_MAX_CONTACTS; i++)
		{
			if(config->contacts_elem[i] != NULL){
				struct filecla_contact* ct = config->contacts_elem[i]->value;

				folders[n_folders] = malloc(sizeof(char) * (strlen(ct->folder) + 1));
				strcpy(folders[n_folders], ct->folder);

				n_folders += 1;
			}
		}
		hal_semaphore_release(config->contacts_semaphore);

		for(size_t i = 0; i<n_folders; i++){

			size_t folder_path_len = strlen(folders[i]);
			DIR *f = opendir(folders[i]);
			if(f != NULL){

				struct dirent *entry;
				while((entry = readdir(f)) != NULL){
					
					size_t file_name_len = strlen(entry->d_name);
					char bundle_ver = entry->d_name[file_name_len - 1];
					char* file_path = malloc(sizeof(char) * (folder_path_len + 1 + file_name_len + 1)); // [folder path] '/' [file name] '\0'
					sprintf(file_path, "%s/%s", folders[i], entry->d_name);

					if(entry->d_type == 8){ //  Regular file
					
						if(bundle_ver == '6' || bundle_ver == '7'){

							FILE* file = fopen(file_path, "r");
							if(file != NULL){
								parser_params.file_path = file_path;

								size_t len;
								while((len = fread(&buffer, sizeof(char), FILECLA_READ_BUFFER_SIZE, file)) > 0){

									struct parser *basedata;

									if(bundle_ver == '6'){
										bundle6_parser_read(&b6_parser, buffer, len);
										basedata = b7_parser.basedata;
									} else if(bundle_ver == '7') {
										bundle7_parser_read(&b7_parser, buffer, len);
										basedata = b6_parser.basedata;
									}

									if(basedata->status == PARSER_STATUS_ERROR){
										if(basedata->flags & PARSER_FLAG_CRC_INVALID){
											LOGF("FileCLA : Invalid CRC for %s", file_path);
										} else {
											LOGF("FileCLA : Parsing error for %s", file_path);
										}
										break;
									}
								}
								
								fclose(file);

								if(bundle_ver == '6'){
									bundle6_parser_reset(&b6_parser);
								} else if(bundle_ver == '7') {
									bundle7_parser_reset(&b7_parser);
								}

							} else {
								LOGF("FileCLA : Unable to open file %s", file_path);
							}
								
						} else {
							LOGF("FileCLA : Could not get bundle version from file name of %s", file_path);
						}

					}

					free(file_path);

				}

				closedir(f);
			} else {
				LOGF("FileCLA : Unable to open directory %s", folders[i]);
			}

			// Free folder name after process
			free(folders[i]);
			folders[i] = NULL;
		}
		// Do NOT user folders[] after this loop (all content is free)
		
		hal_task_delay(FILECLA_FOLDER_WATCHING_DELAY);
	}

	ASSERT(0);
}

struct filecla_queue_folder {
	char* folder;
	struct cla_tx_queue queue;
};

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

static void write_to_file(void* file, const void * b, const size_t size){

	FILE* f = (FILE*) file;
	const uint8_t* buffer = (const uint8_t*) b;

	if(fwrite(buffer, 1, size, f) != size) {
		LOG("FileCLA : failed to write file buffer");
	}
}

// BPv7 5.4-4 / RFC5050 5.4-5
static void prepare_bundle_for_forwarding(struct bundle *bundle, char* previous_node_eid)
{
	struct bundle_block_list **blocks = &bundle->blocks;

	// BPv7 5.4-4: "If the bundle has a Previous Node block ..., then that
	// block MUST be removed ... before the bundle is forwarded."
	while (*blocks != NULL) {
		if ((*blocks)->data->type == BUNDLE_BLOCK_TYPE_PREVIOUS_NODE) {
			// Replace the first occurrence of the previous node
			// block by its successor and free it.
			*blocks = bundle_block_entry_free(*blocks);
			break;
		}
		blocks = &(*blocks)->next;
	}

	// Adding previous node block
	char* eid = previous_node_eid + 4;
	size_t eid_len = strlen(eid);
	struct bundle_block* block = bundle_block_create(BUNDLE_BLOCK_TYPE_PREVIOUS_NODE);
	block->data = malloc(sizeof(char) * eid_len);
	memcpy(block->data, eid, eid_len);
	block->length = eid_len;
	struct bundle_block_list* entry = bundle_block_entry_create(block);
	entry->next = bundle->blocks;
	bundle->blocks = entry;

	const uint8_t dwell_time_ms = hal_time_get_timestamp_ms() -
		bundle->reception_timestamp_ms;

	// BPv7 5.4-4: "If the bundle has a bundle age block ... at the last
	// possible moment ... the bundle age value MUST be increased ..."
	if (bundle_age_update(bundle, dwell_time_ms) == UD3TN_FAIL)
		LOGF("TX: Bundle %p age block update failed!", bundle);
}

static void filecla_bundle_writing(void *p){
	struct filecla_config *const config = (
		(struct filecla_config *)p
	);

	LOG("FileCLA : bundle writing task started");

	for (;;) {

		struct filecla_queue_folder contacts[FILECLA_MAX_CONTACTS];
		size_t n_contacts = 0;

		hal_semaphore_take_blocking(config->contacts_semaphore);
		for (size_t i = 0; i < FILECLA_MAX_CONTACTS; i++)
		{
			if(config->contacts_elem[i] != NULL){
				struct filecla_contact* ct = config->contacts_elem[i]->value;

				// No TX defined for this contact
				if(ct->queue.tx_queue_handle == NULL){
					continue;
				}

				hal_semaphore_take_blocking(ct->queue.tx_queue_sem);
				if(hal_queue_nr_of_items_waiting(ct->queue.tx_queue_handle) > 0){
					contacts[n_contacts].folder = malloc(sizeof(char) * (strlen(ct->folder)+1));
					strcpy(contacts[n_contacts].folder, ct->folder);
					contacts[n_contacts].queue = ct->queue;
					n_contacts += 1;
				}
				hal_semaphore_release(ct->queue.tx_queue_sem);
			}
		}
		hal_semaphore_release(config->contacts_semaphore);
		

		for (size_t i = 0; i < n_contacts; i++)
		{
			struct filecla_queue_folder* qf = &contacts[i];

			hal_semaphore_take_blocking(qf->queue.tx_queue_sem);

			struct cla_contact_tx_task_command cmd; 
			while(hal_queue_nr_of_items_waiting(qf->queue.tx_queue_handle) > 0){
				if(hal_queue_receive(qf->queue.tx_queue_handle, &cmd, -1) == UD3TN_OK) {
					if(cmd.type == TX_COMMAND_FINALIZE)
						break;
					if(!cmd.bundles)
						continue;

					struct routed_bundle_list* bundlelist = cmd.bundles;
					do {
						struct bundle* bundle = bundlelist->data;
						
						char seq_num[25];
						sprintf(seq_num, "%ld", bundle->sequence_number);

						char protocol_version[5];
						sprintf(protocol_version, "%d", bundle->protocol_version);

						char *eid = malloc(sizeof(char) * (strlen(bundle->source) + 1));
						strcpy(eid, bundle->source);
						eid_to_filename(eid);

						uint64_t filename_size =	sizeof(char) * (
													strlen(qf->folder) + 1 + // Folder path + '/'
													strlen(seq_num) + 1 + // Sequence number + '_'
													strlen(eid) + // bundle source
													7 + strlen(protocol_version) + // extension .bundle6 or .bundle7
													1); // ending \0
						
						char* filename = malloc(filename_size);
						sprintf(filename, "%s/%s_%s.bundle%s", qf->folder, seq_num, eid, protocol_version);

						FILE *f = fopen(filename, "w");
						if(f == NULL){

							LOGF("FileCLA : Failed to open file %s in write mode",filename);

						} else {
							prepare_bundle_for_forwarding(bundle, config->local_eid);
							bundle_serialize(bundle, write_to_file, f);
							fclose(f);

							LOGF("FileCLA : Bundle written in %s",filename);
						}

						free(filename);
						free(eid);
						bundlelist = bundlelist->next;
					} while(bundlelist != NULL);
				}
			}

			hal_semaphore_release(qf->queue.tx_queue_sem);

			free(qf->folder);
		}
		
		hal_task_delay(FILECLA_IDLE_TIMEOUT);
	}

	ASSERT(0);
}

/* Starts the TX/RX tasks and, e.g., the socket listener */
static enum ud3tn_result filecla_launch(struct cla_config *const config)
{
	struct filecla_config *const file_config = (
		(struct filecla_config *)config
	);

	file_config->folder_watching_task = hal_task_create(
		filecla_folder_watching_task,
		"filecla_folder_watching",
		CONTACT_RX_TASK_PRIORITY,
		config,
		CONTACT_RX_TASK_STACK_SIZE,
		(void *)CONTACT_RX_TASK_TAG
	);

	file_config->folder_watching_task = hal_task_create(
		filecla_bundle_writing,
		"filecla_bundle_writing",
		CONTACT_TX_TASK_PRIORITY,
		config,
		CONTACT_TX_TASK_STACK_SIZE,
		(void *)CONTACT_TX_TASK_TAG
	);

	return UD3TN_OK;
}

/* Obtains the max. serialized size of outgoing bundles for this CLA. */
static size_t filecla_mbs_get(struct cla_config *const config)
{
	(void)config;
	return SIZE_MAX;
}

/* Returns the transmission queue for the given node EID and address */
static struct cla_tx_queue filecla_get_tx_queue(
	struct cla_config *config, 
	const char *eid, 
	const char *cla_addr
){
	(void)cla_addr;
	struct filecla_config *const file_config = (
		(struct filecla_config *)config
	);

	struct cla_tx_queue queue = {
			.tx_queue_handle = NULL,
			.tx_queue_sem = NULL,
		};

	hal_semaphore_take_blocking(file_config->contacts_semaphore);
	struct filecla_contact* ctt = htab_get(&file_config->contacts, eid);

	if(ctt == NULL){

		LOGF("FileCLA: Undefined contact for \"%s\"", eid);

	} else {
		if(ctt->queue.tx_queue_handle == NULL){
			ctt->queue.tx_queue_handle = hal_queue_create(
				CONTACT_TX_TASK_QUEUE_LENGTH,
				sizeof(struct cla_contact_tx_task_command));

			ctt->queue.tx_queue_sem = hal_semaphore_init_binary();
			hal_semaphore_release(ctt->queue.tx_queue_sem);
		}

		queue = ctt->queue;
	}

	hal_semaphore_release(file_config->contacts_semaphore);
	return queue;
}

/* Initiates a scheduled contact for a given EID and CLA address */
static enum ud3tn_result filecla_start_scheduled_contact(
	struct cla_config *config, const char *eid, const char *cla_addr)
{
	struct filecla_config *const file_config = (
		(struct filecla_config *)config
	);

	const char* folder_path = cla_addr + 5; // Remove prefix "file:" (5 char)

	LOGF("FileCLA: Added contact \"%s\" using folder %s",
		     eid,
		     folder_path);

	struct filecla_contact* contact = malloc(sizeof(contact));
	contact->eid = eid;
	contact->folder = folder_path;

	hal_semaphore_take_blocking(file_config->contacts_semaphore);
	htab_add(&file_config->contacts, eid, contact);
	hal_semaphore_release(file_config->contacts_semaphore);

	return UD3TN_OK;
}

/* Ends a scheduled contact for a given EID and CLA address */
static enum ud3tn_result filecla_end_scheduled_contact(
	struct cla_config *config, const char *eid, const char *cla_addr)
{
	struct filecla_config *const file_config = (
		(struct filecla_config *)config
	);

	hal_semaphore_take_blocking(file_config->contacts_semaphore);
	struct filecla_contact* contact =  htab_remove(&file_config->contacts, eid); // TODO Fix that
	hal_semaphore_release(file_config->contacts_semaphore);

	if(contact->queue.tx_queue_handle != NULL){
		hal_semaphore_take_blocking(contact->queue.tx_queue_sem);
		hal_queue_delete(contact->queue.tx_queue_handle);
		hal_semaphore_delete(contact->queue.tx_queue_sem);
	}
	free(contact);

	LOGF("FileCLA: Removed contact \"%s\"", eid);

	return UD3TN_OK;
}

// Vtable of methods used for communication with this CLA
const struct cla_vtable filecla_vtable = {
	.cla_name_get = filecla_name_get,
	.cla_launch = filecla_launch,
	.cla_mbs_get = filecla_mbs_get,

	.cla_get_tx_queue = filecla_get_tx_queue,
	.cla_start_scheduled_contact = filecla_start_scheduled_contact,
	.cla_end_scheduled_contact = filecla_end_scheduled_contact,
};

// Entry point of cla creation and configuration
struct cla_config *filecla_create(
	const char *const options[], const size_t option_count,
	const struct bundle_agent_interface *bundle_agent_interface)
{
	(void)options;
	(void)option_count;

	struct filecla_config *config = malloc(sizeof(struct filecla_config));

	if (!config) {
		LOG("FileCLA: Memory allocation failed!");
		return NULL;
	}

	config->local_eid = bundle_agent_interface->local_eid;
	config->folder_watching_task = NULL;
	config->signaling_queue = bundle_agent_interface->bundle_signaling_queue;

	htab_init(&config->contacts, FILECLA_MAX_CONTACTS, config->contacts_elem);
	config->contacts_semaphore = hal_semaphore_init_binary();
	hal_semaphore_release(config->contacts_semaphore);

	cla_config_init(&config->base, bundle_agent_interface);
    
	config->base.vtable = &filecla_vtable;

	return &config->base;
}
