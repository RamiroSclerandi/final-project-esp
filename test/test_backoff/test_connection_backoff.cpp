// Characterizes ConnectionBackoff (v1.1.0): retry timing, jitter, and the
// boot-stamped elapsed-since-success clock that drives the offline reboot.
#include <unity.h>

#include <Arduino.h>

#include "transport/ConnectionBackoff.h"

void setUp() {}

void tearDown()
{
    nativeResetMillisOffset();
}

void test_never_connected_reports_real_elapsed_since_boot(void)
{
    // A device that never connects must still see its offline time grow, so
    // the 30-min reboot threshold is reachable. The old `_lastSuccessAt == 0`
    // sentinel made this always read 0 and the device never rebooted.
    ConnectionBackoff backoff;

    nativeAdvanceMillis(5000);
    TEST_ASSERT_EQUAL_UINT32(5000, backoff.millisSinceSuccess());

    nativeAdvanceMillis(30UL * 60UL * 1000UL - 5000);
    TEST_ASSERT_EQUAL_UINT32(30UL * 60UL * 1000UL, backoff.millisSinceSuccess());
}

void test_should_retry_immediately_at_boot(void)
{
    ConnectionBackoff backoff;

    TEST_ASSERT_TRUE(backoff.shouldRetry());
}

void test_record_failure_increments_failures_and_delays_retry(void)
{
    ConnectionBackoff backoff;

    backoff.recordFailure();

    TEST_ASSERT_EQUAL_UINT32(1, backoff.consecutiveFailures());
    // INITIAL_DELAY_MS (1000ms) minus jitter is still far longer than this
    // assertion takes to run, so the next attempt is not due yet.
    TEST_ASSERT_FALSE(backoff.shouldRetry());
}

void test_record_success_resets_delay_and_failures(void)
{
    ConnectionBackoff backoff;
    backoff.recordFailure();
    backoff.recordFailure();
    TEST_ASSERT_EQUAL_UINT32(2, backoff.consecutiveFailures());

    backoff.recordSuccess();

    TEST_ASSERT_EQUAL_UINT32(0, backoff.consecutiveFailures());
    TEST_ASSERT_TRUE(backoff.shouldRetry());
}

void test_record_success_resets_elapsed_then_tracks_forward(void)
{
    ConnectionBackoff backoff;
    nativeAdvanceMillis(12345);

    backoff.recordSuccess();
    TEST_ASSERT_EQUAL_UINT32(0, backoff.millisSinceSuccess());

    nativeAdvanceMillis(750);
    TEST_ASSERT_EQUAL_UINT32(750, backoff.millisSinceSuccess());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_never_connected_reports_real_elapsed_since_boot);
    RUN_TEST(test_should_retry_immediately_at_boot);
    RUN_TEST(test_record_failure_increments_failures_and_delays_retry);
    RUN_TEST(test_record_success_resets_delay_and_failures);
    RUN_TEST(test_record_success_resets_elapsed_then_tracks_forward);
    return UNITY_END();
}
