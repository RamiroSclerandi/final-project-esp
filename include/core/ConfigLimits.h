#pragma once

#include <stdint.h>

/**
 * @brief Shared range validation for device configuration values.
 *
 * A single call site for the sampling interval range keeps the MQTT remote
 * config handler and the serial provisioning menu from drifting apart, so a
 * bad value cannot slip in through one path just because it is only
 * validated on the other.
 */
namespace ConfigLimits
{
    constexpr uint32_t SAMPLING_INTERVAL_MIN_MS = 1000;
    constexpr uint32_t SAMPLING_INTERVAL_MAX_MS = 300000;

    /** @return true when intervalMs is an acceptable sampling interval. */
    inline bool isValidSamplingIntervalMs(uint32_t intervalMs)
    {
        return intervalMs >= SAMPLING_INTERVAL_MIN_MS && intervalMs <= SAMPLING_INTERVAL_MAX_MS;
    }

    /**
     * @brief Repairs a stored value that predates this validation, or that
     *        otherwise ended up out of range.
     * @return intervalMs unchanged if already valid, otherwise the nearest
     *         bound.
     */
    inline uint32_t clampSamplingIntervalMs(uint32_t intervalMs)
    {
        if (intervalMs < SAMPLING_INTERVAL_MIN_MS)
        {
            return SAMPLING_INTERVAL_MIN_MS;
        }
        if (intervalMs > SAMPLING_INTERVAL_MAX_MS)
        {
            return SAMPLING_INTERVAL_MAX_MS;
        }
        return intervalMs;
    }
}
