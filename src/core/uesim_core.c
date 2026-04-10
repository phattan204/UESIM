/*
 * 5G UE Simulation Application
 * Core initialization and management
 */

#include "../uesim.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Global variables
static atomic_bool g_initialized = false;
static pthread_mutex_t g_init_mutex = PTHREAD_MUTEX_INITIALIZER;

uesim_error_t uesim_init(void) {
    uesim_error_t result = UESIM_SUCCESS;
    
    // Check if already initialized
    if (atomic_load(&g_initialized)) {
        return UESIM_SUCCESS;
    }
    
    // Acquire initialization lock
    if (pthread_mutex_lock(&g_init_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    // Double-check pattern
    if (atomic_load(&g_initialized)) {
        pthread_mutex_unlock(&g_init_mutex);
        return UESIM_SUCCESS;
    }
    
    // Initialize memory system
    result = memory_init(UESIM_HEAP_SIZE);
    if (result != UESIM_SUCCESS) {
        fprintf(stderr, "Failed to initialize memory system: %d\n", result);
        pthread_mutex_unlock(&g_init_mutex);
        return result;
    }
    
    // Initialize other subsystems
    // TODO: Initialize socket system
    // TODO: Initialize protocol stack
    // TODO: Initialize thread pool
    
    // Mark as initialized
    atomic_store(&g_initialized, true);
    
    printf("UE Simulation core initialized successfully\n");
    
    // Release initialization lock
    pthread_mutex_unlock(&g_init_mutex);
    
    return UESIM_SUCCESS;
}

void uesim_cleanup(void) {
    // Check if initialized
    if (!atomic_load(&g_initialized)) {
        return;
    }
    
    // Acquire initialization lock
    if (pthread_mutex_lock(&g_init_mutex) != 0) {
        return;
    }
    
    // Double-check pattern
    if (!atomic_load(&g_initialized)) {
        pthread_mutex_unlock(&g_init_mutex);
        return;
    }
    
    // Cleanup subsystems
    // TODO: Cleanup thread pool
    // TODO: Cleanup protocol stack
    // TODO: Cleanup socket system
    
    // Cleanup memory system
    memory_cleanup();
    
    // Mark as not initialized
    atomic_store(&g_initialized, false);
    
    printf("UE Simulation core cleanup completed\n");
    
    // Release initialization lock
    pthread_mutex_unlock(&g_init_mutex);
}

uesim_error_t uesim_create_ue_instance(ue_context_t** ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Allocate UE context
    ue_context_t* ctx = (ue_context_t*)uesim_calloc(1, sizeof(ue_context_t));
    if (ctx == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    // Initialize mutex and condition variable
    if (pthread_mutex_init(&ctx->state_mutex, NULL) != 0) {
        uesim_free(ctx);
        return UESIM_ERROR_THREAD;
    }
    
    if (pthread_cond_init(&ctx->state_cond, NULL) != 0) {
        pthread_mutex_destroy(&ctx->state_mutex);
        uesim_free(ctx);
        return UESIM_ERROR_THREAD;
    }
    
    // Set initial state
    ctx->current_state = RRC_STATE_IDLE;
    ctx->active = false;
    
    // Allocate buffers
    ctx->rx_buffer_size = MAX_BUFFER_SIZE;
    ctx->tx_buffer_size = MAX_BUFFER_SIZE;
    ctx->rx_buffer = uesim_malloc(ctx->rx_buffer_size);
    ctx->tx_buffer = uesim_malloc(ctx->tx_buffer_size);
    
    if (ctx->rx_buffer == NULL || ctx->tx_buffer == NULL) {
        // Cleanup on error
        if (ctx->rx_buffer) uesim_free(ctx->rx_buffer);
        if (ctx->tx_buffer) uesim_free(ctx->tx_buffer);
        pthread_cond_destroy(&ctx->state_cond);
        pthread_mutex_destroy(&ctx->state_mutex);
        uesim_free(ctx);
        return UESIM_ERROR_MEMORY;
    }
    
    // Set default configuration
    snprintf(ctx->imsi, sizeof(ctx->imsi), "00101%010d", rand() % 1000000000);
    snprintf(ctx->msisdn, sizeof(ctx->msisdn), "12345%06d", rand() % 1000000);
    ctx->tac = 1;
    ctx->gnb_ip = inet_addr("127.0.0.1");
    ctx->gnb_port = 38412; // NGAP default port
    
    *ue_ctx = ctx;
    return UESIM_SUCCESS;
}

uesim_error_t uesim_start_ue(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Lock state
    if (uesim_lock_state(ue_ctx) != UESIM_SUCCESS) {
        return UESIM_ERROR_THREAD;
    }
    
    // Check if already active
    if (ue_ctx->active) {
        uesim_unlock_state(ue_ctx);
        return UESIM_SUCCESS;
    }
    
    // Create socket connections
    // TODO: Implement socket creation
    
    // Set active flag
    ue_ctx->active = true;
    
    // Unlock state
    uesim_unlock_state(ue_ctx);
    
    printf("UE instance %u started\n", ue_ctx->ue_id);
    return UESIM_SUCCESS;
}

uesim_error_t uesim_stop_ue(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Lock state
    if (uesim_lock_state(ue_ctx) != UESIM_SUCCESS) {
        return UESIM_ERROR_THREAD;
    }
    
    // Check if already inactive
    if (!ue_ctx->active) {
        uesim_unlock_state(ue_ctx);
        return UESIM_SUCCESS;
    }
    
    // Close socket connections
    if (ue_ctx->ngap_socket >= 0) {
        close(ue_ctx->ngap_socket);
        ue_ctx->ngap_socket = -1;
    }
    
    if (ue_ctx->gtpu_socket >= 0) {
        close(ue_ctx->gtpu_socket);
        ue_ctx->gtpu_socket = -1;
    }
    
    // Clear active flag
    ue_ctx->active = false;
    
    // Unlock state
    uesim_unlock_state(ue_ctx);
    
    printf("UE instance %u stopped\n", ue_ctx->ue_id);
    return UESIM_SUCCESS;
}

uesim_error_t uesim_execute_procedure(ue_context_t* ue_ctx, rrc_procedure_t procedure) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    uesim_error_t result = UESIM_SUCCESS;
    
    // Lock state
    if (uesim_lock_state(ue_ctx) != UESIM_SUCCESS) {
        return UESIM_ERROR_THREAD;
    }
    
    // Execute procedure based on type
    switch (procedure) {
        case RRC_PROC_REGISTRATION:
            result = execute_registration_procedure(ue_ctx);
            break;
        case RRC_PROC_ESTABLISHMENT:
            result = execute_establishment_procedure(ue_ctx);
            break;
        case RRC_PROC_REESTABLISHMENT:
            result = execute_reestablishment_procedure(ue_ctx);
            break;
        case RRC_PROC_HANDOVER:
            result = execute_handover_procedure(ue_ctx);
            break;
        default:
            result = UESIM_ERROR_INVALID_PARAM;
            break;
    }
    
    // Unlock state
    uesim_unlock_state(ue_ctx);
    
    return result;
}

// Thread-safe functions
uesim_error_t uesim_lock_state(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&ue_ctx->state_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t uesim_unlock_state(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_unlock(&ue_ctx->state_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t uesim_wait_for_state_change(ue_context_t* ue_ctx, rrc_state_t expected_state) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 30; // 30 second timeout
    
    if (pthread_mutex_lock(&ue_ctx->state_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    while (ue_ctx->current_state != expected_state) {
        int result = pthread_cond_timedwait(&ue_ctx->state_cond, &ue_ctx->state_mutex, &timeout);
        if (result == ETIMEDOUT) {
            pthread_mutex_unlock(&ue_ctx->state_mutex);
            return UESIM_ERROR_TIMEOUT;
        } else if (result != 0) {
            pthread_mutex_unlock(&ue_ctx->state_mutex);
            return UESIM_ERROR_THREAD;
        }
    }
    
    pthread_mutex_unlock(&ue_ctx->state_mutex);
    return UESIM_SUCCESS;
}

// Procedure implementations (to be completed)
static uesim_error_t execute_registration_procedure(ue_context_t* ue_ctx) {
    // TODO: Implement RRC registration procedure
    printf("Executing RRC registration procedure for UE %u\n", ue_ctx->ue_id);
    return UESIM_SUCCESS;
}

static uesim_error_t execute_establishment_procedure(ue_context_t* ue_ctx) {
    // TODO: Implement RRC establishment procedure
    printf("Executing RRC establishment procedure for UE %u\n", ue_ctx->ue_id);
    return UESIM_SUCCESS;
}

static uesim_error_t execute_reestablishment_procedure(ue_context_t* ue_ctx) {
    // TODO: Implement RRC re-establishment procedure
    printf("Executing RRC re-establishment procedure for UE %u\n", ue_ctx->ue_id);
    return UESIM_SUCCESS;
}

static uesim_error_t execute_handover_procedure(ue_context_t* ue_ctx) {
    // TODO: Implement RRC handover procedure
    printf("Executing RRC handover procedure for UE %u\n", ue_ctx->ue_id);
    return UESIM_SUCCESS;
}