/*
 * 5G UE Simulation Application
 * NGAP (NG Application Protocol) Message Definitions
 * 3GPP TS 38.413
 */

#ifndef NGAP_MESSAGES_H
#define NGAP_MESSAGES_H

#include "../uesim.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ============== Constants ============== */

#define NGAP_PORT               38412
#define NGAP_MAX_MESSAGE_SIZE   65535
#define NGAP_MAX_PDU_SESSIONS    16
#define NGAP_MAX_QOS_FLOWS       64
#define NGAP_MAX_DRBS            8
#define NGAP_MAX_SRBS            3
#define NGAP_MAX_PLMN_ID         6
#define NGAP_MAX_TAI             16
#define NGAP_MAX_GNB_ID_LEN      32

/* ============== NGAP Procedure Codes ============== */

typedef enum {
    NGAP_PROC_NG_SETUP = 21,
    NGAP_PROC_NG_RESET = 22,
    NGAP_PROC_NG_STATUS = 23,
    NGAP_PROC_INITIAL_UE = 24,
    NGAP_PROC_INITIAL_CONTEXT_SETUP = 25,
    NGAP_PROC_UE_CONTEXT_RELEASE = 26,
    NGAP_PROC_UE_CONTEXT_MODIFICATION = 27,
    NGAP_PROC_UE_CONTEXT_RELEASE_REQUEST = 28,
    NGAP_PROC_PDU_SESSION_SETUP = 29,
    NGAP_PROC_PDU_SESSION_MODIFICATION = 30,
    NGAP_PROC_PDU_SESSION_RELEASE = 31,
    NGAP_PROC_PDU_SESSION_RESOURCE_NOTIFY = 32,
    NGAP_PROC_HANDOVER_PREPARATION = 33,
    NGAP_PROC_HANDOVER_RESOURCE_ALLOCATION = 34,
    NGAP_PROC_HANDOVER_CANCEL = 35,
    NGAP_PROC_UPLINK_NAS_TRANSPORT = 36,
    NGAP_PROC_DOWNLINK_NAS_TRANSPORT = 37,
    NGAP_PROC_UPLINK_RAN_STATUS_TRANSFER = 38,
    NGAP_PROC_DOWNLINK_RAN_STATUS_TRANSFER = 39,
    NGAP_PROC_PATH_SWITCH_REQUEST = 40,
    NGAP_PROC_LOCATION_REPORT = 41,
    NGAP_PROC_INITIAL_CONTEXT_MODIFICATION = 42,
    NGAP_PROC_REROUTE_NAS_REQUEST = 43,
    NGAP_PROC_NAS_NON_DELIVERY_INDICATION = 44,
    NGAP_PROC_AMF_CONFIGURATION_UPDATE = 45,
    NGAP_PROC_RAN_CONFIGURATION_UPDATE = 46,
    NGAP_PROC_ERROR_INDICATION = 47,
    NGAP_PROC_WRITE_REPLACE_WARNING = 48,
    NGAP_PROC_PWS_CANCEL = 49,
    NGAP_PROC_PWS_RESTART = 50,
    NGAP_PROC_PWS_FAILURE = 51,
    NGAP_PROC_UPLINK_UE_ASSOCIATED_NAS_TRANSPORT = 52,
    NGAP_PROC_DOWNLINK_UE_ASSOCIATED_NAS_TRANSPORT = 53,
    NGAP_PROC_CELL_TRAFFIC_TRACE = 54,
    NGAP_PROC_UPLINK_RAN_CONFIGURATION_TRANSFER = 55,
    NGAP_PROC_DOWNLINK_RAN_CONFIGURATION_TRANSFER = 56,
    NGAP_PROC_AMF_STATUS_INDICATION = 57
} ngap_procedure_code_t;

/* ============== NGAP Message Types ============== */

