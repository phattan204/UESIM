/*
 * 5G UE Simulation Application
 * F1AP (F1 Application Protocol) Message Definitions
 * 3GPP TS 38.473
 */

#ifndef F1AP_MESSAGES_H
#define F1AP_MESSAGES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ============== F1AP Constants ============== */

#define F1AP_PORT                   38472
#define F1AP_MAX_MESSAGE_SIZE       65535
#define F1AP_MAX_DRB_ID             32
#define F1AP_MAX_SRB_ID             3
#define F1AP_MAX_CELL_COUNT         512
#define F1AP_MAX_PLMN_COUNT         12
#define F1AP_MAX_TAI_COUNT          16
#define F1AP_MAX_SLICE_COUNT        8
#define F1AP_MAX_QOS_FLOWS          64
#define F1AP_MAX_RRC_CONTAINER_SIZE 65536
#define F1AP_MAX_DRB_COUNT          8

/* ============== F1AP Procedure Codes (TS 38.473 Section 9) ============== */

typedef enum {
    F1AP_PROC_F1_SETUP = 1,
    F1AP_PROC_F1_RESET = 2,
    F1AP_PROC_ERROR_INDICATION = 3,
    F1AP_PROC_F1_REMOVAL = 4,
    F1AP_PROC_GNB_DU_CONFIG_UPDATE = 5,
    F1AP_PROC_GNB_CU_CONFIG_UPDATE = 6,
    F1AP_PROC_UE_CONTEXT_SETUP = 7,
    F1AP_PROC_UE_CONTEXT_RELEASE = 8,
    F1AP_PROC_UE_CONTEXT_MODIFICATION = 9,
    F1AP_PROC_UE_CONTEXT_RELEASE_REQUEST = 10,
    F1AP_PROC_DL_RRC_MESSAGE_TRANSFER = 11,
    F1AP_PROC_UL_RRC_MESSAGE_TRANSFER = 12,
    F1AP_PROC_F1_UES_CONTEXT_MANAGEMENT = 13,
    F1AP_PROC_NOTIFY = 14,
    F1AP_PROC_ACCESS_AND_MOBILITY_INDICATION = 15,
    F1AP_PROC_SYSTEM_INFORMATION_DELIVERY = 16,
    F1AP_PROC_POSITIONING_MEASUREMENT = 17,
    F1AP_PROC_POSITIONING_ASSISTANCE_DATA = 18,
    F1AP_PROC_POSITIONING_INFORMATION = 19,
    F1AP_PROC_POSITIONING_INFORMATION_UPDATE = 20,
    F1AP_PROC_POSITIONING_MEASUREMENT_ABORT = 21,
    F1AP_PROC_POSITIONING_MEASUREMENT_FAILURE = 22,
    F1AP_PROC_POSITIONING_ACTIVATION = 23,
    F1AP_PROC_POSITIONING_DEACTIVATION = 24,
    F1AP_PROC_TRACING = 25,
    F1AP_PROC_DEACTIVATION = 26
} f1ap_procedure_code_t;

/* ============== F1AP Message Types ============== */

