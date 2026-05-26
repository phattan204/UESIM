/*
 * 5G UE Simulation Application
 * Resource Tracker - Unified resource management and cleanup
 * 
 * This module provides:
 * - Resource tracking across the application
 * - Automatic cleanup on owner destruction
 * - Leak detection and reporting
 */

#ifndef RESOURCE_TRACKER_H
#define RESOURCE_TRACKER_H

#include "../uesim.h"
#include <stdint.h>
#include <stdbool.h>

/* ============== Constants ============== */

#define RESOURCE_TRACKER_MAX_ENTRIES   4096
#define RESOURCE_TRACKER_MAX_NAME_LEN  32

/* ============== Resource Types ============== */

typedef enum {
    RESOURCE_TYPE_MEMORY = 0,
    RESOURCE_TYPE_SOCKET,
    RESOURCE_TYPE_THREAD,
    RESOURCE_TYPE_MUTEX,
    RESOURCE_TYPE_COND,
    RESOURCE_TYPE_TIMER,
    RESOURCE_TYPE_CONTEXT,
    RESOURCE_TYPE_BUFFER,
    RESOURCE_TYPE_FILE,
    RESOURCE_TYPE_MAX
} resource_type_t;

/* ============== Resource State ============== */

typedef enum {
    RESOURCE_STATE_ACTIVE = 0,
    RESOURCE_STATE_RELEASED,
    RESOURCE_STATE_LEAKED
} resource_state_t;

/* ============== Resource Entry ============== */

typedef struct {
    resource_type_t type;
    resource_state_t state;
    void* handle;
    char name[RESOURCE_TRACKER_MAX_NAME_LEN];
    uint32_t owner_id;          /* UE ID or 0 for global */
    uint32_t size;              /* Size for memory/buffer resources */
    uint64_t alloc_time_ms;      /* Allocation timestamp */
    uint32_t flags;              /* Resource-specific flags */
} resource_entry_t;

/* ============== Resource Statistics ============== */

typedef struct {
    uint32_t total_allocated;
    uint32_t total_released;
    uint32_t current_active;
    uint32_t memory_bytes_allocated;
    uint32_t memory_bytes_active;
    uint32_t sockets_open;
    uint32_t threads_running;
    uint32_t leaks_detected;
} resource_stats_t;

/* ============== Error Codes ============== */

typedef enum {
    RESOURCE_SUCCESS = 0,
    RESOURCE_ERROR_INVALID_PARAM = -1,
    RESOURCE_ERROR_NOT_FOUND = -2,
    RESOURCE_ERROR_CAPACITY = -3,
    RESOURCE_ERROR_ALREADY_TRACKED = -4,
    RESOURCE_ERROR_NOT_INITIALIZED = -5
} resource_error_t;

/* ============== Initialization ============== */

/**
 * Initialize resource tracker
 * @return RESOURCE_SUCCESS or error code
 */
resource_error_t resource_tracker_init(void);

/**
 * Cleanup resource tracker (releases all tracked resources)
 */
void resource_tracker_cleanup(void);

/**
 * Check if resource tracker is initialized
 * @return true if initialized
 */
bool resource_tracker_is_initialized(void);

/* ============== Resource Tracking ============== */

/**
 * Track a resource
 * @param type Resource type
 * @param handle Resource handle (pointer, fd, etc.)
 * @param name Resource name for debugging
 * @param owner_id Owner ID (UE ID or 0 for global)
 * @param size Size for memory resources (0 for others)
 * @return RESOURCE_SUCCESS or error code
 */
resource_error_t resource_track(resource_type_t type, void* handle, 
                                 const char* name, uint32_t owner_id,
                                 uint32_t size);

/**
 * Untrack a resource (does not free)
 * @param handle Resource handle
 * @return RESOURCE_SUCCESS or error code
 */
resource_error_t resource_untrack(void* handle);

/**
 * Update resource state
 * @param handle Resource handle
 * @param state New state
 * @return RESOURCE_SUCCESS or error code
 */
resource_error_t resource_update_state(void* handle, resource_state_t state);

/* ============== Cleanup Functions ============== */

/**
 * Cleanup all resources owned by an owner
 * @param owner_id Owner ID
 * @return Number of resources cleaned up
 */
uint32_t resource_cleanup_by_owner(uint32_t owner_id);

/**
 * Cleanup all resources of a specific type
 * @param type Resource type
 * @return Number of resources cleaned up
 */
uint32_t resource_cleanup_by_type(resource_type_t type);

/**
 * Release a specific resource (calls appropriate cleanup function)
 * @param handle Resource handle
 * @return RESOURCE_SUCCESS or error code
 */
resource_error_t resource_release(void* handle);

/* ============== Query Functions ============== */

/**
 * Find resource by handle
 * @param handle Resource handle
 * @return Resource entry or NULL
 */
const resource_entry_t* resource_find(void* handle);

/**
 * Get resource statistics
 * @param stats Statistics structure to fill
 * @return RESOURCE_SUCCESS or error code
 */
resource_error_t resource_get_stats(resource_stats_t* stats);

/**
 * Check for resource leaks
 * @return Number of leaked resources
 */
uint32_t resource_check_leaks(void);

/**
 * Print resource report
 * @param owner_id Owner ID (0 for all)
 */
void resource_print_report(uint32_t owner_id);

/**
 * Print leak report
 */
void resource_print_leaks(void);

/* ============== Helper Macros ============== */

/* Track memory allocation */
#define RESOURCE_TRACK_MEM(ptr, owner, size) \
    resource_track(RESOURCE_TYPE_MEMORY, (ptr), #ptr, (owner), (size))

/* Track socket */
#define RESOURCE_TRACK_SOCKET(fd, owner) \
    resource_track(RESOURCE_TYPE_SOCKET, (void*)(intptr_t)(fd), #fd, (owner), 0)

/* Track mutex */
#define RESOURCE_TRACK_MUTEX(mutex, owner) \
    resource_track(RESOURCE_TYPE_MUTEX, (mutex), #mutex, (owner), 0)

/* Track context */
#define RESOURCE_TRACK_CTX(ctx, owner) \
    resource_track(RESOURCE_TYPE_CONTEXT, (ctx), #ctx, (owner), sizeof(*ctx))

/* Untrack and set to NULL */
#define RESOURCE_UNTRACK(ptr) do { \
    if (ptr) { \
        resource_untrack(ptr); \
        ptr = NULL; \
    } \
} while(0)

/* ============== Utility Functions ============== */

/**
 * Convert resource type to string
 * @param type Resource type
 * @return Type string
 */
const char* resource_type_to_string(resource_type_t type);

/**
 * Convert resource state to string
 * @param state Resource state
 * @return State string
 */
const char* resource_state_to_string(resource_state_t state);

/**
 * Convert error code to string
 * @param error Error code
 * @return Error string
 */
const char* resource_error_to_string(resource_error_t error);

#endif /* RESOURCE_TRACKER_H */