typedef enum {
    /* NG Setup */
    NGAP_MSG_NG_SETUP_REQUEST = 0,
    NGAP_MSG_NG_SETUP_RESPONSE,
    NGAP_MSG_NG_SETUP_FAILURE,
    
    /* Initial UE */
    NGAP_MSG_INITIAL_UE_MESSAGE,
    NGAP_MSG_INITIAL_CONTEXT_SETUP_REQUEST,
    NGAP_MSG_INITIAL_CONTEXT_SETUP_RESPONSE,
    NGAP_MSG_INITIAL_CONTEXT_SETUP_FAILURE,
    
    /* UE Context */
    NGAP_MSG_UE_CONTEXT_RELEASE_REQUEST,
    NGAP_MSG_UE_CONTEXT_RELEASE_COMMAND,
    NGAP_MSG_UE_CONTEXT_RELEASE_COMPLETE,
    NGAP_MSG_UE_CONTEXT_MODIFICATION_REQUEST,
    NGAP_MSG_UE_CONTEXT_MODIFICATION_RESPONSE,
    NGAP_MSG_UE_CONTEXT_MODIFICATION_FAILURE,
    
    /* NAS Transport */
    NGAP_MSG_UPLINK_NAS_TRANSPORT,
    NGAP_MSG_DOWNLINK_NAS_TRANSPORT,
    NGAP_MSG_NAS_NON_DELIVERY_INDICATION,
    
    /* PDU Session */
    NGAP_MSG_PDU_SESSION_SETUP_REQUEST,
    NGAP_MSG_PDU_SESSION_SETUP_RESPONSE,
    NGAP_MSG_PDU_SESSION_SETUP_FAILURE,
    NGAP_MSG_PDU_SESSION_MODIFICATION_REQUEST,
    NGAP_MSG_PDU_SESSION_MODIFICATION_RESPONSE,
    NGAP_MSG_PDU_SESSION_MODIFICATION_FAILURE,
    NGAP_MSG_PDU_SESSION_RELEASE_COMMAND,
    NGAP_MSG_PDU_SESSION_RELEASE_RESPONSE,
    NGAP_MSG_PDU_SESSION_RESOURCE_NOTIFY,
    NGAP_MSG_PDU_SESSION_RESOURCE_NOTIFY_ACK,
    
    /* Handover */
    NGAP_MSG_HANDOVER_REQUIRED,
    NGAP_MSG_HANDOVER_COMMAND,
    NGAP_MSG_HANDOVER_PREPARATION_FAILURE,
    NGAP_MSG_HANDOVER_REQUEST,
    NGAP_MSG_HANDOVER_REQUEST_ACKNOWLEDGE,
    NGAP_MSG_HANDOVER_FAILURE,
    NGAP_MSG_HANDOVER_NOTIFY,
    NGAP_MSG_HANDOVER_CANCEL,
    NGAP_MSG_HANDOVER_CANCEL_ACKNOWLEDGE,
    NGAP_MSG_PATH_SWITCH_REQUEST,
    NGAP_MSG_PATH_SWITCH_REQUEST_ACKNOWLEDGE,
    NGAP_MSG_PATH_SWITCH_REQUEST_FAILURE,
    
    /* Error & Status */
    NGAP_MSG_ERROR_INDICATION,
    NGAP_MSG_NG_RESET,
    NGAP_MSG_NG_RESET_ACKNOWLEDGE,
    NGAP_MSG_AMF_STATUS_INDICATION,
    
    NGAP_MSG_MAX
} ngap_message_type_t;

/* ============== NGAP Cause Types ============== */

typedef enum {
    NGAP_CAUSE_RADIO_NETWORK = 0,
    NGAP_CAUSE_TRANSPORT,
    NGAP_CAUSE_NAS,
    NGAP_CAUSE_PROTOCOL,
    NGAP_CAUSE_MISC
} ngap_cause_type_t;