typedef enum {
    /* F1 Setup */
    F1AP_MSG_F1_SETUP_REQUEST = 0,
    F1AP_MSG_F1_SETUP_RESPONSE,
    F1AP_MSG_F1_SETUP_FAILURE,
    
    /* F1 Reset */
    F1AP_MSG_F1_RESET_REQUEST,
    F1AP_MSG_F1_RESET_RESPONSE,
    
    /* Error Indication */
    F1AP_MSG_ERROR_INDICATION,
    
    /* F1 Removal */
    F1AP_MSG_F1_REMOVAL_REQUEST,
    F1AP_MSG_F1_REMOVAL_RESPONSE,
    
    /* gNB-DU Configuration Update */
    F1AP_MSG_GNB_DU_CONFIG_UPDATE,
    F1AP_MSG_GNB_DU_CONFIG_UPDATE_ACKNOWLEDGE,
    F1AP_MSG_GNB_DU_CONFIG_UPDATE_FAILURE,
    
    /* gNB-CU Configuration Update */
    F1AP_MSG_GNB_CU_CONFIG_UPDATE,
    F1AP_MSG_GNB_CU_CONFIG_UPDATE_ACKNOWLEDGE,
    F1AP_MSG_GNB_CU_CONFIG_UPDATE_FAILURE,
    
    /* UE Context Setup */
    F1AP_MSG_UE_CONTEXT_SETUP_REQUEST,
    F1AP_MSG_UE_CONTEXT_SETUP_RESPONSE,
    F1AP_MSG_UE_CONTEXT_SETUP_FAILURE,
    
    /* UE Context Release */
    F1AP_MSG_UE_CONTEXT_RELEASE_COMMAND,
    F1AP_MSG_UE_CONTEXT_RELEASE_COMPLETE,
    
    /* UE Context Modification */
    F1AP_MSG_UE_CONTEXT_MODIFICATION_REQUEST,
    F1AP_MSG_UE_CONTEXT_MODIFICATION_RESPONSE,
    F1AP_MSG_UE_CONTEXT_MODIFICATION_FAILURE,
    
    /* UE Context Release Request */
    F1AP_MSG_UE_CONTEXT_RELEASE_REQUEST,
    
    /* DL RRC Message Transfer */
    F1AP_MSG_DL_RRC_MESSAGE_TRANSFER,
    
    /* UL RRC Message Transfer */
    F1AP_MSG_UL_RRC_MESSAGE_TRANSFER,
    
    /* Notify */
    F1AP_MSG_NOTIFY,
    
    /* System Information Delivery */
    F1AP_MSG_SYSTEM_INFORMATION_DELIVERY_COMMAND,
    
    /* Access and Mobility Indication */
    F1AP_MSG_ACCESS_AND_MOBILITY_INDICATION,
    
    F1AP_MSG_MAX
} f1ap_message_type_t;

/* ============== F1AP Cause Types ============== */

typedef enum {
    F1AP_CAUSE_RADIO_NETWORK = 0,
    F1AP_CAUSE_TRANSPORT,
    F1AP_CAUSE_PROTOCOL,
    F1AP_CAUSE_MISC
} f1ap_cause_type_t;

/* Radio Network Cause Values */
typedef enum {
    F1AP_CAUSE_RADIO_UNSPECIFIED = 0,
    F1AP_CAUSE_RADIO_RLC_FAILURE = 1,
    F1AP_CAUSE_RADIO_UNEXPECTED_RLC_MSG = 2,
    F1AP_CAUSE_RADIO_RLC_REESTABLISHMENT_FAILURE = 3,
    F1AP_CAUSE_RADIO_ACTION_REQUIRED = 4,
    F1AP_CAUSE_RADIO_NORMAL_RELEASE = 5,
    F1AP_CAUSE_RADIO_RADIO_CONNECTION_WITH_UE_LOST = 6,
    F1AP_CAUSE_RADIO_FAILURE_IN_RADIO_INTERFACE_PROCEDURE = 7,
    F1AP_CAUSE_RADIO_NO_RADIO_RESOURCES_AVAILABLE = 8,
    F1AP_CAUSE_RADIO_UNSPECIFIED_QOS_REASON = 9,
    F1AP_CAUSE_RADIO_QOS_NOT_GUARANTEED = 10,
    F1AP_CAUSE_RADIO_RLF_DETECTED = 11,
    F1AP_CAUSE_RADIO_DRB_RELEASED = 12,
    F1AP_CAUSE_RADIO_DU_SRB_ESTABLISHMENT_FAILURE = 13,
    F1AP_CAUSE_RADIO_DU_DRB_ESTABLISHMENT_FAILURE = 14,
    F1AP_CAUSE_RADIO_MEASUREMENT_FAILURE = 15,
    F1AP_CAUSE_RADIO_HANDOVER_CANCELLED = 16,
    F1AP_CAUSE_RADIO_INACTIVITY_TIMER_EXPIRED = 17,
    F1AP_CAUSE_RADIO_DU_RESOURCE_COORDINATION_FAILURE = 18
} f1ap_cause_radio_value_t;

/* Transport Cause Values */
typedef enum {
    F1AP_CAUSE_TRANSPORT_TRANSPORT_RESOURCE_UNAVAILABLE = 0,
    F1AP_CAUSE_TRANSPORT_UNSPECIFIED = 1,
    F1AP_CAUSE_TRANSPORT_F1_TRANSPORT_LOGICAL_ERROR = 2,
    F1AP_CAUSE_TRANSPORT_F1_TRANSPORT_PROTOCOL_ERROR = 3,
    F1AP_CAUSE_TRANSPORT_F1_TRANSPORT_BACKOFF_TIMER_EXPIRED = 4
} f1ap_cause_transport_value_t;

