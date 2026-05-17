/*
 * 5G UE Simulation Application
 * XnAP (Xn Application Protocol) Message Definitions
 * 3GPP TS 38.423 - gNB to gNB Interface
 */

#ifndef XNAP_MESSAGES_H
#define XNAP_MESSAGES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ============== XnAP Constants ============== */

#define XNAP_PORT                   38422
#define XNAP_MAX_MESSAGE_SIZE       65535
#define XNAP_MAX_CELL_COUNT         512
#define XNAP_MAX_PLMN_COUNT         12
#define XNAP_MAX_TAI_COUNT          16
#define XNAP_MAX_SLICE_COUNT        8
#define XNAP_MAX_DRB_COUNT          8
#define XNAP_MAX_QOS_FLOWS          64
#define XNAP_MAX_RRC_CONTAINER_SIZE 65536
#define XNAP_MAX_NEIGHBOR_COUNT     64
#define XNAP_MAX_SERVED_CELL_COUNT  64

/* ============== XnAP Procedure Codes (TS 38.423 Section 9) ============== */

typedef enum {
    XNAP_PROC_XN_SETUP = 1,
    XNAP_PROC_XN_RESET = 2,
    XNAP_PROC_ERROR_INDICATION = 3,
    XNAP_PROC_XN_REMOVAL = 4,
    XNAP_PROC_PAGING = 5,
    XNAP_PROC_HANDOVER_PREPARATION = 6,
    XNAP_PROC_HANDOVER_CANCEL = 7,
    XNAP_PROC_RETRIEVE_UE_CONTEXT = 8,
    XNAP_PROC_XN_UES_CONTEXT_TRANSFER = 9,
    XNAP_PROC_HANDOVER_RESTRICTION_INFORMATION = 10,
    XNAP_PROC_RAN_PAGINGING = 11,
    XNAP_PROC_XN_GENERAL = 12,
    XNAP_PROC_XN_GENERAL_TRANSFER = 13,
    XNAP_PROC_DUAL_CONNECTIVITY_PREPARATION = 14,
    XNAP_PROC_XN_INTERFACE_RECONFIGURATION = 15,
    XNAP_PROC_SIB_TRANSFER = 16,
    XNAP_PROC_ACCESS_AND_MOBILITY_INDICATION = 17,
    XNAP_PROC_LOCATION_REPORTING = 18,
    XNAP_PROC_NEIGHBOR_CELL_INFORMATION_EXCHANGE = 19
} xnap_procedure_code_t;

/* ============== XnAP Message Types ============== */

