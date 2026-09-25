#pragma once

#include "transport/ITransport.h"

/**
 * @brief LoRaWAN transport — INTERFACE ONLY, NOT IMPLEMENTED YET.
 *
 * The class exists so the transport can be selected, logged and reasoned about
 * before the radio hardware is available. begin() fails loudly rather than
 * pretending to work, so a node configured for LoRaWAN reports honestly instead
 * of silently dropping every reading.
 *
 * Pending before this can be implemented:
 *
 *  - Radio hardware. The ESP32-WROOM-32D has no LoRa transceiver; it needs an
 *    SX1276/SX1262 over SPI, or a board that integrates both radios.
 *  - A LoRaWAN stack (MCCI LMIC or RadioLib) and the OTAA credentials
 *    (DevEUI, JoinEUI, AppKey) issued by the network server.
 *  - The regional frequency plan, which must match the gateway.
 *  - CompactBinaryCodec: JSON does not fit in a LoRaWAN uplink.
 *
 * Downlink note: in Class A the node only listens during two short windows
 * right after transmitting, so remote configuration arrives with a latency of
 * up to one transmit interval rather than immediately.
 */
class LoRaWANTransport : public ITransport
{
public:
    /**
     * Conservative payload budget: the slowest, longest-range data rates cap the
     * application payload at roughly this many bytes. Encoding against the
     * smallest budget keeps a message valid at every data rate.
     */
    static constexpr size_t MAX_UPLINK_BYTES = 51;

    bool begin() override;
    bool isReady() const override;
    void poll() override;
    bool send(const uint8_t *payload, size_t length) override;
    size_t maxPayloadSize() const override;
    int linkQuality() const override;
    const char *name() const override;
};
