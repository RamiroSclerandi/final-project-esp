#pragma once

#include <stdint.h>

#include "core/EmissionOutcome.h"

/**
 * @brief Owns the seq/lost/pendingResetReason state driven by sensorTask's
 *        per-reading send attempts (G-10).
 *
 * Extracted from main.cpp so the seq-vs-lost wiring is host-testable: a
 * regression like re-advancing `seq` on a pre-emission drop, or clearing
 * the reset reason before a payload is actually emitted, fails a native
 * test instead of only showing up on hardware. Single writer (sensorTask),
 * so no synchronization is needed.
 *
 * This ledger only ever counts `lost` — the pre-emission device drops
 * (encode failure, oversize, queue-full-and-buffer-append-failure). A
 * post-emission loss (networkTask's send-then-reappend double failure)
 * happens after `seq` was already advanced here, so it is counted
 * separately in `meta.store.drop` (see LittleFsBuffer::recordEmittedLoss),
 * never in `lost`.
 */
class EmissionLedger
{
public:
    /** @brief Call once per boot, before any send attempt is recorded. */
    void reset(const char *resetReason)
    {
        _sequence = 0;
        _lost = 0;
        _pendingResetReason = resetReason;
    }

    /**
     * @brief Applies one sensorTask send attempt outcome.
     *
     * An emitted attempt (queued or buffered) advances `seq` and consumes
     * the pending reset reason. Anything earlier is a pre-emission
     * device-side loss counted in `lost`; `seq` stays untouched, so it
     * never opens a gap.
     */
    void recordSendAttempt(EmissionOutcome::SendAttempt attempt)
    {
        if (EmissionOutcome::isEmitted(attempt))
        {
            _sequence++;
            _pendingResetReason = nullptr;
        }
        else
        {
            _lost++;
        }
    }

    uint32_t sequence() const { return _sequence; }
    uint32_t lostCount() const { return _lost; }
    const char *pendingResetReason() const { return _pendingResetReason; }

private:
    uint32_t _sequence = 0;
    uint32_t _lost = 0;
    const char *_pendingResetReason = nullptr;
};