typedef enum {
    /* Xn Setup */
    XNAP_MSG_XN_SETUP_REQUEST = 0,
    XNAP_MSG_XN_SETUP_RESPONSE,
    XNAP_MSG_XN_SETUP_FAILURE,
    
    /* Xn Reset */
    XNAP_MSG_XN_RESET_REQUEST,
    XNAP_MSG_XN_RESET_RESPONSE,
    
    /* Error Indication */
    XNAP_MSG_ERROR_INDICATION,
    
    /* Xn Removal */
    XNAP_MSG_XN_REMOVAL_REQUEST,
    XNAP_MSG_XN_REMOVAL_RESPONSE,
    
    /* Paging */
    XNAP_MSG_PAGING,
    
    /* Handover Preparation */
    XNAP_MSG_HANDOVER_REQUEST,
    XNAP_MSG_HANDOVER_REQUEST_ACKNOWLEDGE,
    XNAP_MSG_HANDOVER_PREPARATION_FAILURE,
    
    /* Handover Execution */
    XNAP_MSG_HANDOVER_COMMAND,
    
    /* Handover Cancel */
    XNAP_MSG_HANDOVER_CANCEL,
    XNAP_MSG_HANDOVER_CANCEL_ACKNOWLEDGE,
    
    /* Handover Notify */
    XNAP_MSG_HANDOVER_NOTIFY,
    
    /* Retrieve UE Context */
    XNAP_MSG_RETRIEVE_UE_CONTEXT_REQUEST,
    XNAP_MSG_RETRIEVE_UE_CONTEXT_RESPONSE,
    XNAP_MSG_RETRIEVE_UE_CONTEXT_FAILURE,
    
    /* UE Context Transfer */
    XNAP_MSG_UE_CONTEXT_TRANSFER,
    XNAP_MSG_UE_CONTEXT_TRANSFER_RESPONSE,
    
    /* Dual Connectivity */
    XNAP_MSG_SGNB_ADDITION_REQUEST,
    XNAP_MSG_SGNB_ADDITION_REQUEST_ACKNOWLEDGE,
    XNAP_MSG_SGNB_ADDITION_REQUEST_REJECT,
    XNAP_MSG_SGNB_MODIFICATION_REQUEST,
    XNAP_MSG_SGNB_MODIFICATION_REQUEST_ACKNOWLEDGE,
    XNAP_MSG_SGNB_MODIFICATION_REQUEST_REJECT,
    XNAP_MSG_SGNB_RELEASE_REQUEST,
    XNAP_MSG_SGNB_RELEASE_REQUEST_ACKNOWLEDGE,
    
    /* SIB Transfer */
    XNAP_MSG_SIB_TRANSFER_REQUEST,
    XNAP_MSG_SIB_TRANSFER_RESPONSE,
    
    /* Access and Mobility Indication */
    XNAP_MSG_ACCESS_AND_MOBILITY_INDICATION,
    
    /* Neighbor Information */
    XNAP_MSG_NEIGHBOR_CELL_INFORMATION_REQUEST,
    XNAP_MSG_NEIGHBOR_CELL_INFORMATION_RESPONSE,
    
    XNAP_MSG_MAX
} xnap_message_type_t;

/* ============== XnAP Cause Types ============== */

typedef enum {
    XNAP_CAUSE_RADIO_NETWORK = 0,
    XNAP_CAUSE_TRANSPORT,
    XNAP_CAUSE_PROTOCOL,
    XNAP_CAUSE_MISC
} xnap_cause_type_t;

/* Radio Network Cause Values */
typedef enum {
    XNAP_CAUSE_RADIO_UNSPECIFIED = 0,
    XNAP_CAUSE_RADIO_NORMAL_RELEASE = 1,
    XNAP_CAUSE_RADIO_RADIO_CONNECTION_WITH_UE_LOST = 2,
    XNAP_CAUSE_RADIO_HANDOVER_CANCELLED = 3,
    XNAP_CAUSE_RADIO_INACTIVITY_TIMER_EXPIRED = 4,
    XNAP_CAUSE_RADIO_UE_NOT_RESPONDING = 5,
    XNAP_CAUSE_RADIO_HANDOVER_FAILURE = 6,
    XNAP_CAUSE_RADIO_RLF_DETECTED = 7,
    XNAP_CAUSE_RADIO_NO_RADIO_RESOURCES_AVAILABLE = 8,
    XNAP_CAUSE_RADIO_UNSPECIFIED_QOS_REASON = 9,
    XNAP_CAUSE_RADIO_QOS_NOT_GUARANTEED = 10,
    XNAP_CAUSE_RADIO_RELEASE_DUE_TO_NGRAN_GENERATED_REASON = 11,
    XNAP_CAUSE_RADIO_REDIRECTION = 12,
    XNAP_CAUSE_RADIO_USER_INACTIVITY = 13,
    XNAP_CAUSE_RADIO_TIME_CRITICAL_RELOCATION = 14,
    XNAP_CAUSE_RADIO_TARGET_CELL_NOT_AVAILABLE = 15,
    XNAP_CAUSE_RADIO_NO_RESOURCE_AVAILABLE_IN_TARGET_CELL = 16,
    XNAP_CAUSE_RADIO_UNKNOWN_TARGET_ID = 17,
    XNAP_CAUSE_RADIO_NO_REPORTING_CELL_AVAILABLE = 18,
    XNAP_CAUSE_RADIO_TARGET_CELL_NOT_PREPARED = 19,
    XNAP_CAUSE_RADIO_PAGING_RETRANSMISSION_EXCEEDED = 20
} xnap_cause_radio_value_t;

