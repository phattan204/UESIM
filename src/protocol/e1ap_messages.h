/*
 * 5G UE Simulation Application
 * E1AP (E1 Application Protocol) Message Definitions
 * 3GPP TS 38.463 - CU-CP to CU-UP Interface
 */

#ifndef E1AP_MESSAGES_H
#define E1AP_MESSAGES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ============== E1AP Constants ============== */

#define E1AP_PORT                   38462
#define E1AP_MAX_MESSAGE_SIZE       65535
#define E1AP_MAX_PDU_SESSIONS       256
#define E1AP_MAX_DRB_ID             32
#define E1AP_MAX_QOS_FLOWS          64
#define E1AP_MAX_TNL_INFO           8
#define E1AP_MAX_RRC_CONTAINER_SIZE 65536

/* ============== E1AP Procedure Codes (TS 38.463 Section 9) ============== */

typedef enum {
    E1AP_PROC_E1_SETUP = 1,
    E1AP_PROC_E1_RESET = 2,
    E1AP_PROC_ERROR_INDICATION = 3,
    E1AP_PROC_E1_REMOVAL = 4,
    E1AP_PROC_GNB_CU_UP_CONFIGURATION_UPDATE = 5,
    E1AP_PROC_GNB_CU_CP_CONFIGURATION_UPDATE = 6,
    E1AP_PROC_BEARER_CONTEXT_SETUP = 7,
    E1AP_PROC_BEARER_CONTEXT_RELEASE = 8,
    E1AP_PROC_BEARER_CONTEXT_MODIFICATION = 9,
    E1AP_PROC_BEARER_CONTEXT_RELEASE_REQUEST = 10,
    E1AP_PROC_PDU_SESSION_RESOURCE_SETUP = 11,
    E1AP_PROC_PDU_SESSION_RESOURCE_MODIFICATION = 12,
    E1AP_PROC_PDU_SESSION_RESOURCE_RELEASE = 13
} e1ap_procedure_code_t;

/* ============== E1AP Message Types ============== */

typedef enum {
    /* E1 Setup */
    E1AP_MSG_E1_SETUP_REQUEST = 0,
    E1AP_MSG_E1_SETUP_RESPONSE,
    E1AP_MSG_E1_SETUP_FAILURE,
    
    /* E1 Reset */
    E1AP_MSG_E1_RESET_REQUEST,
    E1AP_MSG_E1_RESET_RESPONSE,
    
    /* Error Indication */
    E1AP_MSG_ERROR_INDICATION,
    
    /* E1 Removal */
    E1AP_MSG_E1_REMOVAL_REQUEST,
    E1AP_MSG_E1_REMOVAL_RESPONSE,
    
    /* gNB-CU-UP Configuration Update */
    E1AP_MSG_GNB_CU_UP_CONFIG_UPDATE,
    E1AP_MSG_GNB_CU_UP_CONFIG_UPDATE_ACK,
    E1AP_MSG_GNB_CU_UP_CONFIG_UPDATE_FAILURE,
    
    /* gNB-CU-CP Configuration Update */
    E1AP_MSG_GNB_CU_CP_CONFIG_UPDATE,
    E1AP_MSG_GNB_CU_CP_CONFIG_UPDATE_ACK,
    E1AP_MSG_GNB_CU_CP_CONFIG_UPDATE_FAILURE,
    
    /* Bearer Context Setup */
    E1AP_MSG_BEARER_CONTEXT_SETUP_REQUEST,
    E1AP_MSG_BEARER_CONTEXT_SETUP_RESPONSE,
    E1AP_MSG_BEARER_CONTEXT_SETUP_FAILURE,
    
    /* Bearer Context Release */
    E1AP_MSG_BEARER_CONTEXT_RELEASE_COMMAND,
    E1AP_MSG_BEARER_CONTEXT_RELEASE_COMPLETE,
    
    /* Bearer Context Modification */
    E1AP_MSG_BEARER_CONTEXT_MODIFICATION_REQUEST,
    E1AP_MSG_BEARER_CONTEXT_MODIFICATION_RESPONSE,
    E1AP_MSG_BEARER_CONTEXT_MODIFICATION_FAILURE,
    
    /* Bearer Context Release Request */
    E1AP_MSG_BEARER_CONTEXT_RELEASE_REQUEST,
    
    /* PDU Session Resource Setup */
    E1AP_MSG_PDU_SESSION_RESOURCE_SETUP_REQUEST,
    E1AP_MSG_PDU_SESSION_RESOURCE_SETUP_RESPONSE,
    
    /* PDU Session Resource Modification */
    E1AP_MSG_PDU_SESSION_RESOURCE_MODIFICATION_REQUEST,
    E1AP_MSG_PDU_SESSION_RESOURCE_MODIFICATION_RESPONSE,
    
    /* PDU Session Resource Release */
    E1AP_MSG_PDU_SESSION_RESOURCE_RELEASE_COMMAND,
    E1AP_MSG_PDU_SESSION_RESOURCE_RELEASE_COMPLETE,
    
    E1AP_MSG_MAX
} e1ap_message_type_t;

