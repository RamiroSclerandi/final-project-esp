#pragma once

#include "core/ISensor.h"

#include <HardwareSerial.h>

/**
 * @brief Three-phase energy meter read over Modbus RTU (RS-485).
 *
 * ===========================================================================
 * PLANTILLA SIN VERIFICAR — el mapa de registros corresponde al CIRWATT serie
 * B. Antes de usarla en campo hay que confirmar el modelo real del equipo: la
 * documentación disponible cubre DOS productos distintos (Cirwatt B y
 * Cir-SET 130/320/340) y las direcciones no son necesariamente las mismas.
 * ===========================================================================
 *
 * Demonstrates that ISensor models "a source of N channels" rather than "one
 * chip": a single Modbus device contributes 22 channels through exactly the
 * same interface a two-channel BMP280 uses, with no change to the registry, the
 * codec or the acquisition task.
 *
 * All instantaneous values live in one contiguous block (0x0732..0x075D), so a
 * full refresh is a single Modbus transaction instead of 22 round trips.
 *
 * Modbus RTU framing is implemented here rather than pulled from a library:
 * the protocol is a handful of bytes plus a CRC, and keeping it explicit avoids
 * a dependency for something that still has to be validated against real
 * hardware.
 */
class ModbusEnergyMeter : public ISensor
{
public:
    /** Number of channels published: V, I, cos phi per phase, frequency, P/Q/S. */
    static constexpr uint8_t CHANNEL_COUNT = 22;

    /**
     * @param serial     UART wired to the RS-485 transceiver.
     * @param dePin      DE/RE pin of the transceiver, or -1 if it is automatic.
     * @param slaveId    Modbus address of the meter ("número de periférico").
     * @param baudRate   9600..38400 according to the meter configuration.
     */
    ModbusEnergyMeter(HardwareSerial &serial, int8_t dePin,
                      uint8_t slaveId = 1, uint32_t baudRate = 9600);

    bool begin() override;
    bool read() override;
    uint8_t channelCount() const override;
    Measurement channelAt(uint8_t index) const override;
    const char *getName() const override;

private:
    /**
     * @brief Issue function 0x04 and store the response payload.
     * @param startAddress  First register address.
     * @param registerCount Number of 16-bit registers to read.
     * @param out           Destination for registerCount 16-bit words.
     * @return true on a complete, CRC-valid response.
     */
    bool readRegisters(uint16_t startAddress, uint8_t registerCount, uint16_t *out);

    void setTransmitMode(bool transmitting);

    HardwareSerial &_serial;
    int8_t _dePin;
    uint8_t _slaveId;
    uint32_t _baudRate;

    // Raw 16-bit registers of the instantaneous block, as received.
    uint16_t _registers[CHANNEL_COUNT * 2];
    bool _lastReadOk;
};