/* Transport Cause Values */
typedef enum {
    XNAP_CAUSE_TRANSPORT_TRANSPORT_RESOURCE_UNAVAILABLE = 0,
    XNAP_CAUSE_TRANSPORT_UNSPECIFIED = 1
} xnap_cause_transport_value_t;

/* Protocol Cause Values */
typedef enum {
    XNAP_CAUSE_PROTOCOL_TRANSFER_SYNTAX_ERROR = 0,
    XNAP_CAUSE_PROTOCOL_ABSTRACT_SYNTAX_ERROR_REJECT = 1,
    XNAP_CAUSE_PROTOCOL_ABSTRACT_SYNTAX_ERROR_IGNORE_AND_NOTIFY = 2,
    XNAP_CAUSE_PROTOCOL_MESSAGE_NOT_COMPATIBLE_WITH_RECEIVER_STATE = 3,
    XNAP_CAUSE_PROTOCOL_SEMANTIC_ERROR = 4,
    XNAP_CAUSE_PROTOCOL_UNSPECIFIED = 5
} xnap_cause_protocol_value_t;

/* Misc Cause Values */
typedef enum {
    XNAP_CAUSE_MISC_CONTROL_PROCESSING_OVERLOAD = 0,
    XNAP_CAUSE_MISC_HARDWARE_FAILURE = 1,
    XNAP_CAUSE_MISC_OM_INTERVENTION = 2,
    XNAP_CAUSE_MISC_UNSPECIFIED = 3
} xnap_cause_misc_value_t;

/* ============== XnAP Cause Structure ============== */

typedef struct {
    xnap_cause_type_t cause_type;
    uint8_t cause_value;
} xnap_cause_t;

/* ============== XnAP Identity Structures ============== */

/* Global gNB ID */
typedef struct {
    uint8_t mcc[3];              /* Mobile Country Code */
    uint8_t mnc[3];              /* Mobile Network Code */
    uint8_t mnc_length;          /* 2 or 3 */
    uint64_t gnb_id;             /* 22-32 bits */
    uint8_t gnb_id_length;       /* Bits used in gnb_id */
} xnap_global_gnb_id_t;

/* gNB Name */
typedef struct {
    char name[64];
} xnap_gnb_name_t;

/* XnAP UE IDs */
typedef struct {
    uint32_t source_gnb_ue_xnap_id;   /* 32 bits */
    uint32_t target_gnb_ue_xnap_id;   /* 32 bits */
} xnap_ue_ids_t;

/* RAN UE ID */
typedef struct {
    uint64_t ran_ue_id;               /* 40 bits */
} xnap_ran_ue_id_t;

/* AMF UE ID */
typedef struct {
    uint64_t amf_ue_id;               /* 40 bits */
} xnap_amf_ue_id_t;

/* ============== XnAP Cell Information ============== */

/* NR Cell Identity */
typedef struct {
    uint64_t nr_cell_id;         /* 36 bits */
} xnap_nr_cell_id_t;

/* NR PCI */
typedef struct {
    uint16_t pci;                /* 0-1007 */
} xnap_pci_t;

/* TAC */
typedef struct {
    uint32_t tac;               /* 24-bit Tracking Area Code */
} xnap_tac_t;

/* PLMN Identity */
typedef struct {
    uint8_t mcc[3];
    uint8_t mnc[3];
    uint8_t mnc_length;
} xnap_plmn_id_t;

/* Served Cell Information */
typedef struct {
    xnap_nr_cell_id_t nr_cell_id;
    xnap_pci_t pci;
    xnap_tac_t tac;
    uint8_t num_plmns;
    xnap_plmn_id_t plmns[XNAP_MAX_PLMN_COUNT];
    uint8_t num_slices;
    uint8_t sst[XNAP_MAX_SLICE_COUNT];
    uint32_t sd[XNAP_MAX_SLICE_COUNT];
    uint16_t ngran_duplex_mode;  /* TDD=1, FDD=2 */
} xnap_served_cell_info_t;

