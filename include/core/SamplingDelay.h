#pragma once

#include <stdint.h>

/**
 * @brief Splits the wait between samples into watchdog-safe chunks.
 *
 * A single vTaskDelay of a 60 s interval outlasts the 30 s task watchdog and
 * reboots a healthy node. Sleeping in bounded chunks lets the task feed the
 * watchdog between them.
 */
namespace SamplingDelay
{
    /** Longest single sleep; well inside the 30 s task watchdog. */
    constexpr uint32_t WDT_SLICE_MS = 5000;

    /**
     * @return Next sleep in ms, at most `maxChunkMs`; 0 once `elapsedMs` has
     *         reached `intervalMs`. Callers re-read the interval per chunk, so
     *         a remote change takes effect within the current wait.
     */
    inline uint32_t nextChunkMs(uint32_t elapsedMs, uint32_t intervalMs, uint32_t maxChunkMs)
    {
        if (elapsedMs >= intervalMs)
        {
            return 0;
        }
        const uint32_t remaining = intervalMs - elapsedMs;
        return remaining < maxChunkMs ? remaining : maxChunkMs;
    }
}
