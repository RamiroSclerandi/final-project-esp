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
    // The exact value the serial menu currently stores unchecked.
    TEST_ASSERT_FALSE(ConfigLimits::isValidSamplingIntervalMs(500));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_accepts_the_boundaries);
    RUN_TEST(test_rejects_just_outside_the_boundaries);
    RUN_TEST(test_rejects_the_reported_field_bug);
    return UNITY_END();
}
