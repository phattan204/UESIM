/*
 * 5G UE Simulation Application
 * Memory management implementation
 */

#include "../uesim.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// Memory pool structure
typedef struct {
    void* base_address;
    size_t total_size;
    size_t used_size;
    pthread_mutex_t lock;
} memory_pool_t;

// Global memory pool
static memory_pool_t g_memory_pool = {0};

uesim_error_t memory_init(size_t heap_size) {
    // Initialize memory pool
    g_memory_pool.base_address = malloc(heap_size);
    if (g_memory_pool.base_address == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    g_memory_pool.total_size = heap_size;
    g_memory_pool.used_size = 0;
    
    if (pthread_mutex_init(&g_memory_pool.lock, NULL) != 0) {
        free(g_memory_pool.base_address);
        g_memory_pool.base_address = NULL;
        return UESIM_ERROR_THREAD;
    }
    
    return UESIM_SUCCESS;
}

void memory_cleanup(void) {
    if (g_memory_pool.base_address != NULL) {
        free(g_memory_pool.base_address);
        g_memory_pool.base_address = NULL;
    }
    
    pthread_mutex_destroy(&g_memory_pool.lock);
    g_memory_pool.total_size = 0;
    g_memory_pool.used_size = 0;
}

void* uesim_malloc(size_t size) {
    void* ptr = NULL;
    
    // Acquire lock
    if (pthread_mutex_lock(&g_memory_pool.lock) != 0) {
        // Fallback to system malloc if lock fails
        return malloc(size);
    }
    
    // Check if we have enough space
    if (g_memory_pool.used_size + size <= g_memory_pool.total_size) {
        ptr = (char*)g_memory_pool.base_address + g_memory_pool.used_size;
        g_memory_pool.used_size += size;
    }
    
    // Release lock
    pthread_mutex_unlock(&g_memory_pool.lock);
    
    // Fallback to system malloc if pool is full
    if (ptr == NULL) {
        ptr = malloc(size);
    }
    
    return ptr;
}

void* uesim_calloc(size_t nmemb, size_t size) {
    size_t total_size = nmemb * size;
    void* ptr = uesim_malloc(total_size);
    
    if (ptr != NULL) {
        memset(ptr, 0, total_size);
    }
    
    return ptr;
}

void uesim_free(void* ptr) {
    // Check if pointer is within our memory pool
    if (ptr >= g_memory_pool.base_address && 
        ptr < (char*)g_memory_pool.base_address + g_memory_pool.total_size) {
        // For simplicity, we don't actually free from pool
        // In a real implementation, we would need a more sophisticated allocator
        return;
    }
    
    // Free using system free
    free(ptr);
}

// Memory layout information
void print_memory_layout(void) {
    printf("Memory Layout Information:\n");
    printf("  Stack Size: %zu bytes\n", UESIM_STACK_SIZE);
    printf("  Heap Size: %zu bytes\n", UESIM_HEAP_SIZE);
    printf("  Data Segment Size: %zu bytes\n", UESIM_DATA_SEGMENT);
    printf("  Memory Pool Base: %p\n", g_memory_pool.base_address);
    printf("  Memory Pool Size: %zu bytes\n", g_memory_pool.total_size);
    printf("  Memory Pool Used: %zu bytes\n", g_memory_pool.used_size);
}