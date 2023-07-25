// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "ud3tn/cmdline.h"
#include "ud3tn/common.h"
#include "ud3tn/config.h"
#include "ud3tn/eid.h"

#include "platform/hal_io.h"

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define QUOTE(s) #s
#define STR(s) QUOTE(s)

static struct ud3tn_cmdline_options global_cmd_opts;

/**
 * Helper function for parsing a 64-bit unsigned integer from a given C-string.
 */
enum ud3tn_result parse_uint64(const char *str, uint64_t *result);

/**
 * Replaces long options in argv with short ones.
 */
static void shorten_long_cli_options(const int argc, char *argv[]);

static void print_usage_text(void);

static void print_help_text(void);

const struct ud3tn_cmdline_options *parse_cmdline(int argc, char *argv[])
{
	// For now, we use a global variable. (Because why not?)
	// Though, this may be refactored easily.
	struct ud3tn_cmdline_options *result = &global_cmd_opts;
	int opt;

	// If we override sth., first deallocate
	if (result->eid)
		free(result->eid);
	if (result->cla_options)
		free(result->cla_options);

	// Set default values
	result->aap_socket = NULL;
	result->aap_node = NULL;
	result->aap_service = NULL;
	result->bundle_version = DEFAULT_BUNDLE_VERSION;
	result->status_reporting = false;
	result->allow_remote_configuration = false;
	result->exit_immediately = false;
	result->lifetime_s = DEFAULT_BUNDLE_LIFETIME_S;
	result->log_level = DEFAULT_LOG_LEVEL;
	// The following values cannot be 0
	result->mbs = 0;
	// The strings are set afterwards if not provided as an option
	result->eid = NULL;
	result->cla_options = NULL;
	int option_index = 0;

	if (!argv || argc <= 1)
		goto finish;

	shorten_long_cli_options(argc, argv);
	while ((opt = getopt(argc, argv, ":a:b:c:e:l:L:m:p:s:rRhu")) != -1) {
		switch (opt) {
		case 'a':
			if (!optarg || strlen(optarg) < 1) {
				LOG("Invalid AAP node provided!");
				return NULL;
			}
			result->aap_node = strdup(optarg);
			break;
		case 'b':
			if (!optarg || strlen(optarg) != 1 || (
					optarg[0] != '6' && optarg[0] != '7')) {
				LOG("Invalid BP version provided!");
				return NULL;
			}
			result->bundle_version = (optarg[0] == '6') ? 6 : 7;
			break;
		case 'c':
			if (!optarg) {
				LOG("Invalid CLA options string provided!");
				return NULL;
			}
			result->cla_options = strdup(optarg);
			break;
		case 'e':
			if (!optarg || validate_local_eid(optarg) != UD3TN_OK ||
					strcmp("dtn:none", optarg) == 0) {
				LOG("Invalid EID provided!");
				return NULL;
			}
			result->eid = strdup(optarg);
			break;
		case 'h':
			print_help_text();
			result->exit_immediately = true;
			return result;
		case 'l':
			if (parse_uint64(optarg, &result->lifetime_s)
					!= UD3TN_OK || !result->lifetime_s) {
				LOG("Invalid lifetime provided!");
				return NULL;
			}
			break;
		case 'L':
			if (!optarg || strlen(optarg) != 1 || (
					optarg[0] != '1' && optarg[0] != '2' && optarg[0] != '3'
					&& optarg[0] != '4')) {
				LOG("Invalid log level provided!");
				return NULL;
			}
			result->log_level = optarg[0] - '0';
			LOG_LEVEL = optarg[0] - '0';
			break;
		case 'm':
			if (parse_uint64(optarg, &result->mbs)
					!= UD3TN_OK || !result->mbs) {
				LOG("Invalid maximum bundle size provided!");
				return NULL;
			}
			break;
		case 'p':
			if (!optarg || strlen(optarg) < 1) {
				LOG("Invalid AAP port provided!");
				return NULL;
			}
			result->aap_service = strdup(optarg);
			break;
		case 'r':
			result->status_reporting = true;
			break;
		case 'R':
			result->allow_remote_configuration = true;
			break;
		case 's':
			if (!optarg || strlen(optarg) < 1) {
				LOG("Invalid AAP unix domain socket provided!");
				return NULL;
			}
			result->aap_socket = strdup(optarg);
			break;
		case 'u':
			print_usage_text();
			result->exit_immediately = true;
			return result;
		case ':':
			LOGF("Required argument of option '%s' is missing",
					argv[option_index + 1]);
			print_usage_text();
			return NULL;
		case '?':
			LOGF("Invalid option: '%s'", argv[option_index + 1]);
			print_usage_text();
			return NULL;
		}

		option_index++;
	}

finish:
	// use Unix domain socket by default
	if (!result->aap_socket &&
	    !result->aap_node &&
	    !result->aap_service)
		result->aap_socket = strdup("./" DEFAULT_AAP_SOCKET_FILENAME);
	// prefere Unix domain socket over TCP
	else if (result->aap_socket &&
		(result->aap_node || result->aap_service))
		result->aap_node = result->aap_service = NULL;
	// set default TCP sevice port
	else if (result->aap_node && !result->aap_service)
		result->aap_service = strdup(DEFAULT_AAP_SERVICE);
	// set default TCP node IP
	else if (!result->aap_node && result->aap_service)
		result->aap_node = strdup(DEFAULT_AAP_NODE);

	if (!result->eid)
		result->eid = strdup(DEFAULT_EID);
	if (!result->cla_options)
		result->cla_options = strdup(DEFAULT_CLA_OPTIONS);

	return result;
}