typedef enum {
    /* Radio Network Causes */
    NGAP_CAUSE_RADIO_UNSPECIFIED = 0,
    NGAP_CAUSE_RADIO_TX2RELOVERALL_EXPIRY = 1,
    NGAP_CAUSE_RADIO_SUCCESSFUL_HANDOVER = 2,
    NGAP_CAUSE_RADIO_RELEASE_DUE_TO_5GC_GENERATED = 3,
    NGAP_CAUSE_RADIO_RELEASE_DUE_TO_5GC_OAM = 4,
    NGAP_CAUSE_RADIO_HANDOVER_CANCELLED = 5,
    NGAP_CAUSE_RADIO_PARTIAL_HANDOVER = 6,
    NGAP_CAUSE_RADIO_HO_FAILURE_IN_TARGET_5GC = 7,
    NGAP_CAUSE_RADIO_HO_TARGET_NOT_ALLOWED = 8,
    NGAP_CAUSE_RADIO_TARGET_RAN_UNAVAILABLE = 9,
    NGAP_CAUSE_RADIO_NO_RADIO_RESOURCES_AVAILABLE = 10,
    NGAP_CAUSE_RADIO_INVALID_QOS_COMBINATION = 11,
    NGAP_CAUSE_RADIO_FAILURE_IN_RADIO_INTERFACE_PROCEDURE = 12,
    NGAP_CAUSE_RADIO_INTERACTION_WITH_OTHER_PROCEDURE = 13,
    NGAP_CAUSE_RADIO_UNKNOWN_PDU_SESSION_ID = 14,
    NGAP_CAUSE_RADIO_UNKNOWN_QOS_FLOW_ID = 15,
    NGAP_CAUSE_RADIO_MULTIPLE_PDU_SESSION_ID = 16,
    NGAP_CAUSE_RADIO_MULTIPLE_QOS_FLOW_ID = 17,
    NGAP_CAUSE_RADIO_RESOURCES_NOT_AVAILABLE = 18,
    NGAP_CAUSE_RADIO_RAN_UNAVAILABLE = 19,
    NGAP_CAUSE_RADIO_REPORT_CHARACTERISTICS_CHANGE = 20,
    NGAP_CAUSE_RADIO_RECOVERY = 21,
    NGAP_CAUSE_RADIO_RELEASE_DUE_TO_CN = 22,
    NGAP_CAUSE_RADIO_RAN_INITIATED_QOS_FLOW_RELEASE = 23,
    NGAP_CAUSE_RADIO_RAN_INITIATED_PDU_SESSION_RELEASE = 24,
    
    /* Transport Causes */
    NGAP_CAUSE_TRANSPORT_TRANSPORT_RESOURCE_UNAVAILABLE = 0,
    NGAP_CAUSE_TRANSPORT_UNSPECIFIED = 1,
    
    /* NAS Causes */
    NGAP_CAUSE_NAS_NORMAL_RELEASE = 0,
    NGAP_CAUSE_NAS_AUTHENTICATION_FAILURE = 1,
    NGAP_CAUSE_NAS_DEREGISTER = 2,
    NGAP_CAUSE_NAS_UNSPECIFIED = 3,
    
    /* Protocol Causes */
    NGAP_CAUSE_PROTOCOL_TRANSFER_SYNTAX_ERROR = 0,
    NGAP_CAUSE_PROTOCOL_ABSTRACT_SYNTAX_ERROR_REJECT = 1,
    NGAP_CAUSE_PROTOCOL_ABSTRACT_SYNTAX_ERROR_IGNORE_AND_NOTIFY = 2,
    NGAP_CAUSE_PROTOCOL_MESSAGE_NOT_COMPATIBLE_WITH_RECEIVER_STATE = 3,
    NGAP_CAUSE_PROTOCOL_SEMANTIC_ERROR = 4,
    NGAP_CAUSE_PROTOCOL_ABSTRACT_SYNTAX_ERROR_FALSELY_CONSTRUCTED_MESSAGE = 5,
    NGAP_CAUSE_PROTOCOL_UNSPECIFIED = 6,
    
    /* Misc Causes */
    NGAP_CAUSE_MISC_CONTROL_PROCESSING_OVERLOAD = 0,
    NGAP_CAUSE_MISC_NOT_ENOUGH_USER_PLANE_PROCESSING_RESOURCES = 1,
    NGAP_CAUSE_MISC_HARDWARE_FAILURE = 2,
    NGAP_CAUSE_MISC_OM_INTERVENTION = 3,
    NGAP_CAUSE_MISC_UNSPECIFIED = 4
} ngap_cause_value_t;

/* ============== NGAP Cause Structure ============== */

typedef struct {
    ngap_cause_type_t cause_type;
    ngap_cause_value_t cause_value;
} ngap_cause_t;

/* ============== NGAP Identity Structures ============== */

typedef struct {
    uint32_t amf_id;           /* AMF Identifier (10 bits) */
    uint32_t set_id;           /* AMF Set ID (10 bits) */
    uint8_t pointer;           /* AMF Pointer (6 bits) */
} ngap_guami_t;

typedef struct {
    uint32_t ran_ue_ngap_id;   /* RAN UE NGAP ID (32 bits) */
    uint64_t amf_ue_ngap_id;   /* AMF UE NGAP ID (40 bits) */
} ngap_ue_ids_t;

typedef struct {
    uint8_t plmn_id[3];        /* PLMN ID (MCC + MNC) */
    uint32_t gnb_id;           /* gNB ID (22-32 bits) */
    uint8_t gnb_id_len;        /* gNB ID length in bits */
    char gnb_name[64];         /* gNB Name (optional) */
} ngap_global_gnb_id_t;

