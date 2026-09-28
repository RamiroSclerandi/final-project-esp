#pragma once

#include <stdint.h>

/**
 * @brief Single source for the task watchdog timeout, shared by main.cpp and
 *        the native tests that must stay under it (e.g. the MQTT connect
 *        budget in test_timing.cpp) — keeping one copy means the two cannot
 *        drift apart.
 */
namespace WatchdogConfig
{
    // Must comfortably exceed the longest legitimate blocking operation,
    // which is the WiFi association plus the TLS handshake (worst case
    // TCP connect + TLS handshake + socket write, see MqttTimeouts.h). A
    // watchdog that fires on a healthy system is worse than none: it
    // produces a reboot loop that looks exactly like the fault it was
    // meant to catch.
    constexpr uint32_t TIMEOUT_S = 30;
}
