#pragma once

// Minimal host stand-in for the slice of the Arduino API that this
// firmware's host-compilable logic (SensorRegistry, ConnectionBackoff)
// touches. It exists only so those files compile and link on `env:native` —
// real timing, randomness and serial output come from the ESP32 framework
// on device, never from this shim.

#include <stdint.h>

class HardwareSerial
{
public:
    void begin(unsigned long baud);
    void print(const char *text);
    void println(const char *text);
    void printf(const char *format, ...);
};

extern HardwareSerial Serial;

/** @return Milliseconds elapsed since the test process started. */
unsigned long millis();

/** @return Pseudo-random value in [minValue, maxValue). Not cryptographic. */
long random(long minValue, long maxValue);