typedef struct {
    uint8_t plmn_id[3];        /* PLMN ID */
    uint32_t tac;              /* Tracking Area Code (24 bits) */
} ngap_tai_t;

typedef struct {
    uint8_t plmn_id[3];
    uint32_t cell_id;          /* NCGI - 36 bits */
} ngap_ncgi_t;

typedef struct {
    uint8_t plmn_id[3];
    uint32_t nssai_sst;        /* S-NSSAI SST (8 bits) */
    uint32_t nssai_sd;         /* S-NSSAI SD (24 bits, optional) */
    bool sd_present;
} ngap_snssai_t;

/* ============== NGAP User Location Info ============== */

typedef struct {
    ngap_ncgi_t nr_cgi;        /* NR Cell Global Identity */
    uint32_t tai;              /* TAC */
    uint8_t time_stamp[4];     /* Time Stamp (optional) */
    bool time_stamp_present;
} ngap_user_location_info_nr_t;

/* ============== NGAP PDU Session Structures ============== */

typedef struct {
    uint8_t pdu_session_id;    /* PDU Session ID (1-15) */
    uint8_t pdu_session_type;  /* IPv4, IPv6, IPv4v6 */
    uint8_t sst;               /* S-NSSAI SST */
    uint32_t sd;               /* S-NSSAI SD */
    bool sd_present;
} ngap_pdu_session_t;

typedef struct {
    uint8_t qfi;               /* QoS Flow Identifier */
    uint8_t five_qi;            /* 5QI */
    uint8_t arp_priority;      /* ARP Priority Level */
    bool arp_preempt_cap;      /* Pre-emption Capability */
    bool arp_preempt_vuln;     /* Pre-emption Vulnerability */
    uint64_t gbr_ul;           /* Guaranteed Bit Rate UL */
    uint64_t gbr_dl;           /* Guaranteed Bit Rate DL */
    uint64_t mbr_ul;           /* Maximum Bit Rate UL */
    uint64_t mbr_dl;           /* Maximum Bit Rate DL */
} ngap_qos_flow_t;

typedef struct {
    uint8_t pdu_session_id;
    uint32_t upf_teid;         /* UPF TEID for DL */
    uint32_t upf_ip;           /* UPF IP address */
    uint32_t gnb_teid;         /* gNB TEID for UL */
    uint32_t gnb_ip;           /* gNB IP address */
    uint8_t num_qos_flows;
    ngap_qos_flow_t qos_flows[8];
} ngap_pdu_session_resource_t;

/* ============== NGAP Security Context ============== */

typedef struct {
    uint8_t security_algorithm_cipher;   /* Ciphering algorithm */
    uint8_t security_algorithm_integrity; /* Integrity algorithm */
    uint8_t knas_enc[16];                /* NAS encryption key */
    uint8_t knas_int[16];                /* NAS integrity key */
    uint32_t ul_count;                   /* Uplink NAS COUNT */
    uint32_t dl_count;                   /* Downlink NAS COUNT */
} ngap_security_context_t;

/* ============== NGAP Message Structures ============== */

/* NG Setup Request */
typedef struct {
    ngap_global_gnb_id_t global_gnb_id;
    ngap_tai_t tai_list[NGAP_MAX_TAI];
    uint8_t num_tai;
    uint32_t default_paging_drx;
    uint32_t max_ue_connections;
    char gnb_name[64];
} ngap_ng_setup_request_t;

/* NG Setup Response */
typedef struct {
    uint8_t plmn_id[3];
    uint32_t amf_id;
    ngap_tai_t tai_list[NGAP_MAX_TAI];
    uint8_t num_tai;
    uint32_t relative_amf_capacity;
    char amf_name[64];
    ngap_snssai_t snssai_list[8];
    uint8_t num_snssai;
} ngap_ng_setup_response_t;

/* NG Setup Failure */
typedef struct {
    ngap_cause_t cause;
    uint32_t time_to_wait;     /* Optional */
    bool time_to_wait_present;
    bool criticality_diagnostics_present;
} ngap_ng_setup_failure_t;

/* Initial UE Message */
typedef struct {
    ngap_ue_ids_t ue_ids;
    ngap_user_location_info_nr_t user_location;
    uint8_t rrc_establishment_cause;
    uint8_t ue_context_request;
    uint8_t nas_pdu[4096];
    size_t nas_pdu_len;
    uint8_t allowed_nssai[16];
    size_t allowed_nssai_len;
} ngap_initial_ue_message_t;

