#pragma once

#include "config/DeviceConfig.h"

/**
 * @brief One-time interactive setup over the serial console.
 *
 * Runs automatically when the device has never been provisioned. On an already
 * provisioned device it offers a short window to press a key and reopen the
 * menu — that escape hatch matters because a device configured for LoRaWAN
 * cannot be reconfigured over the air: LoRaWAN downlink is too constrained to
 * carry a transport change, and a wrong choice would otherwise brick the node
 * until it is reflashed.
 */
namespace Provisioning
{
    /** Seconds the boot menu waits for a keypress on an already-set-up device. */
    constexpr uint32_t REOPEN_WINDOW_MS = 3000;

    /**
     * @brief Run provisioning if needed, then leave the configuration ready.
     *
     * Blocking by design: it runs in setup(), before any task is created, so
     * the device never samples with a half-applied configuration.
     *
     * @param config  Opened DeviceConfig to read and update.
     */
    void runIfNeeded(DeviceConfig &config);
}
