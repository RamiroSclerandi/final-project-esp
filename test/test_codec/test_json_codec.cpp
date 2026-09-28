// Byte-for-byte characterization of JsonCodec's datalogger.v1 output. These
// fixtures ARE the canonical contract: the backend ingest worker must accept
// them unchanged (see spec datalogger-v1-contract-fixtures). PR4 adds the
// meta.lost field (see the meta_lost_* cases below); the rest of the
// envelope shape is unchanged since firmware 1.1.0.
#include <unity.h>

#include <cstdio>
#include <string>

#include "codec/JsonCodec.h"
#include "sensors/SensorRegistry.h"

#include "../support/FakeSensor.h"

void setUp() {}
void tearDown() {}

namespace
{
    /**
     * @brief Reads a fixture file whole. Shared by every case below so a
     *        golden mismatch always reports the same, obvious cause.
     */
    std::string readFixture(const char *name)
    {
        const std::string path =
            std::string(NATIVE_PROJECT_DIR) + "/test/fixtures/datalogger.v1/" + name;
        FILE *file = std::fopen(path.c_str(), "rb");
        TEST_ASSERT_NOT_NULL_MESSAGE(file, path.c_str());

        std::string content;
        char chunk[256];
        size_t read = 0;
        while ((read = std::fread(chunk, 1, sizeof(chunk), file)) > 0)
        {
            content.append(chunk, read);
        }
        std::fclose(file);
        return content;
    }

    /** One sensor, two channels: a working one and a normally-failing one. */
    void buildTwoChannelAccumulator(SensorRegistry &registry, FakeSensor &sensor,
                                     Measurement (&channels)[2], MeasurementAccumulator &accumulator)
    {
        channels[0] = {Channel::TEMPERATURE, Tag::NONE, Unit::CELSIUS, 0.0f, true};
        channels[1] = {Channel::PRESSURE, Tag::NONE, Unit::HECTOPASCAL, 0.0f, true};
        registry.add(&sensor);

        accumulator.reset(registry);
        sensor.setValue(0, 21.5f, true);
        sensor.setValue(1, 0.0f, false);
        accumulator.accumulate(registry);
    }

    /**
     * @brief Shared by the meta.lost golden cases below, which differ only
     *        in meta.lostCount and the golden file they must match.
     */
    void assertMetaLostEncodesToGolden(uint32_t lostCount, const char *goldenName)
    {
        SensorRegistry registry;
        Measurement channels[2];
        FakeSensor sensor("TestSensor", channels, 2);
        MeasurementAccumulator accumulator;
        buildTwoChannelAccumulator(registry, sensor, channels, accumulator);

        PayloadMeta meta{};
        meta.rssi = -55;
        meta.sequence = 42;
        meta.bootCount = 1;
        meta.lostCount = lostCount;
        meta.resetReason = nullptr;
        meta.storeKind = nullptr;

        uint8_t out[512];
        JsonCodec codec;
        const size_t written = codec.encode(accumulator, meta, out, sizeof(out));

        const std::string golden = readFixture(goldenName);
        TEST_ASSERT_EQUAL_UINT32(golden.size(), written);
        TEST_ASSERT_EQUAL_STRING_LEN(golden.c_str(), (const char *)out, golden.size());
    }
}

void test_valid_payload_matches_golden(void)
{
    SensorRegistry registry;
    Measurement channels[2];
    FakeSensor sensor("TestSensor", channels, 2);
    MeasurementAccumulator accumulator;
    buildTwoChannelAccumulator(registry, sensor, channels, accumulator);

    PayloadMeta meta{};
    meta.rssi = -55;
    meta.sequence = 42;
    meta.bootCount = 1;
    meta.resetReason = nullptr;
    meta.storeKind = nullptr;

    uint8_t out[512];
    JsonCodec codec;
    const size_t written = codec.encode(accumulator, meta, out, sizeof(out));

    const std::string golden = readFixture("valid.json");
    TEST_ASSERT_EQUAL_UINT32(golden.size(), written);
    TEST_ASSERT_EQUAL_STRING_LEN(golden.c_str(), (const char *)out, golden.size());
}

void test_oversize_payload_guards_to_empty_output(void)
{
    SensorRegistry registry;
    Measurement channels[2];
    FakeSensor sensor("TestSensor", channels, 2);
    MeasurementAccumulator accumulator;
    buildTwoChannelAccumulator(registry, sensor, channels, accumulator);

    PayloadMeta meta{};
    meta.rssi = -55;
    meta.sequence = 42;
    meta.bootCount = 1;

    // Deliberately too small for this payload, regardless of channel count —
    // forces the same overflow guard a real oversize reading would hit.
    uint8_t out[4] = {0xAA, 0xAA, 0xAA, 0xAA};
    JsonCodec codec;
    const size_t written = codec.encode(accumulator, meta, out, sizeof(out));

    const std::string golden = readFixture("oversize.json");
    TEST_ASSERT_EQUAL_UINT32(golden.size(), written);
    TEST_ASSERT_EQUAL_UINT32(0, written);
    TEST_ASSERT_EQUAL_UINT8('\0', out[0]);
}

void test_meta_lost_zero_matches_golden(void)
{
    assertMetaLostEncodesToGolden(0, "meta_lost_zero.json");
}

void test_meta_lost_nonzero_matches_golden(void)
{
    assertMetaLostEncodesToGolden(3, "meta_lost_nonzero.json");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_valid_payload_matches_golden);
    RUN_TEST(test_oversize_payload_guards_to_empty_output);
    RUN_TEST(test_meta_lost_zero_matches_golden);
    RUN_TEST(test_meta_lost_nonzero_matches_golden);
    return UNITY_END();
}
