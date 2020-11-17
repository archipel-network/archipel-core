#include "unity_fixture.h"

void testud3tn(void)
{
	RUN_TEST_GROUP(ud3tn);
	RUN_TEST_GROUP(simplehtab);
	RUN_TEST_GROUP(sdnv);
	RUN_TEST_GROUP(node);
	RUN_TEST_GROUP(routingTable);
	RUN_TEST_GROUP(bundleStorageManager);
	RUN_TEST_GROUP(eidList);
	RUN_TEST_GROUP(random);
	RUN_TEST_GROUP(malloc);
	RUN_TEST_GROUP(crc);
	RUN_TEST_GROUP(bundle6Create);
	RUN_TEST_GROUP(bundle6ParserSerializer);
	RUN_TEST_GROUP(bundle7Parser);
	RUN_TEST_GROUP(bundle7Serializer);
	RUN_TEST_GROUP(bundle7Reports);
	RUN_TEST_GROUP(bundle7Fragmentation);
	RUN_TEST_GROUP(bundle7Create);
	RUN_TEST_GROUP(spp);
	RUN_TEST_GROUP(spp_parser);
	RUN_TEST_GROUP(spp_timecodes);
	RUN_TEST_GROUP(aap);
	RUN_TEST_GROUP(aap_parser);
	RUN_TEST_GROUP(aap_serializer);
#ifdef PLATFORM_POSIX
	RUN_TEST_GROUP(simple_queue);
#endif // PLATFORM_POSIX
}