/* Neighbor Cell Information */
typedef struct {
    xnap_nr_cell_id_t nr_cell_id;
    xnap_pci_t pci;
    uint8_t nr_mode_info;        /* FDD=0, TDD=1 */
    int16_t ssb_frequency;
    uint8_t subcarrier_spacing;
} xnap_neighbor_cell_info_t;

/* ============== XnAP TNL Information ============== */

typedef struct {
    uint32_t ip_address;
    uint16_t port;
    uint32_t teid;               /* GTP-U Tunnel Endpoint ID */
    uint8_t transport_type;      /* 0=IPv4, 1=IPv6 */
} xnap_tnl_info_t;

/* ============== XnAP QoS Flow Info ============== */

typedef struct {
    uint8_t qfi;                 /* QoS Flow Identifier */
    uint8_t five_qi;             /* 5G QoS Identifier */
    uint64_t gfbr_ul;
    uint64_t gfbr_dl;
    uint64_t mfbr_ul;
    uint64_t mfbr_dl;
} xnap_qos_flow_t;

/* ============== XnAP DRB Info ============== */

typedef struct {
    uint8_t drb_id;              /* 1-32 */
    uint8_t pdcp_sn_size;        /* 12 or 18 bits */
    uint8_t rlc_mode;            /* 1=AM, 2=UM */
    uint64_t ul_aggregate_maximum_bit_rate;
    uint64_t dl_aggregate_maximum_bit_rate;
    
    /* QoS Flows */
    uint8_t num_qos_flows;
    xnap_qos_flow_t qos_flows[XNAP_MAX_QOS_FLOWS];
    
    /* DL TNL Info */
    uint8_t num_dl_up_tnl_info;
    xnap_tnl_info_t dl_up_tnl_info[XNAP_MAX_DRB_COUNT];
} xnap_drb_info_t;

/* ============== XnAP RRC Container ============== */

typedef struct {
    uint8_t rrc_container[XNAP_MAX_RRC_CONTAINER_SIZE];
    size_t length;
} xnap_rrc_container_t;

/* ============== XnAP Message Structures ============== */

/* Xn Setup Request */
typedef struct {
    xnap_global_gnb_id_t global_gnb_id;
    xnap_gnb_name_t gnb_name;
    
    /* Served Cells */
    uint8_t num_served_cells;
    xnap_served_cell_info_t served_cells[XNAP_MAX_SERVED_CELL_COUNT];
    
    /* gNB-CU ID */
    uint64_t gnb_cu_id;
    uint8_t gnb_cu_id_present;
    
    /* gNB-DU ID */
    uint32_t gnb_du_id;
    uint8_t gnb_du_id_present;
} xnap_xn_setup_request_t;

/* Xn Setup Response */
typedef struct {
    xnap_global_gnb_id_t global_gnb_id;
    xnap_gnb_name_t gnb_name;
    
    /* Served Cells */
    uint8_t num_served_cells;
    xnap_served_cell_info_t served_cells[XNAP_MAX_SERVED_CELL_COUNT];
    
    /* gNB-CU ID */
    uint64_t gnb_cu_id;
    uint8_t gnb_cu_id_present;
} xnap_xn_setup_response_t;

/* Xn Setup Failure */
typedef struct {
    xnap_cause_t cause;
    uint32_t time_to_wait;       /* Seconds */
} xnap_xn_setup_failure_t;

/* Xn Reset Request */
typedef struct {
    uint8_t reset_type;          /* 0=All, 1=Part of Xn interface */
    xnap_ue_ids_t ue_ids;        /* Valid if reset_type == 1 */
    uint8_t ue_ids_present;
} xnap_xn_reset_request_t;

/* Xn Reset Response */
typedef struct {
    uint8_t reset_type;
    xnap_ue_ids_t ue_ids;
    uint8_t ue_ids_present;
} xnap_xn_reset_response_t;

