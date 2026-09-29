#include "Arduino.h"

#include <atomic>
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
    // Fixed seed keeps jitter-dependent tests reproducible run to run;
    // mt19937_64 covers a 64-bit long, unlike MinGW's 15-bit rand().
    std::mt19937_64 randomEngine(0x5EEDu);

    // A pure counter, not tied to wall-clock time: real time elapsing between
    // a stamp and a read must never change the result on host, or exact-millis
    // assertions become flaky depending on how the process got scheduled.
    std::atomic<unsigned long> millisValue{0};
}

unsigned long millis()
{
    return millisValue.load();
}

void nativeAdvanceMillis(unsigned long deltaMs)
{
    millisValue.fetch_add(deltaMs);
}

void nativeResetMillisOffset()
{
    millisValue.store(0);
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
