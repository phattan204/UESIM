/*
 * 5G UE Simulation Application
 * RRC Measurement Event System Implementation (3GPP TS 38.331)
 */

#include "rrc_meas.h"
#include "rrc.h"
#include "../core/memory.h"
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define strdup _strdup
#endif

/* Event name strings */
static const char* g_event_names[] = {
    "A1", "A2", "A3", "A4", "A5", "A6", "B1", "B2"
};

/* Quantity name strings */
static const char* g_quantity_names[] = {
    "RSRP", "RSRQ", "SINR"
};

/* ============== Utility Functions ============== */

const char* rrc_meas_event_to_string(rrc_meas_event_t event) {
    if (event >= RRC_MEAS_EVENT_A1 && event < RRC_MEAS_EVENT_MAX) {
        return g_event_names[event];
    }
    return "Unknown";
}

const char* rrc_quantity_to_string(rrc_quantity_t qty) {
    if (qty >= RRC_QUANTITY_RSRP && qty <= RRC_QUANTITY_SINR) {
        return g_quantity_names[qty];
    }
    return "Unknown";
}

int32_t rrc_meas_get_quantity_value(const rrc_meas_result_t* result, rrc_quantity_t qty) {
    if (!result) return -140;  /* Minimum value */
    
    switch (qty) {
        case RRC_QUANTITY_RSRP: return result->rsrp;
        case RRC_QUANTITY_RSRQ: return result->rsrq;
        case RRC_QUANTITY_SINR: return result->sinr;
        default: return result->rsrp;
    }
}

/* ============== Default Configurations ============== */

rrc_meas_config_t rrc_meas_get_default_a3_config(void) {
    rrc_meas_config_t config = {0};
    config.event_id = RRC_MEAS_EVENT_A3;
    config.enabled = true;
    config.hysteresis_db = 1;      /* 0.5 dB */
    config.time_to_trigger = 640;  /* 640 ms */
    config.threshold1 = 3;         /* 3 dB offset */
    config.quantity = RRC_QUANTITY_RSRP;
    config.report_trigger = RRC_REPORT_TRIGGER_EVENT;
    config.max_report_cells = MAX_GNB_CANDIDATES;
    return config;
}

rrc_meas_config_t rrc_meas_get_default_a4_config(void) {
    rrc_meas_config_t config = {0};
    config.event_id = RRC_MEAS_EVENT_A4;
    config.enabled = true;
    config.hysteresis_db = 1;      /* 0.5 dB */
    config.time_to_trigger = 640;  /* 640 ms */
    config.threshold1 = -100;      /* -100 dBm threshold */
    config.quantity = RRC_QUANTITY_RSRP;
    config.report_trigger = RRC_REPORT_TRIGGER_EVENT;
    config.max_report_cells = MAX_GNB_CANDIDATES;
    return config;
}

rrc_meas_config_t rrc_meas_get_default_a5_config(void) {
    rrc_meas_config_t config = {0};
    config.event_id = RRC_MEAS_EVENT_A5;
    config.enabled = true;
    config.hysteresis_db = 1;      /* 0.5 dB */
    config.time_to_trigger = 640;  /* 640 ms */
    config.threshold1 = -110;      /* Serving worse than -110 dBm */
    config.threshold2 = -100;      /* Neighbor better than -100 dBm */
    config.quantity = RRC_QUANTITY_RSRP;
    config.report_trigger = RRC_REPORT_TRIGGER_EVENT;
    config.max_report_cells = MAX_GNB_CANDIDATES;
    return config;
}

/* ============== Initialization/Cleanup ============== */

uesim_error_t rrc_meas_init(ue_context_t* ue_ctx) {
    if (!ue_ctx) return UESIM_ERROR_INVALID_PARAM;
    
    /* Allocate measurement context */
    rrc_meas_context_t* ctx = (rrc_meas_context_t*)uesim_calloc(1, sizeof(rrc_meas_context_t));
    if (!ctx) return UESIM_ERROR_MEMORY;
    
    ctx->initialized = true;
    ctx->next_meas_id = 1;
    ctx->meas_interval_ms = 200;  /* 200ms default */
    ctx->last_meas_time = 0;
    ctx->num_active_events = 0;
    ctx->report_pending = false;
    
    pthread_mutex_init(&ctx->meas_mutex, NULL);
    
    /* Store in UE context (we'll add a field for this) */
    /* For now, we use a global map or extend ue_context_t */
    
    return UESIM_SUCCESS;
}

