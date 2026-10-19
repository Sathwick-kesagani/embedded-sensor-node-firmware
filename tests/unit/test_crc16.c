#include <string.h>

#include "crc16.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_standard_check_value(void)
{
    const char *s = "123456789";
    TEST_ASSERT_EQUAL_HEX16(0x29B1, crc16((const uint8_t *)s, strlen(s)));
}

static void test_empty_input_returns_init(void)
{
    TEST_ASSERT_EQUAL_HEX16(CRC16_INIT, crc16(NULL, 0));
}

static void test_bytewise_matches_block(void)
{
    const uint8_t d[] = {0x00, 0xFF, 0x55, 0xAA, 0x12};
    uint16_t c = CRC16_INIT;
    for (size_t i = 0; i < sizeof(d); i++) c = crc16_update(c, d[i]);
    TEST_ASSERT_EQUAL_HEX16(crc16(d, sizeof(d)), c);
}

static void test_detects_single_bit_flip(void)
{
    uint8_t d[] = {1, 2, 3, 4, 5, 6, 7, 8};
    uint16_t good = crc16(d, sizeof(d));
    for (size_t byte = 0; byte < sizeof(d); byte++) {
        for (int bit = 0; bit < 8; bit++) {
            d[byte] ^= (uint8_t)(1u << bit);
            TEST_ASSERT_NOT_EQUAL(good, crc16(d, sizeof(d)));
            d[byte] ^= (uint8_t)(1u << bit);
        }
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_standard_check_value);
    RUN_TEST(test_empty_input_returns_init);
    RUN_TEST(test_bytewise_matches_block);
    RUN_TEST(test_detects_single_bit_flip);
    return UNITY_END();
}
