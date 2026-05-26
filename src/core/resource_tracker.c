/*
 * 5G UE Simulation Application
 * Resource Tracker - Implementation
 */

#include "resource_tracker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
/* Use GetTickCount (always available) instead of GetTickCount64 (Vista+) */
#define get_time_ms() GetTickCount()
#else
#include <sys/time.h>
static uint64_t get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
#endif

/* ============== Internal State ============== */

static resource_entry_t g_resources[RESOURCE_TRACKER_MAX_ENTRIES];
static uint32_t g_resource_count = 0;
static resource_stats_t g_stats = {0};
static bool g_initialized = false;

#ifdef _WIN32
static CRITICAL_SECTION g_resource_lock;
#else
static pthread_mutex_t g_resource_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/* ============== Internal Helpers ============== */

static void lock_resources(void) {
#ifdef _WIN32
    EnterCriticalSection(&g_resource_lock);
#else
    pthread_mutex_lock(&g_resource_mutex);
#endif
}

static void unlock_resources(void) {
#ifdef _WIN32
    LeaveCriticalSection(&g_resource_lock);
#else
    pthread_mutex_unlock(&g_resource_mutex);
#endif
}

static int find_resource_index(void* handle) {
    for (uint32_t i = 0; i < g_resource_count; i++) {
        if (g_resources[i].handle == handle && 
            g_resources[i].state == RESOURCE_STATE_ACTIVE) {
            return (int)i;
        }
    }
    return -1;
}

static void release_resource_internal(resource_entry_t* entry) {
    if (entry == NULL || entry->state != RESOURCE_STATE_ACTIVE) {
        return;
    }
    
    switch (entry->type) {
        case RESOURCE_TYPE_MEMORY:
            if (entry->handle != NULL) {
                uesim_free(entry->handle);
            }
            break;
            
        case RESOURCE_TYPE_SOCKET:
            if ((intptr_t)entry->handle >= 0) {
                uesim_sock_close((int)(intptr_t)entry->handle);
            }
            break;
            
        case RESOURCE_TYPE_MUTEX:
            if (entry->handle != NULL) {
#ifdef _WIN32
                CloseHandle((HANDLE)entry->handle);
#else
                pthread_mutex_destroy((pthread_mutex_t*)entry->handle);
                uesim_free(entry->handle);
#endif
            }
            break;
            
        case RESOURCE_TYPE_COND:
            if (entry->handle != NULL) {
#ifdef _WIN32
                CloseHandle((HANDLE)entry->handle);
#else
                pthread_cond_destroy((pthread_cond_t*)entry->handle);
                uesim_free(entry->handle);
#endif
            }
            break;
            
        case RESOURCE_TYPE_THREAD:
            /* Threads should be joined before cleanup */
#ifdef _WIN32
            if (entry->handle != NULL) {
                CloseHandle((HANDLE)entry->handle);
            }
#endif
            break;
            
        case RESOURCE_TYPE_CONTEXT:
        case RESOURCE_TYPE_BUFFER:
            if (entry->handle != NULL) {
                uesim_free(entry->handle);
            }
            break;
            
        default:
            break;
    }
    
    entry->state = RESOURCE_STATE_RELEASED;
    g_stats.total_released++;
    g_stats.current_active--;
    
    if (entry->type == RESOURCE_TYPE_MEMORY || entry->type == RESOURCE_TYPE_BUFFER) {
        g_stats.memory_bytes_active -= entry->size;
    }
    if (entry->type == RESOURCE_TYPE_SOCKET) {
        if (g_stats.sockets_open > 0) g_stats.sockets_open--;
    }
    if (entry->type == RESOURCE_TYPE_THREAD) {
        if (g_stats.threads_running > 0) g_stats.threads_running--;
    }
}

/* ============== Initialization ============== */

resource_error_t resource_tracker_init(void) {
    if (g_initialized) {
        return RESOURCE_SUCCESS;
    }
    
#ifdef _WIN32
    InitializeCriticalSection(&g_resource_lock);
#endif
    
    memset(g_resources, 0, sizeof(g_resources));
    memset(&g_stats, 0, sizeof(g_stats));
    g_resource_count = 0;
    g_initialized = true;
    
    return RESOURCE_SUCCESS;
}

