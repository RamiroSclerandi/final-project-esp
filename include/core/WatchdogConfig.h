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
    // Must comfortably exceed the longest legitimate blocking stretch
    // between two feeds. For networkTask that is one MQTT connect stage
    // (see ConnectSequence.h: the attempt is fed before each stage, because
    // the whole attempt, DNS included, can outlast this timeout). A
    // watchdog that fires on a healthy system is worse than none: it
    // produces a reboot loop that looks exactly like the fault it was
    // meant to catch.
    constexpr uint32_t TIMEOUT_S = 30;
}
