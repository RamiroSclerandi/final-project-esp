// LittleFsBuffer against the in-memory LittleFS fake in lib/native_support/:
// storage behavior plus the cross-task guard (mutex, lock timeout, peek token).
#include <unity.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

#include <LittleFS.h>
#include <freertos/semphr.h>

#include "storage/LittleFsBuffer.h"

namespace
{
    constexpr size_t RECORD_BYTES = 1000;
    constexpr size_t RECORD_FILE_BYTES = RECORD_BYTES + 1; // payload + newline
    constexpr int RECORDS_THAT_FIT = (int)(LittleFsBuffer::MAX_BYTES / RECORD_FILE_BYTES);
    constexpr int DROPS_TO_CROSS_COMPACT =
        (int)(LittleFsBuffer::COMPACT_THRESHOLD / RECORD_FILE_BYTES) + 1;
    // A handful more than DROPS_TO_CROSS_COMPACT, so the compaction test still
    // has records left pending after crossing the threshold.
    constexpr int RECORDS_FOR_COMPACT_TEST = DROPS_TO_CROSS_COMPACT + 7;

    std::string indexText(int index)
    {
        char text[5];
        std::snprintf(text, sizeof(text), "%04d", index);
        return text;
    }

    bool appendText(LittleFsBuffer &buffer, const char *text)
    {
        return buffer.append((const uint8_t *)text, std::strlen(text));
    }

    std::string peekText(LittleFsBuffer &buffer, size_t outSize = 64)
    {
        uint8_t out[RECORD_BYTES + 1] = {};
        const size_t length = buffer.peekOldest(out, outSize);
        return std::string((const char *)out, length);
    }

    /** Fixed-size record whose first four bytes are its zero-padded index. */
    bool appendIndexedRecord(LittleFsBuffer &buffer, int index)
    {
        std::string record(RECORD_BYTES, 'x');
        record.replace(0, 4, indexText(index));
        return buffer.append((const uint8_t *)record.data(), record.size());
    }

    size_t recordsFileSize()
    {
        File file = LittleFS.open("/buffer.jsonl", FILE_READ);
        const size_t size = file ? file.size() : 0;
        file.close();
        return size;
    }
}

void setUp()
{
    LittleFS.format();
}

void tearDown()
{
    nativeForceSemaphoreTimeout(false);
}

void test_begin_on_empty_fs_mounts_with_nothing_pending(void)
{
    LittleFsBuffer buffer;

    TEST_ASSERT_TRUE(buffer.begin());
    TEST_ASSERT_EQUAL_STRING("littlefs", buffer.kind());
    TEST_ASSERT_FALSE(buffer.hasPending());
    TEST_ASSERT_EQUAL_UINT32(0, buffer.pendingCount());
    TEST_ASSERT_EQUAL_UINT8(0, buffer.usedPercent());
}

void test_unmounted_buffer_rejects_append(void)
{
    LittleFsBuffer buffer;

    TEST_ASSERT_FALSE(appendText(buffer, "never-stored"));
    TEST_ASSERT_EQUAL_STRING("none", buffer.kind());
    TEST_ASSERT_FALSE(buffer.hasPending());
}

void test_append_rejects_empty_payload(void)
{
    LittleFsBuffer buffer;
    buffer.begin();

    TEST_ASSERT_FALSE(buffer.append((const uint8_t *)"", 0));
    TEST_ASSERT_EQUAL_UINT32(0, buffer.pendingCount());
}

void test_records_are_read_in_fifo_order(void)
{
    LittleFsBuffer buffer;
    buffer.begin();
    TEST_ASSERT_TRUE(appendText(buffer, "first"));
    TEST_ASSERT_TRUE(appendText(buffer, "second"));
    TEST_ASSERT_EQUAL_UINT32(2, buffer.pendingCount());

    TEST_ASSERT_EQUAL_STRING("first", peekText(buffer).c_str());
    TEST_ASSERT_TRUE(buffer.dropOldest());
    TEST_ASSERT_EQUAL_STRING("second", peekText(buffer).c_str());
    TEST_ASSERT_TRUE(buffer.dropOldest());

    TEST_ASSERT_FALSE(buffer.hasPending());
    TEST_ASSERT_FALSE(buffer.dropOldest());
    TEST_ASSERT_EQUAL_STRING("", peekText(buffer).c_str());
}

void test_peek_truncates_to_out_size_minus_terminator(void)
{
    LittleFsBuffer buffer;
    buffer.begin();
    appendText(buffer, "abcdef");

    TEST_ASSERT_EQUAL_STRING("abc", peekText(buffer, 4).c_str());
    TEST_ASSERT_EQUAL_UINT32(1, buffer.pendingCount());
}

void test_read_offset_survives_remount(void)
{
    {
        LittleFsBuffer beforeReboot;
        beforeReboot.begin();
        appendText(beforeReboot, "a");
        appendText(beforeReboot, "b");
        appendText(beforeReboot, "c");
        beforeReboot.dropOldest();
    }

    LittleFsBuffer afterReboot;
    TEST_ASSERT_TRUE(afterReboot.begin());

    TEST_ASSERT_EQUAL_UINT32(2, afterReboot.pendingCount());
    TEST_ASSERT_EQUAL_STRING("b", peekText(afterReboot).c_str());
}

void test_stale_offset_past_end_restarts_from_first_record(void)
{
    File records = LittleFS.open("/buffer.jsonl", FILE_WRITE);
    records.write((const uint8_t *)"only\n", 5);
    records.close();
    File offset = LittleFS.open("/buffer.off", FILE_WRITE);
    offset.print((size_t)9999);
    offset.close();

    LittleFsBuffer buffer;
    buffer.begin();

    TEST_ASSERT_EQUAL_UINT32(1, buffer.pendingCount());
    TEST_ASSERT_EQUAL_STRING("only", peekText(buffer).c_str());
}

