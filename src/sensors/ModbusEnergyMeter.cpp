#include "sensors/ModbusEnergyMeter.h"

#include "core/Deadline.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace
{
    // First register of the contiguous instantaneous block (Cirwatt B).
    constexpr uint16_t BLOCK_START = 0x0732;
    constexpr uint8_t BLOCK_REGISTERS = ModbusEnergyMeter::CHANNEL_COUNT * 2;

    constexpr uint8_t FUNCTION_READ_INPUT = 0x04;
    constexpr uint32_t RESPONSE_TIMEOUT_MS = 500;

    /**
     * Layout of the instantaneous block. Every value spans two 16-bit registers
     * and is a signed 32-bit integer with an implicit decimal scale — the meter
     * measures in four quadrants, so power can legitimately be negative and
     * treating these as unsigned would turn a small export into ~4.29e9.
     */
    struct ChannelSpec
    {
        const char *channel;
        const char *tag;
        Unit unit;
        float scale;
    };

    // Order matches the register block; index i starts at register 2*i.
    const ChannelSpec CHANNEL_MAP[ModbusEnergyMeter::CHANNEL_COUNT] = {
        {Channel::VOLTAGE, Tag::L1, Unit::VOLT, 0.1f},        // 0x0732
        {Channel::VOLTAGE, Tag::L2, Unit::VOLT, 0.1f},        // 0x0734
        {Channel::VOLTAGE, Tag::L3, Unit::VOLT, 0.1f},        // 0x0736
        {Channel::CURRENT, Tag::L1, Unit::AMPERE, 0.01f},     // 0x0738
        {Channel::CURRENT, Tag::L2, Unit::AMPERE, 0.01f},     // 0x073A
        {Channel::CURRENT, Tag::L3, Unit::AMPERE, 0.01f},     // 0x073C
        {Channel::POWER_FACTOR, Tag::L1, Unit::NONE, 0.01f},  // 0x073E
        {Channel::POWER_FACTOR, Tag::L2, Unit::NONE, 0.01f},  // 0x0740
        {Channel::POWER_FACTOR, Tag::L3, Unit::NONE, 0.01f},  // 0x0742
        {Channel::FREQUENCY, Tag::NONE, Unit::HERTZ, 0.1f},   // 0x0744
        {Channel::POWER, Tag::L1, Unit::KILOWATT, 0.01f},     // 0x0746
        {Channel::POWER, Tag::L2, Unit::KILOWATT, 0.01f},     // 0x0748
        {Channel::POWER, Tag::L3, Unit::KILOWATT, 0.01f},     // 0x074A
        {Channel::POWER, Tag::TOTAL, Unit::KILOWATT, 0.01f},  // 0x074C
        {Channel::REACTIVE_POWER, Tag::L1, Unit::KILOVAR, 0.01f},    // 0x074E
        {Channel::REACTIVE_POWER, Tag::L2, Unit::KILOVAR, 0.01f},    // 0x0750
        {Channel::REACTIVE_POWER, Tag::L3, Unit::KILOVAR, 0.01f},    // 0x0752
        {Channel::REACTIVE_POWER, Tag::TOTAL, Unit::KILOVAR, 0.01f}, // 0x0754
        {Channel::APPARENT_POWER, Tag::L1, Unit::KILOVOLTAMPERE, 0.01f},    // 0x0756
        {Channel::APPARENT_POWER, Tag::L2, Unit::KILOVOLTAMPERE, 0.01f},    // 0x0758
        {Channel::APPARENT_POWER, Tag::L3, Unit::KILOVOLTAMPERE, 0.01f},    // 0x075A
        {Channel::APPARENT_POWER, Tag::TOTAL, Unit::KILOVOLTAMPERE, 0.01f}, // 0x075C
    };

    /** Modbus RTU CRC-16 (polynomial 0xA001, initial value 0xFFFF). */
    uint16_t modbusCrc(const uint8_t *data, size_t length)
    {
        uint16_t crc = 0xFFFF;

        for (size_t i = 0; i < length; i++)
        {
            crc ^= data[i];
            for (uint8_t bit = 0; bit < 8; bit++)
            {
                if (crc & 0x0001)
                {
                    crc = (crc >> 1) ^ 0xA001;
                }
                else
                {
                    crc >>= 1;
                }
            }
        }

        return crc;
    }

    /**
     * Combine two 16-bit registers into a signed 32-bit value, high word first.
     *
     * TODO: verify the word order against the physical meter. Vendors differ,
     * and the wrong order yields plausible-looking garbage rather than an error.
     * Check by comparing a known phase voltage against the meter's own display.
     */
    int32_t toInt32(uint16_t high, uint16_t low)
    {
        return (int32_t)(((uint32_t)high << 16) | (uint32_t)low);
    }
}

