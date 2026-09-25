// Characterizes the CURRENT behavior of LittleFsBuffer (v1.1.0) against the
// in-memory LittleFS fake in lib/native_support/. No production logic
// changes in this PR; the concurrency guard and its RED test land in PR3.
#include <unity.h>

#include <cstdio>
#include <cstring>
#include <string>

#include <LittleFS.h>

#include "storage/LittleFsBuffer.h"

namespace
{
    constexpr size_t RECORD_BYTES = 1000;

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
        char prefix[5];
        std::snprintf(prefix, sizeof(prefix), "%04d", index);
        record.replace(0, 4, prefix);
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

void tearDown() {}

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

    // 204 records of 1001 bytes (payload + newline) fit under MAX_BYTES; the
    // 205th forces exactly one eviction.
    for (int index = 0; index < 205; index++)
    {
        TEST_ASSERT_TRUE(appendIndexedRecord(buffer, index));
    }

    TEST_ASSERT_EQUAL_UINT32(1, buffer.droppedCount());
    TEST_ASSERT_EQUAL_UINT32(204, buffer.pendingCount());
    TEST_ASSERT_EQUAL_STRING("0001", peekText(buffer, 5).c_str());
    TEST_ASSERT_EQUAL_UINT8(99, buffer.usedPercent());
}

void test_consumed_prefix_is_compacted_past_threshold(void)
{
    LittleFsBuffer buffer;
    buffer.begin();
    for (int index = 0; index < 40; index++)
    {
        appendIndexedRecord(buffer, index);
    }

    // 33 x 1001 bytes is the first drop that crosses COMPACT_THRESHOLD (32 KiB).
    for (int dropped = 0; dropped < 33; dropped++)
    {
        TEST_ASSERT_TRUE(buffer.dropOldest());
    }

    TEST_ASSERT_EQUAL_UINT32(7, buffer.pendingCount());
    TEST_ASSERT_EQUAL_STRING("0033", peekText(buffer, 5).c_str());
    TEST_ASSERT_EQUAL_UINT32(7 * (RECORD_BYTES + 1), recordsFileSize());
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
    return UNITY_END();
}
