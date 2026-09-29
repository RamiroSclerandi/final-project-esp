// Pure timing helpers: rollover-safe deadlines, the WDT-sliced sampling delay,
// the transmit schedule, the clock-synced first-send gate, the provisioning
// input timeout and the MQTT/TLS timeout budget.
#include <unity.h>

#include "core/Deadline.h"
#include "core/ProvisioningTimeout.h"
#include "core/SamplingDelay.h"
#include "core/TransmitSchedule.h"
#include "transport/MqttTimeouts.h"

namespace
{
    constexpr uint32_t NEAR_ROLLOVER_MS = 0xFFFFFF00UL;
}

void setUp() {}
void tearDown() {}

void test_deadline_not_elapsed_before_duration(void)
{
    TEST_ASSERT_FALSE(Deadline::hasElapsed(1999, 1000, 1000));
    TEST_ASSERT_TRUE(Deadline::hasElapsed(2000, 1000, 1000));
}

void test_deadline_stays_correct_across_millis_rollover(void)
{
    // A naive `now < start + duration` wraps here and reports elapsed at once.
    const uint32_t start = NEAR_ROLLOVER_MS;

    TEST_ASSERT_FALSE(Deadline::hasElapsed(start + 999, start, 1000));
    TEST_ASSERT_TRUE(Deadline::hasElapsed(start + 1000, start, 1000));
    TEST_ASSERT_EQUAL_UINT32(1000, Deadline::elapsedMs(start + 1000, start));
}

void test_sampling_delay_slices_long_interval(void)
{
    TEST_ASSERT_EQUAL_UINT32(SamplingDelay::WDT_SLICE_MS,
                             SamplingDelay::nextChunkMs(0, 60000, SamplingDelay::WDT_SLICE_MS));
    TEST_ASSERT_EQUAL_UINT32(SamplingDelay::WDT_SLICE_MS,
                             SamplingDelay::nextChunkMs(50000, 60000, SamplingDelay::WDT_SLICE_MS));
    TEST_ASSERT_EQUAL_UINT32(2000,
                             SamplingDelay::nextChunkMs(58000, 60000, SamplingDelay::WDT_SLICE_MS));
    TEST_ASSERT_EQUAL_UINT32(0,
                             SamplingDelay::nextChunkMs(60000, 60000, SamplingDelay::WDT_SLICE_MS));
}

void test_sampling_delay_short_interval_is_one_chunk(void)
{
    TEST_ASSERT_EQUAL_UINT32(1000,
                             SamplingDelay::nextChunkMs(0, 1000, SamplingDelay::WDT_SLICE_MS));
}

void test_sampling_delay_follows_interval_shortened_mid_wait(void)
{
    // The interval is re-read per chunk: a remote change to 10 s during a
    // 60 s wait ends it at 10 s instead of sleeping out the old value.
    TEST_ASSERT_EQUAL_UINT32(0,
                             SamplingDelay::nextChunkMs(15000, 10000, SamplingDelay::WDT_SLICE_MS));
    TEST_ASSERT_EQUAL_UINT32(1000,
                             SamplingDelay::nextChunkMs(9000, 10000, SamplingDelay::WDT_SLICE_MS));
}

void test_effective_interval_caps_long_sampling_while_first_send_pending(void)
{
    // Without the cap, a 300 s sampling interval would delay the very first
    // send by up to 300 s instead of the intended ~30 s timeout.
    TEST_ASSERT_EQUAL_UINT32(30000, SamplingDelay::effectiveIntervalMs(300000, true, 30000));
}

void test_effective_interval_keeps_short_sampling_while_first_send_pending(void)
{
    // No cap needed: the sampling interval is already inside the timeout.
    TEST_ASSERT_EQUAL_UINT32(10000, SamplingDelay::effectiveIntervalMs(10000, true, 30000));
}

void test_effective_interval_ignores_cap_once_first_send_is_done(void)
{
    // The cap only applies while the first send is still pending; afterwards
    // the wait must return to the full configured sampling cadence.
    TEST_ASSERT_EQUAL_UINT32(300000, SamplingDelay::effectiveIntervalMs(300000, false, 30000));
}

void test_transmit_equal_to_sampling_sends_every_sample(void)
{
    // Raw mode: a sample taken a few ms early must not wait a whole extra cycle.
    TEST_ASSERT_TRUE(TransmitSchedule::isDue(10995, 6000, 5000, 5000));
    TEST_ASSERT_TRUE(TransmitSchedule::isDue(11000, 6000, 5000, 5000));
}

