// Characterizes the CURRENT behavior of ConnectionBackoff (v1.1.0),
// including the known `_lastSuccessAt == 0` sentinel ambiguity fixed later
// in this change (PR3): a device that never connected and a device that
// just connected both report millisSinceSuccess() == 0. No production
// logic changes in this PR — see design's "Backoff" decision.
#include <unity.h>

#include "transport/ConnectionBackoff.h"

void setUp() {}
void tearDown() {}

void test_never_connected_reports_zero_elapsed(void)
{
    ConnectionBackoff backoff;

    // This is the sentinel bug this change fixes in PR3: 0 here means "never
    // connected", but a device that just connected also reads 0.
    TEST_ASSERT_EQUAL_UINT32(0, backoff.millisSinceSuccess());
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

void test_record_success_sets_last_success_at_near_now(void)
{
    ConnectionBackoff backoff;

    backoff.recordSuccess();

    // Generous tolerance: this only guards against gross regressions
    // (e.g. forgetting to update _lastSuccessAt), not exact timing.
    TEST_ASSERT_UINT32_WITHIN(50, 0, backoff.millisSinceSuccess());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_never_connected_reports_zero_elapsed);
    RUN_TEST(test_should_retry_immediately_at_boot);
    RUN_TEST(test_record_failure_increments_failures_and_delays_retry);
    RUN_TEST(test_record_success_resets_delay_and_failures);
    RUN_TEST(test_record_success_sets_last_success_at_near_now);
    return UNITY_END();
}