/* Protocol Cause Values */
typedef enum {
    F1AP_CAUSE_PROTOCOL_TRANSFER_SYNTAX_ERROR = 0,
    F1AP_CAUSE_PROTOCOL_ABSTRACT_SYNTAX_ERROR_REJECT = 1,
    F1AP_CAUSE_PROTOCOL_ABSTRACT_SYNTAX_ERROR_IGNORE_AND_NOTIFY = 2,
    F1AP_CAUSE_PROTOCOL_MESSAGE_NOT_COMPATIBLE_WITH_RECEIVER_STATE = 3,
    F1AP_CAUSE_PROTOCOL_SEMANTIC_ERROR = 4,
    F1AP_CAUSE_PROTOCOL_ABSTRACT_SYNTAX_ERROR_FALSELY_CONSTRUCTED_MESSAGE = 5,
    F1AP_CAUSE_PROTOCOL_UNSPECIFIED = 6
} f1ap_cause_protocol_value_t;

/* Misc Cause Values */
typedef enum {
    F1AP_CAUSE_MISC_CONTROL_PROCESSING_OVERLOAD = 0,
    F1AP_CAUSE_MISC_NOT_ENOUGH_USER_PLANE_PROCESSING_RESOURCES = 1,
    F1AP_CAUSE_MISC_HARDWARE_FAILURE = 2,
    F1AP_CAUSE_MISC_OM_INTERVENTION = 3,
    F1AP_CAUSE_MISC_UNSPECIFIED = 4
} f1ap_cause_misc_value_t;

/* ============== F1AP Cause Structure ============== */

typedef struct {
    f1ap_cause_type_t cause_type;
    uint8_t cause_value;
} f1ap_cause_t;

/* ============== F1AP Identity Structures ============== */

/* gNB-DU ID */
typedef struct {
    uint32_t gnb_du_id;
    uint8_t gnb_du_name[64];
} f1ap_gnb_du_id_t;

/* gNB-CU ID */
typedef struct {
    uint64_t gnb_cu_id;        /* 22-32 bits */
    uint8_t gnb_cu_name[64];
} f1ap_gnb_cu_id_t;

/* F1AP UE IDs */
typedef struct {
    uint32_t gnb_cu_ue_f1ap_id;  /* 32 bits */
    uint32_t gnb_du_ue_f1ap_id;  /* 32 bits */
} f1ap_ue_ids_t;

/* RAN UE ID (for NGAP correlation) */
typedef struct {
    uint64_t ran_ue_id;          /* 40 bits */
} f1ap_ran_ue_id_t;

/* ============== F1AP Cell Information ============== */

/* PLMN Identity */
typedef struct {
    uint8_t mcc[3];              /* Mobile Country Code */
    uint8_t mnc[3];              /* Mobile Network Code */
    uint8_t mnc_length;          /* 2 or 3 */
} f1ap_plmn_id_t;

/* TAC */
typedef struct {
    uint32_t tac;               /* 24-bit Tracking Area Code */
} f1ap_tac_t;

/* NR Cell Identity */
typedef struct {
    uint64_t nr_cell_id;         /* 36 bits */
} f1ap_nr_cell_id_t;

/* NR PCI */
typedef struct {
    uint16_t pci;                /* 0-1007 */
} f1ap_pci_t;

/* Served Cell Information */
typedef struct {
    f1ap_nr_cell_id_t nr_cell_id;
    f1ap_pci_t pci;
    f1ap_tac_t tac;
    uint8_t num_plmns;
    f1ap_plmn_id_t plmns[F1AP_MAX_PLMN_COUNT];
    uint8_t num_slices;
    uint8_t sst[F1AP_MAX_SLICE_COUNT];
    uint32_t sd[F1AP_MAX_SLICE_COUNT];
    uint16_t ngran_duplex_mode;  /* TDD=1, FDD=2 */
} f1ap_served_cell_info_t;

/* gNB-DU Served Cells List */
typedef struct {
    uint8_t num_cells;
    f1ap_served_cell_info_t cells[F1AP_MAX_CELL_COUNT];
} f1ap_gnb_du_served_cells_t;

/* ============== F1AP DRB/SRB Structures ============== */

/* DRB ID */
typedef struct {
    uint8_t drb_id;              /* 1-32 */
} f1ap_drb_id_t;

/* SRB ID */
typedef struct {
    uint8_t srb_id;              /* 1-3 */
} f1ap_srb_id_t;

