#include "protocol.h"

#include <string.h>

#include "crc16.h"

enum {
    ST_WAIT_SOF = 0,
    ST_LEN,
    ST_CMD,
    ST_PAYLOAD,
    ST_CRC_HI,
    ST_CRC_LO
};

void proto_init(proto_parser_t *p)
{
    memset(p, 0, sizeof(*p));
    p->state = ST_WAIT_SOF;
}

static uint16_t frame_crc(const proto_frame_t *f)
{
    uint16_t crc = CRC16_INIT;
    crc = crc16_update(crc, f->len);
    crc = crc16_update(crc, f->cmd);
    for (uint8_t i = 0; i < f->len; i++) {
        crc = crc16_update(crc, f->payload[i]);
    }
    return crc;
}

void proto_tick(proto_parser_t *p, uint32_t now_ms)
{
    if (p->state != ST_WAIT_SOF && (uint32_t)(now_ms - p->last_byte_ms) > PROTO_TIMEOUT_MS) {
        p->timeouts++;
        p->state = ST_WAIT_SOF;
    }
}

proto_result_t proto_feed(proto_parser_t *p, uint8_t byte, uint32_t now_ms, proto_frame_t *out)
{
    proto_tick(p, now_ms);
    p->last_byte_ms = now_ms;

    switch (p->state) {
    case ST_WAIT_SOF:
        if (byte == PROTO_SOF) {
            p->state = ST_LEN;
        }
        return PROTO_NONE;

    case ST_LEN:
        if (byte > PROTO_MAX_PAYLOAD) {
            p->len_errors++;
            p->state = ST_WAIT_SOF;
            return PROTO_ERR_LEN;
        }
        p->frame.len = byte;
        p->state = ST_CMD;
        return PROTO_NONE;

    case ST_CMD:
        p->frame.cmd = byte;
        p->idx = 0;
        p->state = (p->frame.len > 0u) ? ST_PAYLOAD : ST_CRC_HI;
        return PROTO_NONE;

    case ST_PAYLOAD:
        p->frame.payload[p->idx++] = byte;
        if (p->idx >= p->frame.len) {
            p->state = ST_CRC_HI;
        }
        return PROTO_NONE;

    case ST_CRC_HI:
        p->crc_hi = byte;
        p->state = ST_CRC_LO;
        return PROTO_NONE;

    case ST_CRC_LO: {
        uint16_t rx = (uint16_t)(((uint16_t)p->crc_hi << 8) | byte);
        p->state = ST_WAIT_SOF;
        if (rx != frame_crc(&p->frame)) {
            p->crc_errors++;
            return PROTO_ERR_CRC;
        }
        p->frames_ok++;
        if (out != NULL) {
            *out = p->frame;
        }
        return PROTO_FRAME;
    }

    default:
        p->state = ST_WAIT_SOF;
        return PROTO_NONE;
    }
}

size_t proto_encode(uint8_t cmd, const uint8_t *payload, uint8_t len, uint8_t *out, size_t out_cap)
{
    if (len > PROTO_MAX_PAYLOAD || out_cap < (size_t)len + PROTO_OVERHEAD) {
        return 0;
    }
    if (len > 0u && payload == NULL) {
        return 0;
    }
    size_t n = 0;
    out[n++] = PROTO_SOF;
    out[n++] = len;
    out[n++] = cmd;
    if (len > 0u) {
        memcpy(&out[n], payload, len);
        n += len;
    }
    uint16_t crc = crc16(&out[1], (size_t)len + 2u); /* LEN, CMD, PAYLOAD */
    out[n++] = (uint8_t)(crc >> 8);
    out[n++] = (uint8_t)(crc & 0xFFu);
    return n;
}
