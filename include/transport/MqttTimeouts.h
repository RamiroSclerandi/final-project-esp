#pragma once

#include <stdint.h>

/**
 * @brief Bounded timeouts for the MQTT/TLS connection attempt.
 *
 * All three values are in SECONDS — verified against the pinned framework
 * sources before wiring them in, since none of the APIs document the unit in
 * their signature:
 * `WiFiClientSecure::setTimeout(uint32_t seconds)` bounds the raw TCP
 * connect() phase (WiFiClientSecure.cpp:378-389, `_timeout = seconds *
 * 1000`). Without it, the constructor's default `_timeout = 30000`
 * (WiFiClientSecure.cpp:35) applies — the whole 30 s WDT budget — and that
 * value reaches lwip_connect()'s select() timeout unchanged
 * (ssl_client.cpp:84-95). PubSubClient::connect() calls the 2-arg
 * connect(host, port) overload that reads `_timeout`
 * (.pio/libdeps/esp32dev/PubSubClient/src/PubSubClient.cpp:190), so this is
 * on the hot path of every reconnect attempt, not just the first one.
 * `WiFiClientSecure::setHandshakeTimeout(unsigned long)`
 * (framework-arduinoespressif32/libraries/WiFiClientSecure/src/
 * WiFiClientSecure.cpp:369-372, multiplies the argument by 1000 internally)
 * and `PubSubClient::setSocketTimeout(uint16_t)`
 * (.pio/libdeps/esp32dev/PubSubClient/src/PubSubClient.{h,cpp}, stores the
 * value as-is and multiplies by 1000 at each use site). Without bounded
 * connect/handshake/socket timeouts, an unreachable broker blocks the
 * connect call long enough to starve networkTask's watchdog feed.
 */
namespace MqttTimeouts
{
    constexpr uint32_t TCP_CONNECT_TIMEOUT_S = 5;
    constexpr unsigned long TLS_HANDSHAKE_TIMEOUT_S = 10;
    constexpr uint16_t SOCKET_TIMEOUT_S = 5;
}
