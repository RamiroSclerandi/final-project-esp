#include "transport/ConnectionBackoff.h"

#include <Arduino.h>

bool ConnectionBackoff::shouldRetry()
{
    return (int32_t)(millis() - _nextAttemptAt) >= 0;
}

void ConnectionBackoff::scheduleNext()
{
    // Spread attempts across a window around the nominal delay so that several
    // nodes recovering from the same outage do not retry in lockstep.
    const uint32_t spread = (_delayMs * JITTER_PERCENT) / 100;
    const uint32_t jittered = _delayMs - spread + (uint32_t)random(0, 2 * spread + 1);
    _nextAttemptAt = millis() + jittered;
}

void ConnectionBackoff::recordSuccess()
{
    _delayMs = INITIAL_DELAY_MS;
    _failures = 0;
    _lastSuccessAt = millis();
    _nextAttemptAt = millis();
}

void ConnectionBackoff::recordFailure()
{
    _failures++;

    if (_delayMs < MAX_DELAY_MS)
    {
        _delayMs = _delayMs * 2;
        if (_delayMs > MAX_DELAY_MS)
        {
            _delayMs = MAX_DELAY_MS;
        }
    }

    scheduleNext();
}

uint32_t ConnectionBackoff::consecutiveFailures() const
{
    return _failures;
}

uint32_t ConnectionBackoff::millisSinceSuccess() const
{
    return _lastSuccessAt == 0 ? 0 : millis() - _lastSuccessAt;
}
