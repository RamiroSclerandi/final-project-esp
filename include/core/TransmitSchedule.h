#pragma once

#include <stdint.h>

#include "core/Deadline.h"

/** @brief When the sensor task hands a message to the transport. */
namespace TransmitSchedule
{
    /** Longest wait for NTP before the first message goes out anyway. */
    constexpr uint32_t FIRST_SEND_SYNC_TIMEOUT_MS = 30000;

    /**
     * @return true when a message is due. Equal intervals mean raw mode: every
     *         sample is sent, since tick jitter can make the elapsed time fall
     *         a few ms short and would otherwise skip a whole cycle.
     */
    inline bool isDue(uint32_t nowMs, uint32_t lastTransmitMs, uint32_t transmitMs,
                      uint32_t samplingMs)
    {
        return transmitMs == samplingMs || Deadline::hasElapsed(nowMs, lastTransmitMs, transmitMs);
    }

    /**
     * @return true once the clock is synced or the timeout has passed, so the
     *         first message carries a real timestamp without blocking forever
     *         on a transport that cannot reach NTP.
     */
    inline bool isFirstSendAllowed(bool isClockSynced, uint32_t nowMs, uint32_t taskStartMs)
    {
        return isClockSynced ||
               Deadline::hasElapsed(nowMs, taskStartMs, FIRST_SEND_SYNC_TIMEOUT_MS);
    }
}
