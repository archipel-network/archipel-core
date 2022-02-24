// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "ud3tn/cmdline.h"
#include "ud3tn/init.h"

int main(int argc, char *argv[])
{
	init(argc, argv);
	start_tasks(parse_cmdline(argc, argv));
	return start_os();
}
