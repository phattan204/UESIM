/*
 * 5G UE Simulation Application
 * RRC (Radio Resource Control) protocol header
 */

#ifndef RRC_H
#define RRC_H

#include "../uesim.h"

/* RRC Timer Constants (3GPP TS 38.331) */
#define RRC_T300_MS         1000    /* Connection establishment timer */
#define RRC_T301_MS         1000    /* Connection re-establishment timer */
#define RRC_T302_MS         4000    /* Registration timer */
#define RRC_T304_MS         2000    /* Handover execution timer */
#define RRC_T310_MS         1000    /* Radio link failure detection */
#define RRC_T311_MS         3000    /* RRC connection re-establishment */

/* Retry Constants (3GPP TS 38.331) */
#define RRC_N300_MAX        3       /* Max RRCSetupRequest transmissions */
#define RRC_N311_MAX        3       /* Max re-establishment attempts */

/* RRC message types */
typedef enum {
    RRC_MESSAGE_TYPE_SETUP_REQUEST = 0,
    RRC_MESSAGE_TYPE_SETUP = 1,
    RRC_MESSAGE_TYPE_SETUP_COMPLETE = 2,
    RRC_MESSAGE_TYPE_REESTABLISHMENT_REQUEST = 3,
    RRC_MESSAGE_TYPE_REESTABLISHMENT = 4,
    RRC_MESSAGE_TYPE_REESTABLISHMENT_COMPLETE = 5,
    RRC_MESSAGE_TYPE_RECONFIGURATION = 6,
    RRC_MESSAGE_TYPE_RECONFIGURATION_COMPLETE = 7,
    RRC_MESSAGE_TYPE_MEASUREMENT_REPORT = 8,
    RRC_MESSAGE_TYPE_HANDOVER_PREPARATION = 9,
    RRC_MESSAGE_TYPE_HANDOVER_COMMAND = 10,
    RRC_MESSAGE_TYPE_HANDOVER_CONFIRMATION = 11,
    RRC_MESSAGE_TYPE_UE_CAPABILITY_ENQUIRY = 12,
    RRC_MESSAGE_TYPE_UE_CAPABILITY_INFORMATION = 13,
    RRC_MESSAGE_TYPE_CONNECTION_RELEASE = 14,
    RRC_MESSAGE_TYPE_SECURITY_MODE_COMMAND = 15,
    RRC_MESSAGE_TYPE_SECURITY_MODE_COMPLETE = 16,
    RRC_MESSAGE_TYPE_MAX
} rrc_message_type_t;

/* RRC procedure status */
typedef enum {
    RRC_PROC_STATUS_IDLE = 0,
    RRC_PROC_STATUS_ONGOING,
    RRC_PROC_STATUS_SUCCESS,
    RRC_PROC_STATUS_TIMEOUT,
    RRC_PROC_STATUS_FAILED,
    RRC_PROC_STATUS_RETRY
} rrc_proc_status_t;

/* RRC error causes */
typedef enum {
    RRC_CAUSE_SUCCESS = 0,
    RRC_CAUSE_TIMEOUT,
    RRC_CAUSE_MAX_RETRIES,
    RRC_CAUSE_NETWORK_FAILURE,
    RRC_CAUSE_RADIO_LINK_FAILURE,
    RRC_CAUSE_HANDOVER_FAILED,
    RRC_CAUSE_RECONFIGURATION_FAILED,
    RRC_CAUSE_SECURITY_FAILED,
    RRC_CAUSE_INVALID_STATE
} rrc_error_cause_t;

/* RRC message structure */
typedef struct {
    rrc_message_type_t message_type;
    uint32_t message_id;
    uint32_t transaction_id;
    size_t data_length;
    void* data;
} rrc_message_t;

/* RRC procedure context */
typedef struct {
    rrc_procedure_t procedure_type;
    uint32_t transaction_id;
    time_t start_time;
    uint32_t retry_count;
    uint32_t timeout_ms;
    rrc_proc_status_t status;
    rrc_error_cause_t error_cause;
#ifdef _WIN32
    volatile LONG completed;
    volatile LONG success;
#else
    atomic_bool completed;
    atomic_bool success;
#endif
    void* procedure_data;
    pthread_mutex_t proc_mutex;
    pthread_cond_t proc_cond;
} rrc_procedure_context_t;

/* RRC state context */
typedef struct {
    rrc_state_t current_state;
    rrc_state_t previous_state;
    time_t state_change_time;
    rrc_procedure_context_t* active_procedure;
    pthread_mutex_t state_mutex;
    pthread_cond_t state_cond;
} rrc_state_context_t;

/* RRC Setup Request data */
typedef struct {
    uint64_t ue_identity;
    uint8_t establishment_cause;
    uint8_t spare_bits[7];
} rrc_setup_request_data_t;

/* RRC Setup response data */
typedef struct {
    uint8_t rrc_transaction_id;
    uint8_t radio_bearer_config[256];
    size_t config_len;
    uint8_t master_cell_group[512];
    size_t cell_group_len;
} rrc_setup_data_t;

/* RRC Setup Complete data */
typedef struct {
    uint8_t rrc_transaction_id;
    uint8_t selected_plmn;
    uint8_t nas_pdu[1024];
    size_t nas_pdu_len;
} rrc_setup_complete_data_t;

