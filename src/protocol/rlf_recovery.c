/*
 * 5G UE Simulation Application
 * RLF (Radio Link Failure) Detection and Recovery Implementation
 * 
 * Implements 3GPP TS 38.331 RRC Radio Link Failure detection and recovery
 */

#include "rlf_recovery.h"
#include "rrc.h"
#include "../core/memory.h"
#include "../uesim.h"
#include <string.h>
#include <time.h>

/* Use centralized uesim_get_time_ms() from uesim.h */

/* State string conversion */
static const char* rlf_state_strings[] = {
    "NORMAL", "OUT_OF_SYNC", "T310_RUNNING", "IN_SYNC_RECOVERY",
    "DETECTED", "REESTABLISHING", "RECOVERED", "FAILED"
};

static const char* rlf_cause_strings[] = {
    "NONE", "T310_EXPIRY", "RANDOM_ACCESS_FAILURE", "RLC_MAX_RETX",
    "HANDOVER_FAILURE", "T304_EXPIRY", "CONNECTION_RELEASE", "PHYSICAL_LAYER_ERROR"
};

static const char* rlf_reest_result_strings[] = {
    "SUCCESS", "T301_EXPIRY", "T311_EXPIRY", "CELL_RESELECTION",
    "CONNECTION_RELEASE", "NO_SUITABLE_CELL", "NETWORK_REJECT"
};

const char* rlf_state_to_string(rlf_state_t state) {
    if (state >= RLF_STATE_MAX) return "UNKNOWN";
    return rlf_state_strings[state];
}

const char* rlf_cause_to_string(rlf_cause_t cause) {
    if (cause >= RLF_CAUSE_MAX) return "UNKNOWN";
    return rlf_cause_strings[cause];
}

const char* rlf_reest_result_to_string(rlf_reest_result_t result) {
    if (result >= RLF_REEST_MAX) return "UNKNOWN";
    return rlf_reest_result_strings[result];
}

