// SPDX-License-Identifier: GPL-3.0-or-later

#ifdef ARCHIPEL_CORE

#include "cla/posix/cla_kiss_uni.h"

#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <termios.h>
#include <string.h>
#include <stdlib.h>
#include "cla/cla.h"
#include "platform/hal_io.h"
#include "platform/hal_task.h"
#include "platform/hal_queue.h"
#include "platform/hal_semaphore.h"
#include "cla/cla_contact_tx_task.h"
#include "errno.h"

#define KISS_FEND ((char) 0xC0)
#define KISS_FESC ((char) 0xDB)
#define KISS_TFEND ((char) 0xDC)
#define KISS_TFESC ((char) 0xDD)
#define KISS_BUFFER_SIZE 4096
#define KISS_CHUNK_SIZE 128

struct kissunicla_config {
    struct cla_config base;

    // Serial configuration
    char serial_device_path[PATH_MAX];
    speed_t serial_device_speed;

    // Transmission handling
    struct cla_tx_queue tx_queue;
};

/* Returns a unique identifier of the CLA as part of the CLA address */
static const char *kissunicla_name_get(void)
{
	return "kiss+uni";
}

/* Obtains the max. serialized size of outgoing bundles for this CLA. */
static size_t kissunicla_mbs_get(struct cla_config *const config)
{
	(void)config;
	return SIZE_MAX;
}

struct kissunicla_runtime {
    struct kissunicla_config* config;
    int serial_device;
};

void kissunicla_receive_kiss_frame(struct kissunicla_runtime* runtime, char* buffer, size_t buffer_size) {
    // TODO Parse and receive
    buffer[buffer_size] = '\0';
    LOGF_INFO("Received KISS frame %s (%d length) !", buffer, buffer_size);
}

enum ud3tn_result kissunicla_send_kiss_frame(struct kissunicla_runtime* runtime, struct bundle* bundle){
    //TODO Serialize and send
    LOG_INFO("Received bundle to send");
    return UD3TN_OK;
}

int kissunicla_open_serial_port(struct kissunicla_config* config){
    int device_fd = open(config->serial_device_path, O_RDWR | O_NOCTTY);
    if(device_fd < 0){
        LOGF_ERROR("KISSUNI CLA: Failed to open %s: %s", config->serial_device_path, strerror(errno));
        return UD3TN_FAIL;
    }
    int serial_device = device_fd;

    if(isatty(serial_device)){

        struct termios ttyconfig;
        if(tcgetattr(serial_device, &ttyconfig) < 0){
            LOGF_ERROR("Failed to get current configuration of %s: %s", config->serial_device_path, strerror(errno));
            goto error_close_fd;
        }

        if(config->serial_device_speed != 0){
            if(cfsetspeed(&ttyconfig, config->serial_device_speed) < 0){
                LOGF_ERROR("Failed set speed of %s to %d: %s", config->serial_device_path, config->serial_device_speed, strerror(errno));
            }
        }

        cfmakeraw(&ttyconfig);

        if(tcsetattr(serial_device, TCSAFLUSH, &ttyconfig) < 0){
                LOGF_ERROR("Failed to configure %s: %s", config->serial_device_path, strerror(errno));
        }

        LOGF_INFO("KISSUNI CLA: Opened and configured serial %s", config->serial_device_path);

    } else {
        LOGF_WARN("KISSUNI CLA: %s is not a tty, no serial configuration needed", config->serial_device_path);
        LOGF_INFO("KISSUNI CLA: Opened serial %s", config->serial_device_path);
    }

    return device_fd;

    error_close_fd:
        close(device_fd);
        return -1;
}

void kissunicla_listen(void * param){
    struct kissunicla_runtime* runtime = (struct kissunicla_runtime*) param;

    char next_frame[KISS_BUFFER_SIZE];
    size_t next_frame_length = 0;

    char buffer[KISS_CHUNK_SIZE];
    ssize_t byte_received;

    do {

        if(runtime->serial_device < 0){
            runtime->serial_device = kissunicla_open_serial_port(runtime->config);
        }

        if(runtime->serial_device >= 0){
            do {
                byte_received = read(
                    runtime->serial_device,
                    buffer,
                    KISS_CHUNK_SIZE);

                if(byte_received == 0){
                    LOGF_ERROR("KISSUNI CLA: Received EOF on %s (Device isn't available anymore ?)", runtime->config->serial_device_path);
                    break;
                } else if(byte_received < 0){
                    LOGF_ERROR("KISSUNI CLA: Failed to read from %s: %s", runtime->config->serial_device_path, strerror(errno));
                    break;
                }

                char escape_mode = 0;
                for(ssize_t i = 0; i<byte_received; i++){
                    char value = buffer[i];
                    if(escape_mode){
                        switch(value){
                            case KISS_TFEND:
                                value = KISS_FEND;
                                break;
                            case KISS_TFESC:
                                value = KISS_FESC;
                                break;
                        }
                        escape_mode = 0;
                    } else if(value == KISS_FESC){
                        escape_mode = 1;
                        continue;
                    }

                    if(value != KISS_FEND){
                        if(next_frame_length > KISS_BUFFER_SIZE){
                            next_frame_length = KISS_BUFFER_SIZE;
                            LOGF_WARN("KISSUNI CLA: Received frame that exceeds maximum frame buffer (%d bytes)", KISS_BUFFER_SIZE);
                        } else {
                            next_frame[next_frame_length] = value;
                        }
                        next_frame_length++;
                    } else {
                        if(next_frame_length > 0 && next_frame[0] == 0){
                            kissunicla_receive_kiss_frame(runtime, &next_frame[1], next_frame_length-1);
                        }
                        next_frame_length = 0;
                    }
                }

            } while(byte_received >= 0);

            close(runtime->serial_device);
            runtime->serial_device = -1;
        }

        // At this point, device was closed (because EOF or read failure)

        hal_task_delay(10000);

    } while(true);
    abort();
}

