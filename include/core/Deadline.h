#pragma once

#include <stdint.h>

/**
 * @brief Rollover-safe millisecond deadlines.
 *
 * millis() wraps every ~49.7 days. Unsigned subtraction measures the elapsed
 * time correctly across the wrap, whereas `now < start + duration` breaks.
 */
namespace Deadline
{
    /** @return Milliseconds from `startMs` to `nowMs`, correct across rollover. */
    inline uint32_t elapsedMs(uint32_t nowMs, uint32_t startMs)
    {
        return (uint32_t)(nowMs - startMs);
    }

    /** @return true once `durationMs` has passed since `startMs`. */
    inline bool hasElapsed(uint32_t nowMs, uint32_t startMs, uint32_t durationMs)
    {
        return elapsedMs(nowMs, startMs) >= durationMs;
    }
}
