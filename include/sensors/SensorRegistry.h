#pragma once

#include "core/ISensor.h"

/**
 * @brief Holds every sensor installed on the device and drives them as a group.
 *
 * This is what makes the datalogger multi-variable: the acquisition task
 * iterates the registry instead of talking to one hardcoded sensor, so the
 * number of sensors stops being a compile-time decision in main.cpp.
 *
 * Storage is a fixed array — no dynamic allocation, which avoids heap
 * fragmentation over a long-running deployment.
 */
class SensorRegistry
{
public:
    static constexpr uint8_t MAX_SENSORS = 8;

    /**
     * @brief Register a sensor. The registry does not take ownership; the
     *        instance must outlive the registry (typically a global).
     * @return false if the registry is full or the pointer is null.
     */
    bool add(ISensor *sensor);

    /**
     * @brief Call begin() on every registered sensor.
     *
     * A sensor that fails to initialize stays registered and keeps reporting
     * invalid channels, so a single wiring fault does not take down the device.
     *
     * @return Number of sensors that initialized successfully.
     */
    uint8_t beginAll();

    /** @brief Call read() on every registered sensor. */
    void readAll();

    uint8_t sensorCount() const;

    /** @return Sensor at index, or nullptr if index is out of range. */
    ISensor *sensorAt(uint8_t index) const;

    /** @return Sum of channelCount() across all registered sensors. */
    uint8_t totalChannelCount() const;

private:
    ISensor *_sensors[MAX_SENSORS] = {};
    uint8_t _count = 0;
};
