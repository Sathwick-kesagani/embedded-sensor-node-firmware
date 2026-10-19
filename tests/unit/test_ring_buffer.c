#include "ring_buffer.h"
#include "unity.h"

static ring_buffer_t rb;

void setUp(void) { rb_init(&rb); }
void tearDown(void) {}

static void test_starts_empty(void)
{
    uint8_t b;
    TEST_ASSERT_TRUE(rb_is_empty(&rb));
    TEST_ASSERT_EQUAL_UINT16(0, rb_count(&rb));
    TEST_ASSERT_FALSE(rb_pop(&rb, &b));
}

static void test_fifo_order(void)
{
    uint8_t b;
    for (uint8_t i = 0; i < 10; i++) TEST_ASSERT_TRUE(rb_push(&rb, i));
    for (uint8_t i = 0; i < 10; i++) {
        TEST_ASSERT_TRUE(rb_pop(&rb, &b));
        TEST_ASSERT_EQUAL_UINT8(i, b);
    }
    TEST_ASSERT_TRUE(rb_is_empty(&rb));
}

static void test_full_rejects_and_counts_overflow(void)
{
    for (unsigned i = 0; i < RB_SIZE; i++) TEST_ASSERT_TRUE(rb_push(&rb, (uint8_t)i));
    TEST_ASSERT_TRUE(rb_is_full(&rb));
    TEST_ASSERT_FALSE(rb_push(&rb, 0xEE));
    TEST_ASSERT_EQUAL_UINT32(1, rb.overflows);
    uint8_t b;
    TEST_ASSERT_TRUE(rb_pop(&rb, &b));
    TEST_ASSERT_EQUAL_UINT8(0, b); /* oldest data kept, new byte dropped */
}

static void test_wraps_around_many_times(void)
{
    uint8_t b;
    for (unsigned i = 0; i < 70000u; i++) { /* > 65536 so the 16-bit indices wrap */
        TEST_ASSERT_TRUE(rb_push(&rb, (uint8_t)i));
        TEST_ASSERT_TRUE(rb_pop(&rb, &b));
        TEST_ASSERT_EQUAL_UINT8((uint8_t)i, b);
    }
    TEST_ASSERT_TRUE(rb_is_empty(&rb));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_starts_empty);
    RUN_TEST(test_fifo_order);
    RUN_TEST(test_full_rejects_and_counts_overflow);
    RUN_TEST(test_wraps_around_many_times);
    return UNITY_END();
}
