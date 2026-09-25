#pragma once

#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <functional>

#include "transport/ConnectionBackoff.h"

/**
 * @brief Manages WiFi and MQTT connectivity with TLS/SSL support.
 *
 * Encapsulates WiFiClientSecure + PubSubClient. Handles connection, automatic
 * reconnection, topic subscription and message publishing. Intended to run
 * inside a dedicated FreeRTOS task.
 *
 * Topics are derived at runtime from the device MAC rather than from a
 * compile-time identifier, so the same binary flashed onto a second board
 * produces a distinct device with no edits — and the identity in the topic
 * matches the one inside the payload and the one stored in the database.
 *
 * Uses a static trampoline pattern to bridge PubSubClient's C-style callback
 * to a member function.
 */
class MQTTManager
{
public:
    /**
     * @param host      MQTT broker hostname.
     * @param port      MQTT broker port (8883 for TLS).
     * @param user      MQTT username.
     * @param password  MQTT password.
     * @param caCert    Root CA certificate in PEM format.
     */
    explicit MQTTManager(const char *caCert);

    /**
     * @brief Apply broker settings loaded from persistent configuration.
     *
     * Called after provisioning and before the first connection attempt. The
     * strings are copied, so the caller may reuse its buffers.
     */
    void configure(const char *host, uint16_t port,
                   const char *user, const char *password);

    /**
     * @brief Connect to WiFi. Blocks until connected or max retries.
     * @return true if connected successfully.
     */
    bool connectWiFi(const char *ssid, const char *wifiPassword);

    /**
     * @brief Establish the TLS connection, register the last will and
     *        subscribe to the config topic.
     * @return true if connected successfully.
     */
    bool connectMQTT();

    /**
     * @brief Ensure both WiFi and MQTT are alive, reconnecting if needed.
     *
     * Returns quickly rather than blocking through a full retry cycle, so the
     * network task keeps servicing its queue while the link is down. Attempts
     * are paced by exponential backoff.
     *
     * @return true if both connections are active after the call.
     */
    bool ensureConnected();

    /** @return Milliseconds since the last successful connection, 0 if never. */
    uint32_t millisSinceConnected() const;

    /**
     * @brief Publish a payload to the given topic.
     * @param topic     Null-terminated topic string.
     * @param payload   Null-terminated payload string.
     * @param retained  Whether the broker should retain the message.
     * @return true if the message was accepted by the broker.
     */
    bool publish(const char *topic, const char *payload, bool retained = false);

    /** @brief Process incoming messages. Call regularly from the network task. */
    void loop();

    /** @brief Register the callback for remote sampling interval updates. */
    void setConfigCallback(std::function<void(int)> cb);

    /** Topic carrying measurement payloads. */
    const char *dataTopic();

    /**
     * @brief Topic carrying online/offline state.
     *
     * Published retained as "online" on connect, and set to "offline" by the
     * broker through the last will if the device disappears without a clean
     * disconnect. That is the only reliable way to detect a dead node, since a
     * node that loses power cannot announce it itself.
     */
    const char *statusTopic();

    /** Topic subscribed to for remote configuration. */
    const char *configTopic();

private:
    /** Builds the topic set from the device identifier. Idempotent. */
    void buildTopics();

    char _host[72];
    uint16_t _port;
    char _user[72];
    char _password[72];
    const char *_caCert;

    char _dataTopic[48];
    char _statusTopic[48];
    char _configTopic[48];
    bool _topicsBuilt;

    WiFiClientSecure _wifiClient;
    PubSubClient _mqttClient;
    std::function<void(int)> _configCallback;
    ConnectionBackoff _backoff;

    // Static trampoline: PubSubClient requires a plain function pointer.
    static MQTTManager *_instance;
    static void _onMessageStatic(char *topic, byte *payload, unsigned int length);
    void _onMessage(char *topic, byte *payload, unsigned int length);
};
