/*
 * sensor.h - ADC filtering and conversion to engineering units.
 *
 * Each channel is smoothed with an 8-sample moving average. Conversions use
 * integer math only (no floating point on the target).
 */
#ifndef SENSOR_H
#define SENSOR_H

#include <stdbool.h>
#include <stdint.h>

#define SENSOR_AVG_N        8u
#define ADC_FULL_SCALE      4095u
#define ADC_VREF_MV         3300u
#define VBUS_DIVIDER        11u   /* 100k / 10k divider */
#define ADC_RAIL_LOW        5u    /* raw values at the rails mean open/short sensor */
#define ADC_RAIL_HIGH       4090u

typedef struct {
    uint16_t samples[SENSOR_AVG_N];
    uint32_t sum;
    uint8_t  idx;
    uint8_t  count;
} moving_avg_t;

void     mavg_init(moving_avg_t *m);
uint16_t mavg_add(moving_avg_t *m, uint16_t sample); /* returns the current average */

uint16_t adc_to_mv(uint16_t raw);
uint16_t vbus_mv_from_raw(uint16_t raw);
int16_t  temp_c_x10_from_raw(uint16_t raw);         /* 0.1 degC units */
bool     adc_raw_at_rail(uint16_t raw);

#endif /* SENSOR_H */
