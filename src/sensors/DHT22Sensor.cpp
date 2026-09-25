#include "sensors/DHT22Sensor.h"

#include <Arduino.h>
#include <math.h>

// Operating limits from the DHT22 datasheet.
static constexpr float TEMPERATURE_MIN_C = -40.0f;
static constexpr float TEMPERATURE_MAX_C = 80.0f;
static constexpr float HUMIDITY_MIN_PCT = 0.0f;
static constexpr float HUMIDITY_MAX_PCT = 100.0f;

DHT22Sensor::DHT22Sensor(uint8_t pin)
    : _dht(pin, DHT22),
      _temperature(0.0f), _humidity(0.0f),
      _temperatureValid(false), _humidityValid(false) {}

bool DHT22Sensor::begin()
{
    _dht.begin();

    // DHT::begin() returns void and never reports whether the device answered,
    // so probe it with a real read to distinguish "present" from "not wired".
    delay(2000); // First conversion after power-up needs the full sampling period.
    return read();
}

bool DHT22Sensor::read()
{
    _temperature = _dht.readTemperature();
    _humidity = _dht.readHumidity();

    // The library returns NAN on checksum failure or timeout, which is a normal
    // intermittent outcome for this single-wire protocol, not a fatal error.
    _temperatureValid = !isnan(_temperature) &&
                        _temperature > TEMPERATURE_MIN_C &&
                        _temperature < TEMPERATURE_MAX_C;

    _humidityValid = !isnan(_humidity) &&
                     _humidity >= HUMIDITY_MIN_PCT &&
                     _humidity <= HUMIDITY_MAX_PCT;

    return _temperatureValid || _humidityValid;
}

uint8_t DHT22Sensor::channelCount() const
{
    return 2;
}

Measurement DHT22Sensor::channelAt(uint8_t index) const
{
    switch (index)
    {
    case 0:
        return {Channel::TEMPERATURE, Tag::NONE, Unit::CELSIUS, _temperature, _temperatureValid};
    case 1:
        return {Channel::HUMIDITY, Tag::NONE, Unit::PERCENT, _humidity, _humidityValid};
    default:
        return {Channel::TEMPERATURE, Tag::NONE, Unit::NONE, 0.0f, false};
    }
}

const char *DHT22Sensor::getName() const
{
    return "DHT22";
}
