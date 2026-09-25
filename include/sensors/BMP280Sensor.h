#pragma once

#include "core/ISensor.h"

#include <Adafruit_BMP280.h>
#include <Wire.h>

/**
 * @brief BMP280 pressure/temperature sensor on I2C.
 *
 * Produces two channels: temperature (degC) and pressure (hPa).
 *
 * The I2C bus and address are injected rather than hardcoded, so several I2C
 * devices — including a second BMP280 at 0x77 — can share the bus without
 * either of them assuming it owns it.
 */
class BMP280Sensor : public ISensor
{
public:
    /** Default address 0x76 applies when SDO is pulled low; use 0x77 if high. */
    explicit BMP280Sensor(TwoWire &bus = Wire, uint8_t address = 0x76);

    bool begin() override;
    bool read() override;
    uint8_t channelCount() const override;
    Measurement channelAt(uint8_t index) const override;
    const char *getName() const override;

private:
    Adafruit_BMP280 _bmp;
    uint8_t _address;
    float _temperature;
    float _pressure;
    bool _temperatureValid;
    bool _pressureValid;
};
