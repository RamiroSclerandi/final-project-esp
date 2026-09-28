// Characterizes the CURRENT behavior of MeasurementAccumulator (v1.1.0):
// no production logic changes in this PR, only pinned-down documentation of
// what it does today so later fixes have a safety net.
#include <unity.h>

#include "core/MeasurementAccumulator.h"
#include "sensors/SensorRegistry.h"

#include "../support/FakeSensor.h"

void setUp() {}
void tearDown() {}

void test_reset_captures_channel_layout_with_zeroed_stats(void)
{
    Measurement channels[2] = {
        {Channel::TEMPERATURE, Tag::NONE, Unit::CELSIUS, 0.0f, true},
        {Channel::PRESSURE, Tag::NONE, Unit::HECTOPASCAL, 0.0f, true},
    };
    FakeSensor sensor("TestSensor", channels, 2);
    SensorRegistry registry;
    registry.add(&sensor);

    MeasurementAccumulator accumulator;
    accumulator.reset(registry);

    TEST_ASSERT_EQUAL_UINT8(2, accumulator.channelCount());

    const AggregatedChannel ch0 = accumulator.channelAt(0);
    TEST_ASSERT_EQUAL_STRING(Channel::TEMPERATURE, ch0.channel);
    TEST_ASSERT_EQUAL(Unit::CELSIUS, ch0.unit);
    TEST_ASSERT_EQUAL_UINT16(0, ch0.count);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, ch0.value);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, ch0.minimum);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, ch0.maximum);

    const AggregatedChannel ch1 = accumulator.channelAt(1);
    TEST_ASSERT_EQUAL_STRING(Channel::PRESSURE, ch1.channel);
    TEST_ASSERT_EQUAL(Unit::HECTOPASCAL, ch1.unit);
    TEST_ASSERT_EQUAL_UINT16(0, ch1.count);
}

void test_accumulate_single_round_reports_exact_value(void)
{
    Measurement channels[1] = {
        {Channel::TEMPERATURE, Tag::NONE, Unit::CELSIUS, 0.0f, true},
    };
    FakeSensor sensor("TestSensor", channels, 1);
    SensorRegistry registry;
    registry.add(&sensor);

    MeasurementAccumulator accumulator;
    accumulator.reset(registry);

    sensor.setValue(0, 21.5f, true);
    accumulator.accumulate(registry);

    const AggregatedChannel ch = accumulator.channelAt(0);
    TEST_ASSERT_EQUAL_UINT16(1, ch.count);
    TEST_ASSERT_EQUAL_FLOAT(21.5f, ch.value);
    TEST_ASSERT_EQUAL_FLOAT(21.5f, ch.minimum);
    TEST_ASSERT_EQUAL_FLOAT(21.5f, ch.maximum);
}

void test_accumulate_skips_invalid_readings(void)
{
    Measurement channels[1] = {
        {Channel::PRESSURE, Tag::NONE, Unit::HECTOPASCAL, 0.0f, true},
    };
    FakeSensor sensor("TestSensor", channels, 1);
    SensorRegistry registry;
    registry.add(&sensor);

    MeasurementAccumulator accumulator;
    accumulator.reset(registry);

    sensor.setValue(0, 999.0f, false);
    accumulator.accumulate(registry);

    const AggregatedChannel ch = accumulator.channelAt(0);
    TEST_ASSERT_EQUAL_UINT16(0, ch.count);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, ch.value);
}

void test_accumulate_multiple_rounds_computes_mean_min_max(void)
{
    Measurement channels[1] = {
        {Channel::TEMPERATURE, Tag::NONE, Unit::CELSIUS, 0.0f, true},
    };
    FakeSensor sensor("TestSensor", channels, 1);
    SensorRegistry registry;
    registry.add(&sensor);

    MeasurementAccumulator accumulator;
    accumulator.reset(registry);

    sensor.setValue(0, 10.0f, true);
    accumulator.accumulate(registry);
    sensor.setValue(0, 20.0f, true);
    accumulator.accumulate(registry);
    sensor.setValue(0, 30.0f, true);
    accumulator.accumulate(registry);

    const AggregatedChannel ch = accumulator.channelAt(0);
    TEST_ASSERT_EQUAL_UINT16(3, ch.count);
    TEST_ASSERT_EQUAL_FLOAT(20.0f, ch.value);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, ch.minimum);
    TEST_ASSERT_EQUAL_FLOAT(30.0f, ch.maximum);
}

void test_channel_at_out_of_range_returns_zeroed_entry(void)
{
    SensorRegistry registry;
    MeasurementAccumulator accumulator;
    accumulator.reset(registry);

    const AggregatedChannel ch = accumulator.channelAt(0);
    TEST_ASSERT_NULL(ch.channel);
    TEST_ASSERT_EQUAL_UINT16(0, ch.count);
}

void test_channel_at_out_of_range_on_populated_accumulator_returns_zeroed_entry(void)
{
    Measurement channels[2] = {
        {Channel::TEMPERATURE, Tag::NONE, Unit::CELSIUS, 0.0f, true},
        {Channel::PRESSURE, Tag::NONE, Unit::HECTOPASCAL, 0.0f, true},
    };
    FakeSensor sensor("TestSensor", channels, 2);
    SensorRegistry registry;
    registry.add(&sensor);

    MeasurementAccumulator accumulator;
    accumulator.reset(registry);

    // The boundary one past the last real channel, not just the empty case.
    const AggregatedChannel ch = accumulator.channelAt(accumulator.channelCount());
    TEST_ASSERT_NULL(ch.channel);
    TEST_ASSERT_EQUAL_UINT16(0, ch.count);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_reset_captures_channel_layout_with_zeroed_stats);
    RUN_TEST(test_accumulate_single_round_reports_exact_value);
    RUN_TEST(test_accumulate_skips_invalid_readings);
    RUN_TEST(test_accumulate_multiple_rounds_computes_mean_min_max);
    RUN_TEST(test_channel_at_out_of_range_returns_zeroed_entry);
    RUN_TEST(test_channel_at_out_of_range_on_populated_accumulator_returns_zeroed_entry);
    return UNITY_END();
}
