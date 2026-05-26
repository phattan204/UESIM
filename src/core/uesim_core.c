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
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#ifdef _WIN32
/* Windows mutex initializer */
static pthread_mutex_t g_init_mutex;
static int g_init_mutex_initialized = 0;
static volatile LONG g_initialized = 0;
#else
static atomic_bool g_initialized = false;
static pthread_mutex_t g_init_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/* gNB ID counter */
static uint32_t g_gnb_id_counter = 0;

/* Forward declarations */
static uesim_error_t execute_registration_procedure(ue_context_t* ue_ctx);
static uesim_error_t execute_establishment_procedure(ue_context_t* ue_ctx);
static uesim_error_t execute_reestablishment_procedure(ue_context_t* ue_ctx);
static uesim_error_t execute_handover_procedure(ue_context_t* ue_ctx);

/* gNB type string conversion */
const char* uesim_gnb_type_str(gnb_type_t type) {
    static const char* gnb_type_strings[] = {
        "OAI", "srsRAN", "Commercial", "Mock", "Unknown"
    };
    if (type >= GNB_TYPE_MAX) type = GNB_TYPE_MAX;
    return gnb_type_strings[type];
}

/* gNB state string conversion */
const char* uesim_gnb_state_str(gnb_state_t state) {
    static const char* gnb_state_strings[] = {
        "Unknown", "Connected", "Disconnected", "Handover_Candidate", "Connecting", "Max"
    };
    if (state >= GNB_STATE_MAX) state = GNB_STATE_MAX;
    return gnb_state_strings[state];
}

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
    
#ifdef _WIN32
    /* Initialize Winsock on Windows */
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        fprintf(stderr, "Failed to initialize Winsock\n");
        ReleaseMutex(g_init_mutex);
        return UESIM_ERROR_SOCKET;
    }
    printf("Winsock initialized (version %d.%d)\n", 
           LOBYTE(wsa_data.wVersion), HIBYTE(wsa_data.wVersion));
#endif
    
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
    
#ifdef _WIN32
    /* Cleanup Winsock */
    WSACleanup();
    printf("Winsock cleanup completed\n");
