#pragma once

/**
 * @brief Detects a mismatch between the stored config and what this build
 *        can actually run: the operator enabled the Modbus meter through
 *        provisioning, but DL_ENABLE_MODBUS excluded its code from this
 *        firmware image.
 *
 * Before this, the mismatch was silent: the meter was simply never
 * registered, with no boot warning and no indication in the serial menu of
 * why. The NVS flag itself is never touched here — only visibility changes.
 */
namespace ModbusAvailability
{
    /**
     * @param compiledIn     True when DL_ENABLE_MODBUS built the Modbus
     *                       stack into this firmware image.
     * @param configEnabled  True when the stored config has the Modbus
     *                       meter marked installed.
     * @return true when the operator needs to be told the meter will not
     *         run despite being marked installed.
     */
    inline bool isConfiguredButExcluded(bool compiledIn, bool configEnabled)
    {
        return configEnabled && !compiledIn;
    }
}
