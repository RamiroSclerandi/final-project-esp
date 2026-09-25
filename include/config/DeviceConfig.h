#pragma once

#include <Preferences.h>
#include <stdint.h>

/**
 * @brief Data transport selected for this device.
 *
 * The numeric values are persisted in NVS, so they must never be renumbered.
 */
enum class TransportKind : uint8_t
{
    UNSET = 0,
    WIFI_MQTT = 1,
    LORAWAN = 2,
};

const char *transportName(TransportKind kind);

/**
 * @brief Keys identifying an installable sensor.
 *
 * A device only registers the sensors marked as installed, so the same binary
 * runs on a board with just a BMP280 and on one with the full set, without
 * reporting phantom channels for hardware that is not there.
 */
namespace SensorKey
{
    constexpr const char *BMP280 = "bmp280";
    constexpr const char *DHT22 = "dht22";
    constexpr const char *MODBUS_METER = "modbus";
}

/**
 * @brief NVS keys for stored credentials.
 *
 * Kept short: NVS limits a key to 15 characters.
 */
namespace CredentialKey
{
    constexpr const char *WifiSsid = "wifiSsid";
    constexpr const char *WifiPassword = "wifiPass";
    constexpr const char *MqttHost = "mqttHost";
    constexpr const char *MqttUser = "mqttUser";
    constexpr const char *MqttPassword = "mqttPass";
}

/** Longest credential this device stores, including the terminator. */
constexpr size_t CREDENTIAL_MAX_LENGTH = 72;

/**
 * @brief Runtime configuration, persisted in NVS under the "datalogger"
 *        namespace so it survives reboots, deep sleep and firmware updates.
 *
 * This replaces compile-time #defines for anything an operator may need to
 * change in the field. In particular the transport is a runtime decision: the
 * firmware ships with both stacks compiled in and the choice is made once
 * during provisioning, which is what allows the same binary to be deployed on a
 * WiFi node and on a LoRaWAN node.
 */
class DeviceConfig
{
public:
    /** @brief Open the NVS namespace. Call once, before any accessor. */
    void begin();

    /** @return false when the device has never been provisioned. */
    bool isProvisioned() const;

    /** @brief Mark provisioning complete so later boots skip the setup menu. */
    void setProvisioned(bool provisioned);

    TransportKind transport() const;
    void setTransport(TransportKind kind);

    bool isSensorEnabled(const char *sensorKey) const;
    void setSensorEnabled(const char *sensorKey, bool enabled);

    /** @brief How often sensors are polled. */
    uint32_t samplingIntervalMs() const;
    void setSamplingIntervalMs(uint32_t ms);

    /**
     * @brief How often a message is transmitted.
     *
     * Separate from the sampling interval because LoRaWAN duty cycle and fair
     * use limits allow far fewer transmissions than a useful sampling rate:
     * the device samples often, aggregates, and sends rarely.
     */
    uint32_t transmitIntervalMs() const;
    void setTransmitIntervalMs(uint32_t ms);

    /**
     * @brief Read a stored credential into `out`.
     *
     * Credentials live in NVS rather than in compile-time defines so a second
     * unit can be deployed, or a network changed in the field, with a USB cable
     * instead of a laptop carrying the toolchain.
     *
     * SECURITY: NVS is not encrypted by default, so these are readable by
     * anyone with physical access and a flash reader. Mitigations, in order:
     * per-device MQTT credentials so one compromised node is not the fleet;
     * NVS encryption; ESP32 flash encryption.
     *
     * @param key       One of the CredentialKey constants.
     * @param out       Destination buffer.
     * @param outSize   Capacity of `out`.
     * @param fallback  Returned when nothing is stored; may be nullptr.
     */
    void credential(const char *key, char *out, size_t outSize,
                    const char *fallback) const;

    void setCredential(const char *key, const char *value);

    /** @return true if a value is stored for this key. */
    bool hasCredential(const char *key) const;

    /**
     * @brief Increment and return the boot counter.
     *
     * Called once during setup. Message sequence numbers restart at zero on
     * every boot, so the server needs this to order sessions and tell a genuine
     * gap from a restart. Persisting the counter itself per message would burn
     * through the NVS write endurance in days; one write per boot does not.
     */
    uint32_t nextBootCount();

    /** @brief Erase all stored configuration; the next boot re-runs provisioning. */
    void factoryReset();

private:
    mutable Preferences _prefs;
};
