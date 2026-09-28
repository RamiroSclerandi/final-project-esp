// On-target Unity suite (real hardware only): the sliced sampling delay
// feeding the real ESP32 task watchdog, and the real cross-core mutex
// serializing LittleFsBuffer under genuine SMP contention (writer on core 0,
// drainer on core 1 — the same split as sensorTask/networkTask in main.cpp).
//
// Build-only in CI (no hardware): `pio test -e esp32dev-test
// --without-uploading`. Run for real with a device attached and flashed.
//
// PlatformIO's embedded Unity runner uses the Arduino setup()/loop() entry
// point, not main() — env:esp32dev-test supplies it instead of src/main.cpp.
#include <unity.h>

#include <atomic>
#include <cstring>

#include <Arduino.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "core/SamplingDelay.h"
#include "storage/LittleFsBuffer.h"

namespace
{
    constexpr uint32_t WDT_TIMEOUT_S = 30;
    constexpr int RECORD_COUNT = 50;

    LittleFsBuffer buffer;
    std::atomic<bool> writerDone{false};

    void writerTask(void *)
    {
        for (int index = 0; index < RECORD_COUNT; index++)
        {
            char record[8];
            snprintf(record, sizeof(record), "%04d", index);
            buffer.append((const uint8_t *)record, strlen(record));
        }
        writerDone.store(true);
        vTaskDelete(nullptr);
    }
}

void setUp() {}
void tearDown() {}

void test_wdt_is_fed_within_every_slice_of_an_interval_longer_than_one_slice(void)
{
    esp_task_wdt_init(WDT_TIMEOUT_S, true);
    esp_task_wdt_add(nullptr);

    // Just over one slice: exercises at least two feeds, nowhere near the
    // 30 s panic timeout, so a healthy run never resets the board.
    constexpr uint32_t INTERVAL_MS = SamplingDelay::WDT_SLICE_MS + 3000;
    const uint32_t waitStart = millis();
    uint32_t feeds = 0;

    while (true)
    {
        esp_task_wdt_reset();
        feeds++;

        const uint32_t chunk = SamplingDelay::nextChunkMs(millis() - waitStart, INTERVAL_MS,
                                                          SamplingDelay::WDT_SLICE_MS);
        if (chunk == 0)
        {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(chunk));
    }

    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(2, feeds);
}

void test_concurrent_append_and_drain_across_cores_does_not_corrupt(void)
{
    TEST_ASSERT_TRUE(buffer.begin());
    xTaskCreatePinnedToCore(writerTask, "Writer", 4096, nullptr, 1, nullptr, 0);

    int drained = 0;
    bool isCorrupted = false;
    uint8_t out[16];

    while (!writerDone.load() || buffer.hasPending())
    {
        const size_t length = buffer.peekOldest(out, sizeof(out));
        if (length > 0)
        {
            char expected[8];
            snprintf(expected, sizeof(expected), "%04d", drained);
            out[length] = '\0';
            isCorrupted = isCorrupted || strcmp((const char *)out, expected) != 0;

            if (buffer.dropOldest())
            {
                drained++;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    TEST_ASSERT_FALSE(isCorrupted);
    TEST_ASSERT_EQUAL(RECORD_COUNT, drained);
}

void setup()
{
    delay(2000); // Let the serial monitor attach before Unity starts printing.
    UNITY_BEGIN();
    RUN_TEST(test_wdt_is_fed_within_every_slice_of_an_interval_longer_than_one_slice);
    RUN_TEST(test_concurrent_append_and_drain_across_cores_does_not_corrupt);
    UNITY_END();
}

void loop() {}
