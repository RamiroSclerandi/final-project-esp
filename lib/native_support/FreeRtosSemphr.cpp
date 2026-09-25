#include "freertos/semphr.h"

#include <atomic>
#include <chrono>
#include <mutex>

struct NativeSemaphore
{
    std::timed_mutex mutex;
};

namespace
{
    std::atomic<bool> isTimeoutForced{false};
}

SemaphoreHandle_t xSemaphoreCreateMutex()
{
    return new NativeSemaphore();
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t mutex, TickType_t ticks)
{
    if (mutex == nullptr || isTimeoutForced.load())
    {
        return pdFALSE;
    }

    if (ticks == portMAX_DELAY)
    {
        mutex->mutex.lock();
        return pdTRUE;
    }

    return mutex->mutex.try_lock_for(std::chrono::milliseconds(ticks)) ? pdTRUE : pdFALSE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t mutex)
{
    if (mutex == nullptr)
    {
        return pdFALSE;
    }

    mutex->mutex.unlock();
    return pdTRUE;
}

void vSemaphoreDelete(SemaphoreHandle_t mutex)
{
    delete mutex;
}

void nativeForceSemaphoreTimeout(bool isForced)
{
    isTimeoutForced.store(isForced);
}