void rrc_meas_cleanup(ue_context_t* ue_ctx) {
    if (!ue_ctx) return;
    
    rrc_meas_context_t* ctx = rrc_meas_get_context(ue_ctx);
    if (ctx) {
        pthread_mutex_destroy(&ctx->meas_mutex);
        uesim_free(ctx);
    }
}

/* ============== Configuration ============== */

uesim_error_t rrc_meas_configure_event(ue_context_t* ue_ctx, 
                                        const rrc_meas_config_t* config) {
    if (!ue_ctx || !config) return UESIM_ERROR_INVALID_PARAM;
    if (config->event_id >= RRC_MEAS_EVENT_MAX) return UESIM_ERROR_INVALID_PARAM;
    
    rrc_meas_context_t* ctx = rrc_meas_get_context(ue_ctx);
    if (!ctx || !ctx->initialized) return UESIM_ERROR_NOT_INITIALIZED;
    
    pthread_mutex_lock(&ctx->meas_mutex);
    
    rrc_meas_event_state_t* state = &ctx->events[config->event_id];
    
    /* Copy configuration */
    memcpy(&state->config, config, sizeof(rrc_meas_config_t));
    state->config.meas_id = ctx->next_meas_id++;
    
    /* Reset state */
    state->event_entered = false;
    state->event_triggered = false;
    state->enter_time = 0;
    state->trigger_time = 0;
    state->num_neighbors = 0;
    state->triggered_neighbor_idx = 0xFF;
    
    if (config->enabled && !state->config.enabled) {
        ctx->num_active_events++;
    } else if (!config->enabled && state->config.enabled) {
        ctx->num_active_events--;
    }
    
    pthread_mutex_unlock(&ctx->meas_mutex);
    
    printf("Configured measurement event %s (meas_id=%u, hys=%.1fdB, TTT=%ums, thresh1=%d)\n",
           rrc_meas_event_to_string(config->event_id),
           state->config.meas_id,
           config->hysteresis_db * 0.5,
           config->time_to_trigger,
           config->threshold1);
    
    return UESIM_SUCCESS;
}

uesim_error_t rrc_meas_remove_event(ue_context_t* ue_ctx, rrc_meas_event_t event) {
    if (!ue_ctx) return UESIM_ERROR_INVALID_PARAM;
    if (event >= RRC_MEAS_EVENT_MAX) return UESIM_ERROR_INVALID_PARAM;
    
    rrc_meas_context_t* ctx = rrc_meas_get_context(ue_ctx);
    if (!ctx || !ctx->initialized) return UESIM_ERROR_NOT_INITIALIZED;
    
    pthread_mutex_lock(&ctx->meas_mutex);
    
    rrc_meas_event_state_t* state = &ctx->events[event];
    if (state->config.enabled) {
        ctx->num_active_events--;
    }
    memset(state, 0, sizeof(rrc_meas_event_state_t));
    
    pthread_mutex_unlock(&ctx->meas_mutex);
    
    return UESIM_SUCCESS;
}

uesim_error_t rrc_meas_enable_event(ue_context_t* ue_ctx, rrc_meas_event_t event, bool enable) {
    if (!ue_ctx) return UESIM_ERROR_INVALID_PARAM;
    if (event >= RRC_MEAS_EVENT_MAX) return UESIM_ERROR_INVALID_PARAM;
    
    rrc_meas_context_t* ctx = rrc_meas_get_context(ue_ctx);
    if (!ctx || !ctx->initialized) return UESIM_ERROR_NOT_INITIALIZED;
    
    pthread_mutex_lock(&ctx->meas_mutex);
    
    rrc_meas_event_state_t* state = &ctx->events[event];
    if (state->config.enabled != enable) {
        state->config.enabled = enable;
        if (enable) {
            ctx->num_active_events++;
        } else {
            ctx->num_active_events--;
            state->event_entered = false;
            state->event_triggered = false;
        }
    }
    
    pthread_mutex_unlock(&ctx->meas_mutex);
    
    return UESIM_SUCCESS;
}

/* ============== Measurement Execution ============== */

