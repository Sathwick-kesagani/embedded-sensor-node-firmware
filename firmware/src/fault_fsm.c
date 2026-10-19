#include "fault_fsm.h"

#include <string.h>

void fsm_init(fault_fsm_t *f)
{
    memset(f, 0, sizeof(*f));
    f->state = STATE_INIT;
}

uint8_t fsm_evaluate_flags(const fsm_inputs_t *in)
{
    uint8_t flags = 0;
    if (!in->sensor_ok) {
        return FAULT_SENSOR; /* readings cannot be trusted */
    }
    if (in->temp_c_x10 >= TEMP_FAULT_C_X10) {
        flags |= FAULT_OVERTEMP;
    }
    if (in->vbus_mv < VBUS_UV_FAULT_MV) {
        flags |= FAULT_UNDERVOLT;
    }
    if (in->vbus_mv > VBUS_OV_FAULT_MV) {
        flags |= FAULT_OVERVOLT;
    }
    return flags;
}

static bool in_warning_band(const fault_fsm_t *f, const fsm_inputs_t *in)
{
    int16_t temp_limit = (f->state == STATE_WARNING)
                             ? (int16_t)(TEMP_WARN_C_X10 - TEMP_HYST_C_X10) /* hysteresis */
                             : (int16_t)TEMP_WARN_C_X10;
    return in->temp_c_x10 >= temp_limit || in->vbus_mv < VBUS_WARN_LOW_MV ||
           in->vbus_mv > VBUS_WARN_HIGH_MV;
}

sys_state_t fsm_step(fault_fsm_t *f, const fsm_inputs_t *in)
{
    f->last = *in;
    uint8_t flags = fsm_evaluate_flags(in);

    if (f->state == STATE_FAULT) {
        return f->state; /* latched until fsm_clear_fault() */
    }

    if (flags != 0u) {
        f->pending_flags |= flags;
        if (++f->debounce >= FAULT_DEBOUNCE) {
            f->state = STATE_FAULT;
            f->fault_flags = f->pending_flags;
            f->fault_count++;
            f->debounce = 0;
            f->pending_flags = 0;
        }
        return f->state;
    }

    f->debounce = 0;
    f->pending_flags = 0;
    f->state = in_warning_band(f, in) ? STATE_WARNING : STATE_NORMAL;
    return f->state;
}

bool fsm_clear_fault(fault_fsm_t *f)
{
    if (f->state != STATE_FAULT) {
        return true;
    }
    if (fsm_evaluate_flags(&f->last) != 0u) {
        return false; /* condition still present */
    }
    f->fault_flags = 0;
    f->state = in_warning_band(f, &f->last) ? STATE_WARNING : STATE_NORMAL;
    return true;
}

bool fsm_output_allowed(const fault_fsm_t *f)
{
    return f->state == STATE_NORMAL || f->state == STATE_WARNING;
}

const char *fsm_state_name(sys_state_t s)
{
    switch (s) {
    case STATE_INIT:    return "INIT";
    case STATE_NORMAL:  return "NORMAL";
    case STATE_WARNING: return "WARNING";
    case STATE_FAULT:   return "FAULT";
    default:            return "?";
    }
}
