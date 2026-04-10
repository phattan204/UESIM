/*
 * 5G UE Simulation Application
 * Ring Buffer implementation for IPC
 */

#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include "../uesim.h"

// Ring buffer structure
typedef struct {
    uint8_t* buffer;
    size_t size;
    atomic_size_t head;
    atomic_size_t tail;
    pthread_mutex_t head_lock;
    pthread_mutex_t tail_lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} ring_buffer_t;

// Function prototypes
uesim_error_t ring_buffer_init(ring_buffer_t* rb, size_t size);
void ring_buffer_destroy(ring_buffer_t* rb);
uesim_error_t ring_buffer_write(ring_buffer_t* rb, const void* data, size_t length);
uesim_error_t ring_buffer_read(ring_buffer_t* rb, void* data, size_t length);
size_t ring_buffer_available(ring_buffer_t* rb);
size_t ring_buffer_used(ring_buffer_t* rb);
bool ring_buffer_is_empty(ring_buffer_t* rb);
bool ring_buffer_is_full(ring_buffer_t* rb);

#endif // RING_BUFFER_H