uesim_error_t rrc_meas_perform_measurement(ue_context_t* ue_ctx) {
    if (!ue_ctx) return UESIM_ERROR_INVALID_PARAM;
    
    rrc_meas_context_t* ctx = rrc_meas_get_context(ue_ctx);
    if (!ctx || !ctx->initialized) return UESIM_ERROR_NOT_INITIALIZED;
    
    time_t now = time(NULL);
    
    pthread_mutex_lock(&ctx->meas_mutex);
    
    /* Update serving cell measurement */
    if (ue_ctx->serving_gnb) {
        ctx->serving_meas.meas_id = 0;
        ctx->serving_meas.pci = ue_ctx->serving_gnb->cell_id;
        ctx->serving_meas.cell_id = ue_ctx->serving_gnb->gnb_id;
        ctx->serving_meas.rsrp = ue_ctx->serving_gnb->rsrp;
        ctx->serving_meas.rsrq = ue_ctx->serving_gnb->rsrq;
        ctx->serving_meas.sinr = ue_ctx->serving_gnb->rsrp + 10;  /* Approximate */
        ctx->serving_meas.timestamp = now;
        ctx->serving_meas.is_serving_cell = true;
        ctx->serving_meas.gnb_index = 0;
    }
    
    /* Update neighbor cell measurements */
    ctx->num_neighbor_meas = 0;
    for (int i = 0; i < ue_ctx->num_candidate_gnbs && i < MAX_GNB_CANDIDATES; i++) {
        gnb_context_t* gnb = ue_ctx->candidate_gnbs[i];
        if (gnb && gnb != ue_ctx->serving_gnb) {
            rrc_meas_result_t* result = &ctx->neighbor_meas[ctx->num_neighbor_meas];
            result->meas_id = 0;
            result->pci = gnb->cell_id;
            result->cell_id = gnb->gnb_id;
            result->rsrp = gnb->rsrp;
            result->rsrq = gnb->rsrq;
            result->sinr = gnb->rsrp + 10;
            result->timestamp = now;
            result->is_serving_cell = false;
            result->gnb_index = i;
            ctx->num_neighbor_meas++;
        }
    }
    
    ctx->last_meas_time = now;
    
    pthread_mutex_unlock(&ctx->meas_mutex);
    
    return UESIM_SUCCESS;
}

uesim_error_t rrc_meas_update_serving_result(ue_context_t* ue_ctx,
                                              const rrc_meas_result_t* result) {
    if (!ue_ctx || !result) return UESIM_ERROR_INVALID_PARAM;
    
    rrc_meas_context_t* ctx = rrc_meas_get_context(ue_ctx);
    if (!ctx || !ctx->initialized) return UESIM_ERROR_NOT_INITIALIZED;
    
    pthread_mutex_lock(&ctx->meas_mutex);
    memcpy(&ctx->serving_meas, result, sizeof(rrc_meas_result_t));
    pthread_mutex_unlock(&ctx->meas_mutex);
    
    return UESIM_SUCCESS;
}

uesim_error_t rrc_meas_update_neighbor_result(ue_context_t* ue_ctx,
                                               const rrc_meas_result_t* result) {
    if (!ue_ctx || !result) return UESIM_ERROR_INVALID_PARAM;
    
    rrc_meas_context_t* ctx = rrc_meas_get_context(ue_ctx);
    if (!ctx || !ctx->initialized) return UESIM_ERROR_NOT_INITIALIZED;
    
    pthread_mutex_lock(&ctx->meas_mutex);
    
    /* Find or add neighbor result */
    bool found = false;
    for (int i = 0; i < ctx->num_neighbor_meas; i++) {
        if (ctx->neighbor_meas[i].pci == result->pci) {
            memcpy(&ctx->neighbor_meas[i], result, sizeof(rrc_meas_result_t));
            found = true;
            break;
        }
    }
    
    if (!found && ctx->num_neighbor_meas < MAX_GNB_CANDIDATES) {
        memcpy(&ctx->neighbor_meas[ctx->num_neighbor_meas], result, sizeof(rrc_meas_result_t));
        ctx->num_neighbor_meas++;
    }
    
    pthread_mutex_unlock(&ctx->meas_mutex);
    
    return UESIM_SUCCESS;
}

/* ============== Event Evaluation ============== */

/*
 * Event A3: Neighbor becomes better than serving cell by offset
 * 
 * Entry condition: Mn + Ofn + Ocn - Hys > Mp + Ofp + Ocp + Off
 * Leave condition: Mn + Ofn + Ocn + Hys < Mp + Ofp + Ocp + Off
 * 
 * Where:
 *   Mn  = Measured neighbor cell quantity
 *   Mp  = Measured serving cell quantity
 *   Ofn = Frequency offset for neighbor (target_freq_offset)
 *   Ocn = Cell offset for neighbor (cell_offset)
 *   Ofp = Frequency offset for serving (freq_offset)
 *   Ocp = Cell offset for serving (0 for serving cell)
 *   Hys = Hysteresis
 *   Off = Offset (threshold1)
 */
