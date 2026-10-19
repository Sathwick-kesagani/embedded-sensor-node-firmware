#include "app.h"

#include "fault_fsm.h"
#include "hal.h"
#include "protocol.h"
#include "ring_buffer.h"
#include "sensor.h"

static ring_buffer_t  s_rx;
static proto_parser_t s_parser;
static fault_fsm_t    s_fsm;
static moving_avg_t   s_avg[ADC_CH_COUNT];
static uint16_t       s_raw_avg[ADC_CH_COUNT];
static bool           s_output_requested;
static uint32_t       s_last_sample_ms;

static void send_reply(uint8_t cmd, const uint8_t *payload, uint8_t len)
{
    uint8_t buf[PROTO_MAX_FRAME];
    size_t  n = proto_encode((uint8_t)(cmd | PROTO_REPLY_FLAG), payload, len, buf, sizeof(buf));
    if (n > 0u) {
        hal_uart_write(buf, n);
    }
}

static void send_nack(uint8_t cmd, uint8_t reason)
{
    uint8_t p[2] = {cmd, reason};
    uint8_t buf[PROTO_MAX_FRAME];
    size_t  n = proto_encode(CMD_NACK, p, 2u, buf, sizeof(buf));
    hal_uart_write(buf, n);
}

static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xFFu);
}

static void apply_output(void)
{
    bool on = s_output_requested && fsm_output_allowed(&s_fsm);
    hal_output_enable(on);
    hal_led_set(s_fsm.state == STATE_NORMAL || s_fsm.state == STATE_WARNING);
}

static bool expect_len(const proto_frame_t *f, uint8_t len)
{
    if (f->len != len) {
        send_nack(f->cmd, ERR_BAD_LEN);
        return false;
    }
    return true;
}

static void handle_frame(const proto_frame_t *f)
{
    uint8_t p[10];

    switch (f->cmd) {
    case CMD_PING:
        if (expect_len(f, 0)) {
            send_reply(f->cmd, (const uint8_t *)"PONG", 4u);
        }
        break;

    case CMD_GET_VERSION:
        if (expect_len(f, 0)) {
            p[0] = FW_VERSION_MAJOR;
            p[1] = FW_VERSION_MINOR;
            p[2] = FW_VERSION_PATCH;
            send_reply(f->cmd, p, 3u);
        }
        break;

    case CMD_READ_SENSORS:
        if (expect_len(f, 0)) {
            put_u16(&p[0], s_fsm.last.vbus_mv);
            put_u16(&p[2], (uint16_t)s_fsm.last.temp_c_x10);
            put_u16(&p[4], s_raw_avg[ADC_CH_VBUS]);
            put_u16(&p[6], s_raw_avg[ADC_CH_TEMP]);
            send_reply(f->cmd, p, 8u);
        }
        break;

    case CMD_GET_STATUS:
        if (expect_len(f, 0)) {
            p[0] = (uint8_t)s_fsm.state;
            p[1] = s_fsm.fault_flags;
            put_u16(&p[2], s_parser.crc_errors);
            p[4] = (uint8_t)(s_output_requested && fsm_output_allowed(&s_fsm));
            put_u16(&p[5], s_fsm.fault_count);
            put_u16(&p[7], (uint16_t)s_rx.overflows);
            send_reply(f->cmd, p, 9u);
        }
        break;

    case CMD_SET_OUTPUT:
        if (!expect_len(f, 1)) {
            break;
        }
        if (f->payload[0] > 1u) {
            send_nack(f->cmd, ERR_BAD_ARG);
            break;
        }
        if (f->payload[0] == 1u && !fsm_output_allowed(&s_fsm)) {
            p[0] = RESULT_REFUSED;
        } else {
            s_output_requested = (f->payload[0] == 1u);
            p[0] = RESULT_OK;
        }
        apply_output();
        send_reply(f->cmd, p, 1u);
        break;

    case CMD_CLEAR_FAULT:
        if (expect_len(f, 0)) {
            p[0] = fsm_clear_fault(&s_fsm) ? RESULT_OK : RESULT_REFUSED;
            apply_output();
            send_reply(f->cmd, p, 1u);
        }
        break;

#ifdef HOST_SIM
    case CMD_SIM_SET_ADC:
        if (!expect_len(f, 3)) {
            break;
        }
        if (f->payload[0] >= ADC_CH_COUNT) {
            send_nack(f->cmd, ERR_BAD_ARG);
            break;
        }
        hal_sim_set_adc((adc_channel_t)f->payload[0],
                        (uint16_t)(((uint16_t)f->payload[1] << 8) | f->payload[2]));
        p[0] = RESULT_OK;
        send_reply(f->cmd, p, 1u);
        break;
#endif

    default:
        send_nack(f->cmd, ERR_UNKNOWN_CMD);
        break;
    }
}

static void sample_and_protect(void)
{
    bool sensor_ok = true;
    for (int ch = 0; ch < (int)ADC_CH_COUNT; ch++) {
        uint16_t raw = hal_adc_read((adc_channel_t)ch);
        if (ch == ADC_CH_TEMP && adc_raw_at_rail(raw)) {
            sensor_ok = false; /* open or shorted temperature sensor */
        }
        s_raw_avg[ch] = mavg_add(&s_avg[ch], raw);
    }

    fsm_inputs_t in = {
        .vbus_mv    = vbus_mv_from_raw(s_raw_avg[ADC_CH_VBUS]),
        .temp_c_x10 = temp_c_x10_from_raw(s_raw_avg[ADC_CH_TEMP]),
        .sensor_ok  = sensor_ok,
    };
    sys_state_t before = s_fsm.state;
    fsm_step(&s_fsm, &in);
    if (s_fsm.state == STATE_FAULT && before != STATE_FAULT) {
        s_output_requested = false; /* a fault always drops the output request */
    }
    apply_output();
}

void app_init(void)
{
    hal_init();
    rb_init(&s_rx);
    proto_init(&s_parser);
    fsm_init(&s_fsm);
    for (int ch = 0; ch < (int)ADC_CH_COUNT; ch++) {
        mavg_init(&s_avg[ch]);
    }
    s_output_requested = false;
    s_last_sample_ms   = hal_millis();
    sample_and_protect();
}

void app_step(void)
{
    uint8_t  byte;
    uint32_t now = hal_millis();

    /* On a target the UART RX interrupt fills s_rx; on the host we poll here.
     * Only take bytes while there is room, so a burst waits in the driver
     * instead of overflowing the ring buffer (found by link test 07). */
    while (!rb_is_full(&s_rx) && hal_uart_read_byte(&byte)) {
        (void)rb_push(&s_rx, byte);
    }

    proto_frame_t frame;
    while (rb_pop(&s_rx, &byte)) {
        if (proto_feed(&s_parser, byte, now, &frame) == PROTO_FRAME) {
            handle_frame(&frame);
        }
    }
    proto_tick(&s_parser, now);

    if ((uint32_t)(now - s_last_sample_ms) >= APP_SAMPLE_PERIOD_MS) {
        s_last_sample_ms = now;
        sample_and_protect();
    }

    hal_watchdog_kick();
}