void test_full_buffer_evicts_oldest_and_counts_drop(void)
{
    LittleFsBuffer buffer;
    buffer.begin();

    // One record past capacity forces exactly one eviction.
    for (int index = 0; index <= RECORDS_THAT_FIT; index++)
    {
        TEST_ASSERT_TRUE(appendIndexedRecord(buffer, index));
    }

    TEST_ASSERT_EQUAL_UINT32(1, buffer.droppedCount());
    TEST_ASSERT_EQUAL_UINT32(RECORDS_THAT_FIT, buffer.pendingCount());
    TEST_ASSERT_EQUAL_STRING("0001", peekText(buffer, 5).c_str());
    const size_t liveBytes = RECORDS_THAT_FIT * RECORD_FILE_BYTES;
    TEST_ASSERT_EQUAL_UINT8(liveBytes * 100 / LittleFsBuffer::MAX_BYTES, buffer.usedPercent());
}

void test_consumed_prefix_is_compacted_past_threshold(void)
{
    LittleFsBuffer buffer;
    buffer.begin();
    for (int index = 0; index < RECORDS_FOR_COMPACT_TEST; index++)
    {
        appendIndexedRecord(buffer, index);
    }

    for (int dropped = 0; dropped < DROPS_TO_CROSS_COMPACT; dropped++)
    {
        TEST_ASSERT_TRUE(buffer.dropOldest());
    }

    const int remaining = RECORDS_FOR_COMPACT_TEST - DROPS_TO_CROSS_COMPACT;
    TEST_ASSERT_EQUAL_UINT32(remaining, buffer.pendingCount());
    TEST_ASSERT_EQUAL_STRING(indexText(DROPS_TO_CROSS_COMPACT).c_str(),
                             peekText(buffer, 5).c_str());
    TEST_ASSERT_EQUAL_UINT32(remaining * RECORD_FILE_BYTES, recordsFileSize());
    TEST_ASSERT_EQUAL_UINT32(0, buffer.droppedCount());
}

void test_drop_after_eviction_keeps_the_unsent_record(void)
{
    LittleFsBuffer buffer;
    buffer.begin();
    for (int index = 0; index < RECORDS_THAT_FIT; index++)
    {
        appendIndexedRecord(buffer, index);
    }

    // networkTask peeks record 0; before it confirms the send, sensorTask's
    // append evicts that record. The late drop must not discard record 1.
    TEST_ASSERT_EQUAL_STRING("0000", peekText(buffer, 5).c_str());
    TEST_ASSERT_TRUE(appendIndexedRecord(buffer, RECORDS_THAT_FIT));
    TEST_ASSERT_FALSE(buffer.dropOldest());

    TEST_ASSERT_EQUAL_STRING("0001", peekText(buffer, 5).c_str());
    TEST_ASSERT_EQUAL_UINT32(RECORDS_THAT_FIT, buffer.pendingCount());
}

void test_lock_timeout_rejects_operations_without_side_effects(void)
{
    LittleFsBuffer buffer;
    buffer.begin();
    appendText(buffer, "kept");

    nativeForceSemaphoreTimeout(true);
    TEST_ASSERT_FALSE(appendText(buffer, "rejected"));
    TEST_ASSERT_EQUAL_STRING("", peekText(buffer).c_str());
    TEST_ASSERT_FALSE(buffer.dropOldest());
    nativeForceSemaphoreTimeout(false);

    TEST_ASSERT_EQUAL_UINT32(1, buffer.pendingCount());
    TEST_ASSERT_EQUAL_STRING("kept", peekText(buffer).c_str());
}

void test_concurrent_append_and_drain_serialize(void)
{
    // Enough drains to cross COMPACT_THRESHOLD, so compaction races the writer.
    constexpr int RECORD_COUNT = 2 * DROPS_TO_CROSS_COMPACT;
    LittleFsBuffer buffer;
    buffer.begin();

    std::thread writer([&]() {
        for (int index = 0; index < RECORD_COUNT; index++)
        {
            appendIndexedRecord(buffer, index);
        }
    });

    int drained = 0;
    bool isInOrder = true;
    const auto giveUpAt = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (drained < RECORD_COUNT && isInOrder && std::chrono::steady_clock::now() < giveUpAt)
    {
        const std::string head = peekText(buffer, 5);
        if (head.empty())
        {
            continue;
        }
        isInOrder = head == indexText(drained);
        drained += buffer.dropOldest() ? 1 : 0;
    }
    writer.join();

    TEST_ASSERT_TRUE(isInOrder);
    TEST_ASSERT_EQUAL(RECORD_COUNT, drained);
    TEST_ASSERT_EQUAL_UINT32(0, buffer.pendingCount());
    TEST_ASSERT_EQUAL_UINT32(0, buffer.droppedCount());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_begin_on_empty_fs_mounts_with_nothing_pending);
    RUN_TEST(test_unmounted_buffer_rejects_append);
    RUN_TEST(test_append_rejects_empty_payload);
    RUN_TEST(test_records_are_read_in_fifo_order);
    RUN_TEST(test_peek_truncates_to_out_size_minus_terminator);
    RUN_TEST(test_read_offset_survives_remount);
    RUN_TEST(test_stale_offset_past_end_restarts_from_first_record);
    RUN_TEST(test_full_buffer_evicts_oldest_and_counts_drop);
    RUN_TEST(test_consumed_prefix_is_compacted_past_threshold);
    RUN_TEST(test_drop_after_eviction_keeps_the_unsent_record);
    RUN_TEST(test_lock_timeout_rejects_operations_without_side_effects);
    RUN_TEST(test_concurrent_append_and_drain_serialize);
    return UNITY_END();
}
