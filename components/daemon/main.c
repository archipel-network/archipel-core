// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "ud3tn/cmdline.h"
#include "ud3tn/init.h"

#ifdef ESP
int app_main()
{
	int argc = 0;
	char* argv[1] = {NULL};

	init(argc, argv);

	const struct ud3tn_cmdline_options options = {
		.eid = "dtn://archipel-core.dtn/",
		.cla_options = "",
		.bundle_version = 7,
		.log_level = 3,
		.status_reporting = true,
		.allow_remote_configuration = false,
		.exit_immediately = false,
		.mbs = 1000000ULL,
		.lifetime_s = 86400ULL
	};

	start_tasks(&options);
	return start_os();
}
#else
int main(int argc, char *argv[])
{
	init(argc, argv);
	start_tasks(parse_cmdline(argc, argv));
	return start_os();
}
#endif