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

    /** @return true once no input has arrived within INPUT_TIMEOUT_MS. */
    inline bool hasTimedOut(uint32_t nowMs, uint32_t waitStartMs)
    {
        return Deadline::hasElapsed(nowMs, waitStartMs, INPUT_TIMEOUT_MS);
    }
}
