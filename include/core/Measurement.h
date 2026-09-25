#pragma once

#include <stdint.h>

/**
 * @brief Physical unit of a measurement channel.
 *
 * The numeric values are part of the wire contract: the compact binary codec
 * used for LoRaWAN transmits the unit as a single byte. Never renumber an
 * existing entry — append new ones at the end.
 */
enum class Unit : uint8_t
{
    NONE = 0,
    CELSIUS = 1,
    HECTOPASCAL = 2,
    PERCENT = 3,
    VOLT = 4,
    AMPERE = 5,
    WATT = 6,
    LUX = 7,
    PPM = 8,
    METER = 9,
    MILLIMETER = 10,
    HERTZ = 11,
    KILOWATT = 12,
    KILOVAR = 13,
    KILOVOLTAMPERE = 14,
    KILOWATTHOUR = 15,
};

/**
 * @brief Canonical channel names.
 *
 * Using these constants instead of string literals keeps the names consistent
 * across sensors, which matters because the ingest worker resolves a sensor row
 * from the (device, channel) pair. A typo here creates a duplicate sensor.
 */
namespace Channel
{
    constexpr const char *TEMPERATURE = "temperature";
    constexpr const char *PRESSURE = "pressure";
    constexpr const char *HUMIDITY = "humidity";
    constexpr const char *VOLTAGE = "voltage";
    constexpr const char *CURRENT = "current";
    constexpr const char *POWER = "power";
    constexpr const char *ILLUMINANCE = "illuminance";
    constexpr const char *CO2 = "co2";
    constexpr const char *SOIL_MOISTURE = "soil_moisture";
    constexpr const char *FREQUENCY = "frequency";
    constexpr const char *REACTIVE_POWER = "reactive_power";
    constexpr const char *APPARENT_POWER = "apparent_power";
    constexpr const char *POWER_FACTOR = "power_factor";
    constexpr const char *ACTIVE_ENERGY = "active_energy";
}

/**
 * @brief A single scalar reading from one channel of one sensor.
 *
 * This is the unit of exchange across the whole system: one Measurement maps to
 * exactly one row of the `measurements` table. Carrying the unit alongside the
 * value keeps the reading self-describing, so no consumer needs a hardcoded
 * table of which sensor reports which magnitude in which unit.
 *
 * `valid` is per channel rather than per sensor, so a device that can still
 * read temperature but lost its pressure reading reports that accurately.
 *
 * `tag` separates channels of the same magnitude coming from the same sensor —
 * the three phases of an energy meter, for instance. Without it the ingest
 * worker cannot tell them apart and their series would be merged into one.
 */
struct Measurement
{
    const char *channel;
    const char *tag;
    Unit unit;
    float value;
    bool valid;
};

/** Conventional tag values. Empty means the sensor has one channel of that magnitude. */
namespace Tag
{
    constexpr const char *NONE = "";
    constexpr const char *L1 = "l1";
    constexpr const char *L2 = "l2";
    constexpr const char *L3 = "l3";
    constexpr const char *TOTAL = "total";
}

/**
 * @brief Wire representation of a unit, e.g. "degC" for Unit::CELSIUS.
 * @return Null-terminated string; "none" for unrecognized values.
 */
const char *unitToString(Unit unit);