uesim_error_t rlf_create_context(rlf_context_t** ctx) {
    if (ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    rlf_context_t* rlf_ctx = (rlf_context_t*)uesim_calloc(1, sizeof(rlf_context_t));
    if (rlf_ctx == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    /* Initialize with defaults */
    rlf_ctx->current_state = RLF_STATE_NORMAL;
    rlf_ctx->detection_cause = RLF_CAUSE_NONE;
    rlf_ctx->t310_value_ms = RLF_T310_DEFAULT_MS;
    rlf_ctx->t311_value_ms = RLF_T311_DEFAULT_MS;
    rlf_ctx->t301_value_ms = RLF_T301_DEFAULT_MS;
    rlf_ctx->n310_threshold = RLF_N310_MAX;
    rlf_ctx->n311_threshold = RLF_N311_MAX;
    rlf_ctx->max_reest_attempts = 3;
    
    /* Initialize mutex */
    if (pthread_mutex_init(&rlf_ctx->rlf_mutex, NULL) != 0) {
        uesim_free(rlf_ctx);
        return UESIM_ERROR_THREAD;
    }
    
    if (pthread_cond_init(&rlf_ctx->rlf_cond, NULL) != 0) {
        pthread_mutex_destroy(&rlf_ctx->rlf_mutex);
        uesim_free(rlf_ctx);
        return UESIM_ERROR_THREAD;
    }
    
    *ctx = rlf_ctx;
    printf("RLF: Context created\n");
    return UESIM_SUCCESS;
}

void rlf_destroy_context(rlf_context_t* ctx) {
    if (ctx != NULL) {
        pthread_cond_destroy(&ctx->rlf_cond);
        pthread_mutex_destroy(&ctx->rlf_mutex);
        uesim_free(ctx);
    }
}

uesim_error_t rlf_init(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Create RLF context */
    rlf_context_t* rlf_ctx = NULL;
    uesim_error_t result = rlf_create_context(&rlf_ctx);
    if (result != UESIM_SUCCESS) {
        printf("RLF: Failed to create context for UE %u, error=%d\n", ue_ctx->ue_id, result);
        return result;
    }
    
    /* Store RLF context in UE context (using rrc_meas_ctx field as RLF context) */
    ue_ctx->rrc_meas_ctx = (struct rrc_meas_context_t*)rlf_ctx;
    
    printf("RLF: Initialized for UE %u\n", ue_ctx->ue_id);
    return UESIM_SUCCESS;
}

void rlf_cleanup(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return;
    }
    
    /* Get and destroy RLF context */
    rlf_context_t* rlf_ctx = ue_ctx->rrc_meas_ctx ? (rlf_context_t*)ue_ctx->rrc_meas_ctx : NULL;
    if (rlf_ctx != NULL) {
        rlf_destroy_context(rlf_ctx);
        ue_ctx->rrc_meas_ctx = NULL;
    }
    
    printf("RLF: Cleanup completed for UE %u\n", ue_ctx->ue_id);
}

uesim_error_t rlf_configure(rlf_context_t* ctx, uint32_t t310_ms, uint32_t t311_ms,
                           uint32_t t301_ms, uint8_t n310, uint8_t n311) {
    if (ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&ctx->rlf_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    ctx->t310_value_ms = t310_ms;
    ctx->t311_value_ms = t311_ms;
    ctx->t301_value_ms = t301_ms;
    ctx->n310_threshold = n310;
    ctx->n311_threshold = n311;
    
    pthread_mutex_unlock(&ctx->rlf_mutex);
    
    printf("RLF: Configured T310=%u, T311=%u, T301=%u, N310=%u, N311=%u\n",
           t310_ms, t311_ms, t301_ms, n310, n311);
    return UESIM_SUCCESS;
}

uesim_error_t rlf_handle_sync_indication(ue_context_t* ue_ctx, sync_indication_t indication) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    rlf_context_t* ctx = ue_ctx->rrc_meas_ctx ? (rlf_context_t*)ue_ctx->rrc_meas_ctx : NULL;
    if (ctx == NULL) {
        return UESIM_ERROR_NOT_INITIALIZED;
    }
    
    if (pthread_mutex_lock(&ctx->rlf_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    if (indication == SYNC_INDICATION_OUT_OF_SYNC) {
        ctx->total_out_of_sync_indications++;
        
        /* Only process out-of-sync if in RRC_CONNECTED state */
        if (ue_ctx->current_state != RRC_STATE_CONNECTED) {
            pthread_mutex_unlock(&ctx->rlf_mutex);
            return UESIM_SUCCESS;
        }
        
        /* Reset in-sync counter */
        ctx->in_sync_count = 0;
        
        if (ctx->current_state == RLF_STATE_NORMAL || ctx->current_state == RLF_STATE_IN_SYNC_RECOVERY) {
            ctx->out_of_sync_count++;
            ctx->current_state = RLF_STATE_OUT_OF_SYNC;
            
            printf("RLF: Out-of-sync indication %u/%u\n", ctx->out_of_sync_count, ctx->n310_threshold);
            
            /* Check if we should start T310 */
            if (ctx->out_of_sync_count >= ctx->n310_threshold && !ctx->t310_running) {
                pthread_mutex_unlock(&ctx->rlf_mutex);
                rlf_start_t310(ue_ctx);
                return UESIM_SUCCESS;
            }
        }
    } else { /* SYNC_INDICATION_IN_SYNC */
        ctx->total_in_sync_indications++;
        
        /* Reset out-of-sync counter */
        ctx->out_of_sync_count = 0;
        
        if (ctx->current_state == RLF_STATE_T310_RUNNING) {
            ctx->in_sync_count++;
            ctx->current_state = RLF_STATE_IN_SYNC_RECOVERY;
            
            printf("RLF: In-sync indication %u/%u\n", ctx->in_sync_count, ctx->n311_threshold);
            
            /* Check if we should stop T310 */
            if (ctx->in_sync_count >= ctx->n311_threshold) {
                rlf_stop_t310(ue_ctx);
                ctx->current_state = RLF_STATE_NORMAL;
                printf("RLF: Radio link recovered, returning to NORMAL\n");
            }
        }
    }
    
    pthread_mutex_unlock(&ctx->rlf_mutex);
    return UESIM_SUCCESS;
}

uesim_error_t rlf_start_t310(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    rlf_context_t* ctx = ue_ctx->rrc_meas_ctx ? (rlf_context_t*)ue_ctx->rrc_meas_ctx : NULL;
    if (ctx == NULL) {
        return UESIM_ERROR_NOT_INITIALIZED;
    }
    
    if (pthread_mutex_lock(&ctx->rlf_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    ctx->t310_start_time = uesim_get_time_ms();
    ctx->t310_running = true;
    ctx->current_state = RLF_STATE_T310_RUNNING;
    
    printf("RLF: T310 timer started (%u ms)\n", ctx->t310_value_ms);
    
    pthread_mutex_unlock(&ctx->rlf_mutex);
    return UESIM_SUCCESS;
}

uesim_error_t rlf_stop_t310(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    rlf_context_t* ctx = ue_ctx->rrc_meas_ctx ? (rlf_context_t*)ue_ctx->rrc_meas_ctx : NULL;
    if (ctx == NULL) {
        return UESIM_ERROR_NOT_INITIALIZED;
    }
    
    if (pthread_mutex_lock(&ctx->rlf_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    ctx->t310_running = false;
    ctx->out_of_sync_count = 0;
    
    printf("RLF: T310 timer stopped\n");
    
    pthread_mutex_unlock(&ctx->rlf_mutex);
    return UESIM_SUCCESS;
}

uesim_error_t rlf_handle_t310_expiry(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    rlf_context_t* ctx = ue_ctx->rrc_meas_ctx ? (rlf_context_t*)ue_ctx->rrc_meas_ctx : NULL;
    if (ctx == NULL) {
        return UESIM_ERROR_NOT_INITIALIZED;
    }
    
    printf("RLF: T310 expired - declaring Radio Link Failure\n");
    
    return rlf_declare_failure(ue_ctx);
}

uesim_error_t rlf_start_t311(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    rlf_context_t* ctx = ue_ctx->rrc_meas_ctx ? (rlf_context_t*)ue_ctx->rrc_meas_ctx : NULL;
    if (ctx == NULL) {
        return UESIM_ERROR_NOT_INITIALIZED;
    }
    
    if (pthread_mutex_lock(&ctx->rlf_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    ctx->t311_start_time = uesim_get_time_ms();
    ctx->t311_running = true;
    
    printf("RLF: T311 timer started (%u ms)\n", ctx->t311_value_ms);
    
    pthread_mutex_unlock(&ctx->rlf_mutex);
    return UESIM_SUCCESS;
}

uesim_error_t rlf_stop_t311(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    rlf_context_t* ctx = ue_ctx->rrc_meas_ctx ? (rlf_context_t*)ue_ctx->rrc_meas_ctx : NULL;
    if (ctx == NULL) {
        return UESIM_ERROR_NOT_INITIALIZED;
    }
    
    if (pthread_mutex_lock(&ctx->rlf_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    ctx->t311_running = false;
    printf("RLF: T311 timer stopped\n");
    
    pthread_mutex_unlock(&ctx->rlf_mutex);
    return UESIM_SUCCESS;
}

uesim_error_t rlf_handle_t311_expiry(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("RLF: T311 expired - going to RRC_IDLE\n");
    
    return rlf_abort_recovery(ue_ctx, RLF_REEST_T311_EXPIRY);
}

uesim_error_t rlf_start_t301(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    rlf_context_t* ctx = ue_ctx->rrc_meas_ctx ? (rlf_context_t*)ue_ctx->rrc_meas_ctx : NULL;
    if (ctx == NULL) {
        return UESIM_ERROR_NOT_INITIALIZED;
    }
    
    if (pthread_mutex_lock(&ctx->rlf_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    ctx->t301_start_time = uesim_get_time_ms();
    ctx->t301_running = true;
    
    printf("RLF: T301 timer started (%u ms)\n", ctx->t301_value_ms);
    
    pthread_mutex_unlock(&ctx->rlf_mutex);
    return UESIM_SUCCESS;
}

uesim_error_t rlf_stop_t301(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    rlf_context_t* ctx = ue_ctx->rrc_meas_ctx ? (rlf_context_t*)ue_ctx->rrc_meas_ctx : NULL;
    if (ctx == NULL) {
        return UESIM_ERROR_NOT_INITIALIZED;
    }
    
    if (pthread_mutex_lock(&ctx->rlf_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    ctx->t301_running = false;
    printf("RLF: T301 timer stopped\n");
    
    pthread_mutex_unlock(&ctx->rlf_mutex);
    return UESIM_SUCCESS;
}

uesim_error_t rlf_handle_t301_expiry(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    rlf_context_t* ctx = ue_ctx->rrc_meas_ctx ? (rlf_context_t*)ue_ctx->rrc_meas_ctx : NULL;
    if (ctx == NULL) {
        return UESIM_ERROR_NOT_INITIALIZED;
    }
    
    printf("RLF: T301 expired - re-establishment failed\n");
    
    /* Retry re-establishment or abort */
    if (ctx->reest_attempt_count < ctx->max_reest_attempts) {
        printf("RLF: Retrying re-establishment (attempt %u/%u)\n",
               ctx->reest_attempt_count + 1, ctx->max_reest_attempts);
        return rlf_start_reestablishment(ue_ctx);
    }
    
    return rlf_abort_recovery(ue_ctx, RLF_REEST_T301_EXPIRY);
}

uesim_error_t rlf_detect_failure(ue_context_t* ue_ctx, rlf_cause_t cause) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    rlf_context_t* ctx = ue_ctx->rrc_meas_ctx ? (rlf_context_t*)ue_ctx->rrc_meas_ctx : NULL;
    if (ctx == NULL) {
        return UESIM_ERROR_NOT_INITIALIZED;
    }
    
    printf("RLF: Failure detected, cause=%s\n", rlf_cause_to_string(cause));
    
    return rlf_declare_failure(ue_ctx);
}

uesim_error_t rlf_declare_failure(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    rlf_context_t* ctx = ue_ctx->rrc_meas_ctx ? (rlf_context_t*)ue_ctx->rrc_meas_ctx : NULL;
    if (ctx == NULL) {
        return UESIM_ERROR_NOT_INITIALIZED;
    }
    
    if (pthread_mutex_lock(&ctx->rlf_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    /* Stop any running timers */
    ctx->t310_running = false;
    
    /* Update state */
    ctx->current_state = RLF_STATE_DETECTED;
    ctx->detection_time = time(NULL);
    ctx->detection_cause = RLF_CAUSE_T310_EXPIRY;
    ctx->total_rlf_count++;
    
    /* Backup current state for potential recovery */
    ctx->pre_rlf_state = ue_ctx->current_state;
    
    printf("RLF: Radio Link Failure declared! Total RLF count: %lu\n", ctx->total_rlf_count);
    
    pthread_mutex_unlock(&ctx->rlf_mutex);
    
    /* Initiate recovery */
    return rlf_initiate_recovery(ue_ctx);
}

bool rlf_is_detected(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) return false;
    
    rlf_context_t* ctx = ue_ctx->rrc_meas_ctx ? (rlf_context_t*)ue_ctx->rrc_meas_ctx : NULL;
    if (ctx == NULL) return false;
    
    return (ctx->current_state == RLF_STATE_DETECTED || 
            ctx->current_state == RLF_STATE_REESTABLISHING);
}

uesim_error_t rlf_initiate_recovery(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("RLF: Initiating recovery procedure\n");
    
    /* Start T311 - overall recovery timer */
    uesim_error_t result = rlf_start_t311(ue_ctx);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    /* Start re-establishment */
    return rlf_start_reestablishment(ue_ctx);
}

uesim_error_t rlf_start_reestablishment(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    rlf_context_t* ctx = ue_ctx->rrc_meas_ctx ? (rlf_context_t*)ue_ctx->rrc_meas_ctx : NULL;
    if (ctx == NULL) {
        return UESIM_ERROR_NOT_INITIALIZED;
    }
    
    if (pthread_mutex_lock(&ctx->rlf_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    ctx->current_state = RLF_STATE_REESTABLISHING;
    ctx->reest_attempt_count++;
    
    printf("RLF: Starting RRC Re-establishment (attempt %u/%u)\n",
           ctx->reest_attempt_count, ctx->max_reest_attempts);
    
    pthread_mutex_unlock(&ctx->rlf_mutex);
    
    /* Start T301 for re-establishment response */
    rlf_start_t301(ue_ctx);
    
    /* Execute RRC re-establishment procedure */
    return rrc_execute_reestablishment(ue_ctx);
}

uesim_error_t rlf_handle_reestablishment_response(ue_context_t* ue_ctx, bool success) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    rlf_context_t* ctx = ue_ctx->rrc_meas_ctx ? (rlf_context_t*)ue_ctx->rrc_meas_ctx : NULL;
    if (ctx == NULL) {
        return UESIM_ERROR_NOT_INITIALIZED;
    }
    
    if (success) {
        printf("RLF: Re-establishment successful\n");
        return rlf_complete_recovery(ue_ctx);
    } else {
        printf("RLF: Re-establishment rejected by network\n");
        return rlf_abort_recovery(ue_ctx, RLF_REEST_NETWORK_REJECT);
    }
}

uesim_error_t rlf_complete_recovery(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    rlf_context_t* ctx = ue_ctx->rrc_meas_ctx ? (rlf_context_t*)ue_ctx->rrc_meas_ctx : NULL;
    if (ctx == NULL) {
        return UESIM_ERROR_NOT_INITIALIZED;
    }
    
    if (pthread_mutex_lock(&ctx->rlf_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    /* Stop all timers */
    ctx->t310_running = false;
    ctx->t311_running = false;
    ctx->t301_running = false;
    
    /* Update state */
    ctx->current_state = RLF_STATE_RECOVERED;
    ctx->successful_recovery_count++;
    ctx->reest_attempt_count = 0;
    
    printf("RLF: Recovery completed! Successful recoveries: %lu\n", ctx->successful_recovery_count);
    
    pthread_mutex_unlock(&ctx->rlf_mutex);
    
    /* Reset to normal after recovery */
    ctx->current_state = RLF_STATE_NORMAL;
    
    return UESIM_SUCCESS;
}

uesim_error_t rlf_abort_recovery(ue_context_t* ue_ctx, rlf_reest_result_t result) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    rlf_context_t* ctx = ue_ctx->rrc_meas_ctx ? (rlf_context_t*)ue_ctx->rrc_meas_ctx : NULL;
    if (ctx == NULL) {
        return UESIM_ERROR_NOT_INITIALIZED;
    }
    
    if (pthread_mutex_lock(&ctx->rlf_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    /* Stop all timers */
    ctx->t310_running = false;
    ctx->t311_running = false;
    ctx->t301_running = false;
    
    /* Update state */
    ctx->current_state = RLF_STATE_FAILED;
    ctx->failed_recovery_count++;
    ctx->last_reest_result = result;
    
    printf("RLF: Recovery aborted, result=%s. Failed recoveries: %lu\n",
           rlf_reest_result_to_string(result), ctx->failed_recovery_count);
    
    pthread_mutex_unlock(&ctx->rlf_mutex);
    
    /* Go to RRC_IDLE state */
    return rlf_go_to_idle(ue_ctx);
}

uesim_error_t rlf_backup_state(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    rlf_context_t* ctx = ue_ctx->rrc_meas_ctx ? (rlf_context_t*)ue_ctx->rrc_meas_ctx : NULL;
    if (ctx == NULL) {
        return UESIM_ERROR_NOT_INITIALIZED;
    }
    
    if (pthread_mutex_lock(&ctx->rlf_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    ctx->pre_rlf_state = ue_ctx->current_state;
    if (ue_ctx->serving_gnb != NULL) {
        ctx->pre_rlf_pci = ue_ctx->serving_gnb->cell_id;
    }
    
    printf("RLF: State backed up (pre-RLF state: %s)\n", 
           rrc_state_to_string(ctx->pre_rlf_state));
    
    pthread_mutex_unlock(&ctx->rlf_mutex);
    return UESIM_SUCCESS;
}

uesim_error_t rlf_restore_state(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    rlf_context_t* ctx = ue_ctx->rrc_meas_ctx ? (rlf_context_t*)ue_ctx->rrc_meas_ctx : NULL;
    if (ctx == NULL) {
        return UESIM_ERROR_NOT_INITIALIZED;
    }
    
    printf("RLF: Restoring state to %s\n", rrc_state_to_string(ctx->pre_rlf_state));
    
    /* Restore state via RRC */
    return rrc_change_state(ue_ctx, ctx->pre_rlf_state);
}

uesim_error_t rlf_trigger_cell_reselection(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("RLF: Triggering cell reselection\n");
    
    /* This would normally involve:
     * 1. Searching for suitable cells
     * 2. Ranking cells based on measurements
     * 3. Selecting best cell
     * 4. Camping on selected cell
     */
    
    return rlf_go_to_idle(ue_ctx);
}

uesim_error_t rlf_go_to_idle(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("RLF: Transitioning to RRC_IDLE\n");
    
    /* Change state to RRC_IDLE */
    uesim_error_t result = rrc_change_state(ue_ctx, RRC_STATE_IDLE);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    /* Reset RLF context */
    rlf_context_t* ctx = ue_ctx->rrc_meas_ctx ? (rlf_context_t*)ue_ctx->rrc_meas_ctx : NULL;
    if (ctx != NULL) {
        ctx->current_state = RLF_STATE_NORMAL;
        ctx->out_of_sync_count = 0;
        ctx->in_sync_count = 0;
        ctx->reest_attempt_count = 0;
    }
    
    return UESIM_SUCCESS;
}

rlf_state_t rlf_get_state(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) return RLF_STATE_NORMAL;
    
    rlf_context_t* ctx = ue_ctx->rrc_meas_ctx ? (rlf_context_t*)ue_ctx->rrc_meas_ctx : NULL;
    if (ctx == NULL) return RLF_STATE_NORMAL;
    
    return ctx->current_state;
}

uesim_error_t rlf_check_timers(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    rlf_context_t* ctx = ue_ctx->rrc_meas_ctx ? (rlf_context_t*)ue_ctx->rrc_meas_ctx : NULL;
    if (ctx == NULL) {
        return UESIM_ERROR_NOT_INITIALIZED;
    }
    
    uint32_t current_time = uesim_get_time_ms();
    bool timer_expired = false;
    rlf_cause_t expiry_cause = RLF_CAUSE_NONE;
    
    if (pthread_mutex_lock(&ctx->rlf_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    /* Check T310 */
    if (ctx->t310_running) {
        if ((current_time - ctx->t310_start_time) >= ctx->t310_value_ms) {
            ctx->t310_running = false;
            timer_expired = true;
            expiry_cause = RLF_CAUSE_T310_EXPIRY;
        }
    }
    
    /* Check T311 */
    if (!timer_expired && ctx->t311_running) {
        if ((current_time - ctx->t311_start_time) >= ctx->t311_value_ms) {
            ctx->t311_running = false;
            timer_expired = true;
        }
    }
    
    /* Check T301 */
    if (!timer_expired && ctx->t301_running) {
        if ((current_time - ctx->t301_start_time) >= ctx->t301_value_ms) {
            ctx->t301_running = false;
            timer_expired = true;
        }
    }
    
    pthread_mutex_unlock(&ctx->rlf_mutex);
    
    /* Handle timer expiry outside of lock */
    if (timer_expired) {
        switch (expiry_cause) {
            case RLF_CAUSE_T310_EXPIRY:
                return rlf_handle_t310_expiry(ue_ctx);
            default:
                if (ctx->t311_start_time > 0 && !ctx->t311_running) {
                    return rlf_handle_t311_expiry(ue_ctx);
                }
                if (ctx->t301_start_time > 0 && !ctx->t301_running) {
                    return rlf_handle_t301_expiry(ue_ctx);
                }
                break;
        }
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t rlf_get_stats(ue_context_t* ue_ctx, uint64_t* total_rlf,
                           uint64_t* successful_recovery, uint64_t* failed_recovery) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    rlf_context_t* ctx = ue_ctx->rrc_meas_ctx ? (rlf_context_t*)ue_ctx->rrc_meas_ctx : NULL;
    if (ctx == NULL) {
        return UESIM_ERROR_NOT_INITIALIZED;
    }
    
    if (pthread_mutex_lock(&ctx->rlf_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    if (total_rlf) *total_rlf = ctx->total_rlf_count;
    if (successful_recovery) *successful_recovery = ctx->successful_recovery_count;
    if (failed_recovery) *failed_recovery = ctx->failed_recovery_count;
    
    pthread_mutex_unlock(&ctx->rlf_mutex);
    return UESIM_SUCCESS;
}

uesim_error_t rlf_reset_stats(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    rlf_context_t* ctx = ue_ctx->rrc_meas_ctx ? (rlf_context_t*)ue_ctx->rrc_meas_ctx : NULL;
    if (ctx == NULL) {
        return UESIM_ERROR_NOT_INITIALIZED;
    }
    
    if (pthread_mutex_lock(&ctx->rlf_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    ctx->total_rlf_count = 0;
    ctx->successful_recovery_count = 0;
    ctx->failed_recovery_count = 0;
    ctx->total_out_of_sync_indications = 0;
    ctx->total_in_sync_indications = 0;
    
    pthread_mutex_unlock(&ctx->rlf_mutex);
    
    printf("RLF: Statistics reset\n");
    return UESIM_SUCCESS;
}
