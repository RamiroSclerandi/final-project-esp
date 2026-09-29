// G-10: classifies what a sensorTask send attempt did, so main.cpp can
// decide whether to advance `seq`/clear pendingResetReason (an emission) or
// count the drop in meta.lost (a pre-emission loss) without burying that
// decision in the task loop itself.
#include <unity.h>

#include "core/EmissionOutcome.h"

void setUp() {}
void tearDown() {}

void test_encode_failure_is_not_emitted(void)
{
    TEST_ASSERT_FALSE(EmissionOutcome::isEmitted(EmissionOutcome::SendAttempt::EncodeFailed));
}

void test_oversize_for_transport_is_not_emitted(void)
{
    TEST_ASSERT_FALSE(
        EmissionOutcome::isEmitted(EmissionOutcome::SendAttempt::OversizeForTransport));
}

void test_queue_full_and_buffer_failed_is_not_emitted(void)
{
    TEST_ASSERT_FALSE(
        EmissionOutcome::isEmitted(EmissionOutcome::SendAttempt::QueueFullAndBufferFailed));
}

void test_queued_ok_is_emitted(void)
{
    TEST_ASSERT_TRUE(EmissionOutcome::isEmitted(EmissionOutcome::SendAttempt::QueuedOk));
}

void test_buffered_ok_is_emitted(void)
{
    // Handing a reading to the local buffer counts as emission too: the
    // record now has a seq gap on loss, not a silent pre-emission drop.
    TEST_ASSERT_TRUE(EmissionOutcome::isEmitted(EmissionOutcome::SendAttempt::BufferedOk));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_encode_failure_is_not_emitted);
    RUN_TEST(test_oversize_for_transport_is_not_emitted);
    RUN_TEST(test_queue_full_and_buffer_failed_is_not_emitted);
    RUN_TEST(test_queued_ok_is_emitted);
    RUN_TEST(test_buffered_ok_is_emitted);
    return UNITY_END();
}
