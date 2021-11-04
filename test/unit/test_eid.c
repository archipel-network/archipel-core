#include "ud3tn/eid.h"
#include "ud3tn/result.h"

#include "unity_fixture.h"

#include <string.h>

TEST_GROUP(eid);

TEST_SETUP(eid)
{
}

TEST_TEAR_DOWN(eid)
{
}

TEST(eid, validate_eid)
{
	TEST_ASSERT_EQUAL(UD3TN_OK, validate_eid("dtn:none"));
	TEST_ASSERT_EQUAL(UD3TN_FAIL, validate_eid("dtn:non"));
	TEST_ASSERT_EQUAL(UD3TN_FAIL, validate_eid("dtn:NONE"));
	TEST_ASSERT_EQUAL(UD3TN_FAIL, validate_eid("dtn:abcd"));
	TEST_ASSERT_EQUAL(UD3TN_FAIL, validate_eid("dtn:"));
	TEST_ASSERT_EQUAL(UD3TN_FAIL, validate_eid("DTN:"));
	TEST_ASSERT_EQUAL(UD3TN_FAIL, validate_eid("DTN:none"));
	TEST_ASSERT_EQUAL(UD3TN_FAIL, validate_eid("dtn"));

	TEST_ASSERT_EQUAL(UD3TN_OK, validate_eid("dtn://ud3tn.dtn/"));
	TEST_ASSERT_EQUAL(UD3TN_OK, validate_eid("dtn://ud3tn.dtn/agent1"));
	TEST_ASSERT_EQUAL(UD3TN_OK, validate_eid("dtn://U/"));
	TEST_ASSERT_EQUAL(UD3TN_OK, validate_eid(
		"dtn://U-D.3_T-N/!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"
	));
	TEST_ASSERT_EQUAL(UD3TN_FAIL, validate_eid("dtn:///"));
	TEST_ASSERT_EQUAL(UD3TN_FAIL, validate_eid("dtn:///agent1"));
	TEST_ASSERT_EQUAL(UD3TN_FAIL, validate_eid("dtn://ud3tn.dtn"));
	TEST_ASSERT_EQUAL(UD3TN_FAIL, validate_eid("dtn://ud3tn+dtn"));
	TEST_ASSERT_EQUAL(UD3TN_FAIL, validate_eid("dtn://=/__"));
	TEST_ASSERT_EQUAL(UD3TN_FAIL, validate_eid("dtn://ud3tn/abc\td"));

	TEST_ASSERT_EQUAL(UD3TN_OK, validate_eid("ipn:0.0"));
	TEST_ASSERT_EQUAL(UD3TN_OK, validate_eid("ipn:1.0"));
	TEST_ASSERT_EQUAL(UD3TN_OK, validate_eid("ipn:0.1"));
	TEST_ASSERT_EQUAL(UD3TN_OK, validate_eid("ipn:1.1"));
	TEST_ASSERT_EQUAL(UD3TN_OK, validate_eid(
		"ipn:18446744073709551615.18446744073709551615"
	));
	TEST_ASSERT_EQUAL(UD3TN_FAIL, validate_eid(
		"ipn:18446744073709551616.18446744073709551616"
	));
	TEST_ASSERT_EQUAL(UD3TN_FAIL, validate_eid("ipn:1"));
	TEST_ASSERT_EQUAL(UD3TN_FAIL, validate_eid("ipn:1."));
	TEST_ASSERT_EQUAL(UD3TN_FAIL, validate_eid("ipn:1.0ABC"));
	TEST_ASSERT_EQUAL(UD3TN_FAIL, validate_eid("ipn:1ABC.0"));
	TEST_ASSERT_EQUAL(UD3TN_FAIL, validate_eid("ipn:-1.0"));
	TEST_ASSERT_EQUAL(UD3TN_FAIL, validate_eid("ipn:1.-1"));
	TEST_ASSERT_EQUAL(UD3TN_FAIL, validate_eid("ipn:"));
	TEST_ASSERT_EQUAL(UD3TN_FAIL, validate_eid("ipn"));
	TEST_ASSERT_EQUAL(UD3TN_FAIL, validate_eid("IPN:1.0"));
}

