// Fake DeviceInfo for env:native. The real src/core/DeviceInfo.cpp reads the
// efuse MAC and SNTP state from the ESP32 SDK, neither of which exists on
// host — this returns fixed, deterministic values instead so codec goldens
// stay byte-stable across runs.
#include "core/DeviceInfo.h"

namespace
{
    constexpr char FAKE_DEVICE_ID[] = "AABBCCDDEEFF";

    // Matches src/core/DeviceInfo.cpp's PLAUSIBLE_EPOCH_FLOOR reference date
    // (2023-11-14) so fixture timestamps read as a real sync, not a boot clock.
    constexpr uint32_t FAKE_EPOCH = 1700000000UL;
}

const char *DeviceInfo::deviceId()
{
    return FAKE_DEVICE_ID;
}

void DeviceInfo::beginTimeSync(const char *)
{
    // No-op on host: there is no NTP server to reach.
}

bool DeviceInfo::isClockSynced()
{
    return true;
}

uint32_t DeviceInfo::epochNow()
{
    return FAKE_EPOCH;
}

void DeviceInfo::serviceTimeSync(bool)
{
    // No-op on host.
}

const char *DeviceInfo::resetReason()
{
    return "poweron";
}
