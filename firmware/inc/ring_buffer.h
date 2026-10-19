/*
 * ring_buffer.h - Single-producer/single-consumer byte FIFO.
 *
 * Used between the UART receive interrupt (producer) and the main loop
 * (consumer). Size must be a power of two so indices wrap with a mask.
 */
#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdbool.h>
#include <stdint.h>

#define RB_SIZE 256u
#define RB_MASK (RB_SIZE - 1u)

typedef struct {
    uint8_t           buf[RB_SIZE];
    volatile uint16_t head; /* next write position (free-running) */
    volatile uint16_t tail; /* next read position (free-running) */
    uint32_t          overflows;
} ring_buffer_t;

void     rb_init(ring_buffer_t *rb);
bool     rb_push(ring_buffer_t *rb, uint8_t byte); /* false and counts overflow when full */
bool     rb_pop(ring_buffer_t *rb, uint8_t *out);  /* false when empty */
uint16_t rb_count(const ring_buffer_t *rb);
bool     rb_is_empty(const ring_buffer_t *rb);
bool     rb_is_full(const ring_buffer_t *rb);

#endif /* RING_BUFFER_H */