/* RRC Reestablishment Request data */
typedef struct {
    uint8_t reestablishment_cause;
    uint16_t pci;
    uint32_t c_rnti;
    uint8_t short_mac_i[2];
} rrc_reest_request_data_t;

/* RRC Reestablishment response data */
typedef struct {
    uint8_t rrc_transaction_id;
    uint8_t radio_bearer_config[256];
    size_t config_len;
} rrc_reest_data_t;

/* RRC Reconfiguration data */
typedef struct {
    uint8_t rrc_transaction_id;
    uint8_t radio_bearer_config[512];
    size_t config_len;
    uint8_t meas_config[256];
    size_t meas_config_len;
    bool has_mobility_config;
    uint8_t mobility_config[128];
    size_t mobility_config_len;
} rrc_reconfig_data_t;

/* RRC Measurement Report data */
typedef struct {
    uint8_t meas_id;
    int32_t rsrp;
    int32_t rsrq;
    uint16_t pci;
    uint32_t cell_id;
} rrc_meas_report_data_t;

/* RRC Handover Command data */
typedef struct {
    uint8_t rrc_transaction_id;
    uint16_t target_pci;
    uint32_t target_cell_id;
    uint8_t new_c_rnti;
    uint8_t radio_bearer_config[512];
    size_t config_len;
} rrc_handover_cmd_data_t;

/* RRC UE Capability data */
typedef struct {
    uint8_t rat_type;
    uint8_t capability_container[1024];
    size_t container_len;
} rrc_ue_cap_data_t;

/* Function prototypes */
uesim_error_t rrc_init(ue_context_t* ue_ctx);
void rrc_cleanup(ue_context_t* ue_ctx);

/* State management */
uesim_error_t rrc_change_state(ue_context_t* ue_ctx, rrc_state_t new_state);
rrc_state_t rrc_get_current_state(ue_context_t* ue_ctx);
bool rrc_is_valid_state_transition(rrc_state_t from, rrc_state_t to);

/* Message handling */
uesim_error_t rrc_send_message(ue_context_t* ue_ctx, rrc_message_t* message);
uesim_error_t rrc_receive_message(ue_context_t* ue_ctx, rrc_message_t* message);
uesim_error_t rrc_process_incoming_message(ue_context_t* ue_ctx, const void* data, size_t len);

/* Procedure execution */
uesim_error_t rrc_execute_procedure(ue_context_t* ue_ctx, rrc_procedure_t procedure);
uesim_error_t rrc_handle_procedure_response(ue_context_t* ue_ctx, rrc_message_t* response);

/* Procedure management */
uesim_error_t rrc_start_procedure(ue_context_t* ue_ctx, rrc_procedure_t procedure, rrc_procedure_context_t** ctx);
uesim_error_t rrc_wait_procedure_complete(ue_context_t* ue_ctx, rrc_procedure_context_t* ctx, uint32_t timeout_ms);
uesim_error_t rrc_abort_procedure(ue_context_t* ue_ctx, rrc_procedure_context_t* ctx);
void rrc_free_procedure_context(rrc_procedure_context_t* ctx);

/* Specific procedure implementations */
uesim_error_t rrc_execute_registration(ue_context_t* ue_ctx);
uesim_error_t rrc_execute_establishment(ue_context_t* ue_ctx);
uesim_error_t rrc_execute_reestablishment(ue_context_t* ue_ctx);
uesim_error_t rrc_execute_handover(ue_context_t* ue_ctx);
uesim_error_t rrc_execute_reconfiguration(ue_context_t* ue_ctx);
uesim_error_t rrc_execute_measurement_report(ue_context_t* ue_ctx);
uesim_error_t rrc_execute_capability_transfer(ue_context_t* ue_ctx);

/* Response handlers */
uesim_error_t rrc_handle_setup_response(ue_context_t* ue_ctx, rrc_message_t* response);
uesim_error_t rrc_handle_reestablishment_response(ue_context_t* ue_ctx, rrc_message_t* response);
uesim_error_t rrc_handle_reconfiguration_response(ue_context_t* ue_ctx, rrc_message_t* response);
uesim_error_t rrc_handle_handover_command(ue_context_t* ue_ctx, rrc_message_t* response);
uesim_error_t rrc_handle_capability_enquiry(ue_context_t* ue_ctx, rrc_message_t* response);
uesim_error_t rrc_handle_connection_release(ue_context_t* ue_ctx, rrc_message_t* response);

/* Message encoding/decoding */
uesim_error_t rrc_encode_message(rrc_message_t* message, void** encoded_data, size_t* encoded_length);
uesim_error_t rrc_decode_message(const void* encoded_data, size_t encoded_length, rrc_message_t* message);

/* Error recovery */
uesim_error_t rrc_retry_procedure(ue_context_t* ue_ctx, rrc_procedure_context_t* ctx);
uesim_error_t rrc_handle_procedure_failure(ue_context_t* ue_ctx, rrc_procedure_context_t* ctx, rrc_error_cause_t cause);
uesim_error_t rrc_fallback_to_idle(ue_context_t* ue_ctx, rrc_error_cause_t cause);

/* Utility functions */
const char* rrc_state_to_string(rrc_state_t state);
const char* rrc_message_type_to_string(rrc_message_type_t type);
const char* rrc_procedure_to_string(rrc_procedure_t proc);
const char* rrc_error_cause_to_string(rrc_error_cause_t cause);

#endif /* RRC_H */