/*
 * hal.h - Hardware abstraction layer.
 *
 * The firmware talks to hardware only through these functions. A target port
 * (for example STM32 with its HAL/LL drivers) implements them with real
 * peripherals; platform/host/hal_host.c implements them on a PC so the same
 * firmware can be unit tested and link tested without a board.
 */
#ifndef HAL_H
#define HAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    ADC_CH_VBUS = 0, /* input bus voltage through an 11:1 divider */
    ADC_CH_TEMP = 1, /* analog temperature sensor, 10 mV/degC, 500 mV at 0 degC */
    ADC_CH_COUNT
} adc_channel_t;

void     hal_init(void);
bool     hal_uart_read_byte(uint8_t *out);            /* non-blocking, false if no byte */
void     hal_uart_write(const uint8_t *data, size_t len);
uint16_t hal_adc_read(adc_channel_t ch);               /* 12-bit raw value, 0..4095 */
void     hal_output_enable(bool on);                   /* load switch for the protected output */
void     hal_led_set(bool on);                         /* status LED */
uint32_t hal_millis(void);                             /* monotonic milliseconds */
void     hal_watchdog_kick(void);

#ifdef HOST_SIM
/* Simulation-only hooks used by the link tests to inject conditions. */
void     hal_sim_set_adc(adc_channel_t ch, uint16_t raw);
#endif

#endif /* HAL_H */
