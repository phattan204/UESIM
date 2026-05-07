/*
 * 5G UE Simulation Application
 * RLF (Radio Link Failure) Detection and Recovery Module
 * 
 * Implements 3GPP TS 38.331 RRC Radio Link Failure detection and recovery
 */

#ifndef RLF_RECOVERY_H
#define RLF_RECOVERY_H

#include "../uesim.h"

/* RLF Detection Constants (3GPP TS 38.331) */
#define RLF_T310_DEFAULT_MS     1000    /* RLF detection timer */
#define RLF_T311_DEFAULT_MS     3000    /* Re-establishment timer */
#define RLF_T301_DEFAULT_MS     1000    /* Re-establishment wait timer */
#define RLF_N310_MAX            1       /* Out-of-sync indications to start T310 */
#define RLF_N311_MAX            2       /* In-sync indications to stop T310 */

/* RLF Detection States */
typedef enum {
    RLF_STATE_NORMAL = 0,           /* Normal operation */
    RLF_STATE_OUT_OF_SYNC,          /* Receiving out-of-sync indications */
    RLF_STATE_T310_RUNNING,         /* T310 timer running */
    RLF_STATE_IN_SYNC_RECOVERY,     /* Receiving in-sync indications */
    RLF_STATE_DETECTED,             /* RLF detected */
    RLF_STATE_REESTABLISHING,       /* Re-establishment in progress */
    RLF_STATE_RECOVERED,            /* Recovery successful */
    RLF_STATE_FAILED,               /* Recovery failed */
    RLF_STATE_MAX
} rlf_state_t;

/* RLF Trigger Causes (3GPP TS 38.331 Section 5.3.10) */
typedef enum {
    RLF_CAUSE_NONE = 0,
    RLF_CAUSE_T310_EXPIRY,          /* T310 timer expired */
    RLF_CAUSE_RANDOM_ACCESS_FAILURE,/* Random access problem */
    RLF_CAUSE_RLC_MAX_RETX,         /* RLC indicated max retransmissions */
    RLF_CAUSE_HANDOVER_FAILURE,     /* Handover preparation/execution failed */
    RLF_CAUSE_T304_EXPIRY,          /* Handover timer expired */
    RLF_CAUSE_CONNECTION_RELEASE,   /* RRC Connection Release received */
    RLF_CAUSE_PHYSICAL_LAYER_ERROR, /* Physical layer problem indication */
    RLF_CAUSE_MAX
} rlf_cause_t;

/* RLF Re-establishment Result */
typedef enum {
    RLF_REEST_SUCCESS = 0,
    RLF_REEST_T301_EXPIRY,          /* T301 expired */
    RLF_REEST_T311_EXPIRY,          /* T311 expired */
    RLF_REEST_CELL_RESELECTION,     /* Cell reselection triggered */
    RLF_REEST_CONNECTION_RELEASE,   /* Connection released during re-establishment */
    RLF_REEST_NO_SUITABLE_CELL,     /* No suitable cell found */
    RLF_REEST_NETWORK_REJECT,       /* Network rejected re-establishment */
    RLF_REEST_MAX
} rlf_reest_result_t;

/* RLF State Context */
typedef struct {
    /* Current state */
    rlf_state_t current_state;
    rlf_cause_t detection_cause;
    time_t detection_time;
    
    /* Out-of-sync/In-sync counters */
    uint8_t out_of_sync_count;      /* N310 counter */
    uint8_t in_sync_count;          /* N311 counter */
    
    /* Timer states */
    uint32_t t310_start_time;       /* T310 start time (ms) */
    uint32_t t311_start_time;       /* T311 start time (ms) */
    uint32_t t301_start_time;       /* T301 start time (ms) */
    bool t310_running;
    bool t311_running;
    bool t301_running;
    
    /* Timer values (configurable) */
    uint32_t t310_value_ms;
    uint32_t t311_value_ms;
    uint32_t t301_value_ms;
    uint8_t n310_threshold;
    uint8_t n311_threshold;
    
    /* Recovery context */
    uint8_t reest_attempt_count;
    uint8_t max_reest_attempts;
    rlf_reest_result_t last_reest_result;
    
    /* State backup for recovery */
    rrc_state_t pre_rlf_state;
    uint32_t pre_rlf_pci;
    uint32_t pre_rlf_cell_id;
    uint16_t pre_rlf_c_rnti;
    
    /* Statistics */
    uint64_t total_rlf_count;
    uint64_t successful_recovery_count;
    uint64_t failed_recovery_count;
    uint64_t total_out_of_sync_indications;
    uint64_t total_in_sync_indications;
    
    /* Thread safety */
    pthread_mutex_t rlf_mutex;
    pthread_cond_t rlf_cond;
} rlf_context_t;

