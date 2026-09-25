#include "transport/WiFiMqttTransport.h"

#include <Arduino.h>
#include <WiFi.h>

// PubSubClient is configured with a 1024 byte buffer, shared between the topic,
// the fixed header and the payload. Leaving headroom avoids a silent truncation.
static constexpr size_t MAX_PAYLOAD_BYTES = 900;

WiFiMqttTransport::WiFiMqttTransport(MQTTManager &mqtt)
    : _mqtt(mqtt), _ssid{}, _password{} {}

void WiFiMqttTransport::setCredentials(const char *ssid, const char *password)
{
    snprintf(_ssid, sizeof(_ssid), "%s", ssid != nullptr ? ssid : "");
    snprintf(_password, sizeof(_password), "%s", password != nullptr ? password : "");
}

bool WiFiMqttTransport::begin()
{
    if (_ssid[0] == '\0')
    {
        Serial.println("[WiFi] Sin SSID configurado. Abri el setup (opcion 7).");
        return false;
    }

    return _mqtt.connectWiFi(_ssid, _password);
}

bool WiFiMqttTransport::isReady() const
{
    return WiFi.isConnected();
}

void WiFiMqttTransport::poll()
{
    _mqtt.ensureConnected();
    _mqtt.loop();
}

bool WiFiMqttTransport::send(const uint8_t *payload, size_t length)
{
    if (length == 0 || length > MAX_PAYLOAD_BYTES)
    {
        return false;
    }

    // MQTTManager publishes null-terminated strings; the JSON codec always
    // produces one, so the length is used only as a guard.
    return _mqtt.publish(_mqtt.dataTopic(), (const char *)payload);
}

size_t WiFiMqttTransport::maxPayloadSize() const
{
    return MAX_PAYLOAD_BYTES;
}

int WiFiMqttTransport::linkQuality() const
{
    return WiFi.isConnected() ? WiFi.RSSI() : 0;
}

const char *WiFiMqttTransport::name() const
{
    return "wifi-mqtt";
}