/**
* Allocate a new cla address
*/
char* kissunicla_get_cla_addr(){
    char* buffer = malloc(sizeof(char) * 10);
    strcpy(buffer, "kiss+uni:");
    return buffer;
}

void kissunicla_send(void * param){
    struct kissunicla_runtime* runtime = (struct kissunicla_runtime*) param;

    struct cla_contact_tx_task_command com;
    while(true){
        if(hal_queue_receive(runtime->config->tx_queue.tx_queue_handle, &com, -1) == UD3TN_FAIL){
            continue;
        }

        if(com.type == TX_COMMAND_BUNDLES){
            struct routed_bundle_list* bundle_list_item = com.bundles;
            while(bundle_list_item != NULL) {
                enum ud3tn_result result = kissunicla_send_kiss_frame(runtime, bundle_list_item->data);

                if(result == UD3TN_FAIL){
                    bundle_processor_inform(
                        runtime->config->base.bundle_agent_interface->bundle_signaling_queue,
                        (struct bundle_processor_signal) {
                            .type = BP_SIGNAL_TRANSMISSION_FAILURE,
                            .bundle = bundle_list_item->data,
                            .peer_cla_addr = kissunicla_get_cla_addr(),
                        }
                    );
                } else {
                    bundle_processor_inform(
                        runtime->config->base.bundle_agent_interface->bundle_signaling_queue,
                        (struct bundle_processor_signal) {
                            .type = BP_SIGNAL_TRANSMISSION_SUCCESS,
                            .bundle = bundle_list_item->data,
                            .peer_cla_addr = kissunicla_get_cla_addr(),
                        }
                    );
                }

                bundle_list_item = bundle_list_item->next;
            }
        }
    }

}

/* Starts the TX/RX tasks and, e.g., the socket listener */
static enum ud3tn_result kissunicla_launch(struct cla_config *const config)
{
    struct kissunicla_config* local_config = (struct kissunicla_config*) config;

    struct kissunicla_runtime* runtime = malloc(sizeof(struct kissunicla_runtime));
    runtime->config = local_config;
    runtime->serial_device = -1;

    if(hal_task_create(kissunicla_listen, runtime) != UD3TN_OK){
        LOG_ERROR("KISSUNI CLA: Failed to start listen task");
        goto error_free_runtime;
    }

    if(hal_task_create(kissunicla_send, runtime) != UD3TN_OK){
        LOG_ERROR("KISSUNI CLA: Failed to start send task");
        goto error_free_runtime;
    }

	return UD3TN_OK;

    error_free_runtime:
        free(runtime);
        return UD3TN_FAIL;
}

struct cla_tx_queue kissunicla_get_tx_queue(struct cla_config* config, const char* _eid, const char* _cla_addr) {
    struct kissunicla_config* local_config = (struct kissunicla_config*) config;
    return local_config->tx_queue;
}

static enum ud3tn_result kissunicla_start_scheduled_contact(
	struct cla_config *_config,
	const char *_eid,
	const char *_cla_addr ){
    return UD3TN_OK;
}

static enum ud3tn_result kissunicla_end_scheduled_contact(
	struct cla_config *_config,
	const char *_eid,
	const char *_cla_addr ){
    return UD3TN_OK;
}

const struct cla_vtable kissunicla_vtable = {
    .cla_name_get = kissunicla_name_get,
    .cla_mbs_get = kissunicla_mbs_get,
    .cla_launch = kissunicla_launch,
    .cla_get_tx_queue = kissunicla_get_tx_queue,
    .cla_start_scheduled_contact = kissunicla_start_scheduled_contact,
    .cla_end_scheduled_contact = kissunicla_end_scheduled_contact
};

