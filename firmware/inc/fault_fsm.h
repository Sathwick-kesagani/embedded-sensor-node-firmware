/*
 * fault_fsm.h - Protection state machine.
 *
 *   INIT --(first valid readings)--> NORMAL <--> WARNING
 *     any state --(fault persists FAULT_DEBOUNCE samples)--> FAULT (latched)
 *   FAULT --(CLEAR_FAULT command and all readings back in range)--> NORMAL
 *
 * In FAULT the protected output is switched off and cannot be re-enabled
 * until the fault is cleared.
 */
#ifndef FAULT_FSM_H
#define FAULT_FSM_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    STATE_INIT = 0,
    STATE_NORMAL,
    STATE_WARNING,
    STATE_FAULT
} sys_state_t;

/* fault flags (bitmask) */
#define FAULT_OVERTEMP   0x01u
#define FAULT_UNDERVOLT  0x02u
#define FAULT_OVERVOLT   0x04u
#define FAULT_SENSOR     0x08u

/* limits */
#define TEMP_WARN_C_X10      700  /* 70.0 degC */
#define TEMP_FAULT_C_X10     850  /* 85.0 degC */
#define TEMP_HYST_C_X10      50   /* leave WARNING below 65.0 degC */
#define VBUS_UV_FAULT_MV     10000u
#define VBUS_OV_FAULT_MV     14000u
#define VBUS_WARN_LOW_MV     11000u
#define VBUS_WARN_HIGH_MV    13000u
#define FAULT_DEBOUNCE       3u   /* consecutive bad evaluations before FAULT */

typedef struct {
    uint16_t vbus_mv;
    int16_t  temp_c_x10;
    bool     sensor_ok;
} fsm_inputs_t;

typedef struct {
    sys_state_t state;
    uint8_t     fault_flags;   /* latched flags while in FAULT */
    uint8_t     pending_flags; /* flags seen in the current debounce window */
    uint8_t     debounce;
    uint16_t    fault_count;   /* total entries into FAULT */
    fsm_inputs_t last;
} fault_fsm_t;

void        fsm_init(fault_fsm_t *f);
uint8_t     fsm_evaluate_flags(const fsm_inputs_t *in);
sys_state_t fsm_step(fault_fsm_t *f, const fsm_inputs_t *in);
bool        fsm_clear_fault(fault_fsm_t *f); /* true if the fault was cleared */
bool        fsm_output_allowed(const fault_fsm_t *f);
const char *fsm_state_name(sys_state_t s);

#endif /* FAULT_FSM_H */
