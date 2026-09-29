#pragma once

// =============================================================================
// TEMPLATE — Copy this file to config.h and fill in your values.
// config.h is listed in .gitignore and must never be committed.
// =============================================================================

// --- WiFi ---------------------------------------------------------------
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// --- HiveMQ Cloud MQTT broker -------------------------------------------
#define MQTT_HOST "YOUR_HIVEMQ_HOST.s1.eu.hivemq.cloud"
#define MQTT_PORT 8883
#define MQTT_USER "YOUR_MQTT_USER"
#define MQTT_PASSWORD "YOUR_MQTT_PASSWORD"

// --- Device identity ---------------------------------------------------
// No se configura: se deriva de la MAC de efuse (DeviceInfo::deviceId()), de
// modo que el mismo binario en otra placa ya es otro dispositivo.
//
// --- MQTT topics -------------------------------------------------------
// Tampoco se configuran: se construyen en runtime como
//   dl/v1/<MAC>/data  |  /status  |  /config

// --- I2C bus (shared by BMP280 and any other I2C device) ----------------
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// --- DHT22 data pin ----------------------------------------------------
#define DHT_DATA_PIN 4

// --- RS-485 / Modbus (medidor de energia) ------------------------------
// Serial2 del ESP32: RX=16, TX=17 por defecto.
#define RS485_DE_PIN 5      // DE/RE del transceiver; -1 si es automatico
#define MODBUS_SLAVE_ID 1   // "Numero de periferico" del medidor
#define MODBUS_BAUD_RATE 9600

// --- SPI pins for SD card — commented until module is available --------
// ATENCION: CS=5 choca con RS485_DE_PIN=5. Elegir otro pin antes de habilitar la SD.
// #define SD_CS_PIN       5
// #define SD_MOSI_PIN     23
// #define SD_MISO_PIN     19
// #define SD_SCK_PIN      18

// --- Sampling ----------------------------------------------------------
// Solo se usa en el PRIMER arranque, para sembrar NVS. Despues manda el setup.
// Debe coincidir con DeviceConfig.cpp (5000); un config.h local con otro valor
// (p. ej. 15000) solo cambia la siembra inicial, no la configuracion ya guardada.
#define DEFAULT_SAMPLING_INTERVAL_MS 5000 // 5 segundos