struct cla_config *kissunicla_create(
	const char *const options[], const size_t option_count,
	const struct bundle_agent_interface *bundle_agent_interface) {

    struct kissunicla_config* config = malloc(sizeof(struct kissunicla_config));
    if(!config){
        LOG_ERROR("KISSUNI CLA: Config allocation failed");
        return NULL;
    }

    config->base.bundle_agent_interface = bundle_agent_interface;
    config->base.vtable = &kissunicla_vtable;

    if(option_count > 0 && strlen(options[0]) > 0){
        strcpy(config->serial_device_path, options[0]);
    } else {
        LOG_ERROR("KISSUNI CLA: First option must be serial device path (e.g. /dev/ttyUSB0)");
        goto error_free_config;
    }

    config->serial_device_speed = 0; // By default, do not change

    for(unsigned int i = 1; i<option_count; i++){
        char* opt = malloc(sizeof(char) * strlen(options[i]));
        strcpy(opt, options[i]);

        char* key = strtok(opt, "=");
        char* value = strtok(NULL, "=");

        if(strcmp(key, "speed") == 0){
            long speed_value = strtol(value, NULL, 10);
            switch(speed_value){
                
                #ifdef B50
                case 50:
                    config->serial_device_speed = B50;
                    break;
                #endif
                
                #ifdef B75
                case 75:
                    config->serial_device_speed = B75;
                    break;
                #endif
                
                #ifdef B110
                case 110:
                    config->serial_device_speed = B110;
                    break;
                #endif

                #ifdef B134
                case 134:
                    config->serial_device_speed = B134;
                    break;
                #endif

                #ifdef B150
                case 150:
                    config->serial_device_speed = B150;
                    break;
                #endif

                #ifdef B200
                case 200:
                    config->serial_device_speed = B200;
                    break;
                #endif

                #ifdef B300
                case 300:
                    config->serial_device_speed = B300;
                    break;
                #endif

                #ifdef B600
                case 600:
                    config->serial_device_speed = B600;
                    break;
                #endif

                #ifdef B1200
                case 1200:
                    config->serial_device_speed = B1200;
                    break;
                #endif

                #ifdef B1800
                case 1800:
                    config->serial_device_speed = B1800;
                    break;
                #endif

                #ifdef B2400
                case 2400:
                    config->serial_device_speed = B2400;
                    break;
                #endif

                #ifdef B4800
                case 4800:
                    config->serial_device_speed = B4800;
                    break;
                #endif

                #ifdef B9600
                case 9600:
                    config->serial_device_speed = B9600;
                    break;
                #endif

                #ifdef B19200
                case 19200:
                    config->serial_device_speed = B19200;
                    break;
                #endif

                #ifdef B38400
                case 38400:
                    config->serial_device_speed = B38400;
                    break;
                #endif

                #ifdef B57600
                case 57600:
                    config->serial_device_speed = B57600;
                    break;
                #endif

                #ifdef B115200
                case 115200:
                    config->serial_device_speed = B115200;
                    break;
                #endif

                #ifdef B230400
                case 230400:
                    config->serial_device_speed = B230400;
                    break;
                #endif

                #ifdef B460800
                case 460800:
                    config->serial_device_speed = B460800;
                    break;
                #endif

                #ifdef B500000
                case 500000:
                    config->serial_device_speed = B500000;
                    break;
                #endif

                #ifdef B576000
                case 576000:
                    config->serial_device_speed = B576000;
                    break;
                #endif

                #ifdef B921600
                case 921600:
                    config->serial_device_speed = B921600;
                    break;
                #endif

                #ifdef B1000000
                case 1000000:
                    config->serial_device_speed = B1000000;
                    break;
                #endif

                #ifdef B1152000
                case 1152000:
                    config->serial_device_speed = B1152000;
                    break;
                #endif

                #ifdef B1500000
                case 1500000:
                    config->serial_device_speed = B1500000;
                    break;
                #endif

                #ifdef B2000000
                case 2000000:
                    config->serial_device_speed = B2000000;
                    break;
                #endif

                #ifdef B76800
                case 76800:
                    config->serial_device_speed = B76800;
                    break;
                #endif

                #ifdef B153600
                case 153600:
                    config->serial_device_speed = B153600;
                    break;
                #endif

                #ifdef B307200
                case 307200:
                    config->serial_device_speed = B307200;
                    break;
                #endif

                #ifdef B614400
                case 614400:
                    config->serial_device_speed = B614400;
                    break;
                #endif

                #ifdef B2500000
                case 2500000:
                    config->serial_device_speed = B2500000;
                    break;
                #endif

                #ifdef B3000000
                case 3000000:
                    config->serial_device_speed = B3000000;
                    break;
                #endif

                #ifdef B3500000
                case 3500000:
                    config->serial_device_speed = B3500000;
                    break;
                #endif

                #ifdef B4000000
                case 4000000:
                    config->serial_device_speed = B4000000;
                    break;
                #endif

                default:
                    LOGF_ERROR("KISSUNI CLA: Unsupported speed %d", speed_value);
                    free(opt);
                    return NULL;
            }
        } else {
            LOGF_WARN("KISSUNI CLA: Unknown option \"%s\"", key);
        }

        free(opt);
    }

    config->tx_queue.tx_queue_sem = hal_semaphore_init_binary();
    config->tx_queue.tx_queue_handle = hal_queue_create(
        CONTACT_TX_TASK_QUEUE_LENGTH * 10,
        sizeof(struct cla_contact_tx_task_command)
    );
    hal_semaphore_release(config->tx_queue.tx_queue_sem);

    return (struct cla_config*) config;

    error_free_config:
        free(config);
        return NULL;
}

#endif