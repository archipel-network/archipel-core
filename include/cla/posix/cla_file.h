#ifdef ARCHIPEL_CORE
#ifndef CLA_FILE
#define CLA_FILE

// Delay between two check of contact folder
#define FILECLA_FOLDER_WATCHING_DELAY 1000
#define FILECLA_IDLE_TIMEOUT 10
#define FILECLA_MAX_CONTACTS 1000
#define FILECLA_READ_BUFFER_SIZE 512

// Buffer size of persisted bundle reader
#define HAL_STORE_READ_BUFFER_SIZE 2048


#include "cla/cla.h"

#include "ud3tn/bundle_processor.h"

#include <stddef.h>

struct cla_config *filecla_create(
	const char *const options[], const size_t option_count,
	const struct bundle_agent_interface *bundle_agent_interface);

#endif // CLA_FILE
#endif