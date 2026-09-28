// G-10: classifies what a sensorTask send attempt did, so main.cpp can
// decide whether to advance `seq`/clear pendingResetReason (an emission) or
// count the drop in meta.lost (a pre-emission loss) without burying that
// decision in the task loop itself.
//
// Also proves the full seq/lost/store.drop/meta.rst wiring end to end
// (EmissionLedger + LittleFsBuffer::recordEmittedLoss) against a sequence of
// outcomes, so a regression like re-advancing seq on a pre-emission drop, or
// clearing the reset reason before a payload is actually emitted, fails a
// test instead of only showing up on hardware (closes review R3).
#include <unity.h>

#include <LittleFS.h>

#include "core/EmissionLedger.h"
#include "core/EmissionOutcome.h"
#include "storage/LittleFsBuffer.h"

void setUp()
{
    LittleFS.format();
}
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

void test_ledger_starts_at_zero_with_the_boot_reset_reason(void)
{
    EmissionLedger ledger;
    ledger.reset("power_on");

    TEST_ASSERT_EQUAL_UINT32(0, ledger.sequence());
    TEST_ASSERT_EQUAL_UINT32(0, ledger.lostCount());
    TEST_ASSERT_EQUAL_STRING("power_on", ledger.pendingResetReason());
}

void test_pre_emission_drop_increments_lost_without_a_seq_gap(void)
{
    EmissionLedger ledger;
    ledger.reset("power_on");

    ledger.recordSendAttempt(EmissionOutcome::SendAttempt::EncodeFailed);
    ledger.recordSendAttempt(EmissionOutcome::SendAttempt::OversizeForTransport);
    ledger.recordSendAttempt(EmissionOutcome::SendAttempt::QueueFullAndBufferFailed);

    TEST_ASSERT_EQUAL_UINT32(0, ledger.sequence());
    TEST_ASSERT_EQUAL_UINT32(3, ledger.lostCount());
    // Never emitted, so the reset reason is still pending for the next attempt.
    TEST_ASSERT_EQUAL_STRING("power_on", ledger.pendingResetReason());
}

void test_emission_advances_seq_and_clears_the_reset_reason(void)
{
    EmissionLedger ledger;
    ledger.reset("power_on");

    ledger.recordSendAttempt(EmissionOutcome::SendAttempt::QueuedOk);

    TEST_ASSERT_EQUAL_UINT32(1, ledger.sequence());
    TEST_ASSERT_EQUAL_UINT32(0, ledger.lostCount());
    TEST_ASSERT_NULL(ledger.pendingResetReason());
}

void test_full_sequence_proves_seq_lost_store_drop_and_reset_reason(void)
{
    // The exact scenario the review flagged: a mix of pre-emission drops
    // (lost, no gap), a real emission (seq advances, resetReason clears),
    // and a post-emission networkTask double failure (seq already stamped,
    // so it lands in store.drop instead of lost — never re-opens lost).
    EmissionLedger ledger;
    LittleFsBuffer buffer;
    buffer.begin();
    ledger.reset("task_wdt");

    ledger.recordSendAttempt(EmissionOutcome::SendAttempt::EncodeFailed);
    ledger.recordSendAttempt(EmissionOutcome::SendAttempt::OversizeForTransport);
    ledger.recordSendAttempt(EmissionOutcome::SendAttempt::QueueFullAndBufferFailed);
    TEST_ASSERT_EQUAL_UINT32(0, ledger.sequence());
    TEST_ASSERT_EQUAL_UINT32(3, ledger.lostCount());
    TEST_ASSERT_EQUAL_STRING("task_wdt", ledger.pendingResetReason());

    // sensorTask hands this one to the queue: seq stamped, reason consumed.
    ledger.recordSendAttempt(EmissionOutcome::SendAttempt::QueuedOk);
    TEST_ASSERT_EQUAL_UINT32(1, ledger.sequence());
    TEST_ASSERT_NULL(ledger.pendingResetReason());

    // networkTask: send fails AND its own re-append fails. seq was already
    // stamped above, so this is store.drop — lostCount must NOT change.
    buffer.recordEmittedLoss();
    TEST_ASSERT_EQUAL_UINT32(1, buffer.droppedCount());
    TEST_ASSERT_EQUAL_UINT32(3, ledger.lostCount());

    // A second emission, this time buffered locally (still an emission).
    ledger.recordSendAttempt(EmissionOutcome::SendAttempt::BufferedOk);

    TEST_ASSERT_EQUAL_UINT32(2, ledger.sequence());
    TEST_ASSERT_EQUAL_UINT32(3, ledger.lostCount());
    TEST_ASSERT_EQUAL_UINT32(1, buffer.droppedCount());
    TEST_ASSERT_NULL(ledger.pendingResetReason());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_encode_failure_is_not_emitted);
    RUN_TEST(test_oversize_for_transport_is_not_emitted);
    RUN_TEST(test_queue_full_and_buffer_failed_is_not_emitted);
    RUN_TEST(test_queued_ok_is_emitted);
    RUN_TEST(test_buffered_ok_is_emitted);
    RUN_TEST(test_ledger_starts_at_zero_with_the_boot_reset_reason);
    RUN_TEST(test_pre_emission_drop_increments_lost_without_a_seq_gap);
    RUN_TEST(test_emission_advances_seq_and_clears_the_reset_reason);
    RUN_TEST(test_full_sequence_proves_seq_lost_store_drop_and_reset_reason);
    return UNITY_END();
}