bool rrc_meas_eval_a3(rrc_meas_event_state_t* state) {
    if (!state || !state->config.enabled) return false;
    
    int32_t hysteresis = state->config.hysteresis_db * 0.5;  /* Convert to dB */
    int32_t offset = state->config.threshold1;
    
    int32_t mp = rrc_meas_get_quantity_value(&state->serving_result, state->config.quantity);
    int32_t ofp = state->config.freq_offset;
    
    /* Check each neighbor */
    for (int i = 0; i < state->num_neighbors; i++) {
        int32_t mn = rrc_meas_get_quantity_value(&state->neighbor_results[i], state->config.quantity);
        int32_t ofn = state->config.target_freq_offset;
        int32_t ocn = state->config.cell_offset[i];
        
        /* Entry condition: Mn + Ofn + Ocn - Hys > Mp + Ofp + Off */
        int32_t left = mn + ofn + ocn - hysteresis;
        int32_t right = mp + ofp + offset;
        
        if (left > right) {
            state->triggered_neighbor_idx = i;
            return true;  /* Entry condition met */
        }
    }
    
    return false;
}

/*
 * Event A4: Neighbor becomes better than threshold
 * 
 * Entry condition: Mn + Ofn + Ocn - Hys > Thresh
 * Leave condition: Mn + Ofn + Ocn + Hys < Thresh
 * 
 * Where:
 *   Thresh = threshold1
 */
bool rrc_meas_eval_a4(rrc_meas_event_state_t* state) {
    if (!state || !state->config.enabled) return false;
    
    int32_t hysteresis = state->config.hysteresis_db * 0.5;
    int32_t threshold = state->config.threshold1;
    
    /* Check each neighbor */
    for (int i = 0; i < state->num_neighbors; i++) {
        int32_t mn = rrc_meas_get_quantity_value(&state->neighbor_results[i], state->config.quantity);
        int32_t ofn = state->config.target_freq_offset;
        int32_t ocn = state->config.cell_offset[i];
        
        /* Entry condition: Mn + Ofn + Ocn - Hys > Thresh */
        int32_t left = mn + ofn + ocn - hysteresis;
        
        if (left > threshold) {
            state->triggered_neighbor_idx = i;
            return true;
        }
    }
    
    return false;
}

/*
 * Event A5: Serving becomes worse than threshold1 AND neighbor better than threshold2
 * 
 * Entry condition 1: Mp + Hys < Thresh1
 * Entry condition 2: Mn + Ofn + Ocn - Hys > Thresh2
 * 
 * Leave condition 1: Mp - Hys > Thresh1
 * Leave condition 2: Mn + Ofn + Ocn + Hys < Thresh2
 */
bool rrc_meas_eval_a5(rrc_meas_event_state_t* state) {
    if (!state || !state->config.enabled) return false;
    
    int32_t hysteresis = state->config.hysteresis_db * 0.5;
    int32_t thresh1 = state->config.threshold1;  /* Serving threshold */
    int32_t thresh2 = state->config.threshold2;  /* Neighbor threshold */
    
    int32_t mp = rrc_meas_get_quantity_value(&state->serving_result, state->config.quantity);
    
    /* Check condition 1: Serving worse than threshold1 */
    if (mp + hysteresis >= thresh1) {
        return false;  /* Serving not bad enough */
    }
    
    /* Check each neighbor for condition 2 */
    for (int i = 0; i < state->num_neighbors; i++) {
        int32_t mn = rrc_meas_get_quantity_value(&state->neighbor_results[i], state->config.quantity);
        int32_t ofn = state->config.target_freq_offset;
        int32_t ocn = state->config.cell_offset[i];
        
        /* Entry condition 2: Mn + Ofn + Ocn - Hys > Thresh2 */
        int32_t left = mn + ofn + ocn - hysteresis;
        
        if (left > thresh2) {
            state->triggered_neighbor_idx = i;
            return true;  /* Both conditions met */
        }
    }
    
    return false;
}

