// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "unity_fixture.h"

void testud3tn(void)
{
	RUN_TEST_GROUP(simplehtab);
	RUN_TEST_GROUP(sdnv);
	RUN_TEST_GROUP(node);
	RUN_TEST_GROUP(routingTable);
	RUN_TEST_GROUP(eid);
	RUN_TEST_GROUP(random);
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
	RUN_TEST_GROUP(bibe_header_encoder);
	RUN_TEST_GROUP(bibe_parser);
	RUN_TEST_GROUP(bibe_validation);
#ifdef PLATFORM_POSIX
	RUN_TEST_GROUP(simple_queue);
#endif // PLATFORM_POSIX
}
