#include "Arduino.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

HardwareSerial Serial;

void HardwareSerial::begin(unsigned long)
{
    // No-op on host: there is no UART to configure.
}

void HardwareSerial::print(const char *text)
{
    std::fputs(text, stdout);
}

void HardwareSerial::println(const char *text)
{
    std::fputs(text, stdout);
    std::fputc('\n', stdout);
}

void HardwareSerial::printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    std::vprintf(format, args);
    va_end(args);
}

namespace
{
    const std::chrono::steady_clock::time_point PROCESS_START = std::chrono::steady_clock::now();
}

unsigned long millis()
{
    const auto elapsed = std::chrono::steady_clock::now() - PROCESS_START;
    return (unsigned long)std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
}

long random(long minValue, long maxValue)
{
    if (maxValue <= minValue)
    {
        return minValue;
    }

    return minValue + (std::rand() % (maxValue - minValue));
}
