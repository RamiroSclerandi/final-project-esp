#include <Arduino.h>
#include <Wire.h>
#include <atomic>
#include <esp_task_wdt.h>

#include "config.h"
#include "certificates.h"

#include "config/DeviceConfig.h"
#include "config/Provisioning.h"
#include "core/DeviceInfo.h"
#include "core/MeasurementAccumulator.h"
#include "core/SamplingDelay.h"
#include "core/TransmitSchedule.h"
#include "codec/JsonCodec.h"
#include "sensors/SensorRegistry.h"
#include "sensors/BMP280Sensor.h"
#include "sensors/DHT22Sensor.h"
#include "sensors/ModbusEnergyMeter.h"
#include "storage/LittleFsBuffer.h"
#include "transport/WiFiMqttTransport.h"
#include "transport/LoRaWANTransport.h"
#include "MQTTManager.h"

// Fallbacks so a config.h written before these sensors existed still compiles.
#ifndef DHT_DATA_PIN
#define DHT_DATA_PIN 4
#endif
#ifndef RS485_DE_PIN
#define RS485_DE_PIN 5
#endif
#ifndef MODBUS_SLAVE_ID
#define MODBUS_SLAVE_ID 1
#endif
#ifndef MODBUS_BAUD_RATE
#define MODBUS_BAUD_RATE 9600
#endif

// Must comfortably exceed the longest legitimate blocking operation, which is
// the WiFi association plus the TLS handshake (5-15 s in the worst case). A
// watchdog that fires on a healthy system is worse than none: it produces a
// reboot loop that looks exactly like the fault it was meant to catch.
static constexpr uint32_t WDT_TIMEOUT_S = 30;

// Last resort for a link that never recovers. Only safe now that unsent
// readings are persisted locally: without the buffer, a restart would discard
// everything still queued in RAM.
static constexpr uint32_t OFFLINE_RESTART_MS = 30UL * 60UL * 1000UL;

struct SensorReading
{
    char payload[1024]; // Sized for the full Modbus meter: 22 channels
};

// ---------------------------------------------------------------------------
// Sensors
//
// Adding a sensor means writing its two files, adding one registry.add() below
// and one entry in the provisioning menu. Which of them are actually polled is
// a runtime decision, so the same binary runs on a board with only a BMP280 and
// on a fully populated one without reporting channels for absent hardware.
// ---------------------------------------------------------------------------
static BMP280Sensor bmpSensor(Wire, 0x76);
static DHT22Sensor dhtSensor(DHT_DATA_PIN);
static ModbusEnergyMeter energyMeter(Serial2, RS485_DE_PIN,
                                     MODBUS_SLAVE_ID, MODBUS_BAUD_RATE);

static SensorRegistry registry;
static MeasurementAccumulator accumulator;

// ---------------------------------------------------------------------------
// Global objects
// ---------------------------------------------------------------------------
static DeviceConfig deviceConfig;
static JsonCodec codec;
static LittleFsBuffer localBuffer;

static MQTTManager mqttManager(ROOT_CA_CERT);
static WiFiMqttTransport wifiTransport(mqttManager);
static LoRaWANTransport loraTransport;

// Selected during provisioning; every stage below talks to the interface only.
static ITransport *transport = nullptr;

static QueueHandle_t dataQueue;
static std::atomic<uint32_t> samplingInterval;
static std::atomic<uint32_t> transmitInterval;
static std::atomic<uint32_t> sequenceCounter;
static uint32_t bootCount = 0;

// Reported once, on the first message after a restart.
static const char *pendingResetReason = nullptr;

// ---------------------------------------------------------------------------
// Fills the node telemetry that travels alongside the readings.
// ---------------------------------------------------------------------------
static PayloadMeta buildMeta()
{
    PayloadMeta meta;
    meta.rssi = transport != nullptr ? transport->linkQuality() : 0;
    meta.sequence = sequenceCounter.fetch_add(1);
    meta.bootCount = bootCount;

    meta.resetReason = pendingResetReason;
    pendingResetReason = nullptr;

    meta.storeKind = localBuffer.kind();
    meta.storeUsedPct = localBuffer.usedPercent();
    meta.storePending = localBuffer.pendingCount();
    meta.storeDropped = localBuffer.droppedCount();

    return meta;
}

