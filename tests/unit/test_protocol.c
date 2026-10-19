#include <string.h>

#include "protocol.h"
#include "unity.h"

static proto_parser_t p;
static proto_frame_t  f;

void setUp(void) { proto_init(&p); memset(&f, 0, sizeof(f)); }
void tearDown(void) {}

/* feed a buffer, return the result of the last byte */
static proto_result_t feed(const uint8_t *b, size_t n, uint32_t t)
{
    proto_result_t r = PROTO_NONE;
    for (size_t i = 0; i < n; i++) r = proto_feed(&p, b[i], t, &f);
    return r;
}

static void test_encode_then_parse_roundtrip(void)
{
    const uint8_t payload[] = {0x10, 0x20, 0x30};
    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(0x03, payload, 3, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_size_t(3 + PROTO_OVERHEAD, n);
    TEST_ASSERT_EQUAL(PROTO_FRAME, feed(buf, n, 0));
    TEST_ASSERT_EQUAL_HEX8(0x03, f.cmd);
    TEST_ASSERT_EQUAL_UINT8(3, f.len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(payload, f.payload, 3);
    TEST_ASSERT_EQUAL_UINT16(1, p.frames_ok);
}

static void test_zero_length_frame(void)
{
    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(0x01, NULL, 0, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_size_t(PROTO_OVERHEAD, n);
    TEST_ASSERT_EQUAL(PROTO_FRAME, feed(buf, n, 0));
    TEST_ASSERT_EQUAL_UINT8(0, f.len);
}

static void test_bad_crc_is_rejected_and_counted(void)
{
    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(0x01, NULL, 0, buf, sizeof(buf));
    buf[n - 1] ^= 0x01;
    TEST_ASSERT_EQUAL(PROTO_ERR_CRC, feed(buf, n, 0));
    TEST_ASSERT_EQUAL_UINT16(1, p.crc_errors);
    TEST_ASSERT_EQUAL_UINT16(0, p.frames_ok);
}

static void test_oversized_length_is_rejected(void)
{
    const uint8_t bad[] = {PROTO_SOF, PROTO_MAX_PAYLOAD + 1};
    TEST_ASSERT_EQUAL(PROTO_ERR_LEN, feed(bad, sizeof(bad), 0));
    TEST_ASSERT_EQUAL_UINT16(1, p.len_errors);
}

static void test_resyncs_after_garbage(void)
{
    uint8_t buf[64];
    const uint8_t junk[] = {0x00, 0x13, 0x37, 0xFF};
    memcpy(buf, junk, sizeof(junk));
    size_t n = proto_encode(0x02, NULL, 0, buf + sizeof(junk), sizeof(buf) - sizeof(junk));
    TEST_ASSERT_EQUAL(PROTO_FRAME, feed(buf, sizeof(junk) + n, 0));
    TEST_ASSERT_EQUAL_HEX8(0x02, f.cmd);
}

static void test_back_to_back_frames(void)
{
    uint8_t buf[2 * PROTO_MAX_FRAME];
    size_t n1 = proto_encode(0x01, NULL, 0, buf, sizeof(buf));
    const uint8_t pl[] = {1};
    size_t n2 = proto_encode(0x05, pl, 1, buf + n1, sizeof(buf) - n1);
    TEST_ASSERT_EQUAL(PROTO_FRAME, feed(buf, n1, 0));
    TEST_ASSERT_EQUAL(PROTO_FRAME, feed(buf + n1, n2, 0));
    TEST_ASSERT_EQUAL_HEX8(0x05, f.cmd);
    TEST_ASSERT_EQUAL_UINT16(2, p.frames_ok);
}

static void test_stalled_frame_times_out(void)
{
    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(0x01, NULL, 0, buf, sizeof(buf));
    feed(buf, 3, 0);                       /* SOF, LEN, CMD then the line goes quiet */
    proto_tick(&p, PROTO_TIMEOUT_MS + 1);
    TEST_ASSERT_EQUAL_UINT16(1, p.timeouts);
    /* the rest of the old frame must not complete anything */
    TEST_ASSERT_NOT_EQUAL(PROTO_FRAME, feed(buf + 3, n - 3, PROTO_TIMEOUT_MS + 2));
    /* a fresh frame still works */
    TEST_ASSERT_EQUAL(PROTO_FRAME, feed(buf, n, PROTO_TIMEOUT_MS + 3));
}

static void test_encode_rejects_bad_arguments(void)
{
    uint8_t buf[PROTO_MAX_FRAME];
    uint8_t big[PROTO_MAX_PAYLOAD + 1] = {0};
    TEST_ASSERT_EQUAL_size_t(0, proto_encode(0x01, big, PROTO_MAX_PAYLOAD + 1, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_size_t(0, proto_encode(0x01, big, 4, buf, 4)); /* buffer too small */
    TEST_ASSERT_EQUAL_size_t(0, proto_encode(0x01, NULL, 2, buf, sizeof(buf)));
}

static void test_max_payload(void)
{
    uint8_t pl[PROTO_MAX_PAYLOAD];
    for (unsigned i = 0; i < PROTO_MAX_PAYLOAD; i++) pl[i] = (uint8_t)(0xA5 ^ i); /* includes SOF bytes */
    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(0x10, pl, PROTO_MAX_PAYLOAD, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(PROTO_FRAME, feed(buf, n, 0));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(pl, f.payload, PROTO_MAX_PAYLOAD);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_encode_then_parse_roundtrip);
    RUN_TEST(test_zero_length_frame);
    RUN_TEST(test_bad_crc_is_rejected_and_counted);
    RUN_TEST(test_oversized_length_is_rejected);
    RUN_TEST(test_resyncs_after_garbage);
    RUN_TEST(test_back_to_back_frames);
    RUN_TEST(test_stalled_frame_times_out);
    RUN_TEST(test_encode_rejects_bad_arguments);
    RUN_TEST(test_max_payload);
    return UNITY_END();
}