void resource_tracker_cleanup(void) {
    if (!g_initialized) {
        return;
    }
    
    lock_resources();
    
    /* Release all active resources */
    for (uint32_t i = 0; i < g_resource_count; i++) {
        if (g_resources[i].state == RESOURCE_STATE_ACTIVE) {
            release_resource_internal(&g_resources[i]);
        }
    }
    
    memset(g_resources, 0, sizeof(g_resources));
    g_resource_count = 0;
    
    unlock_resources();
    
#ifdef _WIN32
    DeleteCriticalSection(&g_resource_lock);
#endif
    
    g_initialized = false;
}

bool resource_tracker_is_initialized(void) {
    return g_initialized;
}

/* ============== Resource Tracking ============== */

resource_error_t resource_track(resource_type_t type, void* handle, 
                                 const char* name, uint32_t owner_id,
                                 uint32_t size) {
    if (!g_initialized) {
        return RESOURCE_ERROR_NOT_INITIALIZED;
    }
    
    if (handle == NULL && type != RESOURCE_TYPE_MUTEX && type != RESOURCE_TYPE_COND) {
        return RESOURCE_ERROR_INVALID_PARAM;
    }
    
    lock_resources();
    
    /* Check for duplicate */
    if (find_resource_index(handle) >= 0) {
        unlock_resources();
        return RESOURCE_ERROR_ALREADY_TRACKED;
    }
    
    /* Check capacity */
    if (g_resource_count >= RESOURCE_TRACKER_MAX_ENTRIES) {
        unlock_resources();
        return RESOURCE_ERROR_CAPACITY;
    }
    
    /* Add entry */
    resource_entry_t* entry = &g_resources[g_resource_count];
    entry->type = type;
    entry->state = RESOURCE_STATE_ACTIVE;
    entry->handle = handle;
    entry->owner_id = owner_id;
    entry->size = size;
    entry->alloc_time_ms = get_time_ms();
    entry->flags = 0;
    
    if (name != NULL) {
        strncpy(entry->name, name, RESOURCE_TRACKER_MAX_NAME_LEN - 1);
        entry->name[RESOURCE_TRACKER_MAX_NAME_LEN - 1] = '\0';
    } else {
        entry->name[0] = '\0';
    }
    
    g_resource_count++;
    g_stats.total_allocated++;
    g_stats.current_active++;
    
    if (type == RESOURCE_TYPE_MEMORY || type == RESOURCE_TYPE_BUFFER) {
        g_stats.memory_bytes_allocated += size;
        g_stats.memory_bytes_active += size;
    }
    if (type == RESOURCE_TYPE_SOCKET) {
        g_stats.sockets_open++;
    }
    if (type == RESOURCE_TYPE_THREAD) {
        g_stats.threads_running++;
    }
    
    unlock_resources();
    
    return RESOURCE_SUCCESS;
}

resource_error_t resource_untrack(void* handle) {
    if (!g_initialized) {
        return RESOURCE_ERROR_NOT_INITIALIZED;
    }
    
    if (handle == NULL) {
        return RESOURCE_ERROR_INVALID_PARAM;
    }
    
    lock_resources();
    
    int idx = find_resource_index(handle);
    if (idx < 0) {
        unlock_resources();
        return RESOURCE_ERROR_NOT_FOUND;
    }
    
    g_resources[idx].state = RESOURCE_STATE_RELEASED;
    g_stats.total_released++;
    if (g_stats.current_active > 0) g_stats.current_active--;
    
    if (g_resources[idx].type == RESOURCE_TYPE_MEMORY || g_resources[idx].type == RESOURCE_TYPE_BUFFER) {
        if (g_stats.memory_bytes_active >= g_resources[idx].size) {
            g_stats.memory_bytes_active -= g_resources[idx].size;
        }
    }
    if (g_resources[idx].type == RESOURCE_TYPE_SOCKET) {
        if (g_stats.sockets_open > 0) g_stats.sockets_open--;
    }
    if (g_resources[idx].type == RESOURCE_TYPE_THREAD) {
        if (g_stats.threads_running > 0) g_stats.threads_running--;
    }
    
    unlock_resources();
    
    return RESOURCE_SUCCESS;
}

