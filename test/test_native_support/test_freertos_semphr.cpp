// Pins the contract of the host FreeRTOS mutex shim that PR3's LittleFsBuffer
// guard is tested against: non-recursive, bounded take, and a forced-timeout
// hook for driving the lock-timeout paths deterministically.
#include <unity.h>

#include <thread>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace
{
    /** Takes from another thread: std::timed_mutex re-lock by its owner is UB. */
    BaseType_t takeFromOtherThread(SemaphoreHandle_t mutex, TickType_t ticks)
    {
        BaseType_t result = pdFALSE;
        std::thread contender([&]() {
            result = xSemaphoreTake(mutex, ticks);
            if (result == pdTRUE)
            {
                xSemaphoreGive(mutex);
            }
        });
        contender.join();
        return result;
    }
}

void setUp() {}

void tearDown()
{
    nativeForceSemaphoreTimeout(false);
}

void test_free_mutex_is_taken(void)
{
    SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
    TEST_ASSERT_NOT_NULL(mutex);

    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(mutex, pdMS_TO_TICKS(10)));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreGive(mutex));
    vSemaphoreDelete(mutex);
}

void test_held_mutex_times_out_for_another_task(void)
{
    SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
    xSemaphoreTake(mutex, portMAX_DELAY);

    TEST_ASSERT_EQUAL(pdFALSE, takeFromOtherThread(mutex, pdMS_TO_TICKS(20)));

    xSemaphoreGive(mutex);
    TEST_ASSERT_EQUAL(pdTRUE, takeFromOtherThread(mutex, pdMS_TO_TICKS(20)));
    vSemaphoreDelete(mutex);
}

void test_forced_timeout_fails_take_on_free_mutex(void)
{
    SemaphoreHandle_t mutex = xSemaphoreCreateMutex();

    nativeForceSemaphoreTimeout(true);
    TEST_ASSERT_EQUAL(pdFALSE, xSemaphoreTake(mutex, portMAX_DELAY));

    nativeForceSemaphoreTimeout(false);
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(mutex, pdMS_TO_TICKS(10)));
    xSemaphoreGive(mutex);
    vSemaphoreDelete(mutex);
}

void test_ticks_are_milliseconds(void)
{
    // ESP32 Arduino runs FreeRTOS at configTICK_RATE_HZ = 1000.
    TEST_ASSERT_EQUAL_UINT32(5000, pdMS_TO_TICKS(5000));
}

void test_give_from_non_owner_is_rejected(void)
{
    // A real FreeRTOS mutex (unlike a plain counting semaphore) tracks its
    // owner and refuses xSemaphoreGive() from any other task.
    SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
    xSemaphoreTake(mutex, portMAX_DELAY);

    BaseType_t giveResult = pdTRUE;
    std::thread contender([&]() { giveResult = xSemaphoreGive(mutex); });
    contender.join();

    TEST_ASSERT_EQUAL(pdFALSE, giveResult);
    // Still held: a take from elsewhere must still time out.
    TEST_ASSERT_EQUAL(pdFALSE, takeFromOtherThread(mutex, pdMS_TO_TICKS(20)));

    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreGive(mutex));
    vSemaphoreDelete(mutex);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_free_mutex_is_taken);
    RUN_TEST(test_held_mutex_times_out_for_another_task);
    RUN_TEST(test_forced_timeout_fails_take_on_free_mutex);
    RUN_TEST(test_ticks_are_milliseconds);
    RUN_TEST(test_give_from_non_owner_is_rejected);
    return UNITY_END();
}
