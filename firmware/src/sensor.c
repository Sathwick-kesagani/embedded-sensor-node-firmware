#include "sensor.h"

#include <string.h>

void mavg_init(moving_avg_t *m)
{
    memset(m, 0, sizeof(*m));
}

uint16_t mavg_add(moving_avg_t *m, uint16_t sample)
{
    if (m->count < SENSOR_AVG_N) {
        m->count++;
    } else {
        m->sum -= m->samples[m->idx]; /* drop the oldest sample */
    }
    m->samples[m->idx] = sample;
    m->sum += sample;
    m->idx = (uint8_t)((m->idx + 1u) % SENSOR_AVG_N);
    return (uint16_t)((m->sum + m->count / 2u) / m->count); /* rounded */
}

uint16_t adc_to_mv(uint16_t raw)
{
    if (raw > ADC_FULL_SCALE) {
        raw = ADC_FULL_SCALE;
    }
    return (uint16_t)(((uint32_t)raw * ADC_VREF_MV + ADC_FULL_SCALE / 2u) / ADC_FULL_SCALE);
}

uint16_t vbus_mv_from_raw(uint16_t raw)
{
    if (raw > ADC_FULL_SCALE) {
        raw = ADC_FULL_SCALE;
    }
    uint32_t mv = ((uint32_t)raw * ADC_VREF_MV * VBUS_DIVIDER + ADC_FULL_SCALE / 2u) / ADC_FULL_SCALE;
    return (uint16_t)(mv > 0xFFFFu ? 0xFFFFu : mv);
}

int16_t temp_c_x10_from_raw(uint16_t raw)
{
    /* 10 mV per degC with a 500 mV offset, so 0.1 degC per mV after the offset */
    return (int16_t)((int32_t)adc_to_mv(raw) - 500);
}

bool adc_raw_at_rail(uint16_t raw)
{
    return raw <= ADC_RAIL_LOW || raw >= ADC_RAIL_HIGH;
}
