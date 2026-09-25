#include "core/DeviceInfo.h"

#include <Arduino.h>
#include <esp_system.h>

namespace
{
    // Any epoch beyond this is necessarily the result of a real sync; the ESP32
    // clock starts at 0 (1970) after a cold boot. 2023-11-14.
    constexpr uint32_t PLAUSIBLE_EPOCH_FLOOR = 1700000000UL;

    char g_deviceId[13] = {0};

    constexpr uint32_t SYNC_RETRY_MS = 30000;
    constexpr uint32_t RESYNC_INTERVAL_MS = 6UL * 60UL * 60UL * 1000UL;

    uint32_t g_nextSyncAttempt = 0;
    bool g_syncRequested = false;
}

const char *DeviceInfo::deviceId()
{
    if (g_deviceId[0] == '\0')
    {
        // getEfuseMac() returns the 48-bit MAC byte-reversed relative to the way
        // it is printed on the board, so swap it back for a readable identifier.
        uint64_t mac = ESP.getEfuseMac();
        snprintf(g_deviceId, sizeof(g_deviceId), "%02X%02X%02X%02X%02X%02X",
                 (uint8_t)(mac >> 0), (uint8_t)(mac >> 8), (uint8_t)(mac >> 16),
                 (uint8_t)(mac >> 24), (uint8_t)(mac >> 32), (uint8_t)(mac >> 40));
    }

    return g_deviceId;
}

void DeviceInfo::beginTimeSync(const char *ntpServer)
{
    // UTC: offsets belong in the presentation layer, not in stored data.
    configTime(0, 0, ntpServer);
    Serial.printf("[Time] SNTP sync requested from %s.\n", ntpServer);
}

bool DeviceInfo::isClockSynced()
{
    return (uint32_t)time(nullptr) >= PLAUSIBLE_EPOCH_FLOOR;
}

uint32_t DeviceInfo::epochNow()
{
    uint32_t now = (uint32_t)time(nullptr);
    return now >= PLAUSIBLE_EPOCH_FLOOR ? now : 0;
}

void DeviceInfo::serviceTimeSync(bool transportReady)
{
    if (!transportReady)
    {
        return;
    }

    const uint32_t now = millis();
    if ((int32_t)(now - g_nextSyncAttempt) < 0)
    {
        return;
    }

    if (isClockSynced())
    {
        // Already set: schedule the next drift correction instead of retrying.
        g_nextSyncAttempt = now + RESYNC_INTERVAL_MS;
        g_syncRequested = false;
        return;
    }

    if (!g_syncRequested)
    {
        beginTimeSync();
        g_syncRequested = true;
    }

    g_nextSyncAttempt = now + SYNC_RETRY_MS;
}

const char *DeviceInfo::resetReason()
{
    switch (esp_reset_reason())
    {
    case ESP_RST_POWERON:
        return "poweron";
    case ESP_RST_SW:
        return "software";
    case ESP_RST_PANIC:
        return "panic";
    case ESP_RST_TASK_WDT:
        return "task_wdt";
    case ESP_RST_INT_WDT:
        return "int_wdt";
    case ESP_RST_WDT:
        return "wdt";
    case ESP_RST_BROWNOUT:
        return "brownout";
    case ESP_RST_DEEPSLEEP:
        return "deepsleep";
    case ESP_RST_EXT:
        return "external";
    default:
        return "unknown";
    }
}
