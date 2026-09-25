#include "sensors/SensorRegistry.h"

#include <Arduino.h>

bool SensorRegistry::add(ISensor *sensor)
{
    if (sensor == nullptr || _count >= MAX_SENSORS)
    {
        return false;
    }

    _sensors[_count++] = sensor;
    return true;
}

uint8_t SensorRegistry::beginAll()
{
    uint8_t ready = 0;

    for (uint8_t i = 0; i < _count; i++)
    {
        if (_sensors[i]->begin())
        {
            Serial.printf("[Registry] %s ready (%u channels).\n",
                          _sensors[i]->getName(), _sensors[i]->channelCount());
            ready++;
        }
        else
        {
            // Kept registered on purpose: it will report invalid channels, which
            // is more useful downstream than the sensor silently disappearing.
            Serial.printf("[Registry] %s FAILED to initialize.\n",
                          _sensors[i]->getName());
        }
    }

    Serial.printf("[Registry] %u/%u sensors ready, %u channels total.\n",
                  ready, _count, totalChannelCount());
    return ready;
}

void SensorRegistry::readAll()
{
    for (uint8_t i = 0; i < _count; i++)
    {
        _sensors[i]->read();
    }
}

uint8_t SensorRegistry::sensorCount() const
{
    return _count;
}

ISensor *SensorRegistry::sensorAt(uint8_t index) const
{
    return index < _count ? _sensors[index] : nullptr;
}

uint8_t SensorRegistry::totalChannelCount() const
{
    uint8_t total = 0;

    for (uint8_t i = 0; i < _count; i++)
    {
        total += _sensors[i]->channelCount();
    }

    return total;
}