/* ============== E1AP Cause Types ============== */

typedef enum {
    E1AP_CAUSE_RADIO_NETWORK = 0,
    E1AP_CAUSE_TRANSPORT,
    E1AP_CAUSE_PROTOCOL,
    E1AP_CAUSE_MISC,
    E1AP_CAUSE_NG_AND_OR_XN
} e1ap_cause_type_t;

/* Radio Network Cause Values */
typedef enum {
    E1AP_CAUSE_RADIO_UNSPECIFIED = 0,
    E1AP_CAUSE_RADIO_NORMAL_RELEASE = 1,
    E1AP_CAUSE_RADIO_RADIO_CONNECTION_WITH_UE_LOST = 2,
    E1AP_CAUSE_RADIO_RLF_DETECTED = 3,
    E1AP_CAUSE_RADIO_HANDOVER_CANCELLED = 4,
    E1AP_CAUSE_RADIO_INACTIVITY_TIMER_EXPIRED = 5,
    E1AP_CAUSE_RADIO_UE_NOT_RESPONDING = 6,
    E1AP_CAUSE_RADIO_DUPLICATE_PDU_SESSION_ID = 7
} e1ap_cause_radio_value_t;

/* Transport Cause Values */
typedef enum {
    E1AP_CAUSE_TRANSPORT_TRANSPORT_RESOURCE_UNAVAILABLE = 0,
    E1AP_CAUSE_TRANSPORT_UNSPECIFIED = 1
} e1ap_cause_transport_value_t;

/* Protocol Cause Values */
typedef enum {
    E1AP_CAUSE_PROTOCOL_TRANSFER_SYNTAX_ERROR = 0,
    E1AP_CAUSE_PROTOCOL_ABSTRACT_SYNTAX_ERROR_REJECT = 1,
    E1AP_CAUSE_PROTOCOL_ABSTRACT_SYNTAX_ERROR_IGNORE_AND_NOTIFY = 2,
    E1AP_CAUSE_PROTOCOL_MESSAGE_NOT_COMPATIBLE_WITH_RECEIVER_STATE = 3,
    E1AP_CAUSE_PROTOCOL_SEMANTIC_ERROR = 4,
    E1AP_CAUSE_PROTOCOL_UNSPECIFIED = 5
} e1ap_cause_protocol_value_t;

/* Misc Cause Values */
typedef enum {
    E1AP_CAUSE_MISC_CONTROL_PROCESSING_OVERLOAD = 0,
    E1AP_CAUSE_MISC_NOT_ENOUGH_USER_PLANE_PROCESSING_RESOURCES = 1,
    E1AP_CAUSE_MISC_HARDWARE_FAILURE = 2,
    E1AP_CAUSE_MISC_OM_INTERVENTION = 3,
    E1AP_CAUSE_MISC_UNSPECIFIED = 4
} e1ap_cause_misc_value_t;

/* ============== E1AP Cause Structure ============== */

typedef struct {
    e1ap_cause_type_t cause_type;
    uint8_t cause_value;
} e1ap_cause_t;

/* ============== E1AP Identity Structures ============== */

