#pragma once

#include <stdint.h>
#include <time.h>

/**
 * @brief Device identity and wall-clock time.
 *
 * Identity is derived from the efuse MAC rather than a compile-time constant so
 * that flashing the same binary onto a second board yields a different device
 * without editing anything. It is also the key the `devices` table uses.
 */
namespace DeviceInfo
{
    /**
     * @brief Device identifier: the 48-bit efuse MAC as 12 uppercase hex chars.
     * @return Pointer to a static buffer, valid for the lifetime of the program.
     */
    const char *deviceId();

    /**
     * @brief Start SNTP synchronization. Non-blocking; call once WiFi is up.
     *        Has no effect on transports without an IP connection.
     */
    void beginTimeSync(const char *ntpServer = "pool.ntp.org");

    /**
     * @brief Whether the clock has been set from a trusted source.
     *
     * Readings taken before synchronization carry a boot-relative time, which
     * the server must not treat as a real timestamp.
     */
    bool isClockSynced();

    /**
     * @brief Current UTC time as a Unix epoch in seconds.
     * @return 0 if the clock has not been synchronized yet.
     */
    uint32_t epochNow();

    /**
     * @brief Drive clock synchronization. Call regularly from the network task.
     *
     * Requesting the sync once during setup is not enough: if WiFi is not up
     * yet at that moment the clock would never be set, and every reading would
     * carry a server timestamp. This retries while disconnected and re-syncs
     * periodically, because the ESP32 oscillator drifts by seconds per day —
     * enough to break deduplication on (sensor_id, timestamp) over a long
     * deployment.
     *
     * @param transportReady Whether the link can currently reach an NTP server.
     */
    void serviceTimeSync(bool transportReady);

    /**
     * @brief Why the device last restarted, e.g. "brownout" or "task_wdt".
     *
     * For an unreachable node this distinguishes a failing power supply from
     * hung software — the difference between checking the wiring and checking
     * the code.
     */
    const char *resetReason();
}
