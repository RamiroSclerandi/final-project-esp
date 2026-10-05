// The offline-restart rule: when a prolonged outage reboots the node, and why
// it must follow the selected transport instead of always reading MQTT.
#include <unity.h>

#include "core/OfflineRestart.h"
#include "transport/LoRaWANTransport.h"

void setUp() {}
void tearDown() {}

void test_offline_restart_not_due_up_to_the_threshold(void)
{
    TEST_ASSERT_FALSE(OfflineRestart::isDue(0));
    TEST_ASSERT_FALSE(OfflineRestart::isDue(OfflineRestart::THRESHOLD_MS));
}

void test_offline_restart_due_past_the_threshold(void)
{
    TEST_ASSERT_TRUE(OfflineRestart::isDue(OfflineRestart::THRESHOLD_MS + 1));
}

void test_offline_restart_threshold_is_thirty_minutes(void)
{
    TEST_ASSERT_EQUAL_UINT32(30UL * 60UL * 1000UL, OfflineRestart::THRESHOLD_MS);
}

void test_lorawan_stub_never_reports_an_outage_a_restart_could_fix(void)
{
    // The radio stack does not exist yet, so a restart can never bring the
    // link up. Reading the MQTT clock instead (never connected under
    // LoRaWAN) rebooted the node every 30 min, resetting seq and meta.lost
    // and bumping meta.boot for nothing.
    LoRaWANTransport lora;
    TEST_ASSERT_EQUAL_UINT32(0, lora.millisOffline());
    TEST_ASSERT_FALSE(OfflineRestart::isDue(lora.millisOffline()));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_offline_restart_not_due_up_to_the_threshold);
    RUN_TEST(test_offline_restart_due_past_the_threshold);
    RUN_TEST(test_offline_restart_threshold_is_thirty_minutes);
    RUN_TEST(test_lorawan_stub_never_reports_an_outage_a_restart_could_fix);
    return UNITY_END();
}
