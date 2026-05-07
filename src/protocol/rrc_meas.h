/*
 * 5G UE Simulation Application
 * RRC Measurement Event System (3GPP TS 38.331)
 */

#ifndef RRC_MEAS_H
#define RRC_MEAS_H

#include "../uesim.h"

/* Measurement Event Types (3GPP TS 38.331 Section 5.5.4) */
typedef enum {
    RRC_MEAS_EVENT_A1 = 0,    /* Serving becomes better than threshold */
    RRC_MEAS_EVENT_A2,        /* Serving becomes worse than threshold */
    RRC_MEAS_EVENT_A3,        /* Neighbor becomes offset better than serving */
    RRC_MEAS_EVENT_A4,        /* Neighbor becomes better than threshold */
    RRC_MEAS_EVENT_A5,        /* Serving worse than threshold1 AND neighbor better than threshold2 */
    RRC_MEAS_EVENT_A6,        /* Neighbor becomes offset better than SCell */
    RRC_MEAS_EVENT_B1,        /* Inter-RAT neighbor better than threshold */
    RRC_MEAS_EVENT_B2,        /* Inter-RAT serving worse + neighbor better */
    RRC_MEAS_EVENT_MAX
} rrc_meas_event_t;

/* Measurement Trigger Quantities */
typedef enum {
    RRC_QUANTITY_RSRP = 0,    /* Reference Signal Received Power (dBm) */
    RRC_QUANTITY_RSRQ,        /* Reference Signal Received Quality (dB) */
    RRC_QUANTITY_SINR         /* Signal to Interference plus Noise Ratio (dB) */
} rrc_quantity_t;

/* Time-to-Trigger values (ms) - 3GPP TS 38.331 */
typedef enum {
    RRC_TTT_0 = 0,
    RRC_TTT_40 = 40,
    RRC_TTT_64 = 64,
    RRC_TTT_128 = 128,
    RRC_TTT_256 = 256,
    RRC_TTT_512 = 512,
    RRC_TTT_1024 = 1024,
    RRC_TTT_640 = 640,
    RRC_TTT_1280 = 1280,
    RRC_TTT_2560 = 2560,
    RRC_TTT_5120 = 5120,
    RRC_TTT_10240 = 10240
} rrc_ttt_t;

/* Measurement Report Configuration */
typedef enum {
    RRC_REPORT_TRIGGER_EVENT = 0,    /* Event-triggered */
    RRC_REPORT_TRIGGER_PERIODIC,     /* Periodic reporting */
    RRC_REPORT_TRIGGER_NONE          /* No reporting */
} rrc_report_trigger_t;

/* Measurement Event Configuration */
typedef struct {
    rrc_meas_event_t event_id;
    uint8_t meas_id;
    bool enabled;
    
    /* Hysteresis in dB (0.5 dB steps, stored as value * 2) */
    uint8_t hysteresis_db;
    
    /* Time-to-trigger in ms */
    uint32_t time_to_trigger;
    
    /* Event-specific thresholds in dBm */
    int32_t threshold1;      /* A3: offset, A4: threshold, A5: serving threshold */
    int32_t threshold2;      /* A5: neighbor threshold */
    
    /* Quantity to measure */
    rrc_quantity_t quantity;
    
    /* Frequency info for inter-frequency measurements */
    uint32_t carrier_freq;
    uint32_t target_carrier_freq;
    bool is_inter_freq;
    
    /* Cell-specific offsets (dB) */
    int8_t cell_offset[MAX_GNB_CANDIDATES];
    
    /* Frequency-specific offsets (dB) */
    int8_t freq_offset;
    int8_t target_freq_offset;
    
    /* Report configuration */
    rrc_report_trigger_t report_trigger;
    uint32_t report_interval_ms;
    uint8_t max_report_cells;
} rrc_meas_config_t;

