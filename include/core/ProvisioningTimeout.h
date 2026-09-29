#pragma once

#include <stdint.h>

#include "core/Deadline.h"

/**
 * @brief Bounds how long the provisioning menu waits for serial input.
 *
 * A device with no serial terminal attached must not hang forever in
 * `waitForKey`/`readNumber` waiting for a keypress that will never come —
 * boot has to proceed with whatever configuration already exists.
 */
namespace ProvisioningTimeout
{
    constexpr uint32_t INPUT_TIMEOUT_MS = 60000;

    // Bounds the whole call regardless of activity. Without this, a stream
    // of accepted characters (e.g. an operator or a script flooding digits)
    // could keep resetting INPUT_TIMEOUT_MS forever and setup() would never
    // return.
    constexpr uint32_t HARD_CAP_MS = 300000;

    /** @return true once no input has arrived within INPUT_TIMEOUT_MS. */
    inline bool hasTimedOut(uint32_t nowMs, uint32_t waitStartMs)
    {
        return Deadline::hasElapsed(nowMs, waitStartMs, INPUT_TIMEOUT_MS);
    }

    /**
     * @return true for a byte that counts as active input for readNumber()
     *         — a digit or a CR/LF terminator. Only these may restart the
     *         activity window; anything else (RX noise, stray letters) is
     *         read and discarded without extending the wait, so a noisy or
     *         floating RX line cannot block setup() forever.
     */
    inline bool isAcceptedInputChar(int c)
    {
        return (c >= '0' && c <= '9') || c == '\r' || c == '\n';
    }

    /**
     * @brief Tracks INPUT_TIMEOUT_MS from the most recent accepted-input
     *        activity instead of from when waiting began, bounded overall
     *        by HARD_CAP_MS from when waiting began.
     *
     * `readNumber`'s multi-character entry used to time out from a single
     * fixed start, so an operator typing slowly (a pause between digits) was
     * cut off mid-entry even though they were actively responding. Every
     * accepted character (see isAcceptedInputChar) restarts the window; only
     * INPUT_TIMEOUT_MS of silence times out. HARD_CAP_MS then bounds the
     * call even under continuous accepted activity, so it always ends.
     */
    class ActivityTimeout
    {
    public:
        explicit ActivityTimeout(uint32_t startMs) : _startMs(startMs), _lastActivityMs(startMs) {}

        /** @brief Call only for a byte where isAcceptedInputChar() is true. */
        void noteActivity(uint32_t nowMs) { _lastActivityMs = nowMs; }

        bool hasTimedOut(uint32_t nowMs) const
        {
            return ProvisioningTimeout::hasTimedOut(nowMs, _lastActivityMs) ||
                   Deadline::hasElapsed(nowMs, _startMs, HARD_CAP_MS);
        }

    private:
        uint32_t _startMs;
        uint32_t _lastActivityMs;
    };

    /** Outcome of one readNumber() wait-loop iteration decision. */
    enum class ReadWaitStep
    {
        TimedOut, ///< Stop and return the fallback.
        HasByte,  ///< A byte is available and should be consumed now.
        Idle,     ///< Nothing available yet and not timed out: sleep and retry.
    };

    /**
     * @brief Decides readNumber()'s next action for one wait-loop iteration.
     *
     * The timeout is evaluated unconditionally, before whether a byte is
     * available — a continuous stream of bytes must not be able to skip the
     * check every iteration and defeat HARD_CAP_MS.
     */
    inline ReadWaitStep nextReadWaitStep(const ActivityTimeout &timeout, uint32_t nowMs,
                                         bool byteAvailable)
    {
        if (timeout.hasTimedOut(nowMs))
        {
            return ReadWaitStep::TimedOut;
        }
        return byteAvailable ? ReadWaitStep::HasByte : ReadWaitStep::Idle;
    }
}