/* QoS Flow Identifier */
typedef struct {
    uint8_t qfi;                 /* 0-63 */
} f1ap_qfi_t;

/* DRB Information */
typedef struct {
    uint8_t drb_id;
    uint8_t sdt_mode;            /* Small Data Transmission */
    uint8_t rlc_mode;            /* RLC AM=1, UM=2 */
    uint8_t num_ul_up_tnl_info;
    uint32_t ul_up_tnl_ip[F1AP_MAX_DRB_COUNT];
    uint16_t ul_up_tnl_port[F1AP_MAX_DRB_COUNT];
    uint32_t ul_up_tnl_teid[F1AP_MAX_DRB_COUNT];
    uint8_t num_qos_flows;
    f1ap_qfi_t qos_flows[F1AP_MAX_QOS_FLOWS];
    uint8_t five_qi[F1AP_MAX_QOS_FLOWS];
} f1ap_drb_info_t;

/* SRB Information */
typedef struct {
    uint8_t srb_id;
    uint8_t rlc_mode;
} f1ap_srb_info_t;

/* ============== F1AP RRC Container ============== */

typedef struct {
    uint8_t rrc_container[F1AP_MAX_RRC_CONTAINER_SIZE];
    size_t length;
} f1ap_rrc_container_t;

/* ============== F1AP Message Structures ============== */

/* F1 Setup Request */
typedef struct {
    f1ap_gnb_du_id_t gnb_du_id;
    f1ap_gnb_du_served_cells_t served_cells;
    uint8_t ranac;               /* RAN Area Code */
    uint8_t gnb_du_rrc_version[4];
} f1ap_f1_setup_request_t;

/* F1 Setup Response */
typedef struct {
    f1ap_gnb_cu_id_t gnb_cu_id;
    uint8_t num_cells_to_activate;
    f1ap_served_cell_info_t cells_to_activate[F1AP_MAX_CELL_COUNT];
    uint8_t gnb_cu_rrc_version[4];
    uint32_t transport_layer_address;
} f1ap_f1_setup_response_t;

/* F1 Setup Failure */
typedef struct {
    f1ap_cause_t cause;
    uint32_t time_to_wait;       /* Seconds */
    uint8_t criticality_diagnostics_present;
} f1ap_f1_setup_failure_t;

/* F1 Reset Request */
typedef struct {
    uint8_t reset_type;          /* 0=All, 1=Part of F1 interface */
    f1ap_ue_ids_t ue_ids;        /* Valid if reset_type == 1 */
    uint8_t ue_ids_present;
} f1ap_f1_reset_request_t;

/* F1 Reset Response */
typedef struct {
    uint8_t reset_type;
    f1ap_ue_ids_t ue_ids;
    uint8_t ue_ids_present;
} f1ap_f1_reset_response_t;

/* gNB-DU Configuration Update */
typedef struct {
    f1ap_gnb_du_served_cells_t cells_to_be_activated;
    f1ap_gnb_du_served_cells_t cells_to_be_deactivated;
    f1ap_gnb_du_served_cells_t cells_to_be_modified;
    uint8_t ranac;
} f1ap_gnb_du_config_update_t;

/* gNB-DU Configuration Update Acknowledge */
typedef struct {
    uint8_t num_cells_activated;
    uint8_t num_cells_deactivated;
    uint8_t num_cells_modified;
    uint8_t cells_failed[F1AP_MAX_CELL_COUNT];
    uint8_t num_cells_failed;
} f1ap_gnb_du_config_update_ack_t;

/* UE Context Setup Request */
typedef struct {
    f1ap_ue_ids_t ue_ids;
    f1ap_ran_ue_id_t ran_ue_id;
    f1ap_plmn_id_t plmn;
    
    /* DRB Setup */
    uint8_t num_drbs_to_setup;
    f1ap_drb_info_t drbs_to_setup[F1AP_MAX_DRB_COUNT];
    
    /* SRB Setup */
    uint8_t num_srbs_to_setup;
    f1ap_srb_info_t srbs_to_setup[F1AP_MAX_SRB_ID];
    
    /* UE Aggregate Maximum Bit Rate */
    uint64_t ue_ambr_dl;
    uint64_t ue_ambr_ul;
    
    /* RRC Container */
    f1ap_rrc_container_t rrc_container;
    
    /* Cell to be activated */
    f1ap_nr_cell_id_t nr_cell_id;
    uint16_t serv_cell_idx;
    
    /* Service Slicing */
    uint8_t sst;
    uint32_t sd;
} f1ap_ue_context_setup_request_t;

