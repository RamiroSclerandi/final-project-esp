#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Contract for anything that carries a payload off the device.
 *
 * Sensors are abstracted so the acquisition logic does not care what is being
 * measured; this is the same idea applied to the other axis of variation, so
 * the acquisition logic does not care how the data leaves either.
 *
 * Implementations differ in more than speed. A LoRaWAN uplink is limited to a
 * few dozen bytes and to a handful of messages per day, so callers must respect
 * maxPayloadSize() and the configured transmit interval instead of assuming the
 * always-on, unmetered link that WiFi provides.
 */
class ITransport
{
public:
    virtual ~ITransport() = default;

    /**
     * @brief Bring the link up. May block while joining the network.
     * @return true if the transport is usable.
     */
    virtual bool begin() = 0;

    /** @return true if a payload can be sent right now. */
    virtual bool isReady() const = 0;

    /**
     * @brief Service the link: reconnects, keep-alives, inbound messages.
     *        Must be called regularly from the network task.
     */
    virtual void poll() = 0;

    /**
     * @brief Transmit one payload.
     * @param payload  Bytes to send; for text transports this is the string.
     * @param length   Number of bytes, excluding any terminator.
     * @return true if the transport accepted the payload.
     */
    virtual bool send(const uint8_t *payload, size_t length) = 0;

    /**
     * @brief Largest payload this transport accepts, in bytes.
     *
     * Callers must check this before encoding: on LoRaWAN it can be as low as
     * ~51 bytes, far below a JSON message.
     */
    virtual size_t maxPayloadSize() const = 0;

    /**
     * @brief Link quality in dBm, or 0 when the transport cannot report it.
     *
     * Under LoRaWAN the meaningful measurement is taken by the gateway, not by
     * the node, so this is only indicative.
     */
    virtual int linkQuality() const = 0;

    /**
     * @brief How long the link has been down, for the offline-restart rule
     *        (see core/OfflineRestart.h).
     * @return Milliseconds since the link was last usable (since boot if it
     *         never was), or 0 when it is up or when a restart could not bring
     *         it back (e.g. a transport whose radio stack does not exist yet).
     */
    virtual uint32_t millisOffline() const = 0;

    /** @return Short transport name for logs and telemetry, e.g. "wifi-mqtt". */
    virtual const char *name() const = 0;
};
