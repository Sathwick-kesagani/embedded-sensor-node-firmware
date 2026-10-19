#include "ring_buffer.h"

#include <string.h>

void rb_init(ring_buffer_t *rb)
{
    memset(rb, 0, sizeof(*rb));
}

uint16_t rb_count(const ring_buffer_t *rb)
{
    return (uint16_t)(rb->head - rb->tail);
}

bool rb_is_empty(const ring_buffer_t *rb)
{
    return rb_count(rb) == 0u;
}

bool rb_is_full(const ring_buffer_t *rb)
{
    return rb_count(rb) >= RB_SIZE;
}

bool rb_push(ring_buffer_t *rb, uint8_t byte)
{
    if (rb_is_full(rb)) {
        rb->overflows++;
        return false;
    }
    rb->buf[rb->head & RB_MASK] = byte;
    rb->head++; /* publish after the data is written */
    return true;
}

bool rb_pop(ring_buffer_t *rb, uint8_t *out)
{
    if (rb_is_empty(rb)) {
        return false;
    }
    *out = rb->buf[rb->tail & RB_MASK];
    rb->tail++;
    return true;
}
