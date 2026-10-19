/*
 * hal_host.c - HAL implementation for running the firmware as a PC process.
 *
 * UART  -> stdin/stdout as a raw byte stream (the link tests drive this)
 * ADC   -> simulated values, changeable with the SIM_SET_ADC command
 * GPIO  -> state kept in variables, logged to stderr on change
 */
#include "hal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

/* Default simulated readings: 12.0 V bus, 25.0 degC */
static uint16_t s_adc[ADC_CH_COUNT] = {1354u, 931u};
static int      s_output = -1;
static int      s_led    = -1;
static int      s_stdin_closed;

void hal_init(void)
{
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

bool hal_uart_read_byte(uint8_t *out)
{
    ssize_t n = read(STDIN_FILENO, out, 1);
    if (n == 1) {
        return true;
    }
    if (n == 0) {
        s_stdin_closed = 1; /* test harness closed the link */
    }
    return false;
}

void hal_uart_write(const uint8_t *data, size_t len)
{
    while (len > 0u) {
        ssize_t n = write(STDOUT_FILENO, data, len);
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            return;
        }
        data += n;
        len -= (size_t)n;
    }
}

uint16_t hal_adc_read(adc_channel_t ch)
{
    return (ch < ADC_CH_COUNT) ? s_adc[ch] : 0u;
}

void hal_output_enable(bool on)
{
    if (s_output != (int)on) {
        s_output = on;
        fprintf(stderr, "[hal] output %s\n", on ? "ON" : "OFF");
    }
}

void hal_led_set(bool on)
{
    if (s_led != (int)on) {
        s_led = on;
        fprintf(stderr, "[hal] led %s\n", on ? "ON" : "OFF");
    }
}

uint32_t hal_millis(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u);
}

void hal_watchdog_kick(void)
{
    /* no watchdog on the host */
}

void hal_sim_set_adc(adc_channel_t ch, uint16_t raw)
{
    if (ch < ADC_CH_COUNT) {
        s_adc[ch] = raw;
        fprintf(stderr, "[sim] adc ch%d = %u\n", (int)ch, raw);
    }
}

int hal_host_link_closed(void)
{
    return s_stdin_closed;
}
