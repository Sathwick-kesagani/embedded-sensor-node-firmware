#include "sensor.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_adc_endpoints(void)
{
    TEST_ASSERT_EQUAL_UINT16(0, adc_to_mv(0));
    TEST_ASSERT_EQUAL_UINT16(ADC_VREF_MV, adc_to_mv(ADC_FULL_SCALE));
    TEST_ASSERT_EQUAL_UINT16(ADC_VREF_MV, adc_to_mv(5000)); /* clamped */
}

static void test_vbus_scaling(void)
{
    /* 1354 counts -> 1.091 V at the pin -> 12.0 V on the bus */
    TEST_ASSERT_UINT16_WITHIN(5, 12000, vbus_mv_from_raw(1354));
}

static void test_temperature_scaling(void)
{
    TEST_ASSERT_INT16_WITHIN(2, 250, temp_c_x10_from_raw(931));   /* 25.0 degC */
    TEST_ASSERT_INT16_WITHIN(2, 0, temp_c_x10_from_raw(620));     /* 0.0 degC */
    TEST_ASSERT_INT16_WITHIN(2, -100, temp_c_x10_from_raw(496));  /* -10.0 degC */
}

static void test_moving_average_settles(void)
{
    moving_avg_t m;
    mavg_init(&m);
    TEST_ASSERT_EQUAL_UINT16(100, mavg_add(&m, 100));
    for (unsigned i = 0; i < SENSOR_AVG_N; i++) mavg_add(&m, 200);
    TEST_ASSERT_EQUAL_UINT16(200, mavg_add(&m, 200));
}

static void test_moving_average_rejects_single_spike(void)
{
    moving_avg_t m;
    mavg_init(&m);
    for (unsigned i = 0; i < SENSOR_AVG_N; i++) mavg_add(&m, 1000);
    uint16_t avg = mavg_add(&m, 4095); /* one noisy sample */
    TEST_ASSERT_LESS_THAN_UINT16(1400, avg);
}

static void test_rail_detection(void)
{
    TEST_ASSERT_TRUE(adc_raw_at_rail(0));
    TEST_ASSERT_TRUE(adc_raw_at_rail(4095));
    TEST_ASSERT_FALSE(adc_raw_at_rail(931));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_adc_endpoints);
    RUN_TEST(test_vbus_scaling);
    RUN_TEST(test_temperature_scaling);
    RUN_TEST(test_moving_average_settles);
    RUN_TEST(test_moving_average_rejects_single_spike);
    RUN_TEST(test_rail_detection);
    return UNITY_END();
}
