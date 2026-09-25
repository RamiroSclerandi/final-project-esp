#pragma once

#include "core/ISensor.h"

/**
 * @brief Test double for ISensor with mutable, directly-inspectable channels.
 *
 * Shared between test_accumulator and test_codec so both exercise the exact
 * same fake shape; MeasurementAccumulator and JsonCodec both consume readings
 * through SensorRegistry, so their tests need the same kind of source.
 */
class FakeSensor : public ISensor
{
public:
    FakeSensor(const char *name, Measurement *channels, uint8_t count)
        : _name(name), _channels(channels), _count(count) {}

    bool begin() override { return true; }
    bool read() override { return true; }
    uint8_t channelCount() const override { return _count; }
    Measurement channelAt(uint8_t index) const override { return _channels[index]; }
    const char *getName() const override { return _name; }

    /** @brief Mutate one channel between accumulate() rounds. */
    void setValue(uint8_t index, float value, bool valid)
    {
        _channels[index].value = value;
        _channels[index].valid = valid;
    }

private:
    const char *_name;
    Measurement *_channels;
    uint8_t _count;
};
