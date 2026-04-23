/*
 * 5G UE Simulation Application
 * Core initialization and management
 */

#include "../uesim.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
/* Windows mutex initializer */
static pthread_mutex_t g_init_mutex;
static int g_init_mutex_initialized = 0;
static volatile LONG g_initialized = 0;
#else
static atomic_bool g_initialized = false;
static pthread_mutex_t g_init_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/* Forward declarations */
static uesim_error_t execute_registration_procedure(ue_context_t* ue_ctx);
static uesim_error_t execute_establishment_procedure(ue_context_t* ue_ctx);
static uesim_error_t execute_reestablishment_procedure(ue_context_t* ue_ctx);
static uesim_error_t execute_handover_procedure(ue_context_t* ue_ctx);

uesim_error_t uesim_init(void) {
    uesim_error_t result = UESIM_SUCCESS;
    
    if (atomic_load(&g_initialized)) {
        return UESIM_SUCCESS;
    }
    
#ifdef _WIN32
    if (!g_init_mutex_initialized) {
        g_init_mutex = CreateMutex(NULL, FALSE, NULL);
        g_init_mutex_initialized = 1;
    }
    WaitForSingleObject(g_init_mutex, INFINITE);
#else
    if (pthread_mutex_lock(&g_init_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
#endif
    
    if (atomic_load(&g_initialized)) {
#ifdef _WIN32
        ReleaseMutex(g_init_mutex);
#else
        pthread_mutex_unlock(&g_init_mutex);
#endif
        return UESIM_SUCCESS;
    }
    
    result = memory_init(UESIM_HEAP_SIZE);
    if (result != UESIM_SUCCESS) {
        fprintf(stderr, "Failed to initialize memory system: %d\n", result);
#ifdef _WIN32
        ReleaseMutex(g_init_mutex);
#else
        pthread_mutex_unlock(&g_init_mutex);
#endif
        return result;
    }
    
    atomic_store(&g_initialized, 1);
    printf("UE Simulation core initialized successfully\n");
    
#ifdef _WIN32
    ReleaseMutex(g_init_mutex);
#else
    pthread_mutex_unlock(&g_init_mutex);
#endif
    
    return UESIM_SUCCESS;
}

void uesim_cleanup(void) {
    if (!atomic_load(&g_initialized)) {
        return;
    }
    
#ifdef _WIN32
    WaitForSingleObject(g_init_mutex, INFINITE);
#else
    if (pthread_mutex_lock(&g_init_mutex) != 0) return;
#endif
    
    if (!atomic_load(&g_initialized)) {
#ifdef _WIN32
        ReleaseMutex(g_init_mutex);
#else
        pthread_mutex_unlock(&g_init_mutex);
#endif
        return;
    }
    
    memory_cleanup();
    atomic_store(&g_initialized, 0);
    printf("UE Simulation core cleanup completed\n");
    
#ifdef _WIN32
    ReleaseMutex(g_init_mutex);
#else
    pthread_mutex_unlock(&g_init_mutex);
#endif
}

uesim_error_t uesim_create_ue_instance(ue_context_t** ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    ue_context_t* ctx = (ue_context_t*)uesim_calloc(1, sizeof(ue_context_t));
    if (ctx == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
#ifdef _WIN32
    ctx->state_mutex = CreateMutex(NULL, FALSE, NULL);
    if (ctx->state_mutex == NULL) {
        uesim_free(ctx);
        return UESIM_ERROR_THREAD;
    }
    ctx->state_cond = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (ctx->state_cond == NULL) {
        CloseHandle(ctx->state_mutex);
        uesim_free(ctx);
        return UESIM_ERROR_THREAD;
    }
#else
    if (pthread_mutex_init(&ctx->state_mutex, NULL) != 0) {
        uesim_free(ctx);
        return UESIM_ERROR_THREAD;
    }
    if (pthread_cond_init(&ctx->state_cond, NULL) != 0) {
        pthread_mutex_destroy(&ctx->state_mutex);
        uesim_free(ctx);
        return UESIM_ERROR_THREAD;
    }
#endif
    
    ctx->current_state = RRC_STATE_IDLE;
    ctx->active = 0;
    
    ctx->rx_buffer_size = MAX_BUFFER_SIZE;
    ctx->tx_buffer_size = MAX_BUFFER_SIZE;
    ctx->rx_buffer = uesim_malloc(ctx->rx_buffer_size);
    ctx->tx_buffer = uesim_malloc(ctx->tx_buffer_size);
    
    if (ctx->rx_buffer == NULL || ctx->tx_buffer == NULL) {
        if (ctx->rx_buffer) uesim_free(ctx->rx_buffer);
        if (ctx->tx_buffer) uesim_free(ctx->tx_buffer);
#ifdef _WIN32
        CloseHandle(ctx->state_cond);
        CloseHandle(ctx->state_mutex);
#else
        pthread_cond_destroy(&ctx->state_cond);
        pthread_mutex_destroy(&ctx->state_mutex);
#endif
        uesim_free(ctx);
        return UESIM_ERROR_MEMORY;
    }
    
    snprintf(ctx->imsi, sizeof(ctx->imsi), "00101%010d", rand() % 1000000000);
    snprintf(ctx->msisdn, sizeof(ctx->msisdn), "12345%06d", rand() % 1000000);
    ctx->tac = 1;
    ctx->gnb_ip = inet_addr("127.0.0.1");
    ctx->gnb_port = 38412;
    
    *ue_ctx = ctx;
    return UESIM_SUCCESS;
}

uesim_error_t uesim_start_ue(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (uesim_lock_state(ue_ctx) != UESIM_SUCCESS) {
        return UESIM_ERROR_THREAD;
    }
    
    if (ue_ctx->active) {
        uesim_unlock_state(ue_ctx);
        return UESIM_SUCCESS;
    }
    
    ue_ctx->active = 1;
    uesim_unlock_state(ue_ctx);
    
    printf("UE instance %u started\n", ue_ctx->ue_id);
    return UESIM_SUCCESS;
}

uesim_error_t uesim_stop_ue(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (uesim_lock_state(ue_ctx) != UESIM_SUCCESS) {
        return UESIM_ERROR_THREAD;
    }
    
    if (!ue_ctx->active) {
        uesim_unlock_state(ue_ctx);
        return UESIM_SUCCESS;
    }
    
    if (ue_ctx->ngap_socket >= 0) {
        uesim_sock_close(ue_ctx->ngap_socket);
        ue_ctx->ngap_socket = -1;
    }
    
    if (ue_ctx->gtpu_socket >= 0) {
        uesim_sock_close(ue_ctx->gtpu_socket);
        ue_ctx->gtpu_socket = -1;
    }
    
    ue_ctx->active = 0;
    uesim_unlock_state(ue_ctx);
    
    printf("UE instance %u stopped\n", ue_ctx->ue_id);
    return UESIM_SUCCESS;
}

uesim_error_t uesim_execute_procedure(ue_context_t* ue_ctx, rrc_procedure_t procedure) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    uesim_error_t result = UESIM_SUCCESS;
    
    if (uesim_lock_state(ue_ctx) != UESIM_SUCCESS) {
        return UESIM_ERROR_THREAD;
    }
    
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
    
    uesim_unlock_state(ue_ctx);
    return result;
}

uesim_error_t uesim_lock_state(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
#ifdef _WIN32
    WaitForSingleObject(ue_ctx->state_mutex, INFINITE);
#else
    if (pthread_mutex_lock(&ue_ctx->state_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
#endif
    return UESIM_SUCCESS;
}

uesim_error_t uesim_unlock_state(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
#ifdef _WIN32
    ReleaseMutex(ue_ctx->state_mutex);
#else
    if (pthread_mutex_unlock(&ue_ctx->state_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
#endif
    return UESIM_SUCCESS;
}

uesim_error_t uesim_wait_for_state_change(ue_context_t* ue_ctx, rrc_state_t expected_state) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
#ifdef _WIN32
    /* Windows: use WaitForSingleObject with 30s timeout */
    DWORD wait_result = WaitForSingleObject(ue_ctx->state_cond, 30000);
    if (wait_result == WAIT_TIMEOUT) {
        return UESIM_ERROR_TIMEOUT;
    }
    if (wait_result != WAIT_OBJECT_0) {
        return UESIM_ERROR_THREAD;
    }
#else
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 30;
    
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
#endif
    
    (void)expected_state;
    return UESIM_SUCCESS;
}

static uesim_error_t execute_registration_procedure(ue_context_t* ue_ctx) {
    printf("Executing RRC registration procedure for UE %u\n", ue_ctx->ue_id);
    return UESIM_SUCCESS;
}

static uesim_error_t execute_establishment_procedure(ue_context_t* ue_ctx) {
    printf("Executing RRC establishment procedure for UE %u\n", ue_ctx->ue_id);
    return UESIM_SUCCESS;
}

static uesim_error_t execute_reestablishment_procedure(ue_context_t* ue_ctx) {
    printf("Executing RRC re-establishment procedure for UE %u\n", ue_ctx->ue_id);
    return UESIM_SUCCESS;
}

static uesim_error_t execute_handover_procedure(ue_context_t* ue_ctx) {
    printf("Executing RRC handover procedure for UE %u\n", ue_ctx->ue_id);
    return UESIM_SUCCESS;
}