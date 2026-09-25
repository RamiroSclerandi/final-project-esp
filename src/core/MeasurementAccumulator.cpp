#include "core/MeasurementAccumulator.h"

void MeasurementAccumulator::reset(const SensorRegistry &registry)
{
    _count = 0;

    for (uint8_t i = 0; i < registry.sensorCount(); i++)
    {
        const ISensor *sensor = registry.sensorAt(i);

        for (uint8_t c = 0; c < sensor->channelCount(); c++)
        {
            if (_count >= MAX_CHANNELS)
            {
                return;
            }

            const Measurement m = sensor->channelAt(c);
            Slot &slot = _slots[_count++];

            slot.channel = m.channel;
            slot.tag = m.tag;
            slot.source = sensor->getName();
            slot.unit = m.unit;
            slot.sum = 0.0f;
            slot.minimum = 0.0f;
            slot.maximum = 0.0f;
            slot.count = 0;
        }
    }
}

void MeasurementAccumulator::accumulate(const SensorRegistry &registry)
{
    uint8_t index = 0;

    for (uint8_t i = 0; i < registry.sensorCount(); i++)
    {
        const ISensor *sensor = registry.sensorAt(i);

        for (uint8_t c = 0; c < sensor->channelCount(); c++)
        {
            if (index >= _count)
            {
                return;
            }

            const Measurement m = sensor->channelAt(c);
            Slot &slot = _slots[index++];

            if (!m.valid)
            {
                continue;
            }

            if (slot.count == 0)
            {
                slot.minimum = m.value;
                slot.maximum = m.value;
            }
            else if (m.value < slot.minimum)
            {
                slot.minimum = m.value;
            }
            else if (m.value > slot.maximum)
            {
                slot.maximum = m.value;
            }

            slot.sum += m.value;
            slot.count++;
        }
    }
}

uint8_t MeasurementAccumulator::channelCount() const
{
    return _count;
}

AggregatedChannel MeasurementAccumulator::channelAt(uint8_t index) const
{
    if (index >= _count)
    {
        return {nullptr, nullptr, nullptr, Unit::NONE, 0.0f, 0.0f, 0.0f, 0};
    }

    const Slot &slot = _slots[index];
    const float mean = slot.count > 0 ? slot.sum / (float)slot.count : 0.0f;

    return {slot.channel, slot.tag, slot.source, slot.unit,
            mean, slot.minimum, slot.maximum, slot.count};
}