// ---------------------------------------------------------------------------
// Sleeps one sampling interval in watchdog-sized chunks. The interval is
// re-read per chunk so a remote change applies within the current wait.
//
// While the first send is still pending, the wait is capped to the first-send
// timeout: otherwise a long samplingInterval would delay it well past that
// timeout, since the transmit gate is only re-checked once the wait returns.
// ---------------------------------------------------------------------------
static void waitForNextSample(bool firstSendPending)
{
    const uint32_t waitStart = millis();
    while (true)
    {
        esp_task_wdt_reset();
        const uint32_t effectiveInterval =
            SamplingDelay::effectiveIntervalMs(samplingInterval.load(), firstSendPending,
                                               TransmitSchedule::FIRST_SEND_SYNC_TIMEOUT_MS);
        const uint32_t chunk = SamplingDelay::nextChunkMs(millis() - waitStart, effectiveInterval,
                                                          SamplingDelay::WDT_SLICE_MS);
        if (chunk == 0)
        {
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(chunk));
    }
}

// ---------------------------------------------------------------------------
// sensorTask — Core 0
//
// Samples every registered sensor at the sampling interval and emits one
// message per transmit interval. When the two intervals are equal every sample
// is sent raw; when they differ the samples in between are folded into mean,
// min, max and count, because a transport that allows only a few messages per
// day cannot carry them individually.
// ---------------------------------------------------------------------------
static void sensorTask(void *pvParameters)
{
    esp_task_wdt_add(nullptr);

    accumulator.reset(registry);
    const uint32_t taskStart = millis();
    uint32_t lastTransmit = taskStart;
    bool first = true;

    while (true)
    {
        esp_task_wdt_reset();

        registry.readAll();
        accumulator.accumulate(registry);

        const uint32_t now = millis();
        const bool isDue =
            first
                ? TransmitSchedule::isFirstSendAllowed(DeviceInfo::isClockSynced(), now, taskStart)
                : TransmitSchedule::isDue(now, lastTransmit, transmitInterval.load(),
                                          samplingInterval.load());
        if (isDue)
        {
            first = false;
            lastTransmit = now;

            const PayloadMeta meta = buildMeta();

            SensorReading reading;
            const size_t written = codec.encode(accumulator, meta,
                                                (uint8_t *)reading.payload,
                                                sizeof(reading.payload));

            // The window is closed regardless of the outcome, so a failed
            // encode cannot let stale samples leak into the next message.
            accumulator.reset(registry);

            if (written == 0)
            {
                Serial.println("[Sensor] El payload no entra en el buffer — descartado.");
            }
            else if (transport != nullptr && written > transport->maxPayloadSize())
            {
                Serial.printf("[Sensor] Payload de %u B supera el limite de %s (%u B).\n",
                              (unsigned)written, transport->name(),
                              (unsigned)transport->maxPayloadSize());
            }
            else if (xQueueSend(dataQueue, &reading, pdMS_TO_TICKS(100)) != pdTRUE)
            {
                // The link is down and the in-RAM queue is full. Persisting here
                // is what keeps the reading instead of dropping it.
                if (localBuffer.append((const uint8_t *)reading.payload, written))
                {
                    Serial.printf("[Sensor] Cola llena — guardado local (%lu pendientes).\n",
                                  (unsigned long)localBuffer.pendingCount());
                }
                else
                {
                    Serial.println("[Sensor] Cola llena y sin respaldo — lectura PERDIDA.");
                }
            }
        }

        waitForNextSample(first);
    }
}

