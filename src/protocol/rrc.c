/*
 * 5G UE Simulation Application
 * RRC (Radio Resource Control) protocol implementation
 */

#include "rrc.h"
#include "rrc_si.h"
#include "rrc_meas.h"
#include "asn1_per.h"
#include "../transport/socket_mgr.h"
#include "../core/memory.h"
#include "../utils/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* RRC transaction ID counter */
static atomic_uint g_transaction_id_counter = 0;

/* Valid state transitions table */
static const bool g_valid_state_transitions[RRC_STATE_MAX][RRC_STATE_MAX] = {
    /* IDLE -> */           { false, true,  false, true  },
    /* CONNECTED -> */      { true,  false, true,  false },
    /* INACTIVE -> */       { true,  true,  false, true  },
    /* CONNECTING -> */     { true,  true,  false, false }
};

/* Forward declarations */
static uesim_error_t rrc_send_setup_complete(ue_context_t* ue_ctx, rrc_procedure_context_t* proc_ctx, rrc_message_t* setup_msg);
static uesim_error_t rrc_send_reest_complete(ue_context_t* ue_ctx, rrc_procedure_context_t* proc_ctx, rrc_message_t* reest_msg);
static uesim_error_t rrc_send_reconfig_complete(ue_context_t* ue_ctx, rrc_procedure_context_t* proc_ctx, rrc_message_t* reconfig_msg);
static uesim_error_t rrc_send_handover_confirm(ue_context_t* ue_ctx, rrc_procedure_context_t* proc_ctx, rrc_message_t* ho_cmd);
static uesim_error_t rrc_send_capability_info(ue_context_t* ue_ctx, rrc_message_t* enquiry);
static uint32_t rrc_get_procedure_timeout(rrc_procedure_t procedure);
static rrc_message_type_t rrc_get_expected_response(rrc_procedure_t procedure);

/* ============== Initialization & Cleanup ============== */

uesim_error_t rrc_init(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Initialize RRC state */
    ue_ctx->current_state = RRC_STATE_IDLE;
    
    /* Initialize RRC state context */
    rrc_state_context_t* state_ctx = (rrc_state_context_t*)uesim_calloc(1, sizeof(rrc_state_context_t));
    if (state_ctx != NULL) {
        state_ctx->current_state = RRC_STATE_IDLE;
        state_ctx->previous_state = RRC_STATE_IDLE;
        ue_set_rrc_state_context(ue_ctx, state_ctx);
    }
    
    /* Initialize RRC measurement context */
    uesim_error_t result = rrc_meas_init(ue_ctx);
    if (result != UESIM_SUCCESS) {
        LOG_WARN(LOG_CAT_NAME_RRC, "Failed to initialize measurement context for UE %u, error=%d", ue_ctx->ue_id, result);
        /* Continue without measurement context - not critical */
    }
    
    /* Initialize RRC SI context */
    rrc_si_context_t* si_ctx = (rrc_si_context_t*)uesim_calloc(1, sizeof(rrc_si_context_t));
    if (si_ctx != NULL) {
        rrc_si_init(si_ctx);
        ue_ctx->rrc_si_ctx = si_ctx;
    }
    
    LOG_INFO(LOG_CAT_NAME_RRC, "Initialized for UE %u (state=IDLE)", ue_ctx->ue_id);
    return UESIM_SUCCESS;
}

void rrc_cleanup(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return;
    }
    
    /* Cleanup RRC SI context */
    if (ue_ctx->rrc_si_ctx != NULL) {
        rrc_si_cleanup(ue_ctx->rrc_si_ctx);
        uesim_free(ue_ctx->rrc_si_ctx);
        ue_ctx->rrc_si_ctx = NULL;
    }
    
    /* Cleanup RRC measurement context */
    rrc_meas_cleanup(ue_ctx);
    
    /* Cleanup RRC state context */
    rrc_state_context_t* state_ctx = ue_get_rrc_state_context(ue_ctx);
    if (state_ctx != NULL) {
        uesim_free(state_ctx);
        ue_set_rrc_state_context(ue_ctx, NULL);
    }
    
    /* Cleanup any active procedures */
    if (ue_ctx->state_mutex) {
        /* Abort any active procedure */
        /* Note: In a full implementation, we'd track active procedures */
    }
    
    LOG_INFO(LOG_CAT_NAME_RRC, "Cleanup completed for UE %u", ue_ctx->ue_id);
}

/* ============== State Management ============== */

uesim_error_t rrc_change_state(ue_context_t* ue_ctx, rrc_state_t new_state) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (new_state >= RRC_STATE_MAX) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Lock state mutex */
    if (pthread_mutex_lock(&ue_ctx->state_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    rrc_state_t previous_state = ue_ctx->current_state;
    
    /* Validate state transition */
    if (!rrc_is_valid_state_transition(previous_state, new_state)) {
        pthread_mutex_unlock(&ue_ctx->state_mutex);
        LOG_ERROR(LOG_CAT_NAME_RRC, "Invalid state transition: %s -> %s",
                rrc_state_to_string(previous_state), rrc_state_to_string(new_state));
        return UESIM_ERROR_PROTOCOL;
    }
    
    /* Change state */
    ue_ctx->current_state = new_state;
    ue_ctx->state_change_time = time(NULL);
    
    /* Signal state change */
    pthread_cond_broadcast(&ue_ctx->state_cond);
    
    LOG_INFO(LOG_CAT_NAME_RRC, "UE %u state changed: %s -> %s", 
           ue_ctx->ue_id, rrc_state_to_string(previous_state), rrc_state_to_string(new_state));
    
    pthread_mutex_unlock(&ue_ctx->state_mutex);
    
    return UESIM_SUCCESS;
}

rrc_state_t rrc_get_current_state(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return RRC_STATE_MAX;
    }
    
    rrc_state_t current_state;
    
    if (pthread_mutex_lock(&ue_ctx->state_mutex) != 0) {
        return RRC_STATE_MAX;
    }
    
    current_state = ue_ctx->current_state;
    pthread_mutex_unlock(&ue_ctx->state_mutex);
    
    return current_state;
}

bool rrc_is_valid_state_transition(rrc_state_t from, rrc_state_t to) {
    if (from >= RRC_STATE_MAX || to >= RRC_STATE_MAX) {
        return false;
    }
    return g_valid_state_transitions[from][to];
}

/* ============== Message Handling ============== */

