/*
 * 5G UE Simulation Application
 * RRC (Radio Resource Control) protocol header
 */

#ifndef RRC_H
#define RRC_H

#include "../uesim.h"

// RRC message types
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
    RRC_MESSAGE_TYPE_MAX
} rrc_message_type_t;

// RRC message structure
typedef struct {
    rrc_message_type_t message_type;
    uint32_t message_id;
    uint32_t transaction_id;
    size_t data_length;
    void* data;
} rrc_message_t;

// RRC procedure context
typedef struct {
    rrc_procedure_t procedure_type;
    uint32_t transaction_id;
    time_t start_time;
    atomic_bool completed;
    atomic_bool success;
    void* procedure_data;
} rrc_procedure_context_t;

// RRC state context
typedef struct {
    rrc_state_t current_state;
    rrc_state_t previous_state;
    time_t state_change_time;
    rrc_procedure_context_t* active_procedure;
    pthread_mutex_t state_mutex;
    pthread_cond_t state_cond;
} rrc_state_context_t;

// Function prototypes
uesim_error_t rrc_init(ue_context_t* ue_ctx);
void rrc_cleanup(ue_context_t* ue_ctx);

// State management
uesim_error_t rrc_change_state(ue_context_t* ue_ctx, rrc_state_t new_state);
rrc_state_t rrc_get_current_state(ue_context_t* ue_ctx);

// Message handling
uesim_error_t rrc_send_message(ue_context_t* ue_ctx, rrc_message_t* message);
uesim_error_t rrc_receive_message(ue_context_t* ue_ctx, rrc_message_t* message);

// Procedure execution
uesim_error_t rrc_execute_procedure(ue_context_t* ue_ctx, rrc_procedure_t procedure);
uesim_error_t rrc_handle_procedure_response(ue_context_t* ue_ctx, rrc_message_t* response);

// Specific procedure implementations
uesim_error_t rrc_execute_registration(ue_context_t* ue_ctx);
uesim_error_t rrc_execute_establishment(ue_context_t* ue_ctx);
uesim_error_t rrc_execute_reestablishment(ue_context_t* ue_ctx);
uesim_error_t rrc_execute_handover(ue_context_t* ue_ctx);

// Message encoding/decoding
uesim_error_t rrc_encode_message(rrc_message_t* message, void** encoded_data, size_t* encoded_length);
uesim_error_t rrc_decode_message(const void* encoded_data, size_t encoded_length, rrc_message_t* message);

#endif // RRC_H