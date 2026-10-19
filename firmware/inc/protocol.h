/*
 * protocol.h - Framed binary command protocol over UART.
 *
 *   +------+-----+-----+-------------+--------+--------+
 *   | 0xA5 | LEN | CMD | PAYLOAD[LEN]| CRC_HI | CRC_LO |
 *   +------+-----+-----+-------------+--------+--------+
 *
 * LEN is the payload length (0..PROTO_MAX_PAYLOAD). The CRC-16 covers
 * LEN, CMD and PAYLOAD. A reply uses CMD | 0x80. The parser is a byte-by-byte
 * state machine that drops bad frames and re-synchronises on the next 0xA5.
 * A frame that stalls for more than PROTO_TIMEOUT_MS is discarded.
 */
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define PROTO_SOF          0xA5u
#define PROTO_MAX_PAYLOAD  32u
#define PROTO_OVERHEAD     5u /* SOF + LEN + CMD + CRC(2) */
#define PROTO_MAX_FRAME    (PROTO_MAX_PAYLOAD + PROTO_OVERHEAD)
#define PROTO_TIMEOUT_MS   50u
#define PROTO_REPLY_FLAG   0x80u

typedef enum {
    PROTO_NONE = 0,     /* byte consumed, no complete frame yet */
    PROTO_FRAME,        /* a valid frame is in *out */
    PROTO_ERR_CRC,      /* frame dropped: CRC mismatch */
    PROTO_ERR_LEN       /* frame dropped: LEN larger than PROTO_MAX_PAYLOAD */
} proto_result_t;

typedef struct {
    uint8_t cmd;
    uint8_t len;
    uint8_t payload[PROTO_MAX_PAYLOAD];
} proto_frame_t;

typedef struct {
    uint8_t       state;
    uint8_t       idx;
    uint8_t       crc_hi;
    uint32_t      last_byte_ms;
    proto_frame_t frame;
    /* statistics, reported by GET_STATUS */
    uint16_t      frames_ok;
    uint16_t      crc_errors;
    uint16_t      len_errors;
    uint16_t      timeouts;
} proto_parser_t;

void           proto_init(proto_parser_t *p);
proto_result_t proto_feed(proto_parser_t *p, uint8_t byte, uint32_t now_ms, proto_frame_t *out);
void           proto_tick(proto_parser_t *p, uint32_t now_ms); /* drops stalled frames */
size_t         proto_encode(uint8_t cmd, const uint8_t *payload, uint8_t len,
                            uint8_t *out, size_t out_cap);     /* returns frame size, 0 on error */

#endif /* PROTOCOL_H */