// ---------------------------------------------------------------------------
// Sends one buffered record, oldest first.
//
// Rate-limited to one record per iteration on purpose: a node that was offline
// for days has thousands of pending records, and draining them at full speed
// would starve the live measurements — exactly backwards.
// ---------------------------------------------------------------------------
static void replayOneBufferedRecord()
{
    static uint8_t record[1024];

    const size_t length = localBuffer.peekOldest(record, sizeof(record));
    if (length == 0)
    {
        return;
    }

    if (transport->send(record, length))
    {
        // Removed only after a confirmed send. If the removal is lost to a
        // reset the record is sent twice, which the database deduplicates on
        // (sensor_id, timestamp) — the safe direction to fail in.
        localBuffer.dropOldest();
    }
}

// ---------------------------------------------------------------------------
// networkTask — Core 1
// ---------------------------------------------------------------------------
static void networkTask(void *pvParameters)
{
    esp_task_wdt_add(nullptr);

    while (true)
    {
        esp_task_wdt_reset();

        transport->poll();
        DeviceInfo::serviceTimeSync(transport->isReady());

        SensorReading reading;
        if (xQueueReceive(dataQueue, &reading, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            const size_t length = strlen(reading.payload);

            if (transport->send((const uint8_t *)reading.payload, length))
            {
                Serial.printf("[%s] Enviado: %s\n", transport->name(), reading.payload);
            }
            else if (!localBuffer.append((const uint8_t *)reading.payload, length))
            {
                Serial.printf("[%s] Envio fallido y sin respaldo — PERDIDO.\n",
                              transport->name());
            }
        }
        else if (transport->isReady() && localBuffer.hasPending())
        {
            // Nothing live to send: use the idle slot to drain the backlog.
            replayOneBufferedRecord();
        }

        const uint32_t offlineFor = mqttManager.millisSinceConnected();
        if (offlineFor > OFFLINE_RESTART_MS)
        {
            Serial.println("[Network] Sin conexion prolongada. Reiniciando...");
            delay(200);
            ESP.restart();
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ---------------------------------------------------------------------------
// Registers the sensors marked as installed during provisioning.
// ---------------------------------------------------------------------------
static void registerConfiguredSensors()
{
    if (deviceConfig.isSensorEnabled(SensorKey::BMP280))
    {
        registry.add(&bmpSensor);
    }

    if (deviceConfig.isSensorEnabled(SensorKey::DHT22))
    {
        registry.add(&dhtSensor);
    }

    if (deviceConfig.isSensorEnabled(SensorKey::MODBUS_METER))
    {
        registry.add(&energyMeter);
    }

    if (registry.sensorCount() == 0)
    {
        Serial.println("[WARNING] Ningun sensor habilitado. Abri el setup para activarlos.");
    }
}

// ---------------------------------------------------------------------------
// Loads credentials from NVS, falling back to the compile-time values so a
// freshly flashed board keeps working without anyone typing anything.
// ---------------------------------------------------------------------------
static void applyStoredCredentials()
{
    char ssid[CREDENTIAL_MAX_LENGTH];
    char wifiPass[CREDENTIAL_MAX_LENGTH];
    char host[CREDENTIAL_MAX_LENGTH];
    char user[CREDENTIAL_MAX_LENGTH];
    char pass[CREDENTIAL_MAX_LENGTH];

    deviceConfig.credential(CredentialKey::WifiSsid, ssid, sizeof(ssid), WIFI_SSID);
    deviceConfig.credential(CredentialKey::WifiPassword, wifiPass, sizeof(wifiPass), WIFI_PASSWORD);
    deviceConfig.credential(CredentialKey::MqttHost, host, sizeof(host), MQTT_HOST);
    deviceConfig.credential(CredentialKey::MqttUser, user, sizeof(user), MQTT_USER);
    deviceConfig.credential(CredentialKey::MqttPassword, pass, sizeof(pass), MQTT_PASSWORD);

    wifiTransport.setCredentials(ssid, wifiPass);
    mqttManager.configure(host, MQTT_PORT, user, pass);

    Serial.printf("[Config] WiFi \"%s\", broker %s:%d\n", ssid, host, MQTT_PORT);
}

static ITransport *selectTransport()
{
    switch (deviceConfig.transport())
    {
    case TransportKind::LORAWAN:
        return &loraTransport;
    case TransportKind::WIFI_MQTT:
    default:
        return &wifiTransport;
    }
}

// ---------------------------------------------------------------------------
// setup
// ---------------------------------------------------------------------------
void setup()
{
    Serial.begin(115200);
    delay(500);

    pendingResetReason = DeviceInfo::resetReason();

    Serial.println("\n========================================");
    Serial.println(" Datalogger ESP32 — Booting");
    Serial.printf(" Device ID: %s\n", DeviceInfo::deviceId());
    Serial.printf(" Reinicio previo: %s\n", pendingResetReason);
    Serial.println("========================================");

    // 1. Restore configuration and run the setup menu if needed
    deviceConfig.begin();

    // Seed the factory defaults from config.h on the very first boot, so the
    // compile-time value is the starting point rather than dead configuration.
    // From then on NVS is the source of truth and this never runs again.
    if (!deviceConfig.isProvisioned())
    {
        deviceConfig.setSamplingIntervalMs(DEFAULT_SAMPLING_INTERVAL_MS);
        deviceConfig.setTransmitIntervalMs(DEFAULT_SAMPLING_INTERVAL_MS);
    }

    Provisioning::runIfNeeded(deviceConfig);

    samplingInterval.store(deviceConfig.samplingIntervalMs());
    transmitInterval.store(deviceConfig.transmitIntervalMs());
    sequenceCounter.store(0);
    bootCount = deviceConfig.nextBootCount();
    Serial.printf("[Config] Arranque numero %lu\n", (unsigned long)bootCount);

    // 2. Credentials: NVS is the source of truth, seeded on first boot from
    //    config.h so the existing build-and-go workflow keeps working.
    applyStoredCredentials();

    // 3. Local buffer, before anything can produce a reading to persist
    localBuffer.begin();

    // 4. Initialize shared buses before any sensor uses them
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

    // 5. Register and initialize the configured sensors
    registerConfiguredSensors();
    registry.beginAll();

    // 6. Bring up the selected transport
    transport = selectTransport();
    Serial.printf("[Transport] Seleccionado: %s\n", transport->name());

    if (!transport->begin())
    {
        Serial.println("[Transport] No inicializo — se reintenta en segundo plano.");
    }

    // 7. Remote configuration updates (MQTT only for now)
    mqttManager.setConfigCallback([](int newInterval)
                                  {
        samplingInterval.store((uint32_t)newInterval);
        deviceConfig.setSamplingIntervalMs((uint32_t)newInterval);
        if (transmitInterval.load() < (uint32_t)newInterval)
        {
            transmitInterval.store((uint32_t)newInterval);
        } });

    // 8. Create inter-task queue
    dataQueue = xQueueCreate(10, sizeof(SensorReading));
    if (dataQueue == nullptr)
    {
        Serial.println("[ERROR] No se pudo crear la cola FreeRTOS. Reiniciando...");
        ESP.restart();
    }

    // 9. Watchdog, armed before the tasks it supervises exist
    esp_task_wdt_init(WDT_TIMEOUT_S, true); // true = panic and reboot on timeout

    // 10. Spawn FreeRTOS tasks
    xTaskCreatePinnedToCore(sensorTask, "SensorTask", 8192, nullptr, 1, nullptr, 0);
    xTaskCreatePinnedToCore(networkTask, "NetworkTask", 8192, nullptr, 1, nullptr, 1);

    Serial.printf("[Setup] Listo. Muestreo cada %lu ms, envio cada %lu ms.\n",
                  (unsigned long)samplingInterval.load(),
                  (unsigned long)transmitInterval.load());
    Serial.println("========================================\n");
}

// ---------------------------------------------------------------------------
// loop — intentionally empty.
// FreeRTOS manages all work through the pinned tasks above. Deleting the
// Arduino loopTask frees ~8KB of stack on Core 1.
// ---------------------------------------------------------------------------
void loop()
{
    vTaskDelete(nullptr);
}
