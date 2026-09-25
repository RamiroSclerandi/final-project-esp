#include "config/Provisioning.h"

#include "core/DeviceInfo.h"

#include <Arduino.h>

namespace
{
    void drainSerialInput()
    {
        while (Serial.available())
        {
            Serial.read();
        }
    }

    /** Blocks until a printable character arrives. */
    char waitForKey()
    {
        while (true)
        {
            if (Serial.available())
            {
                const int c = Serial.read();
                if (c > 0x20)
                {
                    return (char)c;
                }
            }
            delay(20);
        }
    }

    /** Reads a decimal number terminated by Enter. Returns fallback if empty. */
    uint32_t readNumber(uint32_t fallback)
    {
        char buffer[16] = {0};
        uint8_t length = 0;

        while (true)
        {
            if (!Serial.available())
            {
                delay(20);
                continue;
            }

            const int c = Serial.read();

            if (c == '\r' || c == '\n')
            {
                if (length == 0)
                {
                    return fallback;
                }
                Serial.println();
                return (uint32_t)strtoul(buffer, nullptr, 10);
            }

            if (c >= '0' && c <= '9' && length < sizeof(buffer) - 1)
            {
                buffer[length++] = (char)c;
                Serial.write((char)c);
            }
        }
    }

    /**
     * Reads a line terminated by Enter. When `masked`, echoes asterisks so a
     * password is not left on screen or in a terminal scrollback.
     * An empty line leaves the current value untouched.
     */
    bool readLine(char *out, size_t outSize, bool masked)
    {
        size_t length = 0;

        while (true)
        {
            if (!Serial.available())
            {
                delay(20);
                continue;
            }

            const int c = Serial.read();

            if (c == '\r' || c == '\n')
            {
                Serial.println();
                out[length] = '\0';
                return length > 0;
            }

            // Backspace and delete.
            if ((c == 8 || c == 127) && length > 0)
            {
                length--;
                Serial.print("\b \b");
                continue;
            }

            if (c >= 0x20 && c < 0x7F && length < outSize - 1)
            {
                out[length++] = (char)c;
                Serial.write(masked ? '*' : (char)c);
            }
        }
    }

    void editCredential(DeviceConfig &config, const char *key,
                        const char *label, bool masked)
    {
        char buffer[CREDENTIAL_MAX_LENGTH];

        Serial.printf("\n\n  %s (Enter = sin cambios): ", label);

        if (readLine(buffer, sizeof(buffer), masked))
        {
            config.setCredential(key, buffer);
            Serial.printf("  %s actualizado.\n", label);
        }
        else
        {
            Serial.println("  Sin cambios.");
        }
    }

    /** Renders a credential without revealing a secret. */
    void printCredential(const DeviceConfig &config, const char *key,
                         const char *label, bool masked)
    {
        char buffer[CREDENTIAL_MAX_LENGTH];
        config.credential(key, buffer, sizeof(buffer), nullptr);

        if (buffer[0] == '\0')
        {
            Serial.printf(" %s <sin configurar>\n", label);
        }
        else if (masked)
        {
            Serial.printf(" %s ********\n", label);
        }
        else
        {
            Serial.printf(" %s %s\n", label, buffer);
        }
    }

    void printMenu(const DeviceConfig &config)
    {
        Serial.println("\n========================================");
        Serial.println(" SETUP — Configuración del datalogger");
        Serial.println("========================================");
        Serial.printf(" Device ID: %s\n\n", DeviceInfo::deviceId());

        Serial.printf(" 1) Transporte ............. %s\n",
                      transportName(config.transport()));
        Serial.printf(" 2) BMP280 (presion/temp) .. %s\n",
                      config.isSensorEnabled(SensorKey::BMP280) ? "SI" : "no");
        Serial.printf(" 3) DHT22 (temp/humedad) ... %s\n",
                      config.isSensorEnabled(SensorKey::DHT22) ? "SI" : "no");
        Serial.printf(" 4) Medidor Modbus ......... %s\n",
                      config.isSensorEnabled(SensorKey::MODBUS_METER) ? "SI" : "no");
        Serial.printf(" 5) Intervalo de muestreo .. %lu ms\n",
                      (unsigned long)config.samplingIntervalMs());
        Serial.printf(" 6) Intervalo de envio ..... %lu ms\n",
                      (unsigned long)config.transmitIntervalMs());
        printCredential(config, CredentialKey::WifiSsid,
                        " 7) WiFi SSID ..............", false);
        printCredential(config, CredentialKey::WifiPassword,
                        " 8) WiFi password ..........", true);
        printCredential(config, CredentialKey::MqttHost,
                        " a) MQTT host ..............", false);
        printCredential(config, CredentialKey::MqttUser,
                        " b) MQTT usuario ...........", false);
        printCredential(config, CredentialKey::MqttPassword,
                        " c) MQTT password ..........", true);

        Serial.println("\n 9) Reset de fabrica");
        Serial.println(" 0) Guardar y arrancar");
        Serial.print("\n Opcion: ");
    }