uesim_error_t rrc_send_message(ue_context_t* ue_ctx, rrc_message_t* message) {
    if (ue_ctx == NULL || message == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    uesim_error_t result = UESIM_SUCCESS;
    void* encoded_data = NULL;
    size_t encoded_length = 0;
    
    /* Encode RRC message */
    result = rrc_encode_message(message, &encoded_data, &encoded_length);
    if (result != UESIM_SUCCESS) {
        fprintf(stderr, "Failed to encode RRC message: %d\n", result);
        return result;
    }
    
    /* Send via NGAP socket */
    result = send_ngap_message(ue_ctx, encoded_data, encoded_length);
    if (result != UESIM_SUCCESS) {
        fprintf(stderr, "Failed to send RRC message: %d\n", result);
        uesim_free(encoded_data);
        return result;
    }
    
    printf("RRC message sent: type=%s, id=%u, length=%zu\n",
           rrc_message_type_to_string(message->message_type), 
           message->message_id, encoded_length);
    
    uesim_free(encoded_data);
    return UESIM_SUCCESS;
}

uesim_error_t rrc_receive_message(ue_context_t* ue_ctx, rrc_message_t* message) {
    if (ue_ctx == NULL || message == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* This is called when a message is received from the socket layer */
    /* Process the message based on type */
    return rrc_handle_procedure_response(ue_ctx, message);
}

uesim_error_t rrc_process_incoming_message(ue_context_t* ue_ctx, const void* data, size_t len) {
    if (ue_ctx == NULL || data == NULL || len == 0) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    rrc_message_t message = {0};
    uesim_error_t result = rrc_decode_message(data, len, &message);
    if (result != UESIM_SUCCESS) {
        fprintf(stderr, "Failed to decode incoming RRC message: %d\n", result);
        return result;
    }
    
    printf("RRC message received: type=%s, id=%u\n",
           rrc_message_type_to_string(message.message_type), message.message_id);
    
    result = rrc_receive_message(ue_ctx, &message);
    
    /* Free allocated data */
    if (message.data != NULL) {
        uesim_free(message.data);
    }
    
    return result;
}

/* ============== Procedure Management ============== */

uesim_error_t rrc_start_procedure(ue_context_t* ue_ctx, rrc_procedure_t procedure, rrc_procedure_context_t** ctx) {
    if (ue_ctx == NULL || ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Allocate procedure context */
    rrc_procedure_context_t* proc_ctx = (rrc_procedure_context_t*)uesim_calloc(1, sizeof(rrc_procedure_context_t));
    if (proc_ctx == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    /* Initialize procedure context */
    proc_ctx->procedure_type = procedure;
    proc_ctx->transaction_id = atomic_fetch_add(&g_transaction_id_counter, 1);
    proc_ctx->start_time = time(NULL);
    proc_ctx->retry_count = 0;
    proc_ctx->timeout_ms = rrc_get_procedure_timeout(procedure);
    proc_ctx->status = RRC_PROC_STATUS_ONGOING;
    proc_ctx->error_cause = RRC_CAUSE_SUCCESS;
    atomic_store(&proc_ctx->completed, 0);
    atomic_store(&proc_ctx->success, 0);
    
    /* Initialize mutex and condition variable */
#ifdef _WIN32
    proc_ctx->proc_mutex = CreateMutex(NULL, FALSE, NULL);
    proc_ctx->proc_cond = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (proc_ctx->proc_mutex == NULL || proc_ctx->proc_cond == NULL) {
        if (proc_ctx->proc_mutex) CloseHandle(proc_ctx->proc_mutex);
        if (proc_ctx->proc_cond) CloseHandle(proc_ctx->proc_cond);
        uesim_free(proc_ctx);
        return UESIM_ERROR_THREAD;
    }
#else
    if (pthread_mutex_init(&proc_ctx->proc_mutex, NULL) != 0 ||
        pthread_cond_init(&proc_ctx->proc_cond, NULL) != 0) {
        pthread_mutex_destroy(&proc_ctx->proc_mutex);
        pthread_cond_destroy(&proc_ctx->proc_cond);
        uesim_free(proc_ctx);
        return UESIM_ERROR_THREAD;
    }
#endif
    
    *ctx = proc_ctx;
    return UESIM_SUCCESS;
}

uesim_error_t rrc_wait_procedure_complete(ue_context_t* ue_ctx, rrc_procedure_context_t* ctx, uint32_t timeout_ms) {
    if (ue_ctx == NULL || ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    uesim_error_t result = UESIM_SUCCESS;
    
#ifdef _WIN32
    /* Windows: use WaitForSingleObject with timeout */
    DWORD wait_ms = (timeout_ms > 0) ? timeout_ms : ctx->timeout_ms;
    DWORD wait_result = WaitForSingleObject(ctx->proc_cond, wait_ms);
    if (wait_result == WAIT_TIMEOUT) {
        ctx->status = RRC_PROC_STATUS_TIMEOUT;
        ctx->error_cause = RRC_CAUSE_TIMEOUT;
        result = UESIM_ERROR_TIMEOUT;
    } else if (wait_result != WAIT_OBJECT_0) {
        result = UESIM_ERROR_THREAD;
    }
#else
    /* POSIX: use pthread_cond_timedwait */
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    uint32_t wait_ms = (timeout_ms > 0) ? timeout_ms : ctx->timeout_ms;
    timeout.tv_sec += wait_ms / 1000;
    timeout.tv_nsec += (wait_ms % 1000) * 1000000;
    if (timeout.tv_nsec >= 1000000000) {
        timeout.tv_sec++;
        timeout.tv_nsec -= 1000000000;
    }
    
    if (pthread_mutex_lock(&ctx->proc_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    while (!atomic_load(&ctx->completed)) {
        int ret = pthread_cond_timedwait(&ctx->proc_cond, &ctx->proc_mutex, &timeout);
        if (ret == ETIMEDOUT) {
            ctx->status = RRC_PROC_STATUS_TIMEOUT;
            ctx->error_cause = RRC_CAUSE_TIMEOUT;
            result = UESIM_ERROR_TIMEOUT;
            break;
        } else if (ret != 0) {
            result = UESIM_ERROR_THREAD;
            break;
        }
    }
    
    pthread_mutex_unlock(&ctx->proc_mutex);
#endif
    
    return result;
}

uesim_error_t rrc_abort_procedure(ue_context_t* ue_ctx, rrc_procedure_context_t* ctx) {
    if (ue_ctx == NULL || ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    ctx->status = RRC_PROC_STATUS_FAILED;
    ctx->error_cause = RRC_CAUSE_NETWORK_FAILURE;
    atomic_store(&ctx->completed, 1);
    
    /* Signal completion */
#ifdef _WIN32
    SetEvent(ctx->proc_cond);
#else
    pthread_cond_signal(&ctx->proc_cond);
#endif
    
    return UESIM_SUCCESS;
}

void rrc_free_procedure_context(rrc_procedure_context_t* ctx) {
    if (ctx == NULL) {
        return;
    }
    
#ifdef _WIN32
    if (ctx->proc_mutex) CloseHandle(ctx->proc_mutex);
    if (ctx->proc_cond) CloseHandle(ctx->proc_cond);
#else
    pthread_mutex_destroy(&ctx->proc_mutex);
    pthread_cond_destroy(&ctx->proc_cond);
#endif
    
    if (ctx->procedure_data != NULL) {
        uesim_free(ctx->procedure_data);
    }
    
    uesim_free(ctx);
}

/* ============== Procedure Execution ============== */

uesim_error_t rrc_execute_procedure(ue_context_t* ue_ctx, rrc_procedure_t procedure) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    switch (procedure) {
        case RRC_PROC_REGISTRATION:
            return rrc_execute_registration(ue_ctx);
        case RRC_PROC_ESTABLISHMENT:
            return rrc_execute_establishment(ue_ctx);
        case RRC_PROC_REESTABLISHMENT:
            return rrc_execute_reestablishment(ue_ctx);
        case RRC_PROC_HANDOVER:
            return rrc_execute_handover(ue_ctx);
        default:
            return UESIM_ERROR_INVALID_PARAM;
    }
}

uesim_error_t rrc_handle_procedure_response(ue_context_t* ue_ctx, rrc_message_t* response) {
    if (ue_ctx == NULL || response == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("Handling RRC procedure response: type=%s\n", 
           rrc_message_type_to_string(response->message_type));
    
    /* Route to appropriate handler based on message type */
    switch (response->message_type) {
        case RRC_MESSAGE_TYPE_SETUP:
            return rrc_handle_setup_response(ue_ctx, response);
            
        case RRC_MESSAGE_TYPE_REESTABLISHMENT:
            return rrc_handle_reestablishment_response(ue_ctx, response);
            
        case RRC_MESSAGE_TYPE_RECONFIGURATION:
            return rrc_handle_reconfiguration_response(ue_ctx, response);
            
        case RRC_MESSAGE_TYPE_HANDOVER_COMMAND:
            return rrc_handle_handover_command(ue_ctx, response);
            
        case RRC_MESSAGE_TYPE_UE_CAPABILITY_ENQUIRY:
            return rrc_handle_capability_enquiry(ue_ctx, response);
            
        case RRC_MESSAGE_TYPE_CONNECTION_RELEASE:
            return rrc_handle_connection_release(ue_ctx, response);
            
        default:
            fprintf(stderr, "Unhandled RRC message type: %d\n", response->message_type);
            return UESIM_ERROR_PROTOCOL;
    }
}

/* ============== Specific Procedure Implementations ============== */

uesim_error_t rrc_execute_registration(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("Executing RRC registration for UE %u\n", ue_ctx->ue_id);
    
    /* Check current state */
    rrc_state_t current_state = rrc_get_current_state(ue_ctx);
    if (current_state != RRC_STATE_IDLE) {
        fprintf(stderr, "Cannot register UE %u: not in IDLE state (current: %s)\n", 
                ue_ctx->ue_id, rrc_state_to_string(current_state));
        return UESIM_ERROR_PROTOCOL;
    }
    
    /* Start procedure */
    rrc_procedure_context_t* proc_ctx = NULL;
    uesim_error_t result = rrc_start_procedure(ue_ctx, RRC_PROC_REGISTRATION, &proc_ctx);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    /* Transition to CONNECTING */
    result = rrc_change_state(ue_ctx, RRC_STATE_CONNECTING);
    if (result != UESIM_SUCCESS) {
        rrc_free_procedure_context(proc_ctx);
        return result;
    }
    
    /* Create RRC Setup Request message */
    rrc_message_t setup_request = {0};
    setup_request.message_type = RRC_MESSAGE_TYPE_SETUP_REQUEST;
    setup_request.message_id = proc_ctx->transaction_id;
    setup_request.transaction_id = proc_ctx->transaction_id;
    
    /* Create setup request data */
    rrc_setup_request_data_t* req_data = (rrc_setup_request_data_t*)uesim_calloc(1, sizeof(rrc_setup_request_data_t));
    if (req_data == NULL) {
        rrc_free_procedure_context(proc_ctx);
        return UESIM_ERROR_MEMORY;
    }
    
    /* Fill with random UE identity and establishment cause */
    req_data->ue_identity = ((uint64_t)rand() << 32) | rand();
    req_data->establishment_cause = 2; /* mo-Signalling */
    setup_request.data = req_data;
    setup_request.data_length = sizeof(rrc_setup_request_data_t);
    proc_ctx->procedure_data = req_data;
    
    /* Send setup request with retry logic */
    result = UESIM_ERROR_PROTOCOL;
    for (uint32_t retry = 0; retry <= RRC_N300_MAX; retry++) {
        proc_ctx->retry_count = retry;
        
        result = rrc_send_message(ue_ctx, &setup_request);
        if (result != UESIM_SUCCESS) {
            fprintf(stderr, "Failed to send RRC Setup Request (attempt %u/%u): %d\n", 
                    retry + 1, RRC_N300_MAX + 1, result);
            if (retry < RRC_N300_MAX) {
                /* Wait before retry */
#ifdef _WIN32
                Sleep(100);
#else
                usleep(100000);
#endif
                continue;
            }
            break;
        }
        
        /* Wait for response */
        result = rrc_wait_procedure_complete(ue_ctx, proc_ctx, 0);
        if (result == UESIM_SUCCESS && atomic_load(&proc_ctx->success)) {
            break;
        } else if (result == UESIM_ERROR_TIMEOUT) {
            fprintf(stderr, "RRC Setup Request timed out (attempt %u/%u)\n", 
                    retry + 1, RRC_N300_MAX + 1);
            if (retry < RRC_N300_MAX) {
                continue;
            }
        }
    }
    
    /* Handle failure */
    if (result != UESIM_SUCCESS || !atomic_load(&proc_ctx->success)) {
        rrc_handle_procedure_failure(ue_ctx, proc_ctx, 
            result == UESIM_ERROR_TIMEOUT ? RRC_CAUSE_TIMEOUT : RRC_CAUSE_MAX_RETRIES);
        rrc_free_procedure_context(proc_ctx);
        return result;
    }
    
    rrc_free_procedure_context(proc_ctx);
    return UESIM_SUCCESS;
}

uesim_error_t rrc_execute_establishment(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("Executing RRC establishment for UE %u\n", ue_ctx->ue_id);
    
    /* Establishment is essentially the same as registration */
    return rrc_execute_registration(ue_ctx);
}

uesim_error_t rrc_execute_reestablishment(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("Executing RRC re-establishment for UE %u\n", ue_ctx->ue_id);
    
    /* Check current state - must not be IDLE */
    rrc_state_t current_state = rrc_get_current_state(ue_ctx);
    if (current_state == RRC_STATE_IDLE) {
        fprintf(stderr, "Cannot re-establish RRC for UE %u: in IDLE state\n", ue_ctx->ue_id);
        return UESIM_ERROR_PROTOCOL;
    }
    
    /* Start procedure */
    rrc_procedure_context_t* proc_ctx = NULL;
    uesim_error_t result = rrc_start_procedure(ue_ctx, RRC_PROC_REESTABLISHMENT, &proc_ctx);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    /* Transition to CONNECTING */
    result = rrc_change_state(ue_ctx, RRC_STATE_CONNECTING);
    if (result != UESIM_SUCCESS) {
        rrc_free_procedure_context(proc_ctx);
        return result;
    }
    
    /* Create RRC Reestablishment Request message */
    rrc_message_t reest_request = {0};
    reest_request.message_type = RRC_MESSAGE_TYPE_REESTABLISHMENT_REQUEST;
    reest_request.message_id = proc_ctx->transaction_id;
    reest_request.transaction_id = proc_ctx->transaction_id;
    
    /* Create reestablishment request data */
    rrc_reest_request_data_t* req_data = (rrc_reest_request_data_t*)uesim_calloc(1, sizeof(rrc_reest_request_data_t));
    if (req_data == NULL) {
        rrc_free_procedure_context(proc_ctx);
        return UESIM_ERROR_MEMORY;
    }
    
    /* Fill with reestablishment parameters */
    req_data->reestablishment_cause = 0; /* reconfiguration failure */
    req_data->pci = ue_ctx->serving_gnb ? ue_ctx->serving_gnb->cell_id : 0;
    req_data->c_rnti = rand() & 0xFFFF;
    reest_request.data = req_data;
    reest_request.data_length = sizeof(rrc_reest_request_data_t);
    proc_ctx->procedure_data = req_data;
    
    /* Send reestablishment request with retry logic */
    result = UESIM_ERROR_PROTOCOL;
    for (uint32_t retry = 0; retry <= RRC_N311_MAX; retry++) {
        proc_ctx->retry_count = retry;
        
        result = rrc_send_message(ue_ctx, &reest_request);
        if (result != UESIM_SUCCESS) {
            fprintf(stderr, "Failed to send RRC Reestablishment Request (attempt %u/%u): %d\n", 
                    retry + 1, RRC_N311_MAX + 1, result);
            if (retry < RRC_N311_MAX) {
#ifdef _WIN32
                Sleep(100);
#else
                usleep(100000);
#endif
                continue;
            }
            break;
        }
        
        /* Wait for response */
        result = rrc_wait_procedure_complete(ue_ctx, proc_ctx, 0);
        if (result == UESIM_SUCCESS && atomic_load(&proc_ctx->success)) {
            break;
        } else if (result == UESIM_ERROR_TIMEOUT) {
            fprintf(stderr, "RRC Reestablishment Request timed out (attempt %u/%u)\n", 
                    retry + 1, RRC_N311_MAX + 1);
            if (retry < RRC_N311_MAX) {
                continue;
            }
        }
    }
    
    /* Handle failure */
    if (result != UESIM_SUCCESS || !atomic_load(&proc_ctx->success)) {
        rrc_handle_procedure_failure(ue_ctx, proc_ctx, 
            result == UESIM_ERROR_TIMEOUT ? RRC_CAUSE_TIMEOUT : RRC_CAUSE_MAX_RETRIES);
        rrc_free_procedure_context(proc_ctx);
        return result;
    }
    
    rrc_free_procedure_context(proc_ctx);
    return UESIM_SUCCESS;
}

uesim_error_t rrc_execute_handover(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("Executing RRC handover for UE %u\n", ue_ctx->ue_id);
    
    /* Check current state */
    rrc_state_t current_state = rrc_get_current_state(ue_ctx);
    if (current_state != RRC_STATE_CONNECTED) {
        fprintf(stderr, "Cannot perform handover for UE %u: not in CONNECTED state (current: %s)\n", 
                ue_ctx->ue_id, rrc_state_to_string(current_state));
        return UESIM_ERROR_PROTOCOL;
    }
    
    /* Check for candidate gNBs */
    if (ue_ctx->num_candidate_gnbs == 0) {
        fprintf(stderr, "No candidate gNBs available for handover\n");
        return UESIM_ERROR_NOT_FOUND;
    }
    
    /* Start procedure */
    rrc_procedure_context_t* proc_ctx = NULL;
    uesim_error_t result = rrc_start_procedure(ue_ctx, RRC_PROC_HANDOVER, &proc_ctx);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    /* Create RRC Handover Preparation message */
    rrc_message_t ho_prep = {0};
    ho_prep.message_type = RRC_MESSAGE_TYPE_HANDOVER_PREPARATION;
    ho_prep.message_id = proc_ctx->transaction_id;
    ho_prep.transaction_id = proc_ctx->transaction_id;
    
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
        fprintf(stderr, "No suitable handover candidate found\n");
        rrc_free_procedure_context(proc_ctx);
        return UESIM_ERROR_NOT_FOUND;
    }
    
    /* Create handover prep data */
    rrc_meas_report_data_t* ho_data = (rrc_meas_report_data_t*)uesim_calloc(1, sizeof(rrc_meas_report_data_t));
    if (ho_data == NULL) {
        rrc_free_procedure_context(proc_ctx);
        return UESIM_ERROR_MEMORY;
    }
    
    ho_data->meas_id = 1;
    ho_data->rsrp = best_rsrp;
    ho_data->rsrq = best_candidate->rsrq;
    ho_data->pci = best_candidate->cell_id;
    ho_data->cell_id = best_candidate->gnb_id;
    ho_prep.data = ho_data;
    ho_prep.data_length = sizeof(rrc_meas_report_data_t);
    proc_ctx->procedure_data = ho_data;
    
    /* Send handover preparation */
    result = rrc_send_message(ue_ctx, &ho_prep);
    if (result != UESIM_SUCCESS) {
        fprintf(stderr, "Failed to send RRC Handover Preparation: %d\n", result);
        rrc_free_procedure_context(proc_ctx);
        return result;
    }
    
    /* Wait for handover command */
    result = rrc_wait_procedure_complete(ue_ctx, proc_ctx, RRC_T304_MS);
    if (result != UESIM_SUCCESS || !atomic_load(&proc_ctx->success)) {
        rrc_handle_procedure_failure(ue_ctx, proc_ctx, 
            result == UESIM_ERROR_TIMEOUT ? RRC_CAUSE_TIMEOUT : RRC_CAUSE_HANDOVER_FAILED);
        rrc_free_procedure_context(proc_ctx);
        return result;
    }
    
    rrc_free_procedure_context(proc_ctx);
    return UESIM_SUCCESS;
}

uesim_error_t rrc_execute_reconfiguration(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("Executing RRC reconfiguration for UE %u\n", ue_ctx->ue_id);
    
    /* Check current state */
    rrc_state_t current_state = rrc_get_current_state(ue_ctx);
    if (current_state != RRC_STATE_CONNECTED) {
        fprintf(stderr, "Cannot reconfigure UE %u: not in CONNECTED state\n", ue_ctx->ue_id);
        return UESIM_ERROR_PROTOCOL;
    }
    
    /* Reconfiguration is typically network-initiated, but we can request it */
    /* This would be used for bearer modification, measurement config, etc. */
    
    return UESIM_SUCCESS;
}

uesim_error_t rrc_execute_measurement_report(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("Executing RRC measurement report for UE %u\n", ue_ctx->ue_id);
    
    /* Check current state */
    rrc_state_t current_state = rrc_get_current_state(ue_ctx);
    if (current_state != RRC_STATE_CONNECTED) {
        fprintf(stderr, "Cannot send measurement report for UE %u: not in CONNECTED state\n", ue_ctx->ue_id);
        return UESIM_ERROR_PROTOCOL;
    }
    
    /* Create measurement report message */
    rrc_message_t meas_report = {0};
    meas_report.message_type = RRC_MESSAGE_TYPE_MEASUREMENT_REPORT;
    meas_report.message_id = atomic_fetch_add(&g_transaction_id_counter, 1);
    meas_report.transaction_id = meas_report.message_id;
    
    /* Create measurement report data */
    rrc_meas_report_data_t* meas_data = (rrc_meas_report_data_t*)uesim_calloc(1, sizeof(rrc_meas_report_data_t));
    if (meas_data == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    /* Fill with measurement results */
    meas_data->meas_id = 1;
    meas_data->rsrp = ue_ctx->serving_gnb ? ue_ctx->serving_gnb->rsrp : -100;
    meas_data->rsrq = ue_ctx->serving_gnb ? ue_ctx->serving_gnb->rsrq : -10;
    meas_data->pci = ue_ctx->serving_gnb ? ue_ctx->serving_gnb->cell_id : 0;
    meas_data->cell_id = ue_ctx->serving_gnb ? ue_ctx->serving_gnb->gnb_id : 0;
    meas_report.data = meas_data;
    meas_report.data_length = sizeof(rrc_meas_report_data_t);
    
    /* Send measurement report */
    uesim_error_t result = rrc_send_message(ue_ctx, &meas_report);
    
    uesim_free(meas_data);
    return result;
}

uesim_error_t rrc_execute_capability_transfer(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("Executing RRC capability transfer for UE %u\n", ue_ctx->ue_id);
    
    /* Capability transfer is network-initiated via UE_CAPABILITY_ENQUIRY */
    /* This function would be called to respond to an enquiry */
    
    return UESIM_SUCCESS;
}

/* ============== Response Handlers ============== */

uesim_error_t rrc_handle_setup_response(ue_context_t* ue_ctx, rrc_message_t* response) {
    if (ue_ctx == NULL || response == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("Handling RRC Setup response for UE %u\n", ue_ctx->ue_id);
    
    /* Parse setup data */
    rrc_setup_data_t* setup_data = (rrc_setup_data_t*)response->data;
    if (setup_data == NULL) {
        fprintf(stderr, "Invalid RRC Setup message: no data\n");
        return UESIM_ERROR_PROTOCOL;
    }
    
    /* Send RRC Setup Complete */
    uesim_error_t result = rrc_send_setup_complete(ue_ctx, NULL, response);
    if (result != UESIM_SUCCESS) {
        fprintf(stderr, "Failed to send RRC Setup Complete: %d\n", result);
        return result;
    }
    
    /* Transition to CONNECTED */
    result = rrc_change_state(ue_ctx, RRC_STATE_CONNECTED);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    printf("UE %u RRC connection established successfully\n", ue_ctx->ue_id);
    return UESIM_SUCCESS;
}

uesim_error_t rrc_handle_reestablishment_response(ue_context_t* ue_ctx, rrc_message_t* response) {
    if (ue_ctx == NULL || response == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("Handling RRC Reestablishment response for UE %u\n", ue_ctx->ue_id);
    
    /* Parse reestablishment data */
    rrc_reest_data_t* reest_data = (rrc_reest_data_t*)response->data;
    if (reest_data == NULL) {
        fprintf(stderr, "Invalid RRC Reestablishment message: no data\n");
        return UESIM_ERROR_PROTOCOL;
    }
    
    /* Send RRC Reestablishment Complete */
    uesim_error_t result = rrc_send_reest_complete(ue_ctx, NULL, response);
    if (result != UESIM_SUCCESS) {
        fprintf(stderr, "Failed to send RRC Reestablishment Complete: %d\n", result);
        return result;
    }
    
    /* Transition to CONNECTED */
    result = rrc_change_state(ue_ctx, RRC_STATE_CONNECTED);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    printf("UE %u RRC reestablishment completed successfully\n", ue_ctx->ue_id);
    return UESIM_SUCCESS;
}

uesim_error_t rrc_handle_reconfiguration_response(ue_context_t* ue_ctx, rrc_message_t* response) {
    if (ue_ctx == NULL || response == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("Handling RRC Reconfiguration for UE %u\n", ue_ctx->ue_id);
    
    /* Parse reconfiguration data */
    rrc_reconfig_data_t* reconfig_data = (rrc_reconfig_data_t*)response->data;
    if (reconfig_data == NULL) {
        fprintf(stderr, "Invalid RRC Reconfiguration message: no data\n");
        return UESIM_ERROR_PROTOCOL;
    }
    
    /* Apply new configuration */
    /* In a full implementation, this would update radio bearers, measurement config, etc. */
    
    /* Send RRC Reconfiguration Complete */
    uesim_error_t result = rrc_send_reconfig_complete(ue_ctx, NULL, response);
    if (result != UESIM_SUCCESS) {
        fprintf(stderr, "Failed to send RRC Reconfiguration Complete: %d\n", result);
        return result;
    }
    
    printf("UE %u RRC reconfiguration completed successfully\n", ue_ctx->ue_id);
    return UESIM_SUCCESS;
}

uesim_error_t rrc_handle_handover_command(ue_context_t* ue_ctx, rrc_message_t* response) {
    if (ue_ctx == NULL || response == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("Handling RRC Handover Command for UE %u\n", ue_ctx->ue_id);
    
    /* Parse handover command data */
    rrc_handover_cmd_data_t* ho_cmd = (rrc_handover_cmd_data_t*)response->data;
    if (ho_cmd == NULL) {
        fprintf(stderr, "Invalid RRC Handover Command: no data\n");
        return UESIM_ERROR_PROTOCOL;
    }
    
    /* Find target gNB */
    gnb_context_t* target_gnb = NULL;
    for (int i = 0; i < ue_ctx->num_candidate_gnbs; i++) {
        if (ue_ctx->candidate_gnbs[i] != NULL && 
            ue_ctx->candidate_gnbs[i]->cell_id == ho_cmd->target_pci) {
            target_gnb = ue_ctx->candidate_gnbs[i];
            break;
        }
    }
    
    if (target_gnb == NULL) {
        fprintf(stderr, "Target gNB not found for handover (PCI: %u)\n", ho_cmd->target_pci);
        return UESIM_ERROR_NOT_FOUND;
    }
    
    /* Connect to target gNB */
    uesim_error_t result = uesim_connect_gnb(ue_ctx, target_gnb);
    if (result != UESIM_SUCCESS) {
        fprintf(stderr, "Failed to connect to target gNB: %d\n", result);
        return result;
    }
    
    /* Send RRC Handover Confirmation */
    result = rrc_send_handover_confirm(ue_ctx, NULL, response);
    if (result != UESIM_SUCCESS) {
        fprintf(stderr, "Failed to send RRC Handover Confirmation: %d\n", result);
        return result;
    }
    
    /* Switch serving gNB */
    result = uesim_switch_serving_gnb(ue_ctx, target_gnb);
    if (result != UESIM_SUCCESS) {
        fprintf(stderr, "Failed to switch serving gNB: %d\n", result);
        return result;
    }
    
    printf("UE %u handover completed successfully to gNB %u\n", ue_ctx->ue_id, target_gnb->gnb_id);
    return UESIM_SUCCESS;
}

uesim_error_t rrc_handle_capability_enquiry(ue_context_t* ue_ctx, rrc_message_t* response) {
    if (ue_ctx == NULL || response == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("Handling RRC UE Capability Enquiry for UE %u\n", ue_ctx->ue_id);
    
    /* Send UE Capability Information */
    uesim_error_t result = rrc_send_capability_info(ue_ctx, response);
    if (result != UESIM_SUCCESS) {
        fprintf(stderr, "Failed to send UE Capability Information: %d\n", result);
        return result;
    }
    
    printf("UE %u capability information sent successfully\n", ue_ctx->ue_id);
    return UESIM_SUCCESS;
}

uesim_error_t rrc_handle_connection_release(ue_context_t* ue_ctx, rrc_message_t* response) {
    if (ue_ctx == NULL || response == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("Handling RRC Connection Release for UE %u\n", ue_ctx->ue_id);
    
    /* Transition to IDLE */
    uesim_error_t result = rrc_change_state(ue_ctx, RRC_STATE_IDLE);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    /* Disconnect from gNB */
    if (ue_ctx->serving_gnb != NULL) {
        uesim_disconnect_gnb(ue_ctx, ue_ctx->serving_gnb);
    }
    
    printf("UE %u RRC connection released\n", ue_ctx->ue_id);
    return UESIM_SUCCESS;
}

/* ============== Helper Functions ============== */

static uesim_error_t rrc_send_setup_complete(ue_context_t* ue_ctx, rrc_procedure_context_t* proc_ctx, rrc_message_t* setup_msg) {
    if (ue_ctx == NULL || setup_msg == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    rrc_setup_data_t* setup_data = (rrc_setup_data_t*)setup_msg->data;
    
    /* Create RRC Setup Complete message */
    rrc_message_t setup_complete = {0};
    setup_complete.message_type = RRC_MESSAGE_TYPE_SETUP_COMPLETE;
    setup_complete.message_id = atomic_fetch_add(&g_transaction_id_counter, 1);
    setup_complete.transaction_id = setup_data ? setup_data->rrc_transaction_id : 0;
    
    /* Create setup complete data */
    rrc_setup_complete_data_t* complete_data = (rrc_setup_complete_data_t*)uesim_calloc(1, sizeof(rrc_setup_complete_data_t));
    if (complete_data == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    complete_data->rrc_transaction_id = setup_data ? setup_data->rrc_transaction_id : 0;
    complete_data->selected_plmn = 1;
    /* In a full implementation, NAS PDU would be included here */
    complete_data->nas_pdu_len = 0;
    
    setup_complete.data = complete_data;
    setup_complete.data_length = sizeof(rrc_setup_complete_data_t);
    
    uesim_error_t result = rrc_send_message(ue_ctx, &setup_complete);
    
    uesim_free(complete_data);
    return result;
}

static uesim_error_t rrc_send_reest_complete(ue_context_t* ue_ctx, rrc_procedure_context_t* proc_ctx, rrc_message_t* reest_msg) {
    if (ue_ctx == NULL || reest_msg == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    rrc_reest_data_t* reest_data = (rrc_reest_data_t*)reest_msg->data;
    
    /* Create RRC Reestablishment Complete message */
    rrc_message_t reest_complete = {0};
    reest_complete.message_type = RRC_MESSAGE_TYPE_REESTABLISHMENT_COMPLETE;
    reest_complete.message_id = atomic_fetch_add(&g_transaction_id_counter, 1);
    reest_complete.transaction_id = reest_data ? reest_data->rrc_transaction_id : 0;
    
    uesim_error_t result = rrc_send_message(ue_ctx, &reest_complete);
    return result;
}

static uesim_error_t rrc_send_reconfig_complete(ue_context_t* ue_ctx, rrc_procedure_context_t* proc_ctx, rrc_message_t* reconfig_msg) {
    if (ue_ctx == NULL || reconfig_msg == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    rrc_reconfig_data_t* reconfig_data = (rrc_reconfig_data_t*)reconfig_msg->data;
    
    /* Create RRC Reconfiguration Complete message */
    rrc_message_t reconfig_complete = {0};
    reconfig_complete.message_type = RRC_MESSAGE_TYPE_RECONFIGURATION_COMPLETE;
    reconfig_complete.message_id = atomic_fetch_add(&g_transaction_id_counter, 1);
    reconfig_complete.transaction_id = reconfig_data ? reconfig_data->rrc_transaction_id : 0;
    
    uesim_error_t result = rrc_send_message(ue_ctx, &reconfig_complete);
    return result;
}

static uesim_error_t rrc_send_handover_confirm(ue_context_t* ue_ctx, rrc_procedure_context_t* proc_ctx, rrc_message_t* ho_cmd) {
    if (ue_ctx == NULL || ho_cmd == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    rrc_handover_cmd_data_t* ho_data = (rrc_handover_cmd_data_t*)ho_cmd->data;
    
    /* Create RRC Handover Confirmation message */
    rrc_message_t ho_confirm = {0};
    ho_confirm.message_type = RRC_MESSAGE_TYPE_HANDOVER_CONFIRMATION;
    ho_confirm.message_id = atomic_fetch_add(&g_transaction_id_counter, 1);
    ho_confirm.transaction_id = ho_data ? ho_data->rrc_transaction_id : 0;
    
    uesim_error_t result = rrc_send_message(ue_ctx, &ho_confirm);
    return result;
}

static uesim_error_t rrc_send_capability_info(ue_context_t* ue_ctx, rrc_message_t* enquiry) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Create RRC UE Capability Information message */
    rrc_message_t cap_info = {0};
    cap_info.message_type = RRC_MESSAGE_TYPE_UE_CAPABILITY_INFORMATION;
    cap_info.message_id = atomic_fetch_add(&g_transaction_id_counter, 1);
    cap_info.transaction_id = cap_info.message_id;
    
    /* Create capability data */
    rrc_ue_cap_data_t* cap_data = (rrc_ue_cap_data_t*)uesim_calloc(1, sizeof(rrc_ue_cap_data_t));
    if (cap_data == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    /* Fill with UE capabilities (simplified) */
    cap_data->rat_type = 0; /* NR */
    cap_data->container_len = 0; /* Would contain actual capability container */
    
    cap_info.data = cap_data;
    cap_info.data_length = sizeof(rrc_ue_cap_data_t);
    
    uesim_error_t result = rrc_send_message(ue_ctx, &cap_info);
    
    uesim_free(cap_data);
    return result;
}

static uint32_t rrc_get_procedure_timeout(rrc_procedure_t procedure) {
    switch (procedure) {
        case RRC_PROC_REGISTRATION:
            return RRC_T300_MS;
        case RRC_PROC_ESTABLISHMENT:
            return RRC_T300_MS;
        case RRC_PROC_REESTABLISHMENT:
            return RRC_T301_MS;
        case RRC_PROC_HANDOVER:
            return RRC_T304_MS;
        default:
            return RRC_T300_MS;
    }
}

static rrc_message_type_t rrc_get_expected_response(rrc_procedure_t procedure) {
    switch (procedure) {
        case RRC_PROC_REGISTRATION:
        case RRC_PROC_ESTABLISHMENT:
            return RRC_MESSAGE_TYPE_SETUP;
        case RRC_PROC_REESTABLISHMENT:
            return RRC_MESSAGE_TYPE_REESTABLISHMENT;
        case RRC_PROC_HANDOVER:
            return RRC_MESSAGE_TYPE_HANDOVER_COMMAND;
        default:
            return RRC_MESSAGE_TYPE_MAX;
    }
}

/* ============== Error Recovery ============== */

uesim_error_t rrc_retry_procedure(ue_context_t* ue_ctx, rrc_procedure_context_t* ctx) {
    if (ue_ctx == NULL || ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    ctx->retry_count++;
    ctx->status = RRC_PROC_STATUS_RETRY;
    
    printf("Retrying RRC procedure %s (attempt %u)\n", 
           rrc_procedure_to_string(ctx->procedure_type), ctx->retry_count);
    
    /* Re-execute the procedure */
    return rrc_execute_procedure(ue_ctx, ctx->procedure_type);
}

uesim_error_t rrc_handle_procedure_failure(ue_context_t* ue_ctx, rrc_procedure_context_t* ctx, rrc_error_cause_t cause) {
    if (ue_ctx == NULL || ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    ctx->status = RRC_PROC_STATUS_FAILED;
    ctx->error_cause = cause;
    atomic_store(&ctx->completed, 1);
    atomic_store(&ctx->success, 0);
    
    fprintf(stderr, "RRC procedure %s failed: %s\n", 
            rrc_procedure_to_string(ctx->procedure_type),
            rrc_error_cause_to_string(cause));
    
    /* Signal completion */
#ifdef _WIN32
    SetEvent(ctx->proc_cond);
#else
    pthread_cond_signal(&ctx->proc_cond);
#endif
    
    /* Fallback to IDLE on critical failures */
    if (cause == RRC_CAUSE_MAX_RETRIES || cause == RRC_CAUSE_RADIO_LINK_FAILURE) {
        return rrc_fallback_to_idle(ue_ctx, cause);
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t rrc_fallback_to_idle(ue_context_t* ue_ctx, rrc_error_cause_t cause) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("UE %u falling back to IDLE due to: %s\n",
           ue_ctx->ue_id, rrc_error_cause_to_string(cause));
    
    /* Disconnect from gNB */
    if (ue_ctx->serving_gnb != NULL) {
        uesim_disconnect_gnb(ue_ctx, ue_ctx->serving_gnb);
    }
    
    /* Transition to IDLE */
    return rrc_change_state(ue_ctx, RRC_STATE_IDLE);
}

/* ============== Utility Functions ============== */

const char* rrc_state_to_string(rrc_state_t state) {
    switch (state) {
        case RRC_STATE_IDLE: return "IDLE";
        case RRC_STATE_CONNECTED: return "CONNECTED";
        case RRC_STATE_INACTIVE: return "INACTIVE";
        case RRC_STATE_CONNECTING: return "CONNECTING";
        case RRC_STATE_MAX:
        default: return "UNKNOWN";
    }
}

const char* rrc_message_type_to_string(rrc_message_type_t type) {
    switch (type) {
        case RRC_MESSAGE_TYPE_SETUP_REQUEST: return "RRCSetupRequest";
        case RRC_MESSAGE_TYPE_SETUP: return "RRCSetup";
        case RRC_MESSAGE_TYPE_SETUP_COMPLETE: return "RRCSetupComplete";
        case RRC_MESSAGE_TYPE_REESTABLISHMENT_REQUEST: return "RRCReestablishmentRequest";
        case RRC_MESSAGE_TYPE_REESTABLISHMENT: return "RRCReestablishment";
        case RRC_MESSAGE_TYPE_REESTABLISHMENT_COMPLETE: return "RRCReestablishmentComplete";
        case RRC_MESSAGE_TYPE_RECONFIGURATION: return "RRCReconfiguration";
        case RRC_MESSAGE_TYPE_RECONFIGURATION_COMPLETE: return "RRCReconfigurationComplete";
        case RRC_MESSAGE_TYPE_MEASUREMENT_REPORT: return "RRCMeasurementReport";
        case RRC_MESSAGE_TYPE_HANDOVER_PREPARATION: return "RRCHandoverPreparation";
        case RRC_MESSAGE_TYPE_HANDOVER_COMMAND: return "RRCHandoverCommand";
        case RRC_MESSAGE_TYPE_HANDOVER_CONFIRMATION: return "RRCHandoverConfirmation";
        case RRC_MESSAGE_TYPE_UE_CAPABILITY_ENQUIRY: return "RRCUECapabilityEnquiry";
        case RRC_MESSAGE_TYPE_UE_CAPABILITY_INFORMATION: return "RRCUECapabilityInformation";
        case RRC_MESSAGE_TYPE_CONNECTION_RELEASE: return "RRCConnectionRelease";
        case RRC_MESSAGE_TYPE_SECURITY_MODE_COMMAND: return "RRCSecurityModeCommand";
        case RRC_MESSAGE_TYPE_SECURITY_MODE_COMPLETE: return "RRCSecurityModeComplete";
        default: return "UNKNOWN";
    }
}

const char* rrc_procedure_to_string(rrc_procedure_t proc) {
    switch (proc) {
        case RRC_PROC_REGISTRATION: return "Registration";
        case RRC_PROC_ESTABLISHMENT: return "Establishment";
        case RRC_PROC_REESTABLISHMENT: return "Reestablishment";
        case RRC_PROC_HANDOVER: return "Handover";
        default: return "Unknown";
    }
}

const char* rrc_error_cause_to_string(rrc_error_cause_t cause) {
    switch (cause) {
        case RRC_CAUSE_SUCCESS: return "Success";
        case RRC_CAUSE_TIMEOUT: return "Timeout";
        case RRC_CAUSE_MAX_RETRIES: return "MaxRetries";
        case RRC_CAUSE_NETWORK_FAILURE: return "NetworkFailure";
        case RRC_CAUSE_RADIO_LINK_FAILURE: return "RadioLinkFailure";
        case RRC_CAUSE_HANDOVER_FAILED: return "HandoverFailed";
        case RRC_CAUSE_RECONFIGURATION_FAILED: return "ReconfigurationFailed";
        case RRC_CAUSE_SECURITY_FAILED: return "SecurityFailed";
        case RRC_CAUSE_INVALID_STATE: return "InvalidState";
        default: return "Unknown";
    }
}

/* ============== Message Encoding/Decoding ============== */

uesim_error_t rrc_encode_message(rrc_message_t* message, void** encoded_data, size_t* encoded_length) {
    if (message == NULL || encoded_data == NULL || encoded_length == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Calculate encoded length */
    size_t length = sizeof(rrc_message_type_t) + sizeof(uint32_t) * 2 + sizeof(size_t);
    if (message->data_length > 0) {
        length += message->data_length;
    }
    
    /* Allocate memory for encoded data */
    void* data = uesim_malloc(length);
    if (data == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    /* Encode message (simplified format) */
    uint8_t* ptr = (uint8_t*)data;
    
    /* Copy message type */
    memcpy(ptr, &message->message_type, sizeof(rrc_message_type_t));
    ptr += sizeof(rrc_message_type_t);
    
    /* Copy message ID */
    memcpy(ptr, &message->message_id, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    
    /* Copy transaction ID */
    memcpy(ptr, &message->transaction_id, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    
    /* Copy data length */
    memcpy(ptr, &message->data_length, sizeof(size_t));
    ptr += sizeof(size_t);
    
    /* Copy data */
    if (message->data_length > 0 && message->data != NULL) {
        memcpy(ptr, message->data, message->data_length);
    }
    
    *encoded_data = data;
    *encoded_length = length;
    
    return UESIM_SUCCESS;
}

uesim_error_t rrc_decode_message(const void* encoded_data, size_t encoded_length, rrc_message_t* message) {
    if (encoded_data == NULL || message == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Check minimum length */
    size_t min_length = sizeof(rrc_message_type_t) + sizeof(uint32_t) * 2 + sizeof(size_t);
    if (encoded_length < min_length) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Decode message */
    const uint8_t* ptr = (const uint8_t*)encoded_data;
    
    /* Copy message type */
    memcpy(&message->message_type, ptr, sizeof(rrc_message_type_t));
    ptr += sizeof(rrc_message_type_t);
    
    /* Copy message ID */
    memcpy(&message->message_id, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    
    /* Copy transaction ID */
    memcpy(&message->transaction_id, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    
    /* Copy data length */
    memcpy(&message->data_length, ptr, sizeof(size_t));
    ptr += sizeof(size_t);
    
    /* Validate data length */
    if (message->data_length > 0) {
        size_t expected_length = min_length + message->data_length;
        if (encoded_length < expected_length) {
            return UESIM_ERROR_INVALID_PARAM;
        }
        
        /* Allocate and copy data */
        message->data = uesim_malloc(message->data_length);
        if (message->data == NULL) {
            return UESIM_ERROR_MEMORY;
        }
        memcpy(message->data, ptr, message->data_length);
    } else {
        message->data = NULL;
    }
    
    return UESIM_SUCCESS;
}
