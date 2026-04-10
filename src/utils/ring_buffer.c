/*
 * 5G UE Simulation Application
 * Ring Buffer implementation for IPC
 */

#include "ring_buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

uesim_error_t ring_buffer_init(ring_buffer_t* rb, size_t size) {
    if (rb == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Allocate buffer
    rb->buffer = (uint8_t*)uesim_malloc(size);
    if (rb->buffer == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
    
    // Initialize mutexes
    if (pthread_mutex_init(&rb->head_lock, NULL) != 0) {
        uesim_free(rb->buffer);
        rb->buffer = NULL;
        return UESIM_ERROR_THREAD;
    }
    
    if (pthread_mutex_init(&rb->tail_lock, NULL) != 0) {
        pthread_mutex_destroy(&rb->head_lock);
        uesim_free(rb->buffer);
        rb->buffer = NULL;
        return UESIM_ERROR_THREAD;
    }
    
    // Initialize condition variables
    if (pthread_cond_init(&rb->not_empty, NULL) != 0) {
        pthread_mutex_destroy(&rb->head_lock);
        pthread_mutex_destroy(&rb->tail_lock);
        uesim_free(rb->buffer);
        rb->buffer = NULL;
        return UESIM_ERROR_THREAD;
    }
    
    if (pthread_cond_init(&rb->not_full, NULL) != 0) {
        pthread_cond_destroy(&rb->not_empty);
        pthread_mutex_destroy(&rb->head_lock);
        pthread_mutex_destroy(&rb->tail_lock);
        uesim_free(rb->buffer);
        rb->buffer = NULL;
        return UESIM_ERROR_THREAD;
    }
    
    return UESIM_SUCCESS;
}

void ring_buffer_destroy(ring_buffer_t* rb) {
    if (rb == NULL) {
        return;
    }
    
    // Destroy condition variables
    pthread_cond_destroy(&rb->not_empty);
    pthread_cond_destroy(&rb->not_full);
    
    // Destroy mutexes
    pthread_mutex_destroy(&rb->head_lock);
    pthread_mutex_destroy(&rb->tail_lock);
    
    // Free buffer
    if (rb->buffer != NULL) {
        uesim_free(rb->buffer);
        rb->buffer = NULL;
    }
    
    rb->size = 0;
    rb->head = 0;
    rb->tail = 0;
}

uesim_error_t ring_buffer_write(ring_buffer_t* rb, const void* data, size_t length) {
    if (rb == NULL || data == NULL || length == 0) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    const uint8_t* src = (const uint8_t*)data;
    size_t written = 0;
    
    while (written < length) {
        // Lock head for writing
        if (pthread_mutex_lock(&rb->head_lock) != 0) {
            return UESIM_ERROR_THREAD;
        }
        
        // Check available space
        size_t available = ring_buffer_available(rb);
        if (available == 0) {
            // Buffer is full, wait for space
            if (pthread_cond_wait(&rb->not_full, &rb->head_lock) != 0) {
                pthread_mutex_unlock(&rb->head_lock);
                return UESIM_ERROR_THREAD;
            }
            available = ring_buffer_available(rb);
        }
        
        if (available == 0) {
            pthread_mutex_unlock(&rb->head_lock);
            continue;
        }
        
        // Calculate write size
        size_t write_size = (length - written) < available ? (length - written) : available;
        size_t contiguous_space = rb->size - rb->head;
        size_t first_write = write_size < contiguous_space ? write_size : contiguous_space;
        size_t second_write = write_size - first_write;
        
        // Write data
        memcpy(rb->buffer + rb->head, src + written, first_write);
        if (second_write > 0) {
            memcpy(rb->buffer, src + written + first_write, second_write);
        }
        
        // Update head
        rb->head = (rb->head + write_size) % rb->size;
        written += write_size;
        
        // Unlock head
        pthread_mutex_unlock(&rb->head_lock);
        
        // Signal that buffer is not empty
        pthread_cond_signal(&rb->not_empty);
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t ring_buffer_read(ring_buffer_t* rb, void* data, size_t length) {
    if (rb == NULL || data == NULL || length == 0) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    uint8_t* dest = (uint8_t*)data;
    size_t read = 0;
    
    while (read < length) {
        // Lock tail for reading
        if (pthread_mutex_lock(&rb->tail_lock) != 0) {
            return UESIM_ERROR_THREAD;
        }
        
        // Check available data
        size_t used = ring_buffer_used(rb);
        if (used == 0) {
            // Buffer is empty, wait for data
            if (pthread_cond_wait(&rb->not_empty, &rb->tail_lock) != 0) {
                pthread_mutex_unlock(&rb->tail_lock);
                return UESIM_ERROR_THREAD;
            }
            used = ring_buffer_used(rb);
        }
        
        if (used == 0) {
            pthread_mutex_unlock(&rb->tail_lock);
            continue;
        }
        
        // Calculate read size
        size_t read_size = (length - read) < used ? (length - read) : used;
        size_t contiguous_data = rb->size - rb->tail;
        size_t first_read = read_size < contiguous_data ? read_size : contiguous_data;
        size_t second_read = read_size - first_read;
        
        // Read data
        memcpy(dest + read, rb->buffer + rb->tail, first_read);
        if (second_read > 0) {
            memcpy(dest + read + first_read, rb->buffer, second_read);
        }
        
        // Update tail
        rb->tail = (rb->tail + read_size) % rb->size;
        read += read_size;
        
        // Unlock tail
        pthread_mutex_unlock(&rb->tail_lock);
        
        // Signal that buffer is not full
        pthread_cond_signal(&rb->not_full);
    }
    
    return UESIM_SUCCESS;
}

size_t ring_buffer_available(ring_buffer_t* rb) {
    if (rb == NULL) {
        return 0;
    }
    
    size_t head = atomic_load(&rb->head);
    size_t tail = atomic_load(&rb->tail);
    
    if (head >= tail) {
        return rb->size - (head - tail) - 1;
    } else {
        return tail - head - 1;
    }
}

size_t ring_buffer_used(ring_buffer_t* rb) {
    if (rb == NULL) {
        return 0;
    }
    
    size_t head = atomic_load(&rb->head);
    size_t tail = atomic_load(&rb->tail);
    
    if (head >= tail) {
        return head - tail;
    } else {
        return rb->size - tail + head;
    }
}

bool ring_buffer_is_empty(ring_buffer_t* rb) {
    if (rb == NULL) {
        return true;
    }
    
    return (atomic_load(&rb->head) == atomic_load(&rb->tail));
}

bool ring_buffer_is_full(ring_buffer_t* rb) {
    if (rb == NULL) {
        return false;
    }
    
    size_t available = ring_buffer_available(rb);
    return (available == 0);
}