#endif
    
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
        fprintf(stderr, "uesim_create_ue_instance: NULL parameter\n");
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    fprintf(stderr, "DEBUG: uesim_create_ue_instance: allocating UE context (%zu bytes)\n", sizeof(ue_context_t));
    
    ue_context_t* ctx = (ue_context_t*)uesim_calloc(1, sizeof(ue_context_t));
    if (ctx == NULL) {
        fprintf(stderr, "uesim_create_ue_instance: memory allocation failed\n");
        return UESIM_ERROR_MEMORY;
    }
    
    fprintf(stderr, "DEBUG: uesim_create_ue_instance: allocation successful, ctx=%p\n", (void*)ctx);
    
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
    ctx->gnb_list_mutex = CreateMutex(NULL, FALSE, NULL);
    if (ctx->gnb_list_mutex == NULL) {
        CloseHandle(ctx->state_cond);
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
    if (pthread_mutex_init(&ctx->gnb_list_mutex, NULL) != 0) {
        pthread_cond_destroy(&ctx->state_cond);
        pthread_mutex_destroy(&ctx->state_mutex);
        uesim_free(ctx);
        return UESIM_ERROR_THREAD;
    }
#endif
    
    ctx->current_state = RRC_STATE_IDLE;
    ctx->active = 0;
    ctx->serving_gnb = NULL;
    ctx->num_candidate_gnbs = 0;
    memset(ctx->candidate_gnbs, 0, sizeof(ctx->candidate_gnbs));
    
    /* Legacy socket fields */
    ctx->ngap_socket = -1;
    ctx->gtpu_socket = -1;
    
    ctx->rx_buffer_size = MAX_BUFFER_SIZE;
    ctx->tx_buffer_size = MAX_BUFFER_SIZE;
    ctx->rx_buffer = uesim_malloc(ctx->rx_buffer_size);
    ctx->tx_buffer = uesim_malloc(ctx->tx_buffer_size);
    
    /* Initialize layer contexts to NULL */
    ctx->nas_ctx = NULL;
    ctx->rrc_state_ctx = NULL;
    ctx->rrc_meas_ctx = NULL;
    
    /* Initialize protocol layer entities */
    ctx->mac_entity = NULL;
    memset(ctx->rlc_entities, 0, sizeof(ctx->rlc_entities));
    memset(ctx->pdcp_entities, 0, sizeof(ctx->pdcp_entities));
    ctx->num_active_rlc_entities = 0;
    ctx->num_active_pdcp_entities = 0;
    
    /* Initialize radio bearer configuration */
    memset(ctx->srb_config, 0, sizeof(ctx->srb_config));
    memset(ctx->drb_config, 0, sizeof(ctx->drb_config));
    ctx->num_active_srbs = 0;
    ctx->num_active_drbs = 0;
    
    /* Initialize UE capabilities with defaults */
    memset(&ctx->capabilities, 0, sizeof(ctx->capabilities));
    ctx->capabilities.ue_category = 15;  /* Default NR category */
    ctx->capabilities.nr_capability = true;
    ctx->capabilities.lte_capability = true;
    ctx->capabilities.pdcp_sn_lengths = 0x06;  /* 12-bit and 18-bit SN support */
    
    /* Initialize DRX configuration (disabled by default) */
    memset(&ctx->drx_config, 0, sizeof(ctx->drx_config));
    ctx->drx_config.enabled = false;
    
    /* Initialize RRC timers */
    memset(&ctx->rrc_timers, 0, sizeof(ctx->rrc_timers));
    
    /* Initialize statistics */
    memset(&ctx->stats, 0, sizeof(ctx->stats));
    ctx->stats.connection_start_time = time(NULL);
    
    if (ctx->rx_buffer == NULL || ctx->tx_buffer == NULL) {
        if (ctx->rx_buffer) uesim_free(ctx->rx_buffer);
        if (ctx->tx_buffer) uesim_free(ctx->tx_buffer);
#ifdef _WIN32
        CloseHandle(ctx->gnb_list_mutex);
        CloseHandle(ctx->state_cond);
        CloseHandle(ctx->state_mutex);
#else
        pthread_mutex_destroy(&ctx->gnb_list_mutex);
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
    
    /* Close all gNB connections */
    if (ue_ctx->serving_gnb != NULL) {
        uesim_disconnect_gnb(ue_ctx, ue_ctx->serving_gnb);
    }
    
    for (int i = 0; i < ue_ctx->num_candidate_gnbs; i++) {
        if (ue_ctx->candidate_gnbs[i] != NULL) {
            uesim_disconnect_gnb(ue_ctx, ue_ctx->candidate_gnbs[i]);
        }
    }
    
    /* Legacy socket cleanup */
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

/* ============== Multi-gNB Management Functions ============== */

uesim_error_t uesim_add_gnb(ue_context_t* ue_ctx, gnb_type_t type,
                            const char* ip, uint16_t port, gnb_context_t** gnb_ctx) {
    if (ue_ctx == NULL || ip == NULL || gnb_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Lock gNB list */
#ifdef _WIN32
    WaitForSingleObject(ue_ctx->gnb_list_mutex, INFINITE);
#else
    if (pthread_mutex_lock(&ue_ctx->gnb_list_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
#endif
    
    /* Check capacity */
    if (ue_ctx->num_candidate_gnbs >= MAX_GNB_CANDIDATES && ue_ctx->serving_gnb != NULL) {
#ifdef _WIN32
        ReleaseMutex(ue_ctx->gnb_list_mutex);
#else
        pthread_mutex_unlock(&ue_ctx->gnb_list_mutex);
#endif
        return UESIM_ERROR_CAPACITY;
    }
    
    /* Allocate gNB context */
    gnb_context_t* gnb = (gnb_context_t*)uesim_calloc(1, sizeof(gnb_context_t));
    if (gnb == NULL) {
#ifdef _WIN32
        ReleaseMutex(ue_ctx->gnb_list_mutex);
#else
        pthread_mutex_unlock(&ue_ctx->gnb_list_mutex);
#endif
        return UESIM_ERROR_MEMORY;
    }
    
    /* Initialize gNB context */
    gnb->gnb_id = ++g_gnb_id_counter;
    gnb->type = type;
    gnb->state = GNB_STATE_DISCONNECTED;
    gnb->addr.sin_family = AF_INET;
    gnb->addr.sin_port = htons(port);
    gnb->addr.sin_addr.s_addr = inet_addr(ip);
    gnb->ngap_socket = -1;
    gnb->gtpu_socket = -1;
    gnb->cell_id = 0;
    gnb->tac = 0;
    gnb->rsrp = -140;  /* Invalid/unknown */
    gnb->rsrq = -20;   /* Invalid/unknown */
    gnb->is_serving = false;
    
#ifdef _WIN32
    gnb->gnb_mutex = CreateMutex(NULL, FALSE, NULL);
    if (gnb->gnb_mutex == NULL) {
        uesim_free(gnb);
        ReleaseMutex(ue_ctx->gnb_list_mutex);
        return UESIM_ERROR_THREAD;
    }
#else
    if (pthread_mutex_init(&gnb->gnb_mutex, NULL) != 0) {
        uesim_free(gnb);
        pthread_mutex_unlock(&ue_ctx->gnb_list_mutex);
        return UESIM_ERROR_THREAD;
    }
#endif
    
    /* Add to candidate list or set as serving if none exists */
    if (ue_ctx->serving_gnb == NULL) {
        gnb->is_serving = true;
        ue_ctx->serving_gnb = gnb;
        
        /* Update legacy fields for backward compatibility */
        ue_ctx->gnb_addr = gnb->addr;
        ue_ctx->gnb_ip = gnb->addr.sin_addr.s_addr;
        ue_ctx->gnb_port = port;
        
        printf("Added gNB %u (%s) as serving gNB for UE %u\n", 
               gnb->gnb_id, uesim_gnb_type_str(type), ue_ctx->ue_id);
    } else {
        /* Check for duplicate */
        for (int i = 0; i < ue_ctx->num_candidate_gnbs; i++) {
            if (ue_ctx->candidate_gnbs[i] != NULL &&
                ue_ctx->candidate_gnbs[i]->addr.sin_addr.s_addr == gnb->addr.sin_addr.s_addr &&
                ue_ctx->candidate_gnbs[i]->addr.sin_port == gnb->addr.sin_port) {
#ifdef _WIN32
                CloseHandle(gnb->gnb_mutex);
#else
                pthread_mutex_destroy(&gnb->gnb_mutex);
#endif
                uesim_free(gnb);
#ifdef _WIN32
                ReleaseMutex(ue_ctx->gnb_list_mutex);
#else
                pthread_mutex_unlock(&ue_ctx->gnb_list_mutex);
#endif
                return UESIM_ERROR_ALREADY_EXISTS;
            }
        }
        
        gnb->state = GNB_STATE_HANDOVER_CANDIDATE;
        ue_ctx->candidate_gnbs[ue_ctx->num_candidate_gnbs++] = gnb;
        
        printf("Added gNB %u (%s) as candidate gNB %d for UE %u\n", 
               gnb->gnb_id, uesim_gnb_type_str(type), ue_ctx->num_candidate_gnbs - 1, ue_ctx->ue_id);
    }
    
    *gnb_ctx = gnb;
    
#ifdef _WIN32
    ReleaseMutex(ue_ctx->gnb_list_mutex);
#else
    pthread_mutex_unlock(&ue_ctx->gnb_list_mutex);
#endif
    
    return UESIM_SUCCESS;
}

uesim_error_t uesim_remove_gnb(ue_context_t* ue_ctx, gnb_context_t* gnb_ctx) {
    if (ue_ctx == NULL || gnb_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
#ifdef _WIN32
    WaitForSingleObject(ue_ctx->gnb_list_mutex, INFINITE);
#else
    if (pthread_mutex_lock(&ue_ctx->gnb_list_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
#endif
    
    /* Cannot remove serving gNB directly */
    if (ue_ctx->serving_gnb == gnb_ctx) {
#ifdef _WIN32
        ReleaseMutex(ue_ctx->gnb_list_mutex);
#else
        pthread_mutex_unlock(&ue_ctx->gnb_list_mutex);
#endif
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Find and remove from candidate list */
    bool found = false;
    for (int i = 0; i < ue_ctx->num_candidate_gnbs; i++) {
        if (ue_ctx->candidate_gnbs[i] == gnb_ctx) {
            /* Disconnect if connected */
            if (gnb_ctx->state == GNB_STATE_CONNECTED) {
                uesim_disconnect_gnb(ue_ctx, gnb_ctx);
            }
            
#ifdef _WIN32
            CloseHandle(gnb_ctx->gnb_mutex);
#else
            pthread_mutex_destroy(&gnb_ctx->gnb_mutex);
#endif
            uesim_free(gnb_ctx);
            
            /* Shift remaining candidates */
            for (int j = i; j < ue_ctx->num_candidate_gnbs - 1; j++) {
                ue_ctx->candidate_gnbs[j] = ue_ctx->candidate_gnbs[j + 1];
            }
            ue_ctx->candidate_gnbs[ue_ctx->num_candidate_gnbs - 1] = NULL;
            ue_ctx->num_candidate_gnbs--;
            found = true;
            break;
        }
    }
    
#ifdef _WIN32
    ReleaseMutex(ue_ctx->gnb_list_mutex);
#else
    pthread_mutex_unlock(&ue_ctx->gnb_list_mutex);
#endif
    
    if (!found) {
        return UESIM_ERROR_NOT_FOUND;
    }
    
    printf("Removed gNB from UE %u candidate list\n", ue_ctx->ue_id);
    return UESIM_SUCCESS;
}

uesim_error_t uesim_switch_serving_gnb(ue_context_t* ue_ctx, gnb_context_t* new_gnb) {
    if (ue_ctx == NULL || new_gnb == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
#ifdef _WIN32
    WaitForSingleObject(ue_ctx->gnb_list_mutex, INFINITE);
#else
    if (pthread_mutex_lock(&ue_ctx->gnb_list_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
#endif
    
    /* Verify new_gnb is in candidate list */
    bool found = false;
    int found_idx = -1;
    for (int i = 0; i < ue_ctx->num_candidate_gnbs; i++) {
        if (ue_ctx->candidate_gnbs[i] == new_gnb) {
            found = true;
            found_idx = i;
            break;
        }
    }
    
    if (!found) {
#ifdef _WIN32
        ReleaseMutex(ue_ctx->gnb_list_mutex);
#else
        pthread_mutex_unlock(&ue_ctx->gnb_list_mutex);
#endif
        return UESIM_ERROR_NOT_FOUND;
    }
    
    /* Disconnect old serving gNB and mark as candidate */
    gnb_context_t* old_serving = ue_ctx->serving_gnb;
    if (old_serving != NULL) {
        old_serving->is_serving = false;
        old_serving->state = GNB_STATE_HANDOVER_CANDIDATE;
        
        /* Add to candidate list if space available */
        if (ue_ctx->num_candidate_gnbs < MAX_GNB_CANDIDATES) {
            ue_ctx->candidate_gnbs[ue_ctx->num_candidate_gnbs++] = old_serving;
        }
    }
    
    /* Switch serving gNB */
    new_gnb->is_serving = true;
    new_gnb->state = GNB_STATE_CONNECTED;
    ue_ctx->serving_gnb = new_gnb;
    
    /* Update legacy fields */
    ue_ctx->gnb_addr = new_gnb->addr;
    ue_ctx->gnb_ip = new_gnb->addr.sin_addr.s_addr;
    ue_ctx->gnb_port = ntohs(new_gnb->addr.sin_port);
    ue_ctx->ngap_socket = new_gnb->ngap_socket;
    ue_ctx->gtpu_socket = new_gnb->gtpu_socket;
    
    /* Remove from candidate list */
    for (int j = found_idx; j < ue_ctx->num_candidate_gnbs - 1; j++) {
        ue_ctx->candidate_gnbs[j] = ue_ctx->candidate_gnbs[j + 1];
    }
    ue_ctx->candidate_gnbs[ue_ctx->num_candidate_gnbs - 1] = NULL;
    ue_ctx->num_candidate_gnbs--;
    
    printf("UE %u switched serving gNB from %u to %u\n", 
           ue_ctx->ue_id, 
           old_serving ? old_serving->gnb_id : 0, 
           new_gnb->gnb_id);
    
#ifdef _WIN32
    ReleaseMutex(ue_ctx->gnb_list_mutex);
#else
    pthread_mutex_unlock(&ue_ctx->gnb_list_mutex);
#endif
    
    return UESIM_SUCCESS;
}

/* Connection retry configuration */
#define UESIM_CONNECT_MAX_RETRIES       3
#define UESIM_CONNECT_INITIAL_DELAY_MS  1000
#define UESIM_CONNECT_MAX_DELAY_MS      10000
#define UESIM_CONNECT_TIMEOUT_SEC       10

/* Helper: Set socket non-blocking mode */
static int set_socket_nonblocking(int sock, bool nonblock) {
#ifdef _WIN32
    u_long mode = nonblock ? 1 : 0;
    return ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) return -1;
    if (nonblock) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    return fcntl(sock, F_SETFL, flags);
#endif
}

/* Helper: Wait for socket with timeout */
static int wait_for_socket(int sock, int timeout_sec, bool for_write) {
    fd_set fds;
    struct timeval tv;
    
    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    
    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;
    
    if (for_write) {
        return select(sock + 1, NULL, &fds, NULL, &tv);
    } else {
        return select(sock + 1, &fds, NULL, NULL, &tv);
    }
}

/* Helper: Connect with timeout and retry */
static uesim_error_t connect_with_retry(gnb_context_t* gnb_ctx) {
    int retry_count = 0;
    int delay_ms = UESIM_CONNECT_INITIAL_DELAY_MS;
    uesim_error_t last_error = UESIM_ERROR_SOCKET;
    
    while (retry_count < UESIM_CONNECT_MAX_RETRIES) {
        /* Create NGAP socket */
        gnb_ctx->ngap_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (gnb_ctx->ngap_socket < 0) {
            fprintf(stderr, "DEBUG: Failed to create NGAP socket on attempt %d\n", retry_count + 1);
            retry_count++;
#ifdef _WIN32
            Sleep(delay_ms);
#else
            usleep(delay_ms * 1000);
#endif
            delay_ms = (delay_ms * 2 < UESIM_CONNECT_MAX_DELAY_MS) ? delay_ms * 2 : UESIM_CONNECT_MAX_DELAY_MS;
            continue;
        }
        
        /* Set non-blocking for timeout */
        if (set_socket_nonblocking(gnb_ctx->ngap_socket, true) != 0) {
            uesim_sock_close(gnb_ctx->ngap_socket);
            gnb_ctx->ngap_socket = -1;
            retry_count++;
            delay_ms = (delay_ms * 2 < UESIM_CONNECT_MAX_DELAY_MS) ? delay_ms * 2 : UESIM_CONNECT_MAX_DELAY_MS;
            continue;
        }
        
        /* Attempt connection */
        int connect_result = connect(gnb_ctx->ngap_socket, 
                                      (struct sockaddr*)&gnb_ctx->addr, 
                                      sizeof(gnb_ctx->addr));
        
        if (connect_result == 0) {
            /* Immediate success */
            set_socket_nonblocking(gnb_ctx->ngap_socket, false);
            return UESIM_SUCCESS;
        }
        
        /* Check if connection is in progress */
#ifdef _WIN32
        int err = WSAGetLastError();
        bool in_progress = (err == WSAEWOULDBLOCK || err == WSAEINPROGRESS);
#else
        bool in_progress = (errno == EINPROGRESS || errno == EWOULDBLOCK);
#endif
        
        if (in_progress) {
            /* Wait for connection with timeout */
            int sel_result = wait_for_socket(gnb_ctx->ngap_socket, UESIM_CONNECT_TIMEOUT_SEC, true);
            
            if (sel_result > 0) {
                /* Check if connection succeeded */
                int sock_error = 0;
                socklen_t optlen = sizeof(sock_error);
                if (getsockopt(gnb_ctx->ngap_socket, SOL_SOCKET, SO_ERROR, 
                               (char*)&sock_error, &optlen) == 0 && sock_error == 0) {
                    set_socket_nonblocking(gnb_ctx->ngap_socket, false);
                    return UESIM_SUCCESS;
                }
            }
        }
        
        /* Connection failed, cleanup and retry */
        fprintf(stderr, "DEBUG: Connection attempt %d failed to %s:%d\n", 
                retry_count + 1, inet_ntoa(gnb_ctx->addr.sin_addr), ntohs(gnb_ctx->addr.sin_port));
        
        uesim_sock_close(gnb_ctx->ngap_socket);
        gnb_ctx->ngap_socket = -1;
        last_error = UESIM_ERROR_TIMEOUT;
        
        retry_count++;
        if (retry_count < UESIM_CONNECT_MAX_RETRIES) {
#ifdef _WIN32
            Sleep(delay_ms);
#else
            usleep(delay_ms * 1000);
#endif
            delay_ms = (delay_ms * 2 < UESIM_CONNECT_MAX_DELAY_MS) ? delay_ms * 2 : UESIM_CONNECT_MAX_DELAY_MS;
        }
    }
    
    return last_error;
}

uesim_error_t uesim_connect_gnb(ue_context_t* ue_ctx, gnb_context_t* gnb_ctx) {
    if (ue_ctx == NULL || gnb_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("DEBUG: uesim_connect_gnb: gnb_id=%u, is_serving=%d, current_state=%d\n",
           gnb_ctx->gnb_id, gnb_ctx->is_serving, gnb_ctx->state);
    
#ifdef _WIN32
    WaitForSingleObject(gnb_ctx->gnb_mutex, INFINITE);
#else
    if (pthread_mutex_lock(&gnb_ctx->gnb_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
#endif
    
    if (gnb_ctx->state == GNB_STATE_CONNECTED) {
#ifdef _WIN32
        ReleaseMutex(gnb_ctx->gnb_mutex);
#else
        pthread_mutex_unlock(&gnb_ctx->gnb_mutex);
#endif
        return UESIM_SUCCESS;  /* Already connected */
    }
    
    gnb_ctx->state = GNB_STATE_CONNECTING;
    
    /* Connect with retry and timeout */
    uesim_error_t result = connect_with_retry(gnb_ctx);
    if (result != UESIM_SUCCESS) {
        gnb_ctx->state = GNB_STATE_DISCONNECTED;
#ifdef _WIN32
        ReleaseMutex(gnb_ctx->gnb_mutex);
#else
        pthread_mutex_unlock(&gnb_ctx->gnb_mutex);
#endif
        return result;
    }
    
    /* Create GTP-U socket */
    gnb_ctx->gtpu_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (gnb_ctx->gtpu_socket < 0) {
        uesim_sock_close(gnb_ctx->ngap_socket);
        gnb_ctx->ngap_socket = -1;
        gnb_ctx->state = GNB_STATE_DISCONNECTED;
#ifdef _WIN32
        ReleaseMutex(gnb_ctx->gnb_mutex);
#else
        pthread_mutex_unlock(&gnb_ctx->gnb_mutex);
#endif
        return UESIM_ERROR_SOCKET;
    }
    
    gnb_ctx->state = gnb_ctx->is_serving ? GNB_STATE_CONNECTED : GNB_STATE_HANDOVER_CANDIDATE;
    gnb_ctx->connect_time = time(NULL);
    gnb_ctx->last_activity = gnb_ctx->connect_time;
    
    /* Update legacy fields if this is serving gNB */
    if (gnb_ctx->is_serving) {
        ue_ctx->ngap_socket = gnb_ctx->ngap_socket;
        ue_ctx->gtpu_socket = gnb_ctx->gtpu_socket;
        printf("DEBUG: Updated UE ngap_socket=%d, gtpu_socket=%d\n", 
               ue_ctx->ngap_socket, ue_ctx->gtpu_socket);
    } else {
        printf("DEBUG: WARNING - gNB is not serving, UE ngap_socket NOT updated (still %d)\n", 
               ue_ctx->ngap_socket);
    }
    
    printf("Connected to gNB %u (%s:%d), socket=%d\n", 
           gnb_ctx->gnb_id, inet_ntoa(gnb_ctx->addr.sin_addr), ntohs(gnb_ctx->addr.sin_port),
           gnb_ctx->ngap_socket);
    
#ifdef _WIN32
    ReleaseMutex(gnb_ctx->gnb_mutex);
#else
    pthread_mutex_unlock(&gnb_ctx->gnb_mutex);
#endif
    
    return UESIM_SUCCESS;
}

uesim_error_t uesim_disconnect_gnb(ue_context_t* ue_ctx, gnb_context_t* gnb_ctx) {
    if (ue_ctx == NULL || gnb_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
#ifdef _WIN32
    WaitForSingleObject(gnb_ctx->gnb_mutex, INFINITE);
#else
    if (pthread_mutex_lock(&gnb_ctx->gnb_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
#endif
    
    if (gnb_ctx->ngap_socket >= 0) {
        uesim_sock_close(gnb_ctx->ngap_socket);
        gnb_ctx->ngap_socket = -1;
    }
    
    if (gnb_ctx->gtpu_socket >= 0) {
        uesim_sock_close(gnb_ctx->gtpu_socket);
        gnb_ctx->gtpu_socket = -1;
    }
    
    gnb_ctx->state = GNB_STATE_DISCONNECTED;
    
    /* Update legacy fields if this was serving gNB */
    if (gnb_ctx->is_serving) {
        ue_ctx->ngap_socket = -1;
        ue_ctx->gtpu_socket = -1;
    }
    
    printf("Disconnected from gNB %u\n", gnb_ctx->gnb_id);
    
#ifdef _WIN32
    ReleaseMutex(gnb_ctx->gnb_mutex);
#else
    pthread_mutex_unlock(&gnb_ctx->gnb_mutex);
#endif
    
    return UESIM_SUCCESS;
}

gnb_context_t* uesim_find_gnb_by_id(ue_context_t* ue_ctx, uint32_t gnb_id) {
    if (ue_ctx == NULL) {
        return NULL;
    }
    
    /* Check serving gNB */
    if (ue_ctx->serving_gnb != NULL && ue_ctx->serving_gnb->gnb_id == gnb_id) {
        return ue_ctx->serving_gnb;
    }
    
    /* Check candidate gNBs */
    for (int i = 0; i < ue_ctx->num_candidate_gnbs; i++) {
        if (ue_ctx->candidate_gnbs[i] != NULL && 
            ue_ctx->candidate_gnbs[i]->gnb_id == gnb_id) {
            return ue_ctx->candidate_gnbs[i];
        }
    }
    
    return NULL;
}

gnb_context_t* uesim_get_serving_gnb(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return NULL;
    }
    return ue_ctx->serving_gnb;
}

/* Reconnect to gNB with retry */
uesim_error_t uesim_reconnect_gnb(ue_context_t* ue_ctx, gnb_context_t* gnb_ctx) {
    if (ue_ctx == NULL || gnb_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* First disconnect if connected */
    if (gnb_ctx->state == GNB_STATE_CONNECTED) {
        uesim_disconnect_gnb(ue_ctx, gnb_ctx);
    }
    
    /* Then reconnect */
    return uesim_connect_gnb(ue_ctx, gnb_ctx);
}

/* Check gNB connection health */
bool uesim_is_gnb_connected(ue_context_t* ue_ctx, gnb_context_t* gnb_ctx) {
    if (ue_ctx == NULL || gnb_ctx == NULL) {
        return false;
    }
    
    bool connected = false;
    
#ifdef _WIN32
    WaitForSingleObject(gnb_ctx->gnb_mutex, INFINITE);
#else
    if (pthread_mutex_lock(&gnb_ctx->gnb_mutex) != 0) {
        return false;
    }
#endif
    
    connected = (gnb_ctx->state == GNB_STATE_CONNECTED && gnb_ctx->ngap_socket >= 0);
    
#ifdef _WIN32
    ReleaseMutex(gnb_ctx->gnb_mutex);
#else
    pthread_mutex_unlock(&gnb_ctx->gnb_mutex);
#endif
    
    return connected;
}

uint8_t uesim_get_candidate_gnb_count(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return 0;
    }
    return ue_ctx->num_candidate_gnbs;
}

/* ============== Procedure Implementations ============== */

static uesim_error_t execute_registration_procedure(ue_context_t* ue_ctx) {
    printf("Executing RRC registration procedure for UE %u\n", ue_ctx->ue_id);
    
    if (ue_ctx->serving_gnb == NULL) {
        printf("  No serving gNB configured\n");
        return UESIM_ERROR_NOT_INITIALIZED;
    }
    
    /* Connect to gNB if not connected */
    if (ue_ctx->serving_gnb->state != GNB_STATE_CONNECTED) {
        uesim_error_t result = uesim_connect_gnb(ue_ctx, ue_ctx->serving_gnb);
        if (result != UESIM_SUCCESS) {
            printf("  Failed to connect to serving gNB: %d\n", result);
            return result;
        }
    }
    
    return UESIM_SUCCESS;
}

static uesim_error_t execute_establishment_procedure(ue_context_t* ue_ctx) {
    printf("Executing RRC establishment procedure for UE %u\n", ue_ctx->ue_id);
    
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (ue_ctx->serving_gnb == NULL) {
        printf("  No serving gNB configured\n");
        return UESIM_ERROR_NOT_INITIALIZED;
    }
    
    /* Connect to gNB if not connected */
    if (ue_ctx->serving_gnb->state != GNB_STATE_CONNECTED) {
        uesim_error_t result = uesim_connect_gnb(ue_ctx, ue_ctx->serving_gnb);
        if (result != UESIM_SUCCESS) {
            printf("  Failed to connect to serving gNB: %d\n", result);
            return result;
        }
    }
    
    /* Update UE state to connecting */
    ue_ctx->current_state = RRC_STATE_CONNECTING;
    ue_ctx->state_change_time = time(NULL);
    
    /* Increment RRC procedure statistics */
    ue_ctx->stats.rrc_procedures_success++;
    
    printf("  RRC establishment procedure completed successfully\n");
    return UESIM_SUCCESS;
}

static uesim_error_t execute_reestablishment_procedure(ue_context_t* ue_ctx) {
    printf("Executing RRC re-establishment procedure for UE %u\n", ue_ctx->ue_id);
    
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Re-establishment requires previous RRC connection */
    if (ue_ctx->current_state != RRC_STATE_CONNECTED) {
        printf("  UE not in RRC_CONNECTED state, cannot re-establish\n");
        return UESIM_ERROR_NOT_INITIALIZED;
    }
    
    if (ue_ctx->serving_gnb == NULL) {
        printf("  No serving gNB configured\n");
        return UESIM_ERROR_NOT_INITIALIZED;
    }
    
    /* Check if we need to reconnect */
    if (ue_ctx->serving_gnb->state != GNB_STATE_CONNECTED) {
        printf("  Serving gNB disconnected, attempting reconnect...\n");
        uesim_error_t result = uesim_connect_gnb(ue_ctx, ue_ctx->serving_gnb);
        if (result != UESIM_SUCCESS) {
            printf("  Failed to reconnect to serving gNB: %d\n", result);
            ue_ctx->stats.rrc_procedures_failed++;
            return result;
        }
    }
    
    /* Update state */
    ue_ctx->state_change_time = time(NULL);
    ue_ctx->stats.rrc_procedures_success++;
    
    printf("  RRC re-establishment procedure completed successfully\n");
    return UESIM_SUCCESS;
}

static uesim_error_t execute_handover_procedure(ue_context_t* ue_ctx) {
    printf("Executing RRC handover procedure for UE %u\n", ue_ctx->ue_id);
    
    if (ue_ctx->num_candidate_gnbs == 0) {
        printf("  No candidate gNBs available for handover\n");
        return UESIM_ERROR_NOT_FOUND;
    }
    
    /* Select best candidate based on RSRP */
    gnb_context_t* best_candidate = NULL;
    int32_t best_rsrp = -140;
    
    for (int i = 0; i < ue_ctx->num_candidate_gnbs; i++) {
        gnb_context_t* candidate = ue_ctx->candidate_gnbs[i];
        if (candidate != NULL && candidate->rsrp > best_rsrp) {
            best_rsrp = candidate->rsrp;
            best_candidate = candidate;
        }
    }
    
    if (best_candidate == NULL) {
        printf("  No suitable candidate found\n");
        return UESIM_ERROR_NOT_FOUND;
    }
    
    printf("  Selected candidate gNB %u (RSRP: %d dBm)\n", best_candidate->gnb_id, best_rsrp);
    
    /* Connect to new gNB */
    uesim_error_t result = uesim_connect_gnb(ue_ctx, best_candidate);
    if (result != UESIM_SUCCESS) {
        printf("  Failed to connect to target gNB: %d\n", result);
        return result;
    }
    
    /* Switch serving gNB */
    return uesim_switch_serving_gnb(ue_ctx, best_candidate);
}

/* ============== Layer Context Accessor Functions ============== */

/* NAS context access */
struct nas_ue_context_t* ue_get_nas_context(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) return NULL;
    return ue_ctx->nas_ctx;
}

uesim_error_t ue_set_nas_context(ue_context_t* ue_ctx, struct nas_ue_context_t* nas_ctx) {
    if (ue_ctx == NULL) return UESIM_ERROR_INVALID_PARAM;
    ue_ctx->nas_ctx = nas_ctx;
    return UESIM_SUCCESS;
}

/* RRC state context access */
struct rrc_state_context_t* ue_get_rrc_state_context(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) return NULL;
    return ue_ctx->rrc_state_ctx;
}

uesim_error_t ue_set_rrc_state_context(ue_context_t* ue_ctx, struct rrc_state_context_t* rrc_ctx) {
    if (ue_ctx == NULL) return UESIM_ERROR_INVALID_PARAM;
    ue_ctx->rrc_state_ctx = rrc_ctx;
    return UESIM_SUCCESS;
}

/* RRC measurement context access */
struct rrc_meas_context_t* ue_get_rrc_meas_context(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) return NULL;
    return ue_ctx->rrc_meas_ctx;
}

uesim_error_t ue_set_rrc_meas_context(ue_context_t* ue_ctx, struct rrc_meas_context_t* meas_ctx) {
    if (ue_ctx == NULL) return UESIM_ERROR_INVALID_PARAM;
    ue_ctx->rrc_meas_ctx = meas_ctx;
    return UESIM_SUCCESS;
}

/* PHY context access */
struct phy_context_t* ue_get_phy_context(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) return NULL;
    return ue_ctx->phy_ctx;
}

uesim_error_t ue_set_phy_context(ue_context_t* ue_ctx, struct phy_context_t* phy_ctx) {
    if (ue_ctx == NULL) return UESIM_ERROR_INVALID_PARAM;
    ue_ctx->phy_ctx = phy_ctx;
    return UESIM_SUCCESS;
}

/* MAC entity access */
struct mac_entity_t* ue_get_mac_entity(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) return NULL;
    return ue_ctx->mac_entity;
}

uesim_error_t ue_set_mac_entity(ue_context_t* ue_ctx, struct mac_entity_t* mac_entity) {
    if (ue_ctx == NULL) return UESIM_ERROR_INVALID_PARAM;
    ue_ctx->mac_entity = mac_entity;
    return UESIM_SUCCESS;
}

/* RLC entity access by bearer ID */
struct rlc_entity_t* ue_get_rlc_entity(ue_context_t* ue_ctx, uint8_t bearer_id) {
    if (ue_ctx == NULL || bearer_id >= UESIM_MAX_RLC_ENTITIES) return NULL;
    return ue_ctx->rlc_entities[bearer_id];
}

uesim_error_t ue_set_rlc_entity(ue_context_t* ue_ctx, uint8_t bearer_id, struct rlc_entity_t* rlc_entity) {
    if (ue_ctx == NULL || bearer_id >= UESIM_MAX_RLC_ENTITIES) return UESIM_ERROR_INVALID_PARAM;
    
    /* If setting a new entity and slot was empty, increment counter */
    if (rlc_entity != NULL && ue_ctx->rlc_entities[bearer_id] == NULL) {
        ue_ctx->num_active_rlc_entities++;
    }
    /* If clearing an entity, decrement counter */
    else if (rlc_entity == NULL && ue_ctx->rlc_entities[bearer_id] != NULL) {
        ue_ctx->num_active_rlc_entities--;
    }
    
    ue_ctx->rlc_entities[bearer_id] = rlc_entity;
    return UESIM_SUCCESS;
}

uesim_error_t ue_remove_rlc_entity(ue_context_t* ue_ctx, uint8_t bearer_id) {
    return ue_set_rlc_entity(ue_ctx, bearer_id, NULL);
}

/* PDCP entity access by bearer ID */
struct pdcp_entity_t* ue_get_pdcp_entity(ue_context_t* ue_ctx, uint8_t bearer_id) {
    if (ue_ctx == NULL || bearer_id >= UESIM_MAX_PDCP_ENTITIES) return NULL;
    return ue_ctx->pdcp_entities[bearer_id];
}

uesim_error_t ue_set_pdcp_entity(ue_context_t* ue_ctx, uint8_t bearer_id, struct pdcp_entity_t* pdcp_entity) {
    if (ue_ctx == NULL || bearer_id >= UESIM_MAX_PDCP_ENTITIES) return UESIM_ERROR_INVALID_PARAM;
    
    /* If setting a new entity and slot was empty, increment counter */
    if (pdcp_entity != NULL && ue_ctx->pdcp_entities[bearer_id] == NULL) {
        ue_ctx->num_active_pdcp_entities++;
    }
    /* If clearing an entity, decrement counter */
    else if (pdcp_entity == NULL && ue_ctx->pdcp_entities[bearer_id] != NULL) {
        ue_ctx->num_active_pdcp_entities--;
    }
    
    ue_ctx->pdcp_entities[bearer_id] = pdcp_entity;
    return UESIM_SUCCESS;
}

uesim_error_t ue_remove_pdcp_entity(ue_context_t* ue_ctx, uint8_t bearer_id) {
    return ue_set_pdcp_entity(ue_ctx, bearer_id, NULL);
}

/* Radio Bearer configuration */
uesim_error_t ue_configure_srb(ue_context_t* ue_ctx, uint8_t srb_id, uint8_t lcid, uint8_t priority) {
    if (ue_ctx == NULL || srb_id >= UESIM_MAX_SRB) return UESIM_ERROR_INVALID_PARAM;
    
    ue_bearer_config_t* config = &ue_ctx->srb_config[srb_id];
    
    /* If activating a previously inactive SRB, increment counter */
    if (!config->active && ue_ctx->num_active_srbs < UESIM_MAX_SRB) {
        ue_ctx->num_active_srbs++;
    }
    
    config->active = true;
    config->bearer_id = srb_id;
    config->lcid = lcid;
    config->priority = priority;
    config->prioritized_bit_rate = 8;  /* Default 8 kbps for signaling */
    config->bucket_size_duration = 10; /* Default 10 ms */
    
    return UESIM_SUCCESS;
}

uesim_error_t ue_configure_drb(ue_context_t* ue_ctx, uint8_t drb_id, uint8_t lcid, uint8_t priority,
                               uint16_t pbr, uint16_t bsd) {
    if (ue_ctx == NULL || drb_id >= UESIM_MAX_DRB) return UESIM_ERROR_INVALID_PARAM;
    
    ue_bearer_config_t* config = &ue_ctx->drb_config[drb_id];
    
    /* If activating a previously inactive DRB, increment counter */
    if (!config->active && ue_ctx->num_active_drbs < UESIM_MAX_DRB) {
        ue_ctx->num_active_drbs++;
    }
    
    config->active = true;
    config->bearer_id = drb_id;
    config->lcid = lcid;
    config->priority = priority;
    config->prioritized_bit_rate = pbr;
    config->bucket_size_duration = bsd;
    
    return UESIM_SUCCESS;
}

uesim_error_t ue_remove_bearer(ue_context_t* ue_ctx, uint8_t bearer_id) {
    if (ue_ctx == NULL) return UESIM_ERROR_INVALID_PARAM;
    
    /* Check if it's an SRB (0-3) */
    if (bearer_id < UESIM_MAX_SRB) {
        if (ue_ctx->srb_config[bearer_id].active) {
            ue_ctx->srb_config[bearer_id].active = false;
            if (ue_ctx->num_active_srbs > 0) {
                ue_ctx->num_active_srbs--;
            }
        }
        return UESIM_SUCCESS;
    }
    
    /* Check if it's a DRB (4-35 mapped to 0-31) */
    uint8_t drb_idx = bearer_id - UESIM_MAX_SRB;
    if (drb_idx < UESIM_MAX_DRB) {
        if (ue_ctx->drb_config[drb_idx].active) {
            ue_ctx->drb_config[drb_idx].active = false;
            if (ue_ctx->num_active_drbs > 0) {
                ue_ctx->num_active_drbs--;
            }
        }
        return UESIM_SUCCESS;
    }
    
    return UESIM_ERROR_NOT_FOUND;
}

ue_bearer_config_t* ue_get_bearer_config(ue_context_t* ue_ctx, uint8_t bearer_id) {
    if (ue_ctx == NULL) return NULL;
    
    /* Check if it's an SRB (0-3) */
    if (bearer_id < UESIM_MAX_SRB) {
        return &ue_ctx->srb_config[bearer_id];
    }
    
    /* Check if it's a DRB (4-35 mapped to 0-31) */
    uint8_t drb_idx = bearer_id - UESIM_MAX_SRB;
    if (drb_idx < UESIM_MAX_DRB) {
        return &ue_ctx->drb_config[drb_idx];
    }
    
    return NULL;
}

/* DRX configuration */
uesim_error_t ue_configure_drx(ue_context_t* ue_ctx, const ue_drx_config_t* drx_config) {
    if (ue_ctx == NULL || drx_config == NULL) return UESIM_ERROR_INVALID_PARAM;
    
    memcpy(&ue_ctx->drx_config, drx_config, sizeof(ue_drx_config_t));
    return UESIM_SUCCESS;
}

uesim_error_t ue_disable_drx(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) return UESIM_ERROR_INVALID_PARAM;
    
    ue_ctx->drx_config.enabled = false;
    return UESIM_SUCCESS;
}

/* UE Capabilities */
uesim_error_t ue_set_capabilities(ue_context_t* ue_ctx, const ue_capabilities_t* caps) {
    if (ue_ctx == NULL || caps == NULL) return UESIM_ERROR_INVALID_PARAM;
    
    memcpy(&ue_ctx->capabilities, caps, sizeof(ue_capabilities_t));
    return UESIM_SUCCESS;
}

const ue_capabilities_t* ue_get_capabilities(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) return NULL;
    return &ue_ctx->capabilities;
}

/* Statistics */
uesim_error_t ue_update_stats(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) return UESIM_ERROR_INVALID_PARAM;
    
    ue_ctx->stats.last_activity_time = time(NULL);
    ue_ctx->stats.connection_duration_s = (uint32_t)(ue_ctx->stats.last_activity_time - ue_ctx->stats.connection_start_time);
    
    return UESIM_SUCCESS;
}

uesim_error_t ue_reset_stats(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) return UESIM_ERROR_INVALID_PARAM;
    
    memset(&ue_ctx->stats, 0, sizeof(ue_stats_t));
    ue_ctx->stats.connection_start_time = time(NULL);
    
    return UESIM_SUCCESS;
}

const ue_stats_t* ue_get_stats(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) return NULL;
    return &ue_ctx->stats;
}