/* UE Context Setup Response */
typedef struct {
    f1ap_ue_ids_t ue_ids;
    
    /* DRB Setup List */
    uint8_t num_drbs_setup;
    f1ap_drb_info_t drbs_setup[F1AP_MAX_DRB_COUNT];
    
    /* DRB Failed to Setup */
    uint8_t num_drbs_failed;
    uint8_t drbs_failed[F1AP_MAX_DRB_COUNT];
    f1ap_cause_t drb_fail_causes[F1AP_MAX_DRB_COUNT];
    
    /* SRB Setup List */
    uint8_t num_srbs_setup;
    uint8_t srbs_setup[F1AP_MAX_SRB_ID];
    
    /* DU to CU RRC Container */
    f1ap_rrc_container_t du_to_cu_rrc_container;
} f1ap_ue_context_setup_response_t;

/* UE Context Setup Failure */
typedef struct {
    f1ap_ue_ids_t ue_ids;
    f1ap_cause_t cause;
    uint8_t criticality_diagnostics_present;
} f1ap_ue_context_setup_failure_t;

/* UE Context Release Command */
typedef struct {
    f1ap_ue_ids_t ue_ids;
    f1ap_cause_t cause;
    f1ap_rrc_container_t rrc_container;
} f1ap_ue_context_release_command_t;

/* UE Context Release Complete */
typedef struct {
    f1ap_ue_ids_t ue_ids;
    f1ap_rrc_container_t rrc_container;
} f1ap_ue_context_release_complete_t;

/* UE Context Modification Request */
typedef struct {
    f1ap_ue_ids_t ue_ids;
    
    /* DRBs to Modify */
    uint8_t num_drbs_to_modify;
    f1ap_drb_info_t drbs_to_modify[F1AP_MAX_DRB_COUNT];
    
    /* DRBs to Setup */
    uint8_t num_drbs_to_setup;
    f1ap_drb_info_t drbs_to_setup[F1AP_MAX_DRB_COUNT];
    
    /* DRBs to Remove */
    uint8_t num_drbs_to_remove;
    uint8_t drbs_to_remove[F1AP_MAX_DRB_COUNT];
    
    /* SRBs to Setup */
    uint8_t num_srbs_to_setup;
    f1ap_srb_info_t srbs_to_setup[F1AP_MAX_SRB_ID];
    
    /* SRBs to Remove */
    uint8_t num_srbs_to_remove;
    uint8_t srbs_to_remove[F1AP_MAX_SRB_ID];
    
    /* UE AMBR */
    uint64_t ue_ambr_dl;
    uint64_t ue_ambr_ul;
    uint8_t ambr_present;
    
    /* RRC Container */
    f1ap_rrc_container_t rrc_container;
    uint8_t rrc_container_present;
} f1ap_ue_context_modification_request_t;

/* UE Context Modification Response */
typedef struct {
    f1ap_ue_ids_t ue_ids;
    
    uint8_t num_drbs_setup;
    f1ap_drb_info_t drbs_setup[F1AP_MAX_DRB_COUNT];
    
    uint8_t num_drbs_modified;
    f1ap_drb_info_t drbs_modified[F1AP_MAX_DRB_COUNT];
    
    uint8_t num_drbs_failed;
    uint8_t drbs_failed[F1AP_MAX_DRB_COUNT];
    
    uint8_t num_srbs_setup;
    uint8_t srbs_setup[F1AP_MAX_SRB_ID];
    
    f1ap_rrc_container_t du_to_cu_rrc_container;
} f1ap_ue_context_modification_response_t;

/* UE Context Modification Failure */
typedef struct {
    f1ap_ue_ids_t ue_ids;
    f1ap_cause_t cause;
} f1ap_ue_context_modification_failure_t;

/* UE Context Release Request */
typedef struct {
    f1ap_ue_ids_t ue_ids;
    f1ap_cause_t cause;
} f1ap_ue_context_release_request_t;

/* DL RRC Message Transfer */
typedef struct {
    f1ap_ue_ids_t ue_ids;
    uint8_t srb_id;
    f1ap_rrc_container_t rrc_container;
    uint8_t old_gnb_du_ue_f1ap_id_present;
    uint32_t old_gnb_du_ue_f1ap_id;
} f1ap_dl_rrc_message_transfer_t;

