#include "fault_fsm.h"
#include "unity.h"

static fault_fsm_t f;

void setUp(void) { fsm_init(&f); }
void tearDown(void) {}

static fsm_inputs_t in(uint16_t mv, int16_t t) { fsm_inputs_t x = {mv, t, true}; return x; }

static void step_n(fsm_inputs_t x, int n) { for (int i = 0; i < n; i++) fsm_step(&f, &x); }

static void test_nominal_goes_normal(void)
{
    TEST_ASSERT_EQUAL(STATE_INIT, f.state);
    step_n(in(12000, 250), 1);
    TEST_ASSERT_EQUAL(STATE_NORMAL, f.state);
    TEST_ASSERT_TRUE(fsm_output_allowed(&f));
}

static void test_warning_band_and_hysteresis(void)
{
    step_n(in(12000, 720), 1);
    TEST_ASSERT_EQUAL(STATE_WARNING, f.state);
    step_n(in(12000, 680), 1);                 /* below 70 but above 65: stays WARNING */
    TEST_ASSERT_EQUAL(STATE_WARNING, f.state);
    step_n(in(12000, 640), 1);
    TEST_ASSERT_EQUAL(STATE_NORMAL, f.state);
}

static void test_overtemp_needs_debounce(void)
{
    step_n(in(12000, 250), 1);
    step_n(in(12000, 900), FAULT_DEBOUNCE - 1);
    TEST_ASSERT_NOT_EQUAL(STATE_FAULT, f.state);
    step_n(in(12000, 900), 1);
    TEST_ASSERT_EQUAL(STATE_FAULT, f.state);
    TEST_ASSERT_EQUAL_HEX8(FAULT_OVERTEMP, f.fault_flags);
    TEST_ASSERT_FALSE(fsm_output_allowed(&f));
}

static void test_glitch_shorter_than_debounce_is_ignored(void)
{
    step_n(in(12000, 250), 1);
    step_n(in(9000, 250), 1);   /* one undervoltage sample */
    step_n(in(12000, 250), 5);
    TEST_ASSERT_EQUAL(STATE_NORMAL, f.state);
    TEST_ASSERT_EQUAL_UINT16(0, f.fault_count);
}

static void test_fault_is_latched(void)
{
    step_n(in(9000, 250), FAULT_DEBOUNCE);
    TEST_ASSERT_EQUAL(STATE_FAULT, f.state);
    TEST_ASSERT_EQUAL_HEX8(FAULT_UNDERVOLT, f.fault_flags);
    step_n(in(12000, 250), 10);                 /* condition gone, still latched */
    TEST_ASSERT_EQUAL(STATE_FAULT, f.state);
}

static void test_clear_refused_while_condition_present(void)
{
    step_n(in(15000, 250), FAULT_DEBOUNCE);
    TEST_ASSERT_EQUAL_HEX8(FAULT_OVERVOLT, f.fault_flags);
    TEST_ASSERT_FALSE(fsm_clear_fault(&f));
    TEST_ASSERT_EQUAL(STATE_FAULT, f.state);
}

static void test_clear_succeeds_after_recovery(void)
{
    step_n(in(15000, 250), FAULT_DEBOUNCE);
    step_n(in(12000, 250), 1);
    TEST_ASSERT_TRUE(fsm_clear_fault(&f));
    TEST_ASSERT_EQUAL(STATE_NORMAL, f.state);
    TEST_ASSERT_EQUAL_HEX8(0, f.fault_flags);
    TEST_ASSERT_EQUAL_UINT16(1, f.fault_count);
}

static void test_sensor_fault(void)
{
    fsm_inputs_t x = {12000, 250, false};
    step_n(x, FAULT_DEBOUNCE);
    TEST_ASSERT_EQUAL(STATE_FAULT, f.state);
    TEST_ASSERT_EQUAL_HEX8(FAULT_SENSOR, f.fault_flags);
}

static void test_multiple_flags_latched_together(void)
{
    step_n(in(9000, 900), FAULT_DEBOUNCE);
    TEST_ASSERT_EQUAL_HEX8(FAULT_UNDERVOLT | FAULT_OVERTEMP, f.fault_flags);
}

static void test_boundaries(void)
{
    TEST_ASSERT_EQUAL_HEX8(0, fsm_evaluate_flags(&(fsm_inputs_t){VBUS_UV_FAULT_MV, 849, true}));
    TEST_ASSERT_EQUAL_HEX8(FAULT_OVERTEMP, fsm_evaluate_flags(&(fsm_inputs_t){12000, 850, true}));
    TEST_ASSERT_EQUAL_HEX8(0, fsm_evaluate_flags(&(fsm_inputs_t){VBUS_OV_FAULT_MV, 250, true}));
    TEST_ASSERT_EQUAL_HEX8(FAULT_OVERVOLT, fsm_evaluate_flags(&(fsm_inputs_t){VBUS_OV_FAULT_MV + 1, 250, true}));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_nominal_goes_normal);
    RUN_TEST(test_warning_band_and_hysteresis);
    RUN_TEST(test_overtemp_needs_debounce);
    RUN_TEST(test_glitch_shorter_than_debounce_is_ignored);
    RUN_TEST(test_fault_is_latched);
    RUN_TEST(test_clear_refused_while_condition_present);
    RUN_TEST(test_clear_succeeds_after_recovery);
    RUN_TEST(test_sensor_fault);
    RUN_TEST(test_multiple_flags_latched_together);
    RUN_TEST(test_boundaries);
    return UNITY_END();
}
