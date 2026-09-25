#pragma once

// Host stand-in for the FreeRTOS mutex API, backed by std::timed_mutex. Like a
// FreeRTOS mutex it is non-recursive: re-taking it from the owning thread is
// undefined here, so tests contend from a second std::thread.

#include "FreeRTOS.h"

struct NativeSemaphore;
typedef NativeSemaphore *SemaphoreHandle_t;

SemaphoreHandle_t xSemaphoreCreateMutex();

/** @return pdTRUE if taken within `ticks`; portMAX_DELAY waits forever. */
BaseType_t xSemaphoreTake(SemaphoreHandle_t mutex, TickType_t ticks);

/** @brief Releases a mutex the caller holds. */
BaseType_t xSemaphoreGive(SemaphoreHandle_t mutex);

void vSemaphoreDelete(SemaphoreHandle_t mutex);

/**
 * @brief Test hook: while forced, every take fails immediately, so lock
 *        timeout paths can be exercised without real contention.
 */
void nativeForceSemaphoreTimeout(bool isForced);