/* Initial Context Setup Request */
typedef struct {
    ngap_ue_ids_t ue_ids;
    ngap_user_location_info_nr_t user_location;
    uint8_t ue_security_capability[3];
    uint8_t security_algorithm_cipher;
    uint8_t security_algorithm_integrity;
    uint8_t security_key[32];
    ngap_pdu_session_resource_t pdu_sessions[NGAP_MAX_PDU_SESSIONS];
    uint8_t num_pdu_sessions;
    uint8_t nas_pdu[4096];
    size_t nas_pdu_len;
    uint8_t ue_aggregate_maximum_bit_rate_ul[8];
    uint8_t ue_aggregate_maximum_bit_rate_dl[8];
} ngap_initial_context_setup_request_t;

/* Initial Context Setup Response */
typedef struct {
    ngap_ue_ids_t ue_ids;
    ngap_pdu_session_resource_t pdu_sessions[NGAP_MAX_PDU_SESSIONS];
    uint8_t num_pdu_sessions;
} ngap_initial_context_setup_response_t;

/* Initial Context Setup Failure */
typedef struct {
    ngap_ue_ids_t ue_ids;
    ngap_cause_t cause;
} ngap_initial_context_setup_failure_t;

/* Uplink NAS Transport */
typedef struct {
    ngap_ue_ids_t ue_ids;
    ngap_user_location_info_nr_t user_location;
    uint8_t nas_pdu[4096];
    size_t nas_pdu_len;
} ngap_uplink_nas_transport_t;

/* Downlink NAS Transport */
typedef struct {
    ngap_ue_ids_t ue_ids;
    uint8_t nas_pdu[4096];
    size_t nas_pdu_len;
    ngap_cause_t cause;        /* For non-delivery indication */
    bool cause_present;
} ngap_downlink_nas_transport_t;

/* UE Context Release Request */
typedef struct {
    ngap_ue_ids_t ue_ids;
    ngap_cause_t cause;
} ngap_ue_context_release_request_t;

/* UE Context Release Command */
typedef struct {
    ngap_ue_ids_t ue_ids;
    ngap_cause_t cause;
} ngap_ue_context_release_command_t;

/* UE Context Release Complete */
typedef struct {
    ngap_ue_ids_t ue_ids;
    ngap_user_location_info_nr_t user_location;
    ngap_pdu_session_resource_t pdu_sessions[NGAP_MAX_PDU_SESSIONS];
    uint8_t num_pdu_sessions;
} ngap_ue_context_release_complete_t;

/* PDU Session Setup Request */
typedef struct {
    ngap_ue_ids_t ue_ids;
    ngap_pdu_session_resource_t pdu_session;
    uint8_t nas_pdu[4096];
    size_t nas_pdu_len;
} ngap_pdu_session_setup_request_t;

/* PDU Session Setup Response */
typedef struct {
    ngap_ue_ids_t ue_ids;
    ngap_pdu_session_resource_t pdu_session;
    ngap_cause_t cause;        /* If failed */
    bool success;
} ngap_pdu_session_setup_response_t;

/* Handover Required */
typedef struct {
    ngap_ue_ids_t ue_ids;
    ngap_cause_t cause;
    uint8_t handover_type;
    ngap_global_gnb_id_t target_gnb_id;
    ngap_pdu_session_resource_t pdu_sessions[NGAP_MAX_PDU_SESSIONS];
    uint8_t num_pdu_sessions;
    uint8_t source_to_target_container[4096];
    size_t container_len;
} ngap_handover_required_t;

/* Handover Command */
typedef struct {
    ngap_ue_ids_t ue_ids;
    ngap_pdu_session_resource_t pdu_sessions[NGAP_MAX_PDU_SESSIONS];
    uint8_t num_pdu_sessions;
    uint8_t target_to_source_container[4096];
    size_t container_len;
} ngap_handover_command_t;

/* Handover Request */
typedef struct {
    ngap_ue_ids_t ue_ids;
    ngap_global_gnb_id_t source_gnb_id;
    uint8_t handover_type;
    ngap_pdu_session_resource_t pdu_sessions[NGAP_MAX_PDU_SESSIONS];
    uint8_t num_pdu_sessions;
    uint8_t source_to_target_container[4096];
    size_t container_len;
} ngap_handover_request_t;