void test_transmit_different_from_sampling_keeps_own_cadence(void)
{
    TEST_ASSERT_FALSE(TransmitSchedule::isDue(59999, 0, 60000, 5000));
    TEST_ASSERT_TRUE(TransmitSchedule::isDue(60000, 0, 60000, 5000));
}

void test_first_send_waits_for_clock_sync(void)
{
    TEST_ASSERT_FALSE(TransmitSchedule::isFirstSendAllowed(false, 1000, 0));
    TEST_ASSERT_TRUE(TransmitSchedule::isFirstSendAllowed(true, 1000, 0));
}

void test_first_send_proceeds_after_sync_timeout(void)
{
    const uint32_t taskStart = NEAR_ROLLOVER_MS;

    TEST_ASSERT_FALSE(TransmitSchedule::isFirstSendAllowed(false, taskStart + 29999, taskStart));
    TEST_ASSERT_TRUE(TransmitSchedule::isFirstSendAllowed(false, taskStart + 30000, taskStart));
}

void test_provisioning_not_timed_out_before_the_input_timeout(void)
{
    TEST_ASSERT_FALSE(
        ProvisioningTimeout::hasTimedOut(ProvisioningTimeout::INPUT_TIMEOUT_MS - 1, 0));
}

void test_provisioning_timed_out_once_the_input_timeout_elapses(void)
{
    TEST_ASSERT_TRUE(ProvisioningTimeout::hasTimedOut(ProvisioningTimeout::INPUT_TIMEOUT_MS, 0));
}

void test_mqtt_timeouts_match_the_verified_seconds_values(void)
{
    // Verified against the pinned Arduino-ESP32 2.0.17 WiFiClientSecure source:
    // setHandshakeTimeout(unsigned long) takes SECONDS, not ms
    // (framework-arduinoespressif32/libraries/WiFiClientSecure/src/
    // WiFiClientSecure.cpp:369-372 multiplies the argument by 1000 itself).
    TEST_ASSERT_EQUAL_UINT32(10, MqttTimeouts::TLS_HANDSHAKE_TIMEOUT_S);

    // Verified against the pinned PubSubClient source: setSocketTimeout()
    // stores the value as-is and multiplies by 1000 at each use site
    // (.pio/libdeps/esp32dev/PubSubClient/src/PubSubClient.cpp:766-768,
    // used at lines 259 and 293), so its unit is SECONDS too.
    TEST_ASSERT_EQUAL_UINT32(5, MqttTimeouts::SOCKET_TIMEOUT_S);
}

void test_mqtt_timeout_budget_stays_well_under_the_task_watchdog(void)
{
    // 30 s WDT (main.cpp WDT_TIMEOUT_S); one poll must not eat the whole
    // budget or a single slow connect attempt panics the board.
    constexpr uint32_t WDT_TIMEOUT_S = 30;
    const uint32_t pollBudgetS =
        MqttTimeouts::TLS_HANDSHAKE_TIMEOUT_S + MqttTimeouts::SOCKET_TIMEOUT_S;

    TEST_ASSERT_LESS_THAN_UINT32(WDT_TIMEOUT_S, pollBudgetS);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_deadline_not_elapsed_before_duration);
    RUN_TEST(test_deadline_stays_correct_across_millis_rollover);
    RUN_TEST(test_sampling_delay_slices_long_interval);
    RUN_TEST(test_sampling_delay_short_interval_is_one_chunk);
    RUN_TEST(test_sampling_delay_follows_interval_shortened_mid_wait);
    RUN_TEST(test_effective_interval_caps_long_sampling_while_first_send_pending);
    RUN_TEST(test_effective_interval_keeps_short_sampling_while_first_send_pending);
    RUN_TEST(test_effective_interval_ignores_cap_once_first_send_is_done);
    RUN_TEST(test_transmit_equal_to_sampling_sends_every_sample);
    RUN_TEST(test_transmit_different_from_sampling_keeps_own_cadence);
    RUN_TEST(test_first_send_waits_for_clock_sync);
    RUN_TEST(test_first_send_proceeds_after_sync_timeout);
    RUN_TEST(test_provisioning_not_timed_out_before_the_input_timeout);
    RUN_TEST(test_provisioning_timed_out_once_the_input_timeout_elapses);
    RUN_TEST(test_mqtt_timeouts_match_the_verified_seconds_values);
    RUN_TEST(test_mqtt_timeout_budget_stays_well_under_the_task_watchdog);
    return UNITY_END();
}