/* gNB-CU-CP ID */
typedef struct {
    uint64_t gnb_cu_cp_id;      /* 22-32 bits */
    char gnb_cu_cp_name[64];
} e1ap_gnb_cu_cp_id_t;

/* gNB-CU-UP ID */
typedef struct {
    uint32_t gnb_cu_up_id;      /* 32 bits */
    char gnb_cu_up_name[64];
} e1ap_gnb_cu_up_id_t;

/* gNB-CU-UP UE E1AP ID */
typedef struct {
    uint32_t gnb_cu_cp_ue_e1ap_id;   /* 32 bits */
    uint32_t gnb_cu_up_ue_e1ap_id;   /* 32 bits */
} e1ap_ue_ids_t;

/* RAN UE ID (for NGAP correlation) */
typedef struct {
    uint64_t ran_ue_id;              /* 40 bits */
} e1ap_ran_ue_id_t;

/* ============== E1AP Network Slice Info ============== */

typedef struct {
    uint8_t sst;                     /* Slice/Service Type */
    uint32_t sd;                     /* Slice Differentiator */
    bool sd_present;
} e1ap_s_nssai_t;

/* ============== E1AP TNL Information ============== */

typedef struct {
    uint32_t ip_address;
    uint16_t port;
    uint32_t teid;                   /* GTP-U Tunnel Endpoint ID */
    uint8_t transport_type;          /* 0=IPv4, 1=IPv6, 2=IPv4v6 */
} e1ap_tnl_info_t;

/* ============== E1AP QoS Flow Info ============== */

typedef struct {
    uint8_t qfi;                     /* QoS Flow Identifier */
    uint8_t five_qi;                 /* 5G QoS Identifier */
    uint64_t gfbr_ul;                /* Guaranteed Flow Bit Rate UL */
    uint64_t gfbr_dl;                /* Guaranteed Flow Bit Rate DL */
    uint64_t mfbr_ul;                /* Maximum Flow Bit Rate UL */
    uint64_t mfbr_dl;                /* Maximum Flow Bit Rate DL */
    uint8_t priority_level;
    bool averaging_window_present;
    uint16_t averaging_window;
} e1ap_qos_flow_t;

/* ============== E1AP DRB Info ============== */

typedef struct {
    uint8_t drb_id;                  /* 1-32 */
    uint8_t pdcp_sn_size;            /* 12 or 18 bits */
    uint8_t pdcp_retransmission_timer;
    uint8_t pdcp_discard_timer;
    uint8_t rlc_mode;                /* 1=AM, 2=UM */
    uint8_t rlc_sn_size;             /* 12, 18, or 24 bits */
    uint64_t ul_aggregate_maximum_bit_rate;
    uint64_t dl_aggregate_maximum_bit_rate;
    
    /* QoS Flows */
    uint8_t num_qos_flows;
    e1ap_qos_flow_t qos_flows[E1AP_MAX_QOS_FLOWS];
    
    /* TNL Info */
    uint8_t num_dl_up_tnl_info;
    e1ap_tnl_info_t dl_up_tnl_info[E1AP_MAX_TNL_INFO];
    
    /* UL TNL Info for UPF */
    e1ap_tnl_info_t ul_upf_tnl_info;
} e1ap_drb_info_t;

/* ============== E1AP PDU Session Info ============== */

typedef struct {
    uint8_t pdu_session_id;          /* 1-255 */
    uint8_t pdu_session_type;        /* IPv4=1, IPv6=2, IPv4v6=3 */
    uint32_t ue_ip_address;
    
    /* S-NSSAI */
    e1ap_s_nssai_t s_nssai;
    
    /* DRBs */
    uint8_t num_drbs;
    e1ap_drb_info_t drbs[E1AP_MAX_DRB_ID];
    
    /* UL TNL Info for UPF */
    e1ap_tnl_info_t upf_ul_tnl_info;
    
    /* Security */
    uint8_t security_indication;     /* 0=not needed, 1=needed */
    uint8_t up_security_policy;      /* 0=none, 1=integrity, 2=encryption, 3=both */
} e1ap_pdu_session_info_t;

/* ============== E1AP Message Structures ============== */

