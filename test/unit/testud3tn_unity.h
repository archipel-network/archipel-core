// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#ifndef TESTUD3TN_UNITY_H_
#define TESTUD3TN_UNITY_H_

// Prevent the Unity malloc() overrides from being included. We want to use the
// normal malloc() and free() functions in our test and Unity should not mess
// with them.
#define UNITY_FIXTURE_MALLOC_OVERRIDES_H_

#include "unity_fixture.h"

#endif // TESTUD3TN_UNITY_H_
