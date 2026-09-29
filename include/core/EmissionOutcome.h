#pragma once

/**
 * @brief Classifies what sensorTask's send attempt did with one reading (G-10).
 *
 * `seq` must advance — and `pendingResetReason` must clear — only once a
 * payload is actually handed to transport/buffer (a successful `xQueueSend`
 * or `localBuffer.append`). Anything earlier is a pre-emission device-side
 * loss counted in `meta.lost` instead, so a device drop never looks like a
 * transport gap downstream.
 */
namespace EmissionOutcome
{
    enum class SendAttempt
    {
        EncodeFailed,
        OversizeForTransport,
        QueuedOk,
        BufferedOk,
        QueueFullAndBufferFailed,
    };

    /** @return true once the reading was actually handed to transport/buffer. */
    inline bool isEmitted(SendAttempt attempt)
    {
        return attempt == SendAttempt::QueuedOk || attempt == SendAttempt::BufferedOk;
    }
}