/* Main event evaluation function */
uesim_error_t rrc_meas_evaluate_events(ue_context_t* ue_ctx,
                                        rrc_meas_event_t* triggered_event,
                                        uint8_t* triggered_neighbor_idx) {
    if (!ue_ctx || !triggered_event || !triggered_neighbor_idx) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    rrc_meas_context_t* ctx = rrc_meas_get_context(ue_ctx);
    if (!ctx || !ctx->initialized) return UESIM_ERROR_NOT_INITIALIZED;
    
    *triggered_event = RRC_MEAS_EVENT_MAX;
    *triggered_neighbor_idx = 0xFF;
    
    /* Perform fresh measurements */
    rrc_meas_perform_measurement(ue_ctx);
    
    pthread_mutex_lock(&ctx->meas_mutex);
    
    time_t now = time(NULL);
    
    /* Update event states with current measurements */
    for (int e = 0; e < RRC_MEAS_EVENT_MAX; e++) {
        rrc_meas_event_state_t* state = &ctx->events[e];
        if (!state->config.enabled) continue;
        
        /* Copy current measurements to event state */
        memcpy(&state->serving_result, &ctx->serving_meas, sizeof(rrc_meas_result_t));
        state->num_neighbors = ctx->num_neighbor_meas;
        memcpy(state->neighbor_results, ctx->neighbor_meas, 
               ctx->num_neighbor_meas * sizeof(rrc_meas_result_t));
    }
    
    /* Evaluate each event type */
    for (int e = 0; e < RRC_MEAS_EVENT_MAX; e++) {
        rrc_meas_event_state_t* state = &ctx->events[e];
        if (!state->config.enabled) continue;
        
        bool entry_met = false;
        
        switch (e) {
            case RRC_MEAS_EVENT_A3:
                entry_met = rrc_meas_eval_a3(state);
                break;
            case RRC_MEAS_EVENT_A4:
                entry_met = rrc_meas_eval_a4(state);
                break;
            case RRC_MEAS_EVENT_A5:
                entry_met = rrc_meas_eval_a5(state);
                break;
            default:
                continue;
        }
        
        if (entry_met) {
            if (!state->event_entered) {
                /* Entry condition just met */
                state->event_entered = true;
                state->enter_time = now;
                state->event_triggered = false;
                printf("Event %s: Entry condition met, starting TTT (%u ms)\n",
                       rrc_meas_event_to_string(e), state->config.time_to_trigger);
            } else {
                /* Check if TTT has elapsed */
                uint32_t elapsed_ms = (uint32_t)(now - state->enter_time) * 1000;
                if (elapsed_ms >= state->config.time_to_trigger && !state->event_triggered) {
                    /* Event triggered! */
                    state->event_triggered = true;
                    state->trigger_time = now;
                    
                    *triggered_event = (rrc_meas_event_t)e;
                    *triggered_neighbor_idx = state->triggered_neighbor_idx;
                    
                    printf("Event %s: TRIGGERED after TTT! Neighbor idx=%u, RSRP=%d dBm\n",
                           rrc_meas_event_to_string(e),
                           state->triggered_neighbor_idx,
                           state->neighbor_results[state->triggered_neighbor_idx].rsrp);
                    
                    /* Reset after trigger */
                    state->event_entered = false;
                    state->event_triggered = false;
                    
                    pthread_mutex_unlock(&ctx->meas_mutex);
                    return UESIM_SUCCESS;
                }
            }
        } else {
            /* Entry condition not met */
            if (state->event_entered) {
                printf("Event %s: Entry condition cleared before TTT\n",
                       rrc_meas_event_to_string(e));
            }
            state->event_entered = false;
            state->event_triggered = false;
        }
    }
    
    pthread_mutex_unlock(&ctx->meas_mutex);
    
    return UESIM_SUCCESS;
}

/* ============== Getters ============== */

const rrc_meas_event_state_t* rrc_meas_get_event_state(ue_context_t* ue_ctx,
                                                        rrc_meas_event_t event) {
    if (!ue_ctx || event >= RRC_MEAS_EVENT_MAX) return NULL;
    
    rrc_meas_context_t* ctx = rrc_meas_get_context(ue_ctx);
    if (!ctx || !ctx->initialized) return NULL;
    
    return &ctx->events[event];
}

/* Temporary global storage for measurement contexts (one per UE) */
static rrc_meas_context_t* g_meas_contexts[MAX_UE_INSTANCES] = {NULL};

rrc_meas_context_t* rrc_meas_get_context(ue_context_t* ue_ctx) {
    if (!ue_ctx || ue_ctx->ue_id >= MAX_UE_INSTANCES) return NULL;
    
    /* Initialize if not exists */
    if (!g_meas_contexts[ue_ctx->ue_id]) {
        g_meas_contexts[ue_ctx->ue_id] = (rrc_meas_context_t*)uesim_calloc(1, sizeof(rrc_meas_context_t));
        if (g_meas_contexts[ue_ctx->ue_id]) {
            g_meas_contexts[ue_ctx->ue_id]->initialized = true;
            g_meas_contexts[ue_ctx->ue_id]->meas_interval_ms = 200;
            pthread_mutex_init(&g_meas_contexts[ue_ctx->ue_id]->meas_mutex, NULL);
        }
    }
    
    return g_meas_contexts[ue_ctx->ue_id];
}