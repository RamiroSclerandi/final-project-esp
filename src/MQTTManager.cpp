#include "MQTTManager.h"

#include "core/DeviceInfo.h"

#include <WiFi.h>
#include <Arduino.h>
#include <ArduinoJson.h>

namespace
{
    // Versioned topic prefix so a future contract change can coexist with
    // already-deployed nodes instead of forcing a simultaneous upgrade.
    constexpr char TOPIC_PREFIX[] = "dl/v1";

    constexpr char STATUS_ONLINE[] = "online";
    constexpr char STATUS_OFFLINE[] = "offline";

    constexpr uint8_t WILL_QOS = 1;
    constexpr bool WILL_RETAIN = true;

    /**
     * Lists the networks in range after a failed association.
     *
     * The ESP32 radio is 2.4 GHz only, so an access point published solely on
     * 5 GHz — or a dual-band router steering both bands under one name — is
     * invisible to it even though every phone in the house sees it. Printing
     * what the radio can actually see distinguishes that from a wrong password
     * or a weak signal, which the connection error alone cannot.
     */
    void diagnoseWiFiFailure(const char *ssid)
    {
        Serial.println("[WiFi] Escaneando redes 2.4 GHz visibles...");

        // Cancel the pending association without powering the radio down.
        // WiFi.disconnect(true) sets wifioff, not eraseap: it switches the
        // radio OFF, and scanning right after races it coming back up, which
        // reports an empty airspace that looks like a hardware fault.
        WiFi.disconnect(false);
        delay(500);

        // Synchronous scan including hidden networks.
        const int found = WiFi.scanNetworks(false, true);

        // A negative result means the scan never ran; zero means it ran and the
        // airspace really is empty. Conflating them hides which fault it is.
        if (found < 0)
        {
            Serial.printf("[WiFi] El escaneo no pudo ejecutarse (codigo %d).\n", found);
            Serial.println("[WiFi] La radio no responde: sospecha de alimentacion insuficiente.");
            return;
        }

        if (found == 0)
        {
            Serial.println("[WiFi] 0 redes detectadas, ni siquiera ajenas.");
            Serial.println("[WiFi] Causa tipica: cable USB de baja calidad o puerto sin corriente.");
            Serial.println("[WiFi] El WiFi consume picos de ~350 mA; la placa arranca igual y la radio no.");
            return;
        }

        bool targetVisible = false;

        for (int i = 0; i < found; i++)
        {
            const bool isTarget = WiFi.SSID(i).equals(ssid);
            targetVisible = targetVisible || isTarget;

            Serial.printf("  %c \"%s\"  %d dBm  canal %d\n",
                          isTarget ? '>' : ' ',
                          WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i));
        }

        if (targetVisible)
        {
            Serial.printf("[WiFi] \"%s\" SI es visible: revisa la contrasena.\n", ssid);
            WiFi.scanDelete();
            return;
        }

        // Before blaming the band, look for a near match. A leading or trailing
        // space in an SSID is invisible in every router UI and makes the name
        // look identical while never matching — the failure is indistinguishable
        // from an absent network unless it is checked for explicitly.
        String target(ssid);
        target.trim();

        for (int i = 0; i < found; i++)
        {
            String candidate = WiFi.SSID(i);
            String trimmed = candidate;
            trimmed.trim();

            if (trimmed.equalsIgnoreCase(target) && !candidate.equals(ssid))
            {
                Serial.printf("[WiFi] Hay una red casi igual: \"%s\" (%d caracteres).\n",
                              candidate.c_str(), candidate.length());
                Serial.printf("[WiFi] Vos configuraste \"%s\" (%d caracteres).\n",
                              ssid, (int)strlen(ssid));
                Serial.println("[WiFi] Copia el nombre EXACTO, espacios incluidos, en el setup.");
                WiFi.scanDelete();
                return;
            }
        }

