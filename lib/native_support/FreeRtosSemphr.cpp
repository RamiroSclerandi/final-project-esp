#include "freertos/semphr.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

struct NativeSemaphore
{
    std::timed_mutex mutex;
    // Tracks the owning task like a real FreeRTOS mutex (unlike a plain
    // counting semaphore), so a give from any other task is rejected instead
    // of corrupting the lock state.
    std::atomic<std::thread::id> owner{};
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

    bool isLocked;
    if (ticks == portMAX_DELAY)
    {
        mutex->mutex.lock();
        isLocked = true;
    }
    else
    {
        isLocked = mutex->mutex.try_lock_for(std::chrono::milliseconds(ticks));
    }

    if (isLocked)
    {
        mutex->owner.store(std::this_thread::get_id());
    }
    return isLocked ? pdTRUE : pdFALSE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t mutex)
{
    if (mutex == nullptr || mutex->owner.load() != std::this_thread::get_id())
    {
        return pdFALSE;
    }

    mutex->owner.store(std::thread::id());
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
