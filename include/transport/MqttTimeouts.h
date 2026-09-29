#pragma once

#include <stdint.h>

/**
 * @brief Bounded timeouts for the MQTT/TLS connection attempt.
 *
 * Both values are in SECONDS — verified against the pinned framework sources
 * before wiring them in, since neither API documents the unit in its
 * signature: `WiFiClientSecure::setHandshakeTimeout(unsigned long)`
 * (framework-arduinoespressif32/libraries/WiFiClientSecure/src/
 * WiFiClientSecure.cpp:369-372, multiplies the argument by 1000 internally)
 * and `PubSubClient::setSocketTimeout(uint16_t)`
 * (.pio/libdeps/esp32dev/PubSubClient/src/PubSubClient.{h,cpp}, stores the
 * value as-is and multiplies by 1000 at each use site). Without a bounded
 * handshake/socket timeout, an unreachable broker blocks the connect call
 * long enough to starve networkTask's watchdog feed.
 */
namespace MqttTimeouts
{
    constexpr unsigned long TLS_HANDSHAKE_TIMEOUT_S = 10;
    constexpr uint16_t SOCKET_TIMEOUT_S = 5;
}
