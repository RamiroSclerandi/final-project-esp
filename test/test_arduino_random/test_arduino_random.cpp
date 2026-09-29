// Pins the host random(min, max) shim to Arduino's contract, [min, max),
// independent of the host libc: MinGW's RAND_MAX is only 32767.
#include <unity.h>

#include <climits>

#include <Arduino.h>

namespace
{
    constexpr int DRAWS = 2000;
}

void setUp() {}
void tearDown() {}

void test_small_range_stays_in_bounds_and_hits_every_value(void)
{
    bool seen[3] = {false, false, false};
    for (int draw = 0; draw < DRAWS; draw++)
    {
        const long value = random(5, 8);
        TEST_ASSERT_TRUE(value >= 5 && value < 8);
        seen[value - 5] = true;
    }

    TEST_ASSERT_TRUE(seen[0] && seen[1] && seen[2]);
}

void test_wide_range_exceeds_libc_rand_max(void)
{
    long largest = 0;
    for (int draw = 0; draw < DRAWS; draw++)
    {
        const long value = random(0, 1000000);
        TEST_ASSERT_TRUE(value >= 0 && value < 1000000);
        if (value > largest)
        {
            largest = value;
        }
    }

    TEST_ASSERT_TRUE(largest > 32767);
}

void test_full_long_range_does_not_overflow(void)
{
    long smallest = LONG_MAX;
    long largest = LONG_MIN;
    for (int draw = 0; draw < DRAWS; draw++)
    {
        const long value = random(LONG_MIN, LONG_MAX);
        TEST_ASSERT_TRUE(value < LONG_MAX);
        smallest = value < smallest ? value : smallest;
        largest = value > largest ? value : largest;
    }

    TEST_ASSERT_TRUE(smallest < 0);
    TEST_ASSERT_TRUE(largest > 0);
}

void test_empty_range_returns_min(void)
{
    TEST_ASSERT_EQUAL_INT32(7, random(7, 7));
    TEST_ASSERT_EQUAL_INT32(7, random(7, 3));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_small_range_stays_in_bounds_and_hits_every_value);
    RUN_TEST(test_wide_range_exceeds_libc_rand_max);
    RUN_TEST(test_full_long_range_does_not_overflow);
    RUN_TEST(test_empty_range_returns_min);
    return UNITY_END();
}