TEST(eid, validate_local_eid)
{
	TEST_ASSERT_EQUAL(UD3TN_FAIL, validate_local_eid("dtn:none"));
	TEST_ASSERT_EQUAL(UD3TN_OK, validate_local_eid("dtn://ud3tn.dtn/"));
	TEST_ASSERT_EQUAL(UD3TN_FAIL, validate_local_eid("dtn://ud3tn.dtn"));
	TEST_ASSERT_EQUAL(UD3TN_FAIL, validate_local_eid(
		"dtn://ud3tn.dtn/agent1"
	));
	TEST_ASSERT_EQUAL(UD3TN_OK, validate_local_eid("dtn://U/"));

	TEST_ASSERT_EQUAL(UD3TN_OK, validate_local_eid("ipn:0.0"));
	TEST_ASSERT_EQUAL(UD3TN_OK, validate_local_eid("ipn:1.0"));
	TEST_ASSERT_EQUAL(UD3TN_FAIL, validate_local_eid("ipn:0.1"));
	TEST_ASSERT_EQUAL(UD3TN_OK, validate_local_eid(
		"ipn:18446744073709551615.0"
	));
}

TEST(eid, get_eid_scheme)
{
	TEST_ASSERT_EQUAL(EID_SCHEME_DTN, get_eid_scheme("dtn:none"));
	TEST_ASSERT_EQUAL(EID_SCHEME_DTN, get_eid_scheme("dtn://ud3tn.dtn/"));

	TEST_ASSERT_EQUAL(EID_SCHEME_IPN, get_eid_scheme("ipn:1.0"));
	TEST_ASSERT_EQUAL(EID_SCHEME_IPN, get_eid_scheme("ipn:1"));

	TEST_ASSERT_EQUAL(EID_SCHEME_UNKNOWN, get_eid_scheme(""));
	TEST_ASSERT_EQUAL(EID_SCHEME_UNKNOWN, get_eid_scheme("dtn"));
	TEST_ASSERT_EQUAL(EID_SCHEME_UNKNOWN, get_eid_scheme("ipn"));
	TEST_ASSERT_EQUAL(EID_SCHEME_UNKNOWN, get_eid_scheme("DTN:"));
	TEST_ASSERT_EQUAL(EID_SCHEME_UNKNOWN, get_eid_scheme("IPN:"));
	TEST_ASSERT_EQUAL(EID_SCHEME_UNKNOWN, get_eid_scheme("http://123"));
}

TEST(eid, validate_ipn_eid)
{
	uint64_t node, service;

	TEST_ASSERT_EQUAL(UD3TN_OK, validate_ipn_eid(
		"ipn:1.0", &node, &service
	));
	TEST_ASSERT_EQUAL_UINT64(1, node);
	TEST_ASSERT_EQUAL_UINT64(0, service);
	TEST_ASSERT_EQUAL(UD3TN_OK, validate_ipn_eid(
		"ipn:18446744073709551615.18446744073709551615",
		&node, &service
	));
	TEST_ASSERT_EQUAL_UINT64(18446744073709551615ULL, node);
	TEST_ASSERT_EQUAL_UINT64(18446744073709551615ULL, service);
	node = service = 0;
	TEST_ASSERT_EQUAL(UD3TN_FAIL, validate_ipn_eid(
		"ipn:18446744073709551616.18446744073709551616",
		&node, &service
	));
	TEST_ASSERT_EQUAL_UINT64(0, node);
	TEST_ASSERT_EQUAL_UINT64(0, service);
}

TEST(eid, parse_ipn_ull)
{
	TEST_ASSERT_NULL(parse_ipn_ull(NULL, NULL));
	TEST_ASSERT_NULL(parse_ipn_ull("", NULL));
}

TEST_GROUP_RUNNER(eid)
{
	RUN_TEST_CASE(eid, validate_eid);
	RUN_TEST_CASE(eid, validate_local_eid);
	RUN_TEST_CASE(eid, get_eid_scheme);
	RUN_TEST_CASE(eid, validate_ipn_eid);
	RUN_TEST_CASE(eid, parse_ipn_ull);
}
