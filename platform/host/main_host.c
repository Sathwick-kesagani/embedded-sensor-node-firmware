/*
 * main_host.c - Entry point for the host simulation build.
 * Runs the same super-loop a microcontroller would, until stdin closes.
 */
#include <time.h>

#include "app.h"

int hal_host_link_closed(void);

int main(void)
{
    app_init();
    struct timespec idle = {0, 200000}; /* 0.2 ms */
    while (!hal_host_link_closed()) {
        app_step();
        nanosleep(&idle, NULL);
    }
    return 0;
}