resource_error_t resource_update_state(void* handle, resource_state_t state) {
    if (!g_initialized) {
        return RESOURCE_ERROR_NOT_INITIALIZED;
    }
    
    if (handle == NULL) {
        return RESOURCE_ERROR_INVALID_PARAM;
    }
    
    lock_resources();
    
    int idx = find_resource_index(handle);
    if (idx < 0) {
        unlock_resources();
        return RESOURCE_ERROR_NOT_FOUND;
    }
    
    g_resources[idx].state = state;
    
    unlock_resources();
    
    return RESOURCE_SUCCESS;
}

/* ============== Cleanup Functions ============== */

uint32_t resource_cleanup_by_owner(uint32_t owner_id) {
    if (!g_initialized) {
        return 0;
    }
    
    uint32_t cleaned = 0;
    
    lock_resources();
    
    for (uint32_t i = 0; i < g_resource_count; i++) {
        if (g_resources[i].owner_id == owner_id && 
            g_resources[i].state == RESOURCE_STATE_ACTIVE) {
            release_resource_internal(&g_resources[i]);
            cleaned++;
        }
    }
    
    unlock_resources();
    
    return cleaned;
}

uint32_t resource_cleanup_by_type(resource_type_t type) {
    if (!g_initialized) {
        return 0;
    }
    
    uint32_t cleaned = 0;
    
    lock_resources();
    
    for (uint32_t i = 0; i < g_resource_count; i++) {
        if (g_resources[i].type == type && 
            g_resources[i].state == RESOURCE_STATE_ACTIVE) {
            release_resource_internal(&g_resources[i]);
            cleaned++;
        }
    }
    
    unlock_resources();
    
    return cleaned;
}

resource_error_t resource_release(void* handle) {
    if (!g_initialized) {
        return RESOURCE_ERROR_NOT_INITIALIZED;
    }
    
    if (handle == NULL) {
        return RESOURCE_ERROR_INVALID_PARAM;
    }
    
    lock_resources();
    
    int idx = find_resource_index(handle);
    if (idx < 0) {
        unlock_resources();
        return RESOURCE_ERROR_NOT_FOUND;
    }
    
    release_resource_internal(&g_resources[idx]);
    
    unlock_resources();
    
    return RESOURCE_SUCCESS;
}

/* ============== Query Functions ============== */

const resource_entry_t* resource_find(void* handle) {
    if (!g_initialized || handle == NULL) {
        return NULL;
    }
    
    lock_resources();
    
    int idx = find_resource_index(handle);
    const resource_entry_t* result = (idx >= 0) ? &g_resources[idx] : NULL;
    
    unlock_resources();
    
    return result;
}

resource_error_t resource_get_stats(resource_stats_t* stats) {
    if (!g_initialized) {
        return RESOURCE_ERROR_NOT_INITIALIZED;
    }
    
    if (stats == NULL) {
        return RESOURCE_ERROR_INVALID_PARAM;
    }
    
    lock_resources();
    memcpy(stats, &g_stats, sizeof(resource_stats_t));
    unlock_resources();
    
    return RESOURCE_SUCCESS;
}

uint32_t resource_check_leaks(void) {
    if (!g_initialized) {
        return 0;
    }
    
    uint32_t leaks = 0;
    
    lock_resources();
    
    for (uint32_t i = 0; i < g_resource_count; i++) {
        if (g_resources[i].state == RESOURCE_STATE_ACTIVE) {
            g_resources[i].state = RESOURCE_STATE_LEAKED;
            leaks++;
        }
    }
    
    g_stats.leaks_detected = leaks;
    
    unlock_resources();
    
    return leaks;
}

