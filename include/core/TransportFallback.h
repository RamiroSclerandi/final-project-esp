#pragma once

/**
 * @brief Resolves the transport chooseTransport() keeps after its input
 *        window times out with no key pressed.
 */
namespace TransportFallback
{
    /**
     * @return defaultValue when current == unsetValue (no transport chosen
     *         yet), otherwise current unchanged.
     *
     * A repeated timeout must not silently override an operator's earlier
     * explicit choice (e.g. LoRaWAN) with the WiFi+MQTT default — only a
     * still-unset transport gets it.
     * @tparam T Transport's underlying type (TransportKind in production).
     */
    template <typename T> inline T resolveAfterTimeout(T current, T unsetValue, T defaultValue)
    {
        return current == unsetValue ? defaultValue : current;
    }
}