/* Measurement Result */
typedef struct {
    uint8_t meas_id;
    uint16_t pci;            /* Physical Cell ID */
    uint32_t cell_id;        /* Global Cell ID */
    uint32_t arfcn;          /* NR-ARFCN (frequency) */
    int32_t rsrp;            /* dBm, typical range: -140 to -44 */
    int32_t rsrq;            /* dB, typical range: -20 to -3 */
    int32_t sinr;            /* dB */
    time_t timestamp;
    bool is_serving_cell;
    uint8_t gnb_index;       /* Index in UE's gNB list */
} rrc_meas_result_t;

/* Event Evaluation State */
typedef struct {
    rrc_meas_config_t config;
    bool event_entered;      /* Entry condition met */
    bool event_triggered;    /* Event triggered after TTT */
    time_t enter_time;       /* When entry condition started */
    time_t trigger_time;      /* When event was triggered */
    rrc_meas_result_t serving_result;
    rrc_meas_result_t neighbor_results[MAX_GNB_CANDIDATES];
    uint8_t num_neighbors;
    uint8_t triggered_neighbor_idx;  /* Index of neighbor that triggered */
} rrc_meas_event_state_t;

/* Measurement Context (per UE) */
typedef struct {
    bool initialized;
    uint8_t next_meas_id;
    
    /* Event configurations and states */
    rrc_meas_event_state_t events[RRC_MEAS_EVENT_MAX];
    uint8_t num_active_events;
    
    /* Latest measurement results */
    rrc_meas_result_t serving_meas;
    rrc_meas_result_t neighbor_meas[MAX_GNB_CANDIDATES];
    uint8_t num_neighbor_meas;
    
    /* Measurement timing */
    uint32_t meas_interval_ms;
    time_t last_meas_time;
    
    /* Report pending */
    bool report_pending;
    rrc_meas_event_t pending_event;
    
    pthread_mutex_t meas_mutex;
} rrc_meas_context_t;

/* ============== API Functions ============== */

/* Initialization/Cleanup */
uesim_error_t rrc_meas_init(ue_context_t* ue_ctx);
void rrc_meas_cleanup(ue_context_t* ue_ctx);

/* Configuration */
uesim_error_t rrc_meas_configure_event(ue_context_t* ue_ctx, 
                                        const rrc_meas_config_t* config);
uesim_error_t rrc_meas_remove_event(ue_context_t* ue_ctx, rrc_meas_event_t event);
uesim_error_t rrc_meas_enable_event(ue_context_t* ue_ctx, rrc_meas_event_t event, bool enable);

/* Measurement Execution */
uesim_error_t rrc_meas_perform_measurement(ue_context_t* ue_ctx);
uesim_error_t rrc_meas_update_serving_result(ue_context_t* ue_ctx,
                                              const rrc_meas_result_t* result);
uesim_error_t rrc_meas_update_neighbor_result(ue_context_t* ue_ctx,
                                               const rrc_meas_result_t* result);

/* Event Evaluation */
uesim_error_t rrc_meas_evaluate_events(ue_context_t* ue_ctx,
                                        rrc_meas_event_t* triggered_event,
                                        uint8_t* triggered_neighbor_idx);

/* Individual event evaluation (internal use) */
bool rrc_meas_eval_a3(rrc_meas_event_state_t* state);
bool rrc_meas_eval_a4(rrc_meas_event_state_t* state);
bool rrc_meas_eval_a5(rrc_meas_event_state_t* state);

/* Get event state */
const rrc_meas_event_state_t* rrc_meas_get_event_state(ue_context_t* ue_ctx,
                                                        rrc_meas_event_t event);

/* Get measurement context */
rrc_meas_context_t* rrc_meas_get_context(ue_context_t* ue_ctx);

/* Utility functions */
const char* rrc_meas_event_to_string(rrc_meas_event_t event);
const char* rrc_quantity_to_string(rrc_quantity_t qty);
int32_t rrc_meas_get_quantity_value(const rrc_meas_result_t* result, rrc_quantity_t qty);

/* Default configurations */
rrc_meas_config_t rrc_meas_get_default_a3_config(void);
rrc_meas_config_t rrc_meas_get_default_a4_config(void);
rrc_meas_config_t rrc_meas_get_default_a5_config(void);

#endif /* RRC_MEAS_H */