/* Sync Indication Types */
typedef enum {
    SYNC_INDICATION_OUT_OF_SYNC = 0,
    SYNC_INDICATION_IN_SYNC = 1
} sync_indication_t;

/* Function Prototypes */

/* Initialization and Cleanup */
uesim_error_t rlf_init(ue_context_t* ue_ctx);
void rlf_cleanup(ue_context_t* ue_ctx);
uesim_error_t rlf_create_context(rlf_context_t** ctx);
void rlf_destroy_context(rlf_context_t* ctx);

/* Configuration */
uesim_error_t rlf_configure(rlf_context_t* ctx, uint32_t t310_ms, uint32_t t311_ms,
                           uint32_t t301_ms, uint8_t n310, uint8_t n311);

/* Sync Indication Handling (from PHY/MAC) */
uesim_error_t rlf_handle_sync_indication(ue_context_t* ue_ctx, sync_indication_t indication);

/* Timer Management */
uesim_error_t rlf_start_t310(ue_context_t* ue_ctx);
uesim_error_t rlf_stop_t310(ue_context_t* ue_ctx);
uesim_error_t rlf_handle_t310_expiry(ue_context_t* ue_ctx);
uesim_error_t rlf_start_t311(ue_context_t* ue_ctx);
uesim_error_t rlf_stop_t311(ue_context_t* ue_ctx);
uesim_error_t rlf_handle_t311_expiry(ue_context_t* ue_ctx);
uesim_error_t rlf_start_t301(ue_context_t* ue_ctx);
uesim_error_t rlf_stop_t301(ue_context_t* ue_ctx);
uesim_error_t rlf_handle_t301_expiry(ue_context_t* ue_ctx);

/* RLF Detection */
uesim_error_t rlf_detect_failure(ue_context_t* ue_ctx, rlf_cause_t cause);
uesim_error_t rlf_declare_failure(ue_context_t* ue_ctx);
bool rlf_is_detected(ue_context_t* ue_ctx);

/* Recovery Actions */
uesim_error_t rlf_initiate_recovery(ue_context_t* ue_ctx);
uesim_error_t rlf_start_reestablishment(ue_context_t* ue_ctx);
uesim_error_t rlf_handle_reestablishment_response(ue_context_t* ue_ctx, bool success);
uesim_error_t rlf_complete_recovery(ue_context_t* ue_ctx);
uesim_error_t rlf_abort_recovery(ue_context_t* ue_ctx, rlf_reest_result_t result);

/* State Backup/Restore */
uesim_error_t rlf_backup_state(ue_context_t* ue_ctx);
uesim_error_t rlf_restore_state(ue_context_t* ue_ctx);

/* Alternative Recovery Paths */
uesim_error_t rlf_trigger_cell_reselection(ue_context_t* ue_ctx);
uesim_error_t rlf_go_to_idle(ue_context_t* ue_ctx);

/* State Query */
rlf_state_t rlf_get_state(ue_context_t* ue_ctx);
const char* rlf_state_to_string(rlf_state_t state);
const char* rlf_cause_to_string(rlf_cause_t cause);
const char* rlf_reest_result_to_string(rlf_reest_result_t result);

/* Timer Check (call periodically) */
uesim_error_t rlf_check_timers(ue_context_t* ue_ctx);

/* Statistics */
uesim_error_t rlf_get_stats(ue_context_t* ue_ctx, uint64_t* total_rlf,
                           uint64_t* successful_recovery, uint64_t* failed_recovery);
uesim_error_t rlf_reset_stats(ue_context_t* ue_ctx);

#endif /* RLF_RECOVERY_H */