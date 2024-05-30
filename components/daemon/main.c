// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "ud3tn/cmdline.h"
#include "ud3tn/init.h"

#ifdef ESP
int app_main()
{
	int argc = 0;
	char *argv[] = {};

	init(argc, argv);
	start_tasks(parse_cmdline(argc, argv));
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