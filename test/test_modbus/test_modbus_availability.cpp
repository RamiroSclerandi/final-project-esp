// Detects when the stored NVS config expects the Modbus meter but this build
// excluded it (DL_ENABLE_MODBUS=0) — a silent compile-out review finding:
// before this, a device provisioned with the meter enabled would just never
// register it, with no boot warning and no menu indication of why.
#include <unity.h>

#include "core/ModbusAvailability.h"

void setUp() {}
void tearDown() {}

void test_not_configured_is_never_a_mismatch(void)
{
    TEST_ASSERT_FALSE(ModbusAvailability::isConfiguredButExcluded(false, false));
    TEST_ASSERT_FALSE(ModbusAvailability::isConfiguredButExcluded(true, false));
}

void test_configured_and_compiled_in_is_not_a_mismatch(void)
{
    TEST_ASSERT_FALSE(ModbusAvailability::isConfiguredButExcluded(true, true));
}

void test_configured_but_compiled_out_is_a_mismatch(void)
{
    TEST_ASSERT_TRUE(ModbusAvailability::isConfiguredButExcluded(false, true));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_not_configured_is_never_a_mismatch);
    RUN_TEST(test_configured_and_compiled_in_is_not_a_mismatch);
    RUN_TEST(test_configured_but_compiled_out_is_a_mismatch);
    return UNITY_END();
}