enum ud3tn_result parse_uint64(const char *str, uint64_t *result)
{
	char *end;
	unsigned long long val;

	if (!str)
		return UD3TN_FAIL;
	errno = 0;
	val = strtoull(str, &end, 10);
	if (errno == ERANGE || end == str || *end != 0)
		return UD3TN_FAIL;
	*result = (uint64_t)val;
	return UD3TN_OK;
}

static void shorten_long_cli_options(const int argc, char *argv[])
{
	struct alias {
		char *long_form;
		char *short_form;
	};

	const struct alias aliases[] = {
		{"--aap-host", "-a"},
		{"--aap-port", "-p"},
		{"--aap-socket", "-s"},
		{"--bp-version", "-b"},
		{"--cla", "-c"},
		{"--eid", "-e"},
		{"--help", "-h"},
		{"--lifetime", "-l"},
		{"--max-bundle-size", "-m"},
		{"--status-reports", "-r"},
		{"--allow-remote-config", "-R"},
		{"--usage", "-u"},
		{"--log-level", "-L"},
	};

	const unsigned long aliases_count = sizeof(aliases) / sizeof(*aliases);

	for (unsigned long i = 1; i < (unsigned long) argc; i++) {
		for (unsigned long j = 0; j < aliases_count; j++) {
			if (strcmp(aliases[j].long_form, argv[i]) == 0) {
				argv[i] = aliases[j].short_form;
				break;
			}
		};
	}
}

static void print_usage_text(void)
{
	const char *usage_text = "Usage: ud3tn\n"
		"    [-a HOST, --aap-host HOST] [-p PORT, --aap-port PORT]\n"
		"    [-b 6|7, --bp-version 6|7] [-c CLA_OPTIONS, --cla CLA_OPTIONS]\n"
		"    [-e EID, --eid EID] [-h, --help] [-l SECONDS, --lifetime SECONDS]\n"
		"    [-m BYTES, --max-bundle-size BYTES] [-r, --status-reports]\n"
		"    [-R, --allow-remote-config]\n"
		"    [-L 1|2|3|4, --log-level 1|2|3|4]\n"
		"    [-s PATH --aap-socket PATH] [-u, --usage]\n";

	hal_io_message_printf(usage_text);
}

static void print_help_text(void)
{
	const char *help_text = "Usage: ud3tn [OPTION]...\n\n"
		"Mandatory arguments to long options are mandatory for short options, too.\n"
		"\n"
		"  -a, --aap-host HOST         IP / hostname of the application agent service\n"
		"  -b, --bp-version 6|7        bundle protocol version of bundles created via AAP\n"
		"  -c, --cla CLA_OPTIONS       configure the CLA subsystem according to the\n"
		"                                syntax documented in the man page\n"
		"  -e, --eid EID               local endpoint identifier\n"
		"  -h, --help                  print this text and exit\n"
		"  -l, --lifetime SECONDS      lifetime of bundles created via AAP\n"
		"  -m, --max-bundle-size BYTES bundle fragmentation threshold\n"
		"  -p, --aap-port PORT         port number of the application agent service\n"
		"  -r, --status-reports        enable status reporting\n"
		"  -R, --allow-remote-config   allow configuration via bundles received from CLAs\n"
		"  -L, --log-level             higher or lower log level 4/3/2/1 specifies more or less detailed output\n"
		"  -s, --aap-socket PATH       path to the UNIX domain socket of the application agent service\n"
		"  -u, --usage                 print usage summary and exit\n"
		"\n"
		"Default invocation: ud3tn \\\n"
		"  -b " STR(DEFAULT_BUNDLE_VERSION) " \\\n"
		"  -c " STR(DEFAULT_CLA_OPTIONS) " \\\n"
		"  -e " DEFAULT_EID " \\\n"
		"  -l " STR(DEFAULT_BUNDLE_LIFETIME) " \\\n"
		"  -L " STR(DEFAULT_LOG_LEVEL) " \\\n"
		"  -m %lu \\\n"
		"  -s $PWD/" DEFAULT_AAP_SOCKET_FILENAME "\n"
		"\n"
		"Please report bugs to <contact@d3tn.com>.\n";

	hal_io_message_printf(help_text, ROUTER_GLOBAL_MBS);
}
