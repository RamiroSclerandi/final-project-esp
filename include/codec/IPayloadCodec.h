#pragma once

#include "core/MeasurementAccumulator.h"

#include <stddef.h>

/**
 * @brief Per-message metadata about the node itself, as opposed to the
 *        phenomena it observes. Sensor readings travel as channels.
 */
struct PayloadMeta
{
    int rssi;              ///< Link quality of the active transport, dBm.
    uint32_t sequence;     ///< Monotonic within a boot; lets the server detect gaps.
    uint32_t bootCount;    ///< Increments once per boot; orders sequences across reboots.
    const char *resetReason; ///< Why the device last restarted, or nullptr.

    // Local buffer health. Reported so a node that fell back from SD to
    // internal flash says so, instead of degrading silently.
    const char *storeKind; ///< "sd" | "littlefs" | "none", or nullptr to omit.
    uint8_t storeUsedPct;
    uint32_t storePending;
    uint32_t storeDropped;
};

/**
 * @brief Contract for turning readings into bytes on the wire.
 *
 * The third axis of variation, alongside sensors and transports. It exists
 * because encoding is dictated by the transport's budget rather than by the
 * data: JSON is the right choice over MQTT and impossible over LoRaWAN, where
 * an uplink can be as small as ~51 bytes.
 */
class IPayloadCodec
{
public:
    virtual ~IPayloadCodec() = default;

    /**
     * @brief Serialize one transmit window.
     * @param readings  Channels accumulated since the last transmission.
     * @param meta      Node metadata for this message.
     * @param out       Destination buffer.
     * @param outSize   Capacity of `out`, including room for any terminator.
     * @return Bytes written, or 0 if the payload did not fit — in which case
     *         `out` is left empty rather than truncated.
     */
    virtual size_t encode(const MeasurementAccumulator &readings,
                          const PayloadMeta &meta,
                          uint8_t *out, size_t outSize) const = 0;

    /** @return Short codec name for logs, e.g. "json". */
    virtual const char *name() const = 0;
};