        Serial.printf("[WiFi] \"%s\" no aparece entre las redes de 2.4 GHz.\n", ssid);
        Serial.println("[WiFi] Si el celular la ve, probablemente sea 5 GHz.");

        WiFi.scanDelete();
    }
}

// Static instance pointer — required for the PubSubClient C-style callback.
MQTTManager *MQTTManager::_instance = nullptr;

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

MQTTManager::MQTTManager(const char *caCert)
    : _host{}, _port(8883), _user{}, _password{},
      _caCert(caCert),
      _dataTopic{}, _statusTopic{}, _configTopic{}, _topicsBuilt(false),
      _mqttClient(_wifiClient)
{
    _instance = this;
    _mqttClient.setCallback(_onMessageStatic);
    _mqttClient.setBufferSize(1024); // Fits the datalogger.v1 payload plus header
    _mqttClient.setKeepAlive(60);
}

void MQTTManager::configure(const char *host, uint16_t port,
                            const char *user, const char *password)
{
    snprintf(_host, sizeof(_host), "%s", host != nullptr ? host : "");
    snprintf(_user, sizeof(_user), "%s", user != nullptr ? user : "");
    snprintf(_password, sizeof(_password), "%s", password != nullptr ? password : "");
    _port = port;

    _mqttClient.setServer(_host, _port);
}

// ---------------------------------------------------------------------------
// Topics
// ---------------------------------------------------------------------------

void MQTTManager::buildTopics()
{
    if (_topicsBuilt)
    {
        return;
    }

    const char *id = DeviceInfo::deviceId();
    snprintf(_dataTopic, sizeof(_dataTopic), "%s/%s/data", TOPIC_PREFIX, id);
    snprintf(_statusTopic, sizeof(_statusTopic), "%s/%s/status", TOPIC_PREFIX, id);
    snprintf(_configTopic, sizeof(_configTopic), "%s/%s/config", TOPIC_PREFIX, id);
    _topicsBuilt = true;
}

const char *MQTTManager::dataTopic()
{
    buildTopics();
    return _dataTopic;
}

const char *MQTTManager::statusTopic()
{
    buildTopics();
    return _statusTopic;
}

const char *MQTTManager::configTopic()
{
    buildTopics();
    return _configTopic;
}

// ---------------------------------------------------------------------------
// WiFi
// ---------------------------------------------------------------------------

bool MQTTManager::connectWiFi(const char *ssid, const char *wifiPassword)
{
    if (WiFi.isConnected())
        return true;

    // Set station mode explicitly. WiFi.begin() enables it implicitly, but
    // leaving it implicit means the radio's state depends on whatever ran
    // before — a previous scan, for instance.
    WiFi.mode(WIFI_STA);

    // Modem sleep drops beacons on a marginal link and turns a weak but usable
    // signal into an association that never completes.
    WiFi.setSleep(false);

    Serial.printf("[WiFi] Conectando a \"%s\"", ssid);
    WiFi.begin(ssid, wifiPassword);

    for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
        Serial.print(".");
    }

    if (WiFi.isConnected())
    {
        Serial.printf("\n[WiFi] Conectado. IP: %s  RSSI: %d dBm\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());
        return true;
    }

    Serial.println("\n[WiFi] Conexion fallida.");
    diagnoseWiFiFailure(ssid);
    return false;
}

// ---------------------------------------------------------------------------
// MQTT
// ---------------------------------------------------------------------------

