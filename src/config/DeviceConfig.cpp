#include "config/DeviceConfig.h"

#include <Arduino.h>

namespace
{
    constexpr char NVS_NAMESPACE[] = "datalogger";

    // NVS keys are limited to 15 characters.
    constexpr char KEY_PROVISIONED[] = "provisioned";
    constexpr char KEY_TRANSPORT[] = "transport";
    constexpr char KEY_SAMPLING[] = "samplingMs";
    constexpr char KEY_TRANSMIT[] = "transmitMs";
    constexpr char KEY_BOOT_COUNT[] = "bootCount";
    constexpr char KEY_SENSOR_PREFIX[] = "s_";

    constexpr uint32_t DEFAULT_SAMPLING_MS = 5000;
    constexpr uint32_t DEFAULT_TRANSMIT_MS = 5000;

    /** Builds the NVS key for a sensor flag, e.g. "bmp280" -> "s_bmp280". */
    void sensorKeyFor(const char *sensorKey, char *out, size_t outSize)
    {
        snprintf(out, outSize, "%s%s", KEY_SENSOR_PREFIX, sensorKey);
    }
}

const char *transportName(TransportKind kind)
{
    switch (kind)
    {
    case TransportKind::WIFI_MQTT:
        return "wifi-mqtt";
    case TransportKind::LORAWAN:
        return "lorawan";
    case TransportKind::UNSET:
    default:
        return "unset";
    }
}

void DeviceConfig::begin()
{
    _prefs.begin(NVS_NAMESPACE, false); // false = read/write
}

bool DeviceConfig::isProvisioned() const
{
    return _prefs.getBool(KEY_PROVISIONED, false);
}

void DeviceConfig::setProvisioned(bool provisioned)
{
    _prefs.putBool(KEY_PROVISIONED, provisioned);
}

TransportKind DeviceConfig::transport() const
{
    const uint8_t stored = _prefs.getUChar(KEY_TRANSPORT,
                                           (uint8_t)TransportKind::UNSET);

    // Reject anything this build does not recognize rather than casting blindly:
    // a value written by a newer firmware must not select an unknown transport.
    switch ((TransportKind)stored)
    {
    case TransportKind::WIFI_MQTT:
    case TransportKind::LORAWAN:
        return (TransportKind)stored;
    default:
        return TransportKind::UNSET;
    }
}

void DeviceConfig::setTransport(TransportKind kind)
{
    _prefs.putUChar(KEY_TRANSPORT, (uint8_t)kind);
}

bool DeviceConfig::isSensorEnabled(const char *sensorKey) const
{
    char key[16];
    sensorKeyFor(sensorKey, key, sizeof(key));

    // Default false: a sensor is only polled once someone declares it installed.
    return _prefs.getBool(key, false);
}

void DeviceConfig::setSensorEnabled(const char *sensorKey, bool enabled)
{
    char key[16];
    sensorKeyFor(sensorKey, key, sizeof(key));
    _prefs.putBool(key, enabled);
}

uint32_t DeviceConfig::samplingIntervalMs() const
{
    return _prefs.getUInt(KEY_SAMPLING, DEFAULT_SAMPLING_MS);
}

void DeviceConfig::setSamplingIntervalMs(uint32_t ms)
{
    // Skip the write when nothing changed. An NVS write erases a flash sector,
    // which blocks for tens of milliseconds and can drop the TLS socket — and
    // remote config messages often repeat the value already in force.
    if (_prefs.getUInt(KEY_SAMPLING, 0) == ms)
    {
        return;
    }

    _prefs.putUInt(KEY_SAMPLING, ms);
}

uint32_t DeviceConfig::transmitIntervalMs() const
{
    const uint32_t stored = _prefs.getUInt(KEY_TRANSMIT, DEFAULT_TRANSMIT_MS);

    // Transmitting faster than sampling would send the same reading twice.
    const uint32_t sampling = samplingIntervalMs();
    return stored < sampling ? sampling : stored;
}

void DeviceConfig::setTransmitIntervalMs(uint32_t ms)
{
    if (_prefs.getUInt(KEY_TRANSMIT, 0) == ms)
    {
        return;
    }

    _prefs.putUInt(KEY_TRANSMIT, ms);
}

void DeviceConfig::credential(const char *key, char *out, size_t outSize,
                              const char *fallback) const
{
    const String stored = _prefs.getString(key, "");

    if (stored.length() > 0)
    {
        snprintf(out, outSize, "%s", stored.c_str());
        return;
    }

    snprintf(out, outSize, "%s", fallback != nullptr ? fallback : "");
}

void DeviceConfig::setCredential(const char *key, const char *value)
{
    _prefs.putString(key, value);
}

bool DeviceConfig::hasCredential(const char *key) const
{
    return _prefs.getString(key, "").length() > 0;
}

uint32_t DeviceConfig::nextBootCount()
{
    const uint32_t next = _prefs.getUInt(KEY_BOOT_COUNT, 0) + 1;
    _prefs.putUInt(KEY_BOOT_COUNT, next);
    return next;
}

void DeviceConfig::factoryReset()
{
    _prefs.clear();
    Serial.println("[Config] Factory reset — setup will run on next boot.");
}
