#pragma once

#include "core/ISensor.h"

#include <DHT.h>

/**
 * @brief DHT22 (AM2302) temperature/humidity sensor on a single data line.
 *
 * Produces two channels: temperature (degC) and humidity (pct).
 *
 * The DHT22 needs roughly 2 seconds between reads; the library returns the
 * cached sample if polled faster, so the acquisition interval should stay above
 * that. Reads are timing-sensitive bit-banging and fail intermittently even on
 * healthy hardware, which is why validity is evaluated per channel on every
 * read rather than latched at begin().
 */
class DHT22Sensor : public ISensor
{
public:
    explicit DHT22Sensor(uint8_t pin);

    bool begin() override;
    bool read() override;
    uint8_t channelCount() const override;
    Measurement channelAt(uint8_t index) const override;
    const char *getName() const override;

private:
    DHT _dht;
    float _temperature;
    float _humidity;
    bool _temperatureValid;
    bool _humidityValid;
};
