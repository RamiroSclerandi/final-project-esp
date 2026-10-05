#pragma once

#include "transport/ITransport.h"
#include "MQTTManager.h"

/**
 * @brief WiFi + MQTT over TLS transport.
 *
 * Wraps MQTTManager, which keeps the working TLS and reconnection logic intact
 * and confines this class to adapting it to the ITransport contract. Topics are
 * an MQTT concept that does not belong in the interface, so they stay inside
 * MQTTManager, which derives them from the device MAC.
 */
class WiFiMqttTransport : public ITransport
{
public:
    explicit WiFiMqttTransport(MQTTManager &mqtt);

    /** @brief Apply the WiFi credentials loaded from persistent configuration. */
    void setCredentials(const char *ssid, const char *password);

    bool begin() override;
    bool isReady() const override;
    void poll() override;
    bool send(const uint8_t *payload, size_t length) override;
    size_t maxPayloadSize() const override;
    int linkQuality() const override;
    uint32_t millisOffline() const override;
    const char *name() const override;

private:
    MQTTManager &_mqtt;
    char _ssid[72];
    char _password[72];
};