bool MQTTManager::connectMQTT()
{
    buildTopics();
    _wifiClient.setCACert(_caCert);

    // The client ID is the device identifier itself: one identity for the
    // topic, the payload, the broker session and the database row.
    const char *clientId = DeviceInfo::deviceId();

    Serial.printf("[MQTT] Conectando a %s:%d como \"%s\"...\n",
                  _host, _port, clientId);

    // The last will is registered with the broker at connect time and published
    // by the broker if the connection drops without a clean disconnect — a
    // power loss, for instance, which the device could never report itself.
    const bool connected = _mqttClient.connect(
        clientId, _user, _password,
        _statusTopic, WILL_QOS, WILL_RETAIN, STATUS_OFFLINE);

    if (!connected)
    {
        Serial.printf("[MQTT] Conexion fallida. Estado PubSubClient: %d\n",
                      _mqttClient.state());
        return false;
    }

    Serial.println("[MQTT] Conectado.");

    // Retained so a consumer connecting later immediately learns the state
    // instead of waiting for the next transition.
    _mqttClient.publish(_statusTopic, STATUS_ONLINE, WILL_RETAIN);

    _mqttClient.subscribe(_configTopic);
    Serial.printf("[MQTT] Suscripto a configuracion: %s\n", _configTopic);
    Serial.printf("[MQTT] Publicando datos en: %s\n", _dataTopic);

    return true;
}

// ---------------------------------------------------------------------------
// ensureConnected
// ---------------------------------------------------------------------------

bool MQTTManager::ensureConnected()
{
    if (WiFi.isConnected() && _mqttClient.connected())
    {
        _backoff.recordSuccess();
        return true;
    }

    // Pace the attempts instead of retrying as fast as the task loops.
    if (!_backoff.shouldRetry())
    {
        return false;
    }

    if (!WiFi.isConnected())
    {
        Serial.printf("[WiFi] Sin conexion. Reintento %lu...\n",
                      (unsigned long)_backoff.consecutiveFailures() + 1);
        WiFi.disconnect();
        WiFi.reconnect();

        // Short, non-blocking wait: the task must keep running.
        for (int i = 0; i < 10 && !WiFi.isConnected(); i++)
        {
            vTaskDelay(pdMS_TO_TICKS(200));
        }

        if (!WiFi.isConnected())
        {
            _backoff.recordFailure();
            return false;
        }
        Serial.println("[WiFi] Reconectado.");
    }

    if (!connectMQTT())
    {
        _backoff.recordFailure();
        return false;
    }

    _backoff.recordSuccess();
    return true;
}

uint32_t MQTTManager::millisSinceConnected() const
{
    return _backoff.millisSinceSuccess();
}

// ---------------------------------------------------------------------------
// Publish
// ---------------------------------------------------------------------------

bool MQTTManager::publish(const char *topic, const char *payload, bool retained)
{
    if (!_mqttClient.connected())
        return false;
    return _mqttClient.publish(topic, payload, retained);
}

// ---------------------------------------------------------------------------
// Loop
// ---------------------------------------------------------------------------

void MQTTManager::loop()
{
    _mqttClient.loop();
}

// ---------------------------------------------------------------------------
// Config callback registration
// ---------------------------------------------------------------------------

void MQTTManager::setConfigCallback(std::function<void(int)> cb)
{
    _configCallback = cb;
}

// ---------------------------------------------------------------------------
// Incoming message handler
// ---------------------------------------------------------------------------

void MQTTManager::_onMessageStatic(char *topic, byte *payload, unsigned int length)
{
    if (_instance != nullptr)
    {
        _instance->_onMessage(topic, payload, length);
    }
}

void MQTTManager::_onMessage(char *topic, byte *payload, unsigned int length)
{
    Serial.printf("[MQTT] Mensaje recibido en: %s\n", topic);

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err)
    {
        Serial.printf("[MQTT] Error de parseo JSON: %s\n", err.c_str());
        return;
    }

    if (doc["samplingInterval"].is<int>())
    {
        int newInterval = doc["samplingInterval"].as<int>();

        // Sanity check: accept values between 1 second and 5 minutes.
        if (newInterval >= 1000 && newInterval <= 300000)
        {
            Serial.printf("[MQTT] Config remota — samplingInterval: %d ms\n",
                          newInterval);
            if (_configCallback)
            {
                _configCallback(newInterval);
            }
        }
        else
        {
            Serial.printf("[MQTT] samplingInterval fuera de rango, rechazado: %d\n",
                          newInterval);
        }
    }
}
