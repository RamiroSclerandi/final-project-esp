#include "sensors/BMP280Sensor.h"

#include <Arduino.h>
#include <math.h>

// Operating limits from the BMP280 datasheet. A reading outside these is either
// a dead device or corrupted bus data, neither of which should reach the server
// labelled as valid.
static constexpr float TEMPERATURE_MIN_C = -40.0f;
static constexpr float TEMPERATURE_MAX_C = 85.0f;
static constexpr float PRESSURE_MIN_HPA = 300.0f;
static constexpr float PRESSURE_MAX_HPA = 1100.0f;

BMP280Sensor::BMP280Sensor(TwoWire &bus, uint8_t address)
    : _bmp(&bus), _address(address),
      _temperature(0.0f), _pressure(0.0f),
      _temperatureValid(false), _pressureValid(false) {}

bool BMP280Sensor::begin()
{
    if (!_bmp.begin(_address))
    {
        Serial.printf("[BMP280] Not found at 0x%02X. Check wiring and address.\n",
                      _address);
        return false;
    }

    // Settings recommended for indoor weather monitoring (low power, high accuracy).
    _bmp.setSampling(
        Adafruit_BMP280::MODE_NORMAL,
        Adafruit_BMP280::SAMPLING_X2,   // Temperature oversampling
        Adafruit_BMP280::SAMPLING_X16,  // Pressure oversampling
        Adafruit_BMP280::FILTER_X16,    // IIR filter coefficient
        Adafruit_BMP280::STANDBY_MS_500 // Standby time between measurements
    );

    return true;
}

bool BMP280Sensor::read()
{
    _temperature = _bmp.readTemperature();
    _pressure = _bmp.readPressure() / 100.0F; // Pascals -> hPa

    // The library returns NAN when the device does not respond. Comparing
    // against 0.0f would not catch that: every comparison involving NaN is
    // false, so a total bus failure would be reported as a successful read.
    _temperatureValid = !isnan(_temperature) &&
                        _temperature > TEMPERATURE_MIN_C &&
                        _temperature < TEMPERATURE_MAX_C;

    _pressureValid = !isnan(_pressure) &&
                     _pressure > PRESSURE_MIN_HPA &&
                     _pressure < PRESSURE_MAX_HPA;

    return _temperatureValid || _pressureValid;
}

uint8_t BMP280Sensor::channelCount() const
{
    return 2;
}

Measurement BMP280Sensor::channelAt(uint8_t index) const
{
    switch (index)
    {
    case 0:
        return {Channel::TEMPERATURE, Tag::NONE, Unit::CELSIUS, _temperature, _temperatureValid};
    case 1:
        return {Channel::PRESSURE, Tag::NONE, Unit::HECTOPASCAL, _pressure, _pressureValid};
    default:
        return {Channel::TEMPERATURE, Tag::NONE, Unit::NONE, 0.0f, false};
    }
}

const char *BMP280Sensor::getName() const
{
    return "BMP280";
}