/* UL RRC Message Transfer */
typedef struct {
    f1ap_ue_ids_t ue_ids;
    uint8_t srb_id;
    f1ap_rrc_container_t rrc_container;
} f1ap_ul_rrc_message_transfer_t;

/* Notify */
typedef struct {
    f1ap_ue_ids_t ue_ids;
    uint8_t notification_type;   /* 0=Handover Cancel, 1=RRC Connection Resume */
    f1ap_cause_t cause;
} f1ap_notify_t;

/* Error Indication */
typedef struct {
    f1ap_ue_ids_t ue_ids;
    uint8_t ue_ids_present;
    f1ap_cause_t cause;
    uint8_t cause_present;
    uint8_t criticality_diagnostics_present;
} f1ap_error_indication_t;

/* System Information Delivery Command */
typedef struct {
    f1ap_nr_cell_id_t nr_cell_id;
    f1ap_rrc_container_t sib1;
    f1ap_rrc_container_t system_information;
    uint8_t system_information_present;
} f1ap_system_information_delivery_command_t;

/* ============== F1AP Message Union ============== */

typedef struct {
    f1ap_message_type_t message_type;
    f1ap_procedure_code_t procedure_code;
    uint8_t criticality;         /* 0=reject, 1=ignore, 2=notify */
    
    union {
        f1ap_f1_setup_request_t f1_setup_request;
        f1ap_f1_setup_response_t f1_setup_response;
        f1ap_f1_setup_failure_t f1_setup_failure;
        f1ap_f1_reset_request_t f1_reset_request;
        f1ap_f1_reset_response_t f1_reset_response;
        f1ap_error_indication_t error_indication;
        f1ap_gnb_du_config_update_t gnb_du_config_update;
        f1ap_gnb_du_config_update_ack_t gnb_du_config_update_ack;
        f1ap_ue_context_setup_request_t ue_context_setup_request;
        f1ap_ue_context_setup_response_t ue_context_setup_response;
        f1ap_ue_context_setup_failure_t ue_context_setup_failure;
        f1ap_ue_context_release_command_t ue_context_release_command;
        f1ap_ue_context_release_complete_t ue_context_release_complete;
        f1ap_ue_context_modification_request_t ue_context_modification_request;
        f1ap_ue_context_modification_response_t ue_context_modification_response;
        f1ap_ue_context_modification_failure_t ue_context_modification_failure;
        f1ap_ue_context_release_request_t ue_context_release_request;
        f1ap_dl_rrc_message_transfer_t dl_rrc_message_transfer;
        f1ap_ul_rrc_message_transfer_t ul_rrc_message_transfer;
        f1ap_notify_t notify;
        f1ap_system_information_delivery_command_t system_information_delivery_command;
    } payload;
} f1ap_message_t;

/* ============== F1AP API Functions ============== */

/* Message encoding/decoding */
int f1ap_encode_message(const f1ap_message_t* msg, uint8_t** buffer, size_t* length);
int f1ap_decode_message(const uint8_t* buffer, size_t length, f1ap_message_t* msg);
void f1ap_free_message(f1ap_message_t* msg);

/* Message type utilities */
const char* f1ap_message_type_to_string(f1ap_message_type_t type);
const char* f1ap_cause_to_string(const f1ap_cause_t* cause);

/* Message initialization helpers */
void f1ap_init_f1_setup_request(f1ap_message_t* msg);
void f1ap_init_f1_setup_response(f1ap_message_t* msg);
void f1ap_init_ue_context_setup_request(f1ap_message_t* msg);
void f1ap_init_ue_context_release_request(f1ap_message_t* msg);
void f1ap_init_dl_rrc_message_transfer(f1ap_message_t* msg);
void f1ap_init_ul_rrc_message_transfer(f1ap_message_t* msg);

/* Cause helpers */
void f1ap_set_cause_radio(f1ap_cause_t* cause, f1ap_cause_radio_value_t value);
void f1ap_set_cause_transport(f1ap_cause_t* cause, f1ap_cause_transport_value_t value);
void f1ap_set_cause_protocol(f1ap_cause_t* cause, f1ap_cause_protocol_value_t value);
void f1ap_set_cause_misc(f1ap_cause_t* cause, f1ap_cause_misc_value_t value);

#endif /* F1AP_MESSAGES_H */