/* E1 Setup Request (CU-UP -> CU-CP) */
typedef struct {
    e1ap_gnb_cu_up_id_t gnb_cu_up_id;
    
    /* Supported PLMNs and S-NSSAIs */
    uint8_t num_supported_plmns;
    uint8_t supported_plmns[12][3];  /* BCD encoded MCC+MNC */
    uint8_t num_supported_slices;
    e1ap_s_nssai_t supported_slices[E1AP_MAX_QOS_FLOWS];
    
    /* Capacity */
    uint32_t capacity;
    
    /* gNB-CU-UP TNLA Information */
    uint8_t num_tnla;
    e1ap_tnl_info_t tnla[E1AP_MAX_TNL_INFO];
} e1ap_e1_setup_request_t;

/* E1 Setup Response (CU-CP -> CU-UP) */
typedef struct {
    e1ap_gnb_cu_cp_id_t gnb_cu_cp_id;
    
    /* Supported PLMNs */
    uint8_t num_supported_plmns;
    uint8_t supported_plmns[12][3];
} e1ap_e1_setup_response_t;

/* E1 Setup Failure */
typedef struct {
    e1ap_cause_t cause;
    uint32_t time_to_wait;
} e1ap_e1_setup_failure_t;

/* Bearer Context Setup Request */
typedef struct {
    e1ap_ue_ids_t ue_ids;
    e1ap_ran_ue_id_t ran_ue_id;
    
    /* PDU Session Info */
    uint8_t num_pdu_sessions;
    e1ap_pdu_session_info_t pdu_sessions[E1AP_MAX_PDU_SESSIONS];
    
    /* UE Aggregate Maximum Bit Rate */
    uint64_t ue_ambr_dl;
    uint64_t ue_ambr_ul;
    
    /* UE Security */
    uint8_t security_key[32];
    uint8_t security_key_present;
} e1ap_bearer_context_setup_request_t;

/* Bearer Context Setup Response */
typedef struct {
    e1ap_ue_ids_t ue_ids;
    
    /* PDU Sessions Setup */
    uint8_t num_pdu_sessions_setup;
    uint8_t pdu_session_ids[E1AP_MAX_PDU_SESSIONS];
    
    /* DRB TNL Info */
    uint8_t num_drbs_setup;
    e1ap_drb_info_t drbs_setup[E1AP_MAX_DRB_ID];
    
    /* Failed Items */
    uint8_t num_failed_pdu_sessions;
    uint8_t failed_pdu_session_ids[E1AP_MAX_PDU_SESSIONS];
    e1ap_cause_t pdu_session_fail_causes[E1AP_MAX_PDU_SESSIONS];
} e1ap_bearer_context_setup_response_t;

/* Bearer Context Setup Failure */
typedef struct {
    e1ap_ue_ids_t ue_ids;
    e1ap_cause_t cause;
} e1ap_bearer_context_setup_failure_t;

/* Bearer Context Release Command */
typedef struct {
    e1ap_ue_ids_t ue_ids;
    e1ap_cause_t cause;
} e1ap_bearer_context_release_command_t;

/* Bearer Context Release Complete */
typedef struct {
    e1ap_ue_ids_t ue_ids;
} e1ap_bearer_context_release_complete_t;

/* Bearer Context Release Request */
typedef struct {
    e1ap_ue_ids_t ue_ids;
    e1ap_cause_t cause;
} e1ap_bearer_context_release_request_t;

/* gNB-CU-UP Configuration Update */
typedef struct {
    e1ap_gnb_cu_up_id_t gnb_cu_up_id;
    
    /* Updated TNLAs */
    uint8_t num_tnla_to_add;
    e1ap_tnl_info_t tnla_to_add[E1AP_MAX_TNL_INFO];
    
    uint8_t num_tnla_to_remove;
    uint32_t tnla_to_remove[E1AP_MAX_TNL_INFO];
    
    uint8_t num_tnla_to_update;
    e1ap_tnl_info_t tnla_to_update[E1AP_MAX_TNL_INFO];
    
    /* Capacity Update */
    uint32_t capacity;
    uint8_t capacity_present;
} e1ap_gnb_cu_up_config_update_t;

