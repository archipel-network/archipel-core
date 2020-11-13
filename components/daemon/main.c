#include "upcn/cmdline.h"
#include "upcn/init.h"

int main(int argc, char *argv[])
{
	init(argc, argv);
	start_tasks(parse_cmdline(argc, argv));
	return start_os();
}