    void chooseTransport(DeviceConfig &config)
    {
        Serial.println("\n\n --- Transporte ---");
        Serial.println("  1) WiFi + MQTT");
        Serial.println("  2) LoRaWAN");
        Serial.print("  Opcion: ");

        const char key = waitForKey();
        Serial.println(key);

        if (key == '1')
        {
            config.setTransport(TransportKind::WIFI_MQTT);
        }
        else if (key == '2')
        {
            config.setTransport(TransportKind::LORAWAN);
        }
        else
        {
            Serial.println("  Opcion invalida, sin cambios.");
        }
    }

    void toggleSensor(DeviceConfig &config, const char *sensorKey, const char *label)
    {
        const bool next = !config.isSensorEnabled(sensorKey);
        config.setSensorEnabled(sensorKey, next);
        Serial.printf("\n  %s -> %s\n", label, next ? "instalado" : "no instalado");
    }

    /** @return true when the operator asked to leave the menu. */
    bool handleChoice(DeviceConfig &config, char choice)
    {
        switch (choice)
        {
        case '1':
            chooseTransport(config);
            return false;
        case '2':
            toggleSensor(config, SensorKey::BMP280, "BMP280");
            return false;
        case '3':
            toggleSensor(config, SensorKey::DHT22, "DHT22");
            return false;
        case '4':
            toggleSensor(config, SensorKey::MODBUS_METER, "Medidor Modbus");
            return false;

        case '5':
            Serial.print("\n\n  Intervalo de muestreo en ms (Enter = sin cambios): ");
            config.setSamplingIntervalMs(readNumber(config.samplingIntervalMs()));
            return false;

        case '6':
            Serial.print("\n\n  Intervalo de envio en ms (Enter = sin cambios): ");
            config.setTransmitIntervalMs(readNumber(config.transmitIntervalMs()));
            return false;

        case '7':
            editCredential(config, CredentialKey::WifiSsid, "WiFi SSID", false);
            return false;
        case '8':
            editCredential(config, CredentialKey::WifiPassword, "WiFi password", true);
            return false;
        case 'a':
        case 'A':
            editCredential(config, CredentialKey::MqttHost, "MQTT host", false);
            return false;
        case 'b':
        case 'B':
            editCredential(config, CredentialKey::MqttUser, "MQTT usuario", false);
            return false;
        case 'c':
        case 'C':
            editCredential(config, CredentialKey::MqttPassword, "MQTT password", true);
            return false;

        case '9':
            config.factoryReset();
            Serial.println("  Reiniciando...");
            delay(500);
            ESP.restart();
            return true;

        case '0':
            return true;

        default:
            Serial.println("\n  Opcion invalida.");
            return false;
        }
    }

    bool operatorRequestedMenu()
    {
        Serial.printf("\n[Setup] Enviá cualquier tecla en %lu s para abrir la configuración...\n",
                      (unsigned long)(Provisioning::REOPEN_WINDOW_MS / 1000));

        const uint32_t deadline = millis() + Provisioning::REOPEN_WINDOW_MS;
        while (millis() < deadline)
        {
            if (Serial.available())
            {
                drainSerialInput();
                return true;
            }
            delay(50);
        }

        return false;
    }
}

void Provisioning::runIfNeeded(DeviceConfig &config)
{
    const bool firstBoot = !config.isProvisioned();

    if (firstBoot)
    {
        Serial.println("\n[Setup] Dispositivo sin configurar. Iniciando setup.");
    }
    else if (!operatorRequestedMenu())
    {
        Serial.printf("[Setup] Config vigente: transporte=%s, muestreo=%lu ms, envio=%lu ms\n",
                      transportName(config.transport()),
                      (unsigned long)config.samplingIntervalMs(),
                      (unsigned long)config.transmitIntervalMs());
        return;
    }

    drainSerialInput();

    while (true)
    {
        printMenu(config);
        const char choice = waitForKey();
        Serial.println(choice);

        if (handleChoice(config, choice))
        {
            break;
        }
    }

    // Refuse to leave setup without a transport: every later stage assumes one.
    while (config.transport() == TransportKind::UNSET)
    {
        Serial.println("\n[Setup] Falta elegir el transporte antes de continuar.");
        chooseTransport(config);
    }

    config.setProvisioned(true);
    Serial.printf("\n[Setup] Guardado. Transporte: %s\n",
                  transportName(config.transport()));
}
