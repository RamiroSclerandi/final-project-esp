// Pure timing helpers: rollover-safe deadlines, the WDT-sliced sampling delay,
// the transmit schedule, the clock-synced first-send gate, the provisioning
// input timeout and the MQTT/TLS connect stages and their watchdog budget.
#include <string>
#include <unity.h>

#include "core/Deadline.h"
#include "core/ProvisioningTimeout.h"
#include "core/SamplingDelay.h"
#include "core/TransmitSchedule.h"
#include "core/WatchdogConfig.h"
#include "transport/ConnectSequence.h"
#include "transport/MqttTimeouts.h"

namespace
{
    constexpr uint32_t NEAR_ROLLOVER_MS = 0xFFFFFF00UL;

    char stageLetter(ConnectSequence::Stage stage)
    {
        switch (stage)
        {
        case ConnectSequence::Stage::ResolveHost:
            return 'R';
        case ConnectSequence::Stage::OpenTls:
            return 'T';
        case ConnectSequence::Stage::MqttHandshake:
            return 'M';
        case ConnectSequence::Stage::Announce:
            return 'A';
        }
        return '?';
    }
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

void test_activity_timeout_not_elapsed_before_the_input_timeout(void)
{
    ProvisioningTimeout::ActivityTimeout timeout(0);
    TEST_ASSERT_FALSE(timeout.hasTimedOut(ProvisioningTimeout::INPUT_TIMEOUT_MS - 1));
}

void test_activity_timeout_elapses_with_no_activity(void)
{
    ProvisioningTimeout::ActivityTimeout timeout(0);
    TEST_ASSERT_TRUE(timeout.hasTimedOut(ProvisioningTimeout::INPUT_TIMEOUT_MS));
}

void test_activity_resets_the_timeout_window(void)
{
    // readNumber's bug: an operator typing one digit every ~55 s never times
    // out mid-entry, because each keystroke is activity — only silence for
    // the full INPUT_TIMEOUT_MS should cut the wait short.
    ProvisioningTimeout::ActivityTimeout timeout(0);

    const uint32_t keystroke = ProvisioningTimeout::INPUT_TIMEOUT_MS - 1000;
    timeout.noteActivity(keystroke);

    // Elapsed since the ORIGINAL start now exceeds INPUT_TIMEOUT_MS, but
    // only 999 ms have passed since the keystroke: must not be timed out.
    TEST_ASSERT_FALSE(timeout.hasTimedOut(keystroke + 999));

    // Now a full INPUT_TIMEOUT_MS of silence has elapsed since that keystroke.
    TEST_ASSERT_TRUE(timeout.hasTimedOut(keystroke + ProvisioningTimeout::INPUT_TIMEOUT_MS));
}

void test_accepted_input_char_matches_digits_and_terminators(void)
{
    TEST_ASSERT_TRUE(ProvisioningTimeout::isAcceptedInputChar('0'));
    TEST_ASSERT_TRUE(ProvisioningTimeout::isAcceptedInputChar('9'));
    TEST_ASSERT_TRUE(ProvisioningTimeout::isAcceptedInputChar('\r'));
    TEST_ASSERT_TRUE(ProvisioningTimeout::isAcceptedInputChar('\n'));
}

void test_accepted_input_char_rejects_noise_bytes(void)
{
    TEST_ASSERT_FALSE(ProvisioningTimeout::isAcceptedInputChar('a'));
    TEST_ASSERT_FALSE(ProvisioningTimeout::isAcceptedInputChar(' '));
    TEST_ASSERT_FALSE(ProvisioningTimeout::isAcceptedInputChar(0x07));
}

void test_accepted_line_char_matches_printable_editing_and_terminators(void)
{
    // readLine() takes free text (SSIDs, hosts, passwords), so every byte it
    // would store or act on counts as an operator still typing.
    TEST_ASSERT_TRUE(ProvisioningTimeout::isAcceptedLineChar('a'));
    TEST_ASSERT_TRUE(ProvisioningTimeout::isAcceptedLineChar(' '));
    TEST_ASSERT_TRUE(ProvisioningTimeout::isAcceptedLineChar('~'));
    TEST_ASSERT_TRUE(ProvisioningTimeout::isAcceptedLineChar('7'));
    TEST_ASSERT_TRUE(ProvisioningTimeout::isAcceptedLineChar(8));   // backspace
    TEST_ASSERT_TRUE(ProvisioningTimeout::isAcceptedLineChar(127)); // delete
    TEST_ASSERT_TRUE(ProvisioningTimeout::isAcceptedLineChar('\r'));
    TEST_ASSERT_TRUE(ProvisioningTimeout::isAcceptedLineChar('\n'));
}

void test_accepted_line_char_rejects_control_and_non_ascii_bytes(void)
{
    // Bytes readLine() discards must not extend the wait, or RX noise could
    // hold a credential prompt open until the hard cap.
    TEST_ASSERT_FALSE(ProvisioningTimeout::isAcceptedLineChar(0x00));
    TEST_ASSERT_FALSE(ProvisioningTimeout::isAcceptedLineChar(0x07));
    TEST_ASSERT_FALSE(ProvisioningTimeout::isAcceptedLineChar(0x1B));
    TEST_ASSERT_FALSE(ProvisioningTimeout::isAcceptedLineChar(0x80));
    TEST_ASSERT_FALSE(ProvisioningTimeout::isAcceptedLineChar(0xFF));
    TEST_ASSERT_FALSE(ProvisioningTimeout::isAcceptedLineChar(-1)); // Serial.read(): no data
}

void test_read_wait_step_times_out_a_line_left_mid_entry(void)
{
    // An operator who opens a credential prompt, types a few characters and
    // walks away: readLine() must give up INPUT_TIMEOUT_MS after the last
    // keystroke instead of blocking setup() forever.
    ProvisioningTimeout::ActivityTimeout timeout(0);
    const uint32_t lastKeystroke = 2000;
    timeout.noteActivity(lastKeystroke);

    TEST_ASSERT_EQUAL(
        ProvisioningTimeout::ReadWaitStep::Idle,
        ProvisioningTimeout::nextReadWaitStep(
            timeout, lastKeystroke + ProvisioningTimeout::INPUT_TIMEOUT_MS - 1, false));
    TEST_ASSERT_EQUAL(ProvisioningTimeout::ReadWaitStep::TimedOut,
                      ProvisioningTimeout::nextReadWaitStep(
                          timeout, lastKeystroke + ProvisioningTimeout::INPUT_TIMEOUT_MS, false));
}

void test_activity_timeout_hard_cap_wins_despite_recent_activity(void)
{
    // Continuous accepted input (e.g. a digit flood) must not be able to
    // keep readNumber() waiting forever: HARD_CAP_MS bounds the whole call
    // regardless of how recently activity was seen.
    ProvisioningTimeout::ActivityTimeout timeout(0);

    const uint32_t justBeforeCap = ProvisioningTimeout::HARD_CAP_MS - 1;
    timeout.noteActivity(justBeforeCap);

    // Only 1 ms of silence since the last keystroke: not timed out on that
    // basis alone, but HARD_CAP_MS has now elapsed since the call started.
    TEST_ASSERT_FALSE(timeout.hasTimedOut(justBeforeCap));
    TEST_ASSERT_TRUE(timeout.hasTimedOut(ProvisioningTimeout::HARD_CAP_MS));
}

void test_read_wait_step_cap_fires_under_a_continuous_byte_stream(void)
{
    // Simulates readNumber()'s wait loop fed an uninterrupted flood of
    // accepted bytes (byteAvailable=true on every single iteration, each
    // one restarting the activity window): the old wiring only evaluated
    // the timeout when no byte was available, so a continuous stream could
    // never trip HARD_CAP_MS. The step decision must check it regardless.
    ProvisioningTimeout::ActivityTimeout timeout(0);

    for (uint32_t nowMs = 0; nowMs < ProvisioningTimeout::HARD_CAP_MS; nowMs += 1000)
    {
        timeout.noteActivity(nowMs);
        TEST_ASSERT_EQUAL(ProvisioningTimeout::ReadWaitStep::HasByte,
                          ProvisioningTimeout::nextReadWaitStep(timeout, nowMs, true));
    }

    TEST_ASSERT_EQUAL(
        ProvisioningTimeout::ReadWaitStep::TimedOut,
        ProvisioningTimeout::nextReadWaitStep(timeout, ProvisioningTimeout::HARD_CAP_MS, true));
}

void test_read_wait_step_is_idle_when_no_byte_and_not_timed_out(void)
{
    ProvisioningTimeout::ActivityTimeout timeout(0);
    TEST_ASSERT_EQUAL(ProvisioningTimeout::ReadWaitStep::Idle,
                      ProvisioningTimeout::nextReadWaitStep(timeout, 100, false));
}

void test_mqtt_timeouts_match_the_verified_seconds_values(void)
{
    // Verified against the pinned Arduino-ESP32 2.0.17 WiFiClientSecure source:
    // WiFiClientSecure::setTimeout(uint32_t seconds) bounds the raw TCP
    // connect() phase (WiFiClientSecure.cpp:378-389, `_timeout = seconds *
    // 1000`); without it the constructor's default `_timeout = 30000`
    // (WiFiClientSecure.cpp:35) applies for the full 30 s, and that value
    // reaches lwip_connect()'s select() timeout unchanged
    // (ssl_client.cpp:84-95). PubSubClient::connect() calls the 2-arg
    // connect(host, port) overload that reads `_timeout`
    // (.pio/libdeps/esp32dev/PubSubClient/src/PubSubClient.cpp:190).
    TEST_ASSERT_EQUAL_UINT32(5, MqttTimeouts::TCP_CONNECT_TIMEOUT_S);

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

void test_dns_bound_matches_the_framework_wait(void)
{
    // WiFiClientSecure::connect(host, ...) resolves through
    // WiFiGenericClass::hostByName(), which waits up to 15000 ms for lwIP
    // (pinned Arduino-ESP32 2.0.17, WiFiGeneric.cpp:1578). It is not
    // configurable, so the connect budget has to account for it.
    TEST_ASSERT_EQUAL_UINT32(15, MqttTimeouts::DNS_RESOLVE_MAX_S);
}

void test_connect_sequence_feeds_the_watchdog_before_every_stage(void)
{
    std::string trace;
    const bool connected = ConnectSequence::run(
        [&trace](ConnectSequence::Stage stage) {
            trace += stageLetter(stage);
            return true;
        },
        [&trace]() { trace += 'F'; });

    TEST_ASSERT_TRUE(connected);
    TEST_ASSERT_EQUAL_STRING("FRFTFMFA", trace.c_str());
}

void test_connect_sequence_stops_at_the_first_failing_stage(void)
{
    std::string trace;
    const bool connected = ConnectSequence::run(
        [&trace](ConnectSequence::Stage stage) {
            trace += stageLetter(stage);
            return stage != ConnectSequence::Stage::OpenTls;
        },
        [&trace]() { trace += 'F'; });

    TEST_ASSERT_FALSE(connected);
    TEST_ASSERT_EQUAL_STRING("FRFT", trace.c_str());
}

void test_an_unsplit_connect_attempt_would_outlast_the_watchdog(void)
{
    // Why the attempt is split: DNS + TCP + TLS + CONNECT/CONNACK + status
    // publish/subscribe, run back to back with a single feed, can block
    // longer than the task watchdog and panic the board on a bad network.
    uint32_t totalS = 0;
    for (const ConnectSequence::Stage stage : ConnectSequence::STAGES)
    {
        totalS += ConnectSequence::worstCaseS(stage);
    }
    TEST_ASSERT_GREATER_THAN_UINT32(WatchdogConfig::TIMEOUT_S, totalS);
}

void test_every_connect_stage_leaves_half_the_watchdog_as_margin(void)
{
    // Each stage runs between two feeds, so only a single stage has to fit
    // under the watchdog. Half of it stays free for what the timeouts do not
    // bound (TLS certificate crypto, the networkTask work around the attempt).
    for (const ConnectSequence::Stage stage : ConnectSequence::STAGES)
    {
        TEST_ASSERT_LESS_OR_EQUAL_UINT32(WatchdogConfig::TIMEOUT_S / 2,
                                         ConnectSequence::worstCaseS(stage));
    }
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
    RUN_TEST(test_activity_timeout_not_elapsed_before_the_input_timeout);
    RUN_TEST(test_activity_timeout_elapses_with_no_activity);
    RUN_TEST(test_activity_resets_the_timeout_window);
    RUN_TEST(test_accepted_input_char_matches_digits_and_terminators);
    RUN_TEST(test_accepted_input_char_rejects_noise_bytes);
    RUN_TEST(test_accepted_line_char_matches_printable_editing_and_terminators);
    RUN_TEST(test_accepted_line_char_rejects_control_and_non_ascii_bytes);
    RUN_TEST(test_read_wait_step_times_out_a_line_left_mid_entry);
    RUN_TEST(test_activity_timeout_hard_cap_wins_despite_recent_activity);
    RUN_TEST(test_read_wait_step_cap_fires_under_a_continuous_byte_stream);
    RUN_TEST(test_read_wait_step_is_idle_when_no_byte_and_not_timed_out);
    RUN_TEST(test_mqtt_timeouts_match_the_verified_seconds_values);
    RUN_TEST(test_dns_bound_matches_the_framework_wait);
    RUN_TEST(test_connect_sequence_feeds_the_watchdog_before_every_stage);
    RUN_TEST(test_connect_sequence_stops_at_the_first_failing_stage);
    RUN_TEST(test_an_unsplit_connect_attempt_would_outlast_the_watchdog);
    RUN_TEST(test_every_connect_stage_leaves_half_the_watchdog_as_margin);
    return UNITY_END();
}
