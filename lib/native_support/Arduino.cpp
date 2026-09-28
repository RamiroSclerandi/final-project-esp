#include "Arduino.h"

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <random>

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

    // Fixed seed keeps jitter-dependent tests reproducible run to run;
    // mt19937_64 covers a 64-bit long, unlike MinGW's 15-bit rand().
    std::mt19937_64 randomEngine(0x5EEDu);

    std::atomic<unsigned long> millisOffset{0};
}

unsigned long millis()
{
    const auto elapsed = std::chrono::steady_clock::now() - PROCESS_START;
    return (unsigned long)std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() +
           millisOffset.load();
}

void nativeAdvanceMillis(unsigned long deltaMs)
{
    millisOffset.fetch_add(deltaMs);
}

void nativeResetMillisOffset()
{
    millisOffset.store(0);
}

long random(long minValue, long maxValue)
{
    if (maxValue <= minValue)
    {
        return minValue;
    }

    // The distribution maps the range without computing maxValue - minValue,
    // so neither modulo bias nor signed overflow can occur.
    std::uniform_int_distribution<long> distribution(minValue, maxValue - 1);
    return distribution(randomEngine);
}
