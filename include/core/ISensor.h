#pragma once

#include "core/Measurement.h"

/**
 * @brief Contract that every sensor in the datalogger implements.
 *
 * A sensor exposes its readings as a list of typed channels rather than
 * serializing itself. That keeps the wire format out of the sensor: the codec
 * decides how a Measurement is encoded, so the same sensor works unchanged over
 * JSON/MQTT and over the compact binary encoding LoRaWAN requires.
 *
 * To add a sensor, implement this interface in its own pair of files and
 * register the instance with the SensorRegistry. No existing sensor, task or
 * codec needs to change.
 */
class ISensor
{
public:
    virtual ~ISensor() = default;

    /**
     * @brief Initialize the sensor hardware.
     *
     * Shared buses (I2C, SPI) are initialized by the composition layer and
     * passed in, so an implementation must not call Wire.begin() itself.
     *
     * @return true if the sensor responded and is ready to be read.
     */
    virtual bool begin() = 0;

    /**
     * @brief Take a reading and cache it internally.
     * @return true if at least one channel produced a valid value.
     */
    virtual bool read() = 0;

    /** @return Number of channels this sensor produces. Constant per sensor. */
    virtual uint8_t channelCount() const = 0;

    /**
     * @brief Channel of the most recent read().
     * @param index  Must be less than channelCount().
     * @return The measurement; `valid` is false if that channel failed.
     */
    virtual Measurement channelAt(uint8_t index) const = 0;

    /** @return Human-readable sensor identifier, e.g. "BMP280". */
    virtual const char *getName() const = 0;
};
