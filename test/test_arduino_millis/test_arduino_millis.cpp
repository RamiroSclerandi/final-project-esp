// Pins the host millis() shim to a fully virtual clock: real wall-clock time
// passing must never change its value, only nativeAdvanceMillis() can. The
// previous shim added real steady_clock elapsed time on top of the offset,
// which made exact-millis assertions in test_backoff flaky whenever the
// process was scheduled between a stamp and a read.
#include <unity.h>

#include <chrono>
#include <thread>

#include <Arduino.h>

void setUp() {}

void tearDown()
{
    nativeResetMillisOffset();
}

void test_millis_does_not_drift_with_real_time_after_advance(void)
{
    nativeAdvanceMillis(1000);
    const unsigned long stamped = millis();

    // Force the race that made backoff's exact-millis assertions flaky: real
    // time elapses here, between the stamp and the read.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    TEST_ASSERT_EQUAL_UINT32(stamped, millis());
}

void test_millis_reflects_cumulative_advances_exactly(void)
{
    nativeAdvanceMillis(500);
    nativeAdvanceMillis(250);

    TEST_ASSERT_EQUAL_UINT32(750, millis());
}

void test_reset_returns_millis_to_zero(void)
{
    nativeAdvanceMillis(4242);
    nativeResetMillisOffset();

    TEST_ASSERT_EQUAL_UINT32(0, millis());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_millis_does_not_drift_with_real_time_after_advance);
    RUN_TEST(test_millis_reflects_cumulative_advances_exactly);
    RUN_TEST(test_reset_returns_millis_to_zero);
    return UNITY_END();
}
