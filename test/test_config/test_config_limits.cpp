// Shared sampling-interval range validation used by both the MQTT remote
// config handler and the serial provisioning menu, so the two paths cannot
// drift apart and one of them ends up accepting an out-of-range value.
#include <unity.h>

#include "core/ConfigLimits.h"

void setUp() {}
void tearDown() {}

void test_accepts_the_boundaries(void)
{
    TEST_ASSERT_TRUE(ConfigLimits::isValidSamplingIntervalMs(1000));
    TEST_ASSERT_TRUE(ConfigLimits::isValidSamplingIntervalMs(300000));
}

void test_rejects_just_outside_the_boundaries(void)
{
    TEST_ASSERT_FALSE(ConfigLimits::isValidSamplingIntervalMs(999));
    TEST_ASSERT_FALSE(ConfigLimits::isValidSamplingIntervalMs(300001));
}

void test_rejects_the_reported_field_bug(void)
{
    // The value the serial menu stored unchecked before ConfigLimits was
    // wired into it (task 3.24); kept here as a regression guard.
    TEST_ASSERT_FALSE(ConfigLimits::isValidSamplingIntervalMs(500));
}

void test_clamp_leaves_an_in_range_value_unchanged(void)
{
    TEST_ASSERT_EQUAL_UINT32(60000, ConfigLimits::clampSamplingIntervalMs(60000));
}

void test_clamp_repairs_a_value_below_the_minimum(void)
{
    // A value already sitting in NVS from before validation existed (e.g.
    // the 500 ms field bug above) must be repaired on load, not just
    // rejected on the next write.
    TEST_ASSERT_EQUAL_UINT32(ConfigLimits::SAMPLING_INTERVAL_MIN_MS,
                             ConfigLimits::clampSamplingIntervalMs(500));
}

void test_clamp_repairs_a_value_above_the_maximum(void)
{
    TEST_ASSERT_EQUAL_UINT32(ConfigLimits::SAMPLING_INTERVAL_MAX_MS,
                             ConfigLimits::clampSamplingIntervalMs(1000000));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_accepts_the_boundaries);
    RUN_TEST(test_rejects_just_outside_the_boundaries);
    RUN_TEST(test_rejects_the_reported_field_bug);
    RUN_TEST(test_clamp_leaves_an_in_range_value_unchanged);
    RUN_TEST(test_clamp_repairs_a_value_below_the_minimum);
    RUN_TEST(test_clamp_repairs_a_value_above_the_maximum);
    return UNITY_END();
}
