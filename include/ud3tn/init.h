#ifndef INIT_H_INCLUDED
#define INIT_H_INCLUDED

#include "platform/hal_types.h"

#include "ud3tn/cmdline.h"

#include <stdint.h>

/**
 * @brief init
 * @param argc the argument count as provided to main(...)
 * @param argv the arguments as provided to main(...)
 */
void init(int argc, char *argv[]);
void start_tasks(const struct ud3tn_cmdline_options *opt);
int start_os(void);

#endif /* INIT_H_INCLUDED */
