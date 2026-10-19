/*
 * app.h - Application: command dispatch and the periodic control loop.
 */
#ifndef APP_H
#define APP_H

#include <stdint.h>

#define FW_VERSION_MAJOR 1
#define FW_VERSION_MINOR 0
#define FW_VERSION_PATCH 0

#define APP_SAMPLE_PERIOD_MS 10u

/* command codes */
enum {
    CMD_PING         = 0x01,
    CMD_GET_VERSION  = 0x02,
    CMD_READ_SENSORS = 0x03,
    CMD_GET_STATUS   = 0x04,
    CMD_SET_OUTPUT   = 0x05,
    CMD_CLEAR_FAULT  = 0x06,
    CMD_SIM_SET_ADC  = 0x70, /* host simulation build only */
    CMD_NACK         = 0x7F
};

/* NACK reasons */
enum {
    ERR_UNKNOWN_CMD = 0x01,
    ERR_BAD_LEN     = 0x02,
    ERR_BAD_ARG     = 0x03
};

/* SET_OUTPUT / CLEAR_FAULT result codes */
enum {
    RESULT_OK      = 0x00,
    RESULT_REFUSED = 0x01
};

void app_init(void);
void app_step(void); /* call continuously from the main loop */

#endif /* APP_H */