/* gNB-CU-UP Configuration Update Acknowledge */
typedef struct {
    uint8_t tnlas_added;
    uint8_t tnlas_removed;
    uint8_t tnlas_updated;
} e1ap_gnb_cu_up_config_update_ack_t;

/* PDU Session Resource Setup Request */
typedef struct {
    e1ap_ue_ids_t ue_ids;
    
    uint8_t num_pdu_sessions;
    e1ap_pdu_session_info_t pdu_sessions[E1AP_MAX_PDU_SESSIONS];
} e1ap_pdu_session_resource_setup_request_t;

/* PDU Session Resource Setup Response */
typedef struct {
    e1ap_ue_ids_t ue_ids;
    
    uint8_t num_pdu_sessions_setup;
    uint8_t pdu_session_ids[E1AP_MAX_PDU_SESSIONS];
    e1ap_tnl_info_t dl_tnl_info[E1AP_MAX_PDU_SESSIONS];
    
    uint8_t num_pdu_sessions_failed;
    uint8_t failed_pdu_session_ids[E1AP_MAX_PDU_SESSIONS];
    e1ap_cause_t fail_causes[E1AP_MAX_PDU_SESSIONS];
} e1ap_pdu_session_resource_setup_response_t;

/* Error Indication */
typedef struct {
    e1ap_ue_ids_t ue_ids;
    uint8_t ue_ids_present;
    e1ap_cause_t cause;
    uint8_t cause_present;
} e1ap_error_indication_t;

/* ============== E1AP Message Union ============== */

typedef struct {
    e1ap_message_type_t message_type;
    e1ap_procedure_code_t procedure_code;
    uint8_t criticality;         /* 0=reject, 1=ignore, 2=notify */
    
    union {
        e1ap_e1_setup_request_t e1_setup_request;
        e1ap_e1_setup_response_t e1_setup_response;
        e1ap_e1_setup_failure_t e1_setup_failure;
        e1ap_bearer_context_setup_request_t bearer_context_setup_request;
        e1ap_bearer_context_setup_response_t bearer_context_setup_response;
        e1ap_bearer_context_setup_failure_t bearer_context_setup_failure;
        e1ap_bearer_context_release_command_t bearer_context_release_command;
        e1ap_bearer_context_release_complete_t bearer_context_release_complete;
        e1ap_bearer_context_release_request_t bearer_context_release_request;
        e1ap_gnb_cu_up_config_update_t gnb_cu_up_config_update;
        e1ap_gnb_cu_up_config_update_ack_t gnb_cu_up_config_update_ack;
        e1ap_pdu_session_resource_setup_request_t pdu_session_resource_setup_request;
        e1ap_pdu_session_resource_setup_response_t pdu_session_resource_setup_response;
        e1ap_error_indication_t error_indication;
    } payload;
} e1ap_message_t;

/* ============== E1AP API Functions ============== */

/* Message encoding/decoding */
int e1ap_encode_message(const e1ap_message_t* msg, uint8_t** buffer, size_t* length);
int e1ap_decode_message(const uint8_t* buffer, size_t length, e1ap_message_t* msg);
void e1ap_free_message(e1ap_message_t* msg);

/* Message type utilities */
const char* e1ap_message_type_to_string(e1ap_message_type_t type);
const char* e1ap_cause_to_string(const e1ap_cause_t* cause);

/* Message initialization helpers */
void e1ap_init_e1_setup_request(e1ap_message_t* msg);
void e1ap_init_e1_setup_response(e1ap_message_t* msg);
void e1ap_init_bearer_context_setup_request(e1ap_message_t* msg);
void e1ap_init_bearer_context_release_request(e1ap_message_t* msg);

/* Cause helpers */
void e1ap_set_cause_radio(e1ap_cause_t* cause, e1ap_cause_radio_value_t value);
void e1ap_set_cause_transport(e1ap_cause_t* cause, e1ap_cause_transport_value_t value);
void e1ap_set_cause_protocol(e1ap_cause_t* cause, e1ap_cause_protocol_value_t value);
void e1ap_set_cause_misc(e1ap_cause_t* cause, e1ap_cause_misc_value_t value);

#endif /* E1AP_MESSAGES_H */