/* Handover Request */
typedef struct {
    xnap_ue_ids_t ue_ids;
    xnap_amf_ue_id_t amf_ue_id;
    xnap_ran_ue_id_t ran_ue_id;
    xnap_global_gnb_id_t target_gnb_id;
    xnap_nr_cell_id_t target_cell_id;
    
    /* Source to Target Transparent Container */
    xnap_rrc_container_t source_to_target_container;
    
    /* DRBs to Setup */
    uint8_t num_drbs_to_setup;
    xnap_drb_info_t drbs_to_setup[XNAP_MAX_DRB_COUNT];
    
    /* UE AMBR */
    uint64_t ue_ambr_dl;
    uint64_t ue_ambr_ul;
    
    /* S-NSSAI */
    uint8_t sst;
    uint32_t sd;
    uint8_t snssai_present;
    
    /* UE Security */
    uint8_t security_key[32];
    uint8_t security_key_present;
    
    /* PLMN */
    xnap_plmn_id_t plmn;
} xnap_handover_request_t;

/* Handover Request Acknowledge */
typedef struct {
    xnap_ue_ids_t ue_ids;
    
    /* Target to Source Transparent Container */
    xnap_rrc_container_t target_to_source_container;
    
    /* DRBs Setup */
    uint8_t num_drbs_setup;
    xnap_drb_info_t drbs_setup[XNAP_MAX_DRB_COUNT];
    
    /* DRBs Failed */
    uint8_t num_drbs_failed;
    uint8_t drbs_failed[XNAP_MAX_DRB_COUNT];
    xnap_cause_t drb_fail_causes[XNAP_MAX_DRB_COUNT];
} xnap_handover_request_ack_t;

/* Handover Preparation Failure */
typedef struct {
    xnap_ue_ids_t ue_ids;
    xnap_cause_t cause;
} xnap_handover_preparation_failure_t;

/* Handover Command */
typedef struct {
    xnap_ue_ids_t ue_ids;
    
    /* Handover Command RRC Container */
    xnap_rrc_container_t handover_command;
} xnap_handover_command_t;

/* Handover Cancel */
typedef struct {
    xnap_ue_ids_t ue_ids;
    xnap_cause_t cause;
} xnap_handover_cancel_t;

/* Handover Cancel Acknowledge */
typedef struct {
    xnap_ue_ids_t ue_ids;
} xnap_handover_cancel_ack_t;

/* Handover Notify */
typedef struct {
    xnap_ue_ids_t ue_ids;
    xnap_nr_cell_id_t target_cell_id;
    xnap_pci_t pci;
} xnap_handover_notify_t;

/* Paging */
typedef struct {
    xnap_amf_ue_id_t amf_ue_id;
    
    /* UE Identity Index */
    uint16_t ue_identity_index;
    
    /* Paging Identity */
    uint8_t paging_identity_type;  /* 0=5G-S-TMSI, 1=I-RNTI */
    uint64_t paging_identity;
    
    /* TAI List */
    uint8_t num_tais;
    xnap_tac_t tai_list[XNAP_MAX_TAI_COUNT];
    
    /* Paging Cause */
    uint8_t paging_cause;          /* 0=normal, 1=emergency */
    
    /* Assistance Data for Paging */
    uint8_t assistance_data_present;
    uint8_t paging_attempt_count;
    uint8_t intended_n_paging_attempts;
} xnap_paging_t;

/* Retrieve UE Context Request */
typedef struct {
    xnap_ue_ids_t ue_ids;
    xnap_ran_ue_id_t ran_ue_id;
} xnap_retrieve_ue_context_request_t;

/* Retrieve UE Context Response */
typedef struct {
    xnap_ue_ids_t ue_ids;
    xnap_rrc_container_t ue_context;
} xnap_retrieve_ue_context_response_t;

/* UE Context Transfer */
typedef struct {
    xnap_ue_ids_t ue_ids;
    xnap_ran_ue_id_t ran_ue_id;
    xnap_rrc_container_t ue_context;
} xnap_ue_context_transfer_t;

/* SgNB Addition Request (Dual Connectivity) */
typedef struct {
    xnap_ue_ids_t ue_ids;
    xnap_ran_ue_id_t ran_ue_id;
    xnap_nr_cell_id_t target_cell_id;
    
    /* DRBs to Setup */
    uint8_t num_drbs_to_setup;
    xnap_drb_info_t drbs_to_setup[XNAP_MAX_DRB_COUNT];
    
    /* UE Security */
    uint8_t security_key[32];
    uint8_t security_key_present;
} xnap_sgnb_addition_request_t;

