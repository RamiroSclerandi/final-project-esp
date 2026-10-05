#pragma once

#include <stdint.h>

/**
 * @brief Last-resort restart for a link that never recovers.
 *
 * Only safe because unsent readings are persisted locally: without the
 * buffer, a restart would discard everything still queued in RAM. The offline
 * time comes from the selected transport (ITransport::millisOffline()), never
 * from one transport's clock in particular.
 */
namespace OfflineRestart
{
    constexpr uint32_t THRESHOLD_MS = 30UL * 60UL * 1000UL;

    /** @return true once the transport has been down for longer than THRESHOLD_MS. */
    inline bool isDue(uint32_t millisOffline)
    {
        return millisOffline > THRESHOLD_MS;
    }
}