ModbusEnergyMeter::ModbusEnergyMeter(HardwareSerial &serial, int8_t dePin,
                                     uint8_t slaveId, uint32_t baudRate)
    : _serial(serial), _dePin(dePin), _slaveId(slaveId), _baudRate(baudRate),
      _registers{}, _lastReadOk(false) {}

bool ModbusEnergyMeter::begin()
{
    if (_dePin >= 0)
    {
        pinMode(_dePin, OUTPUT);
        setTransmitMode(false);
    }

    // 8N1 is the Modbus RTU default; some meters are configured for 8E1.
    _serial.begin(_baudRate, SERIAL_8N1);

    // Probe the device: a meter that does not answer must fail here rather than
    // silently reporting invalid channels forever.
    uint16_t probe[2] = {0};
    if (!readRegisters(BLOCK_START, 2, probe))
    {
        Serial.printf("[Modbus] Medidor %u sin respuesta a %lu baudios.\n",
                      _slaveId, (unsigned long)_baudRate);
        return false;
    }

    Serial.printf("[Modbus] Medidor %u respondiendo.\n", _slaveId);
    return true;
}

void ModbusEnergyMeter::setTransmitMode(bool transmitting)
{
    if (_dePin >= 0)
    {
        digitalWrite(_dePin, transmitting ? HIGH : LOW);
    }
}

bool ModbusEnergyMeter::readRegisters(uint16_t startAddress, uint8_t registerCount,
                                      uint16_t *out)
{
    uint8_t request[8];
    request[0] = _slaveId;
    request[1] = FUNCTION_READ_INPUT;
    request[2] = (uint8_t)(startAddress >> 8);
    request[3] = (uint8_t)(startAddress & 0xFF);
    request[4] = 0x00;
    request[5] = registerCount;

    const uint16_t requestCrc = modbusCrc(request, 6);
    request[6] = (uint8_t)(requestCrc & 0xFF); // CRC travels low byte first
    request[7] = (uint8_t)(requestCrc >> 8);

    while (_serial.available())
    {
        _serial.read();
    }

    setTransmitMode(true);
    _serial.write(request, sizeof(request));
    _serial.flush(); // Must complete before releasing the bus driver
    setTransmitMode(false);

    // Response: slave, function, byte count, data, CRC (2 bytes).
    const size_t expected = 3 + (size_t)registerCount * 2 + 2;
    uint8_t response[3 + (size_t)BLOCK_REGISTERS * 2 + 2];

    if (expected > sizeof(response))
    {
        return false;
    }

    size_t received = 0;
    const uint32_t waitStart = millis();

    // Deadline.h keeps this correct across a millis() rollover, unlike a raw
    // `millis() < start + timeout` comparison. Yielding instead of
    // busy-spinning while waiting for the next byte lets other tasks run —
    // in particular sensorTask's own watchdog feed on the same core.
    while (received < expected && !Deadline::hasElapsed(millis(), waitStart, RESPONSE_TIMEOUT_MS))
    {
        if (_serial.available())
        {
            response[received++] = (uint8_t)_serial.read();
        }
        else
        {
            vTaskDelay(1);
        }
    }

    if (received < expected)
    {
        return false;
    }

    if (response[0] != _slaveId || response[1] != FUNCTION_READ_INPUT)
    {
        return false;
    }

    if (response[2] != registerCount * 2)
    {
        return false;
    }

    const uint16_t receivedCrc = (uint16_t)response[expected - 2] |
                                 ((uint16_t)response[expected - 1] << 8);
    if (receivedCrc != modbusCrc(response, expected - 2))
    {
        return false;
    }

    for (uint8_t i = 0; i < registerCount; i++)
    {
        out[i] = ((uint16_t)response[3 + i * 2] << 8) | response[4 + i * 2];
    }

    return true;
}

bool ModbusEnergyMeter::read()
{
    // The whole instantaneous block is contiguous, so one transaction refreshes
    // all 22 channels instead of 22 separate round trips over the bus.
    _lastReadOk = readRegisters(BLOCK_START, BLOCK_REGISTERS, _registers);

    if (!_lastReadOk)
    {
        Serial.println("[Modbus] Lectura fallida (timeout o CRC).");
    }

    return _lastReadOk;
}

uint8_t ModbusEnergyMeter::channelCount() const
{
    return CHANNEL_COUNT;
}

Measurement ModbusEnergyMeter::channelAt(uint8_t index) const
{
    if (index >= CHANNEL_COUNT)
    {
        return {Channel::VOLTAGE, Tag::NONE, Unit::NONE, 0.0f, false};
    }

    const ChannelSpec &spec = CHANNEL_MAP[index];

    if (!_lastReadOk)
    {
        return {spec.channel, spec.tag, spec.unit, 0.0f, false};
    }

    const int32_t raw = toInt32(_registers[index * 2], _registers[index * 2 + 1]);
    return {spec.channel, spec.tag, spec.unit, raw * spec.scale, true};
}

const char *ModbusEnergyMeter::getName() const
{
    return "CIRWATT-B";
}
