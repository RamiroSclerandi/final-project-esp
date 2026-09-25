#pragma once

// Host stand-in for the FreeRTOS base types the firmware uses. Ticks are
// milliseconds because ESP32 Arduino builds FreeRTOS with
// configTICK_RATE_HZ = 1000.

#include <stdint.h>

typedef int BaseType_t;
typedef uint32_t TickType_t;

#define pdFALSE ((BaseType_t)0)
#define pdTRUE ((BaseType_t)1)
#define portMAX_DELAY ((TickType_t)0xffffffffUL)
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
