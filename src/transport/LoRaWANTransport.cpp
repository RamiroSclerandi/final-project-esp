#include "transport/LoRaWANTransport.h"

#include <Arduino.h>

bool LoRaWANTransport::begin()
{
    Serial.println("[LoRaWAN] NO IMPLEMENTADO — falta el radio SX1276/SX1262 y el stack.");
    Serial.println("[LoRaWAN] El nodo no va a transmitir. Elegí WiFi en el setup (tecla 1).");
    return false;
}

bool LoRaWANTransport::isReady() const
{
    return false;
}

void LoRaWANTransport::poll()
{
    // Nothing to service until the radio stack exists.
}

bool LoRaWANTransport::send(const uint8_t *payload, size_t length)
{
    (void)payload;
    (void)length;
    return false;
}

size_t LoRaWANTransport::maxPayloadSize() const
{
    return MAX_UPLINK_BYTES;
}

int LoRaWANTransport::linkQuality() const
{
    return 0;
}

uint32_t LoRaWANTransport::millisOffline() const
{
    // No radio stack yet: restarting cannot bring the link up, it would only
    // reset seq and meta.lost and bump meta.boot every 30 min.
    return 0;
}

const char *LoRaWANTransport::name() const
{
    return "lorawan";
}