void resource_print_report(uint32_t owner_id) {
    if (!g_initialized) {
        printf("[ResourceTracker] Not initialized\n");
        return;
    }
    
    lock_resources();
    
    printf("\n========================================\n");
    printf("       Resource Tracker Report\n");
    printf("========================================\n\n");
    
    if (owner_id == 0) {
        printf("Owner: ALL\n");
    } else {
        printf("Owner: %u\n", owner_id);
    }
    
    printf("\nStatistics:\n");
    printf("  Total Allocated: %u\n", g_stats.total_allocated);
    printf("  Total Released: %u\n", g_stats.total_released);
    printf("  Currently Active: %u\n", g_stats.current_active);
    printf("  Memory Active: %u bytes\n", g_stats.memory_bytes_active);
    printf("  Sockets Open: %u\n", g_stats.sockets_open);
    printf("  Threads Running: %u\n", g_stats.threads_running);
    printf("  Leaks Detected: %u\n", g_stats.leaks_detected);
    
    printf("\nActive Resources:\n");
    printf("----------------------------------------\n");
    
    uint32_t count = 0;
    for (uint32_t i = 0; i < g_resource_count; i++) {
        if (g_resources[i].state == RESOURCE_STATE_ACTIVE) {
            if (owner_id == 0 || g_resources[i].owner_id == owner_id) {
                printf("  [%s] %s (owner=%u, size=%u)\n",
                       resource_type_to_string(g_resources[i].type),
                       g_resources[i].name[0] ? g_resources[i].name : "unnamed",
                       g_resources[i].owner_id,
                       g_resources[i].size);
                count++;
            }
        }
    }
    
    if (count == 0) {
        printf("  No active resources\n");
    }
    
    printf("\n========================================\n\n");
    
    unlock_resources();
}

void resource_print_leaks(void) {
    if (!g_initialized) {
        printf("[ResourceTracker] Not initialized\n");
        return;
    }
    
    lock_resources();
    
    uint32_t leaks = resource_check_leaks();
    
    printf("\n========================================\n");
    printf("       Resource Leak Report\n");
    printf("========================================\n\n");
    
    if (leaks == 0) {
        printf("No resource leaks detected.\n");
    } else {
        printf("WARNING: %u resource leak(s) detected!\n\n", leaks);
        
        for (uint32_t i = 0; i < g_resource_count; i++) {
            if (g_resources[i].state == RESOURCE_STATE_LEAKED) {
                printf("  LEAK: [%s] %s\n",
                       resource_type_to_string(g_resources[i].type),
                       g_resources[i].name[0] ? g_resources[i].name : "unnamed");
                printf("        Handle: %p, Owner: %u, Size: %u\n",
                       g_resources[i].handle,
                       g_resources[i].owner_id,
                       g_resources[i].size);
                printf("        Allocated: %llu ms ago\n\n",
                       (unsigned long long)(get_time_ms() - g_resources[i].alloc_time_ms));
            }
        }
    }
    
    printf("\n========================================\n\n");
    
    unlock_resources();
}

/* ============== Utility Functions ============== */

const char* resource_type_to_string(resource_type_t type) {
    static const char* type_strings[] = {
        "MEMORY", "SOCKET", "THREAD", "MUTEX", "COND",
        "TIMER", "CONTEXT", "BUFFER", "FILE", "UNKNOWN"
    };
    if (type >= RESOURCE_TYPE_MAX) type = RESOURCE_TYPE_MAX;
    return type_strings[type];
}

const char* resource_state_to_string(resource_state_t state) {
    static const char* state_strings[] = {
        "ACTIVE", "RELEASED", "LEAKED"
    };
    if (state > RESOURCE_STATE_LEAKED) return "UNKNOWN";
    return state_strings[state];
}

const char* resource_error_to_string(resource_error_t error) {
    switch (error) {
        case RESOURCE_SUCCESS: return "Success";
        case RESOURCE_ERROR_INVALID_PARAM: return "Invalid parameter";
        case RESOURCE_ERROR_NOT_FOUND: return "Resource not found";
        case RESOURCE_ERROR_CAPACITY: return "Capacity exceeded";
        case RESOURCE_ERROR_ALREADY_TRACKED: return "Resource already tracked";
        case RESOURCE_ERROR_NOT_INITIALIZED: return "Not initialized";
        default: return "Unknown error";
    }
}