/* SgNB Addition Request Acknowledge */
typedef struct {
    xnap_ue_ids_t ue_ids;
    
    /* DRBs Setup */
    uint8_t num_drbs_setup;
    xnap_drb_info_t drbs_setup[XNAP_MAX_DRB_COUNT];
} xnap_sgnb_addition_request_ack_t;

/* Error Indication */
typedef struct {
    xnap_ue_ids_t ue_ids;
    uint8_t ue_ids_present;
    xnap_cause_t cause;
    uint8_t cause_present;
} xnap_error_indication_t;

/* Neighbor Cell Information Request */
typedef struct {
    xnap_nr_cell_id_t requested_cell_id;
    uint8_t information_type;     /* 0=CGI, 1=TA, 2=both */
} xnap_neighbor_cell_info_request_t;

/* Neighbor Cell Information Response */
typedef struct {
    xnap_nr_cell_id_t cell_id;
    xnap_plmn_id_t plmn;
    xnap_tac_t tac;
    uint8_t success;
} xnap_neighbor_cell_info_response_t;

/* ============== XnAP Message Union ============== */

typedef struct {
    xnap_message_type_t message_type;
    xnap_procedure_code_t procedure_code;
    uint8_t criticality;         /* 0=reject, 1=ignore, 2=notify */
    
    union {
        xnap_xn_setup_request_t xn_setup_request;
        xnap_xn_setup_response_t xn_setup_response;
        xnap_xn_setup_failure_t xn_setup_failure;
        xnap_xn_reset_request_t xn_reset_request;
        xnap_xn_reset_response_t xn_reset_response;
        xnap_handover_request_t handover_request;
        xnap_handover_request_ack_t handover_request_ack;
        xnap_handover_preparation_failure_t handover_preparation_failure;
        xnap_handover_command_t handover_command;
        xnap_handover_cancel_t handover_cancel;
        xnap_handover_cancel_ack_t handover_cancel_ack;
        xnap_handover_notify_t handover_notify;
        xnap_paging_t paging;
        xnap_retrieve_ue_context_request_t retrieve_ue_context_request;
        xnap_retrieve_ue_context_response_t retrieve_ue_context_response;
        xnap_ue_context_transfer_t ue_context_transfer;
        xnap_sgnb_addition_request_t sgnb_addition_request;
        xnap_sgnb_addition_request_ack_t sgnb_addition_request_ack;
        xnap_error_indication_t error_indication;
        xnap_neighbor_cell_info_request_t neighbor_cell_info_request;
        xnap_neighbor_cell_info_response_t neighbor_cell_info_response;
    } payload;
} xnap_message_t;

/* ============== XnAP API Functions ============== */

/* Message encoding/decoding */
int xnap_encode_message(const xnap_message_t* msg, uint8_t** buffer, size_t* length);
int xnap_decode_message(const uint8_t* buffer, size_t length, xnap_message_t* msg);
void xnap_free_message(xnap_message_t* msg);

/* Message type utilities */
const char* xnap_message_type_to_string(xnap_message_type_t type);
const char* xnap_cause_to_string(const xnap_cause_t* cause);

/* Message initialization helpers */
void xnap_init_xn_setup_request(xnap_message_t* msg);
void xnap_init_xn_setup_response(xnap_message_t* msg);
void xnap_init_handover_request(xnap_message_t* msg);
void xnap_init_handover_command(xnap_message_t* msg);
void xnap_init_paging(xnap_message_t* msg);

/* Cause helpers */
void xnap_set_cause_radio(xnap_cause_t* cause, xnap_cause_radio_value_t value);
void xnap_set_cause_transport(xnap_cause_t* cause, xnap_cause_transport_value_t value);
void xnap_set_cause_protocol(xnap_cause_t* cause, xnap_cause_protocol_value_t value);
void xnap_set_cause_misc(xnap_cause_t* cause, xnap_cause_misc_value_t value);

#endif /* XNAP_MESSAGES_H */