/* Handover Request Acknowledge */
typedef struct {
    ngap_ue_ids_t ue_ids;
    ngap_pdu_session_resource_t pdu_sessions[NGAP_MAX_PDU_SESSIONS];
    uint8_t num_pdu_sessions;
    uint8_t target_to_source_container[4096];
    size_t container_len;
} ngap_handover_request_ack_t;

/* Handover Notify */
typedef struct {
    ngap_ue_ids_t ue_ids;
    ngap_user_location_info_nr_t user_location;
} ngap_handover_notify_t;

/* Path Switch Request */
typedef struct {
    ngap_ue_ids_t ue_ids;
    ngap_user_location_info_nr_t user_location;
    ngap_pdu_session_resource_t pdu_sessions[NGAP_MAX_PDU_SESSIONS];
    uint8_t num_pdu_sessions;
} ngap_path_switch_request_t;

/* Error Indication */
typedef struct {
    ngap_ue_ids_t ue_ids;      /* Optional */
    bool ue_ids_present;
    ngap_cause_t cause;         /* Optional */
    bool cause_present;
    uint8_t criticality_diagnostics[256];
    size_t diagnostics_len;
} ngap_error_indication_t;

/* ============== Generic NGAP Message Structure ============== */

typedef struct {
    ngap_message_type_t message_type;
    ngap_procedure_code_t procedure_code;
    uint8_t criticality;
    uint32_t transaction_id;
    union {
        ngap_ng_setup_request_t ng_setup_request;
        ngap_ng_setup_response_t ng_setup_response;
        ngap_ng_setup_failure_t ng_setup_failure;
        ngap_initial_ue_message_t initial_ue_message;
        ngap_initial_context_setup_request_t initial_context_setup_request;
        ngap_initial_context_setup_response_t initial_context_setup_response;
        ngap_initial_context_setup_failure_t initial_context_setup_failure;
        ngap_uplink_nas_transport_t uplink_nas_transport;
        ngap_downlink_nas_transport_t downlink_nas_transport;
        ngap_ue_context_release_request_t ue_context_release_request;
        ngap_ue_context_release_command_t ue_context_release_command;
        ngap_ue_context_release_complete_t ue_context_release_complete;
        ngap_pdu_session_setup_request_t pdu_session_setup_request;
        ngap_pdu_session_setup_response_t pdu_session_setup_response;
        ngap_handover_required_t handover_required;
        ngap_handover_command_t handover_command;
        ngap_handover_request_t handover_request;
        ngap_handover_request_ack_t handover_request_ack;
        ngap_handover_notify_t handover_notify;
        ngap_path_switch_request_t path_switch_request;
        ngap_error_indication_t error_indication;
    } payload;
} ngap_message_t;

/* ============== Function Prototypes ============== */

/* Message encoding/decoding */
uesim_error_t ngap_encode_message(const ngap_message_t* msg, uint8_t** data, size_t* len);
uesim_error_t ngap_decode_message(const uint8_t* data, size_t len, ngap_message_t* msg);
uesim_error_t ngap_free_message(ngap_message_t* msg);

/* Specific message creation */
uesim_error_t ngap_create_ng_setup_request(const ngap_global_gnb_id_t* gnb_id, 
                                            ngap_message_t* msg);
uesim_error_t ngap_create_initial_ue_message(uint32_t ran_ue_id, 
                                              const uint8_t* nas_pdu, size_t nas_len,
                                              ngap_message_t* msg);
uesim_error_t ngap_create_uplink_nas_transport(uint32_t ran_ue_id, uint64_t amf_ue_id,
                                                const uint8_t* nas_pdu, size_t nas_len,
                                                ngap_message_t* msg);
uesim_error_t ngap_create_ue_context_release_request(uint32_t ran_ue_id, uint64_t amf_ue_id,
                                                      ngap_cause_t cause, ngap_message_t* msg);

/* Utility functions */
const char* ngap_message_type_to_string(ngap_message_type_t type);
const char* ngap_cause_to_string(ngap_cause_t cause);
uint32_t ngap_encode_plmn_id(uint16_t mcc, uint16_t mnc, uint8_t mnc_len);
void ngap_decode_plmn_id(uint8_t plmn_id[3], uint16_t* mcc, uint16_t* mnc, uint8_t* mnc_len);

#endif /* NGAP_MESSAGES_H */