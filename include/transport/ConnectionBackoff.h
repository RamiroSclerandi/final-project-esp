#pragma once

#include <stdint.h>

/**
 * @brief Exponential backoff with jitter for reconnection attempts.
 *
 * Retrying every 500 ms against a broker that is down wastes energy and hammers
 * the network for nothing. The delay doubles up to a cap and resets on success.
 *
 * The jitter matters once more than one node is deployed: without it a network
 * outage synchronises every device, and they all reconnect in lockstep,
 * hitting the broker hardest exactly as it recovers.
 */
class ConnectionBackoff
{
public:
    static constexpr uint32_t INITIAL_DELAY_MS = 1000;
    static constexpr uint32_t MAX_DELAY_MS = 60000;
    static constexpr uint8_t JITTER_PERCENT = 20;

    /** Stamps `_lastSuccessAt` at construction time (boot), not a 0 sentinel. */
    ConnectionBackoff();

    /** @return true if the delay has elapsed and another attempt is due. */
    bool shouldRetry();

    /** @brief Reset the delay after a successful connection. */
    void recordSuccess();

    /** @brief Double the delay, up to the cap. */
    void recordFailure();

    uint32_t consecutiveFailures() const;

    /**
     * @brief Milliseconds since the last success.
     *
     * A device that has never connected still reports real elapsed time
     * since boot, so a prolonged never-connected state reaches the offline
     * reboot threshold the same as a device that connected once and dropped.
     */
    uint32_t millisSinceSuccess() const;

private:
    void scheduleNext();

    uint32_t _delayMs = INITIAL_DELAY_MS;
    uint32_t _nextAttemptAt = 0;
    uint32_t _lastSuccessAt;
    uint32_t _failures = 0;
};
