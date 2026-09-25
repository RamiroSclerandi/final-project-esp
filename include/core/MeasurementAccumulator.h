#pragma once

#include "sensors/SensorRegistry.h"

/**
 * @brief One channel's readings collapsed over a transmit window.
 *
 * When only one sample was taken, `value` is that sample and the statistics
 * carry no extra information. When several were taken, `value` is their mean
 * and min/max/count describe the shape that averaging would otherwise hide —
 * which matters for cyclic loads such as an air conditioning compressor, where
 * the peak and the mean are very different numbers.
 */
struct AggregatedChannel
{
    const char *channel;
    const char *tag;
    const char *source;
    Unit unit;
    float value;
    float minimum;
    float maximum;
    uint16_t count; ///< Valid samples in the window; 0 means the channel failed
};

/**
 * @brief Collapses several sampling rounds into one message.
 *
 * Aggregation is not a global policy: it is a consequence of the transport's
 * bandwidth budget. Over WiFi the transmit interval equals the sampling
 * interval, every reading is sent raw and count is always 1 — averaging there
 * would destroy resolution for nothing, since the server can always aggregate
 * on read. Over LoRaWAN a handful of messages per day must represent thousands
 * of samples, and this is lossy compression that the link forces.
 */
class MeasurementAccumulator
{
public:
    /**
     * Upper bound on distinct channels. The largest configuration today is a
     * Modbus meter (22) plus two environmental sensors (4).
     */
    static constexpr uint8_t MAX_CHANNELS = 32;

    /** @brief Capture the channel layout and clear all statistics. */
    void reset(const SensorRegistry &registry);

    /**
     * @brief Fold the registry's current readings into the window.
     *
     * Invalid channels are skipped rather than counted, so a flaky sensor
     * lowers `count` instead of contaminating the mean — and the count itself
     * becomes a quality indicator.
     */
    void accumulate(const SensorRegistry &registry);

    uint8_t channelCount() const;
    AggregatedChannel channelAt(uint8_t index) const;

private:
    struct Slot
    {
        const char *channel;
        const char *tag;
        const char *source;
        Unit unit;
        float sum;
        float minimum;
        float maximum;
        uint16_t count;
    };

    Slot _slots[MAX_CHANNELS] = {};
    uint8_t _count = 0;
};
