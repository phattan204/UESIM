/*
 * 5G UE Simulation Application
 * PFCP (Packet Forwarding Control Protocol) Message Definitions
 * 3GPP TS 29.244 - SMF to UPF Interface
 */

#ifndef PFCP_MESSAGES_H
#define PFCP_MESSAGES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ============== PFCP Constants ============== */

#define PFCP_PORT                   8805
#define PFCP_MAX_MESSAGE_SIZE       65535
#define PFCP_MAX_PDR_ID             65535
#define PFCP_MAX_FAR_ID             4294967295
#define PFCP_MAX_QER_ID             4294967295
#define PFCP_MAX_URR_ID             4294967295
#define PFCP_MAX_BAR_ID             255
#define PFCP_MAX_PDR_PER_SESSION    64
#define PFCP_MAX_FAR_PER_SESSION    64
#define PFCP_MAX_QER_PER_SESSION    64
#define PFCP_MAX_URR_PER_SESSION    32

/* ============== PFCP Message Types ============== */

typedef enum {
    PFCP_MSG_HEARTBEAT_REQUEST = 1,
    PFCP_MSG_HEARTBEAT_RESPONSE = 2,
    PFCP_MSG_PFD_MANAGEMENT_REQUEST = 3,
    PFCP_MSG_PFD_MANAGEMENT_RESPONSE = 4,
    PFCP_MSG_ASSOCIATION_SETUP_REQUEST = 5,
    PFCP_MSG_ASSOCIATION_SETUP_RESPONSE = 6,
    PFCP_MSG_ASSOCIATION_UPDATE_REQUEST = 7,
    PFCP_MSG_ASSOCIATION_UPDATE_RESPONSE = 8,
    PFCP_MSG_ASSOCIATION_RELEASE_REQUEST = 9,
    PFCP_MSG_ASSOCIATION_RELEASE_RESPONSE = 10,
    PFCP_MSG_VERSION_NOT_SUPPORTED_RESPONSE = 11,
    PFCP_MSG_NODE_REPORT_REQUEST = 12,
    PFCP_MSG_NODE_REPORT_RESPONSE = 13,
    PFCP_MSG_SESSION_SET_DELETION_REQUEST = 14,
    PFCP_MSG_SESSION_SET_DELETION_RESPONSE = 15,
    PFCP_MSG_SESSION_ESTABLISHMENT_REQUEST = 50,
    PFCP_MSG_SESSION_ESTABLISHMENT_RESPONSE = 51,
    PFCP_MSG_SESSION_MODIFICATION_REQUEST = 52,
    PFCP_MSG_SESSION_MODIFICATION_RESPONSE = 53,
    PFCP_MSG_SESSION_DELETION_REQUEST = 54,
    PFCP_MSG_SESSION_DELETION_RESPONSE = 55,
    PFCP_MSG_SESSION_REPORT_REQUEST = 56,
    PFCP_MSG_SESSION_REPORT_RESPONSE = 57
} pfcp_message_type_t;

/* ============== PFCP IE Types ============== */

typedef enum {
    PFCP_IE_CREATE_PDR = 1,
    PFCP_IE_PDI = 2,
    PFCP_IE_CREATE_FAR = 3,
    PFCP_IE_FORWARDING_PARAMETERS = 4,
    PFCP_IE_DUPLICATING_PARAMETERS = 5,
    PFCP_IE_CREATE_URR = 6,
    PFCP_IE_CREATE_QER = 7,
    PFCP_IE_CREATED_PDR = 8,
    PFCP_IE_UPDATE_PDR = 9,
    PFCP_IE_UPDATE_FAR = 10,
    PFCP_IE_UPDATE_FORWARDING_PARAMETERS = 11,
    PFCP_IE_UPDATE_BAR = 12,
    PFCP_IE_UPDATE_URR = 13,
    PFCP_IE_UPDATE_QER = 14,
    PFCP_IE_REMOVE_PDR = 15,
    PFCP_IE_REMOVE_FAR = 16,
    PFCP_IE_REMOVE_URR = 17,
    PFCP_IE_REMOVE_QER = 18,
    PFCP_IE_CAUSE = 19,
    PFCP_IE_SOURCE_INTERFACE = 20,
    PFCP_IE_F_TEID = 21,
    PFCP_IE_NETWORK_INSTANCE = 22,
    PFCP_IE_SDF_FILTER = 23,
    PFCP_IE_APPLICATION_ID = 24,
    PFCP_IE_GATE_STATUS = 25,
    PFCP_IE_MBR = 26,
    PFCP_IE_GBR = 27,
    PFCP_IE_QER_CORRELATION_ID = 28,
    PFCP_IE_SOURCE_IF_TYPE = 29,
    PFCP_IE_DESTINATION_INTERFACE = 30,
    PFCP_IE_TRANSPORT_LEVEL_MARKING = 31,
    PFCP_IE_FORWARDING_POLICY = 32,
    PFCP_IE_HEADER_ENRICHMENT = 33,
    PFCP_IE_TRIGGER_TYPE = 34,
    PFCP_IE_QUOTA_HOLD_TIME = 35,
    PFCP_IE_VOLUME_THRESHOLD = 36,
    PFCP_IE_TIME_THRESHOLD = 37,
    PFCP_IE_MONITORING_TIME = 38,
    PFCP_IE_SUBSEQUENT_VOLUME_THRESHOLD = 39,
    PFCP_IE_SUBSEQUENT_TIME_THRESHOLD = 40,
    PFCP_IE_INACTIVITY_DETECTION_TIME = 41,
    PFCP_IE_REPORTING_TRIGGERS = 42,
    PFCP_IE_REDIRECT_INFORMATION = 43,
    PFCP_IE_REPORT_TYPE = 44,
    PFCP_IE_DOWNLINK_DATA_SERVICE_INFORMATION = 45,
    PFCP_IE_DOWNLINK_DATA_REPORT = 46,
    PFCP_IE_PFCPASRSP_FLAGS = 47,
    PFCP_IE_PFCPAUREQ_FLAGS = 48,
    PFCP_IE_PFCPAURSP_FLAGS = 49,
    PFCP_IE_FQCSID = 50,
    PFCP_IE_RATE_LIMIT = 51,
    PFCP_IE_SEID = 52,
    PFCP_IE_NODE_ID = 60,
    PFCP_IE_PFD_CONTENTS = 61,
    PFCP_IE_MEASUREMENT_METHOD = 62,
    PFCP_IE_USAGE_REPORT_TRIGGER = 63,
    PFCP_IE_VOLUME_QUOTA = 64,
    PFCP_IE_TIME_QUOTA = 65,
    PFCP_IE_QUOTA_VALIDITY_TIME = 66,
    PFCP_IE_APPLIED_ENFORCING_ACTION = 67,
    PFCP_IE_SEID_RANGE = 68,
    PFCP_IE_SEQUENCE_NUMBER = 69,
    PFCP_IE_METRIC = 70,
    PFCP_IE_TIME_OF_FIRST_PACKET = 71,
    PFCP_IE_TIME_OF_LAST_PACKET = 72,
    PFCP_IE_VOLUME_MEASUREMENT = 73,
    PFCP_IE_DURATION_MEASUREMENT = 74,
    PFCP_IE_RECOVERY_TIME_STAMP = 75,
    PFCP_IE_PFCPSR_REQ_FLAGS = 76,
    PFCP_IE_PFCPSR_RSP_FLAGS = 77,
    PFCP_IE_PFCPA_REQ_FLAGS = 78,
    PFCP_IE_PFCPA_RSP_FLAGS = 79,
    PFCP_IE_AVERAGING_WINDOW = 80,
    PFCP_IE_PAGING_POLICY_INDICATOR = 81,
    PFCP_IE_APN_DNN = 82,
    PFCP_IE_UE_IP_ADDRESS = 93,
    PFCP_IE_PACKET_RATE = 94,
    PFCP_IE_OUTER_HEADER_CREATION = 84,
    PFCP_IE_AGGREGATED_URRS = 95,
    PFCP_IE_PRIORITY = 96,
    PFCP_IE_MPTCP_CONTROL_INFORMATION = 97,
    PFCP_IE_MPTCP_ADDRESS = 98,
    PFCP_IE_FQDN = 99,
    PFCP_IE_MAC_ADDRESS = 100,
    PFCP_IE_CP_FUNCTION_FEATURES = 101,
    PFCP_IE_UP_FUNCTION_FEATURES = 102,
    PFCP_IE_FLOW_INFORMATION = 103,
    PFCP_IE_TIME_STAMP = 104,
    PFCP_IE_F_SEID = 105,
    PFCP_IE_PDR_ID = 56,
    PFCP_IE_FAR_ID = 108,
    PFCP_IE_QER_ID = 109,
    PFCP_IE_URR_ID = 110,
    PFCP_IE_BAR_ID = 111
} pfcp_ie_type_t;

/* ============== PFCP Cause Values ============== */

typedef enum {
    PFCP_CAUSE_REQUEST_ACCEPTED = 1,
    PFCP_CAUSE_REQUEST_REJECTED = 2,
    PFCP_CAUSE_SESSION_CONTEXT_NOT_FOUND = 3,
    PFCP_CAUSE_MANDATORY_IE_MISSING = 4,
    PFCP_CAUSE_MANDATORY_IE_INCORRECT = 5,
    PFCP_CAUSE_SYSTEM_FAILURE = 6,
    PFCP_CAUSE_REQUEST_TIMEOUT = 7,
    PFCP_CAUSE_PFCP_ERROR_INDICATION_RECEIVED = 8,
    PFCP_CAUSE_NO_ESTABLISHED_PFCP_ASSOCIATION = 9,
    PFCP_CAUSE_RULE_CREATION_MODIFICATION_FAILURE = 10,
    PFCP_CAUSE_PFCP_ENTITY_IN_CONGESTION = 11,
    PFCP_CAUSE_NO_RESOURCES_AVAILABLE = 12,
    PFCP_CAUSE_SERVICE_NOT_SUPPORTED = 13,
    PFCP_CAUSE_SYSTEM_FAILURE_2 = 14
} pfcp_cause_t;

/* ============== Source/Destination Interface Types ============== */

typedef enum {
    PFCP_INTERFACE_ACCESS = 0,
    PFCP_INTERFACE_CORE = 1,
    PFCP_INTERFACE_SGI_LAN = 2,
    PFCP_INTERFACE_CP_FUNCTION = 3,
    PFCP_INTERFACE_LI_FUNCTION = 4,
    PFCP_INTERFACE_N6_LAN = 5,
    PFCP_INTERFACE_N9_GTP = 6
} pfcp_interface_t;

/* ============== Gate Status ============== */

typedef enum {
    PFCP_GATE_OPEN = 0,
    PFCP_GATE_CLOSED = 1
} pfcp_gate_status_t;

/* ============== Apply Action Flags ============== */

typedef enum {
    PFCP_ACTION_DROP = 0x01,
    PFCP_ACTION_FORW = 0x02,
    PFCP_ACTION_BUFF = 0x04,
    PFCP_ACTION_NOCP = 0x08,
    PFCP_ACTION_DUPL = 0x10,
    PFCP_ACTION_IPMA = 0x20,
    PFCP_ACTION_DDPN = 0x40,
    PFCP_ACTION_BDPN = 0x80
} pfcp_apply_action_t;

/* ============== PFCP Node ID ============== */

typedef struct {
    uint8_t node_id_type;       /* 0=IPv4, 1=IPv6, 2=FQDN */
    uint32_t ipv4_address;
    uint8_t ipv6_address[16];
    char fqdn[256];
} pfcp_node_id_t;

/* ============== PFCP F-SEID (SEID with IP) ============== */

typedef struct {
    uint64_t seid;              /* Session Endpoint Identifier */
    uint32_t ipv4_address;
    uint8_t ipv6_address[16];
    uint8_t v4_present;
    uint8_t v6_present;
} pfcp_f_seid_t;

/* ============== PFCP F-TEID ============== */

typedef struct {
    uint32_t teid;              /* Tunnel Endpoint Identifier */
    uint32_t ipv4_address;
    uint8_t ipv6_address[16];
    uint8_t v4_present;
    uint8_t v6_present;
    uint8_t chid;               /* Choose ID flag */
    uint8_t choose_id;
} pfcp_f_teid_t;

/* ============== PFCP UE IP Address ============== */

typedef struct {
    uint32_t ipv4_address;
    uint8_t ipv6_address[16];
    uint8_t v4_present;
    uint8_t v6_present;
    uint8_t sd;                 /* Source/Destination flag */
} pfcp_ue_ip_address_t;

/* ============== PFCP PDI (Packet Detection Information) ============== */

typedef struct {
    uint8_t source_interface;
    uint8_t network_instance[256];
    uint8_t network_instance_len;
    
    /* UE IP Address */
    uint8_t ue_ip_present;
    pfcp_ue_ip_address_t ue_ip;
    
    /* F-TEID (for GTP-U detection) */
    uint8_t f_teid_present;
    pfcp_f_teid_t f_teid;
    
    /* SDF Filter */
    uint8_t sdf_filter_present;
    uint8_t sdf_filter[256];
    uint8_t sdf_filter_len;
    
    /* Application ID */
    uint8_t app_id_present;
    uint8_t app_id[64];
    uint8_t app_id_len;
} pfcp_pdi_t;

/* ============== PFCP PDR (Packet Detection Rule) ============== */

typedef struct {
    uint16_t pdr_id;
    uint8_t precedence;         /* 0-255, higher = lower precedence */
    
    /* PDI */
    pfcp_pdi_t pdi;
    
    /* Outer Header Removal */
    uint8_t outer_header_removal_present;
    uint8_t outer_header_removal;  /* 0=GTP-U/UDP/IPv4, 1=GTP-U/UDP/IPv6, 2=UDP/IPv4, etc */
    
    /* FAR ID */
    uint8_t far_id_present;
    uint32_t far_id;
    
    /* QER IDs */
    uint8_t num_qer_ids;
    uint32_t qer_ids[4];
    
    /* URR IDs */
    uint8_t num_urr_ids;
    uint32_t urr_ids[4];
} pfcp_pdr_t;

/* ============== PFCP FAR (Forwarding Action Rule) ============== */

typedef struct {
    uint32_t far_id;
    
    /* Apply Action */
    uint8_t apply_action;       /* DROP, FORW, BUFF, etc */
    
    /* Forwarding Parameters */
    uint8_t forwarding_params_present;
    uint8_t destination_interface;
    
    /* Outer Header Creation */
    uint8_t outer_header_creation_present;
    pfcp_f_teid_t outer_header_creation;
    
    /* Redirect Information */
    uint8_t redirect_present;
    uint8_t redirect_type;      /* 0=URL, 1=SIP URI, 2=IPv4, 3=IPv6 */
    uint8_t redirect_address[256];
    uint8_t redirect_address_len;
    
    /* Transport Level Marking (DSCP/ToS) */
    uint8_t transport_marking_present;
    uint16_t tos_traffic_class;
} pfcp_far_t;

/* ============== PFCP QER (QoS Enforcement Rule) ============== */

typedef struct {
    uint32_t qer_id;
    
    /* Gate Status */
    uint8_t ul_gate_status;
    uint8_t dl_gate_status;
    
    /* MBR (Maximum Bit Rate) */
    uint64_t ul_mbr;
    uint64_t dl_mbr;
    
    /* GBR (Guaranteed Bit Rate) */
    uint64_t ul_gbr;
    uint64_t dl_gbr;
    
    /* QoS Flow Identifier */
    uint8_t qfi_present;
    uint8_t qfi;
    
    /* QER Correlation ID */
    uint8_t qer_correlation_id_present;
    uint32_t qer_correlation_id;
} pfcp_qer_t;

/* ============== PFCP URR (Usage Reporting Rule) ============== */

typedef struct {
    uint32_t urr_id;
    
    /* Measurement Method */
    uint8_t measurement_method;  /* DURATION=1, VOLUME=2, EVENT=4 */
    
    /* Reporting Triggers */
    uint8_t reporting_triggers;  /* PERIO=1, VOLTH=2, TIMTH=4, etc */
    
    /* Volume Threshold */
    uint8_t volume_threshold_present;
    uint64_t total_volume_threshold;
    uint64_t uplink_volume_threshold;
    uint64_t downlink_volume_threshold;
    
    /* Time Threshold */
    uint8_t time_threshold_present;
    uint32_t time_threshold;
    
    /* Volume Quota */
    uint8_t volume_quota_present;
    uint64_t total_volume_quota;
    
    /* Time Quota */
    uint8_t time_quota_present;
    uint32_t time_quota;
    
    /* Quota Validity Time */
    uint8_t quota_validity_time_present;
    uint32_t quota_validity_time;
} pfcp_urr_t;

/* ============== PFCP Volume Measurement ============== */

typedef struct {
    uint64_t total_volume;
    uint64_t uplink_volume;
    uint64_t downlink_volume;
    uint8_t total_present;
    uint8_t uplink_present;
    uint8_t downlink_present;
} pfcp_volume_measurement_t;

/* ============== PFCP Usage Report ============== */

typedef struct {
    uint32_t urr_id;
    uint64_t start_time;
    uint64_t end_time;
    uint32_t duration_measurement;
    pfcp_volume_measurement_t volume_measurement;
} pfcp_usage_report_t;

/* ============== PFCP Session Establishment Request ============== */

typedef struct {
    pfcp_node_id_t cp_node_id;
    pfcp_f_seid_t cp_f_seid;
    
    /* PDRs */
    uint8_t num_create_pdr;
    pfcp_pdr_t create_pdr[PFCP_MAX_PDR_PER_SESSION];
    
    /* FARs */
    uint8_t num_create_far;
    pfcp_far_t create_far[PFCP_MAX_FAR_PER_SESSION];
    
    /* QERs */
    uint8_t num_create_qer;
    pfcp_qer_t create_qer[PFCP_MAX_QER_PER_SESSION];
    
    /* URRs */
    uint8_t num_create_urr;
    pfcp_urr_t create_urr[PFCP_MAX_URR_PER_SESSION];
    
    /* APN/DNN */
    uint8_t apn_dnn[256];
    uint8_t apn_dnn_len;
    
    /* S-NSSAI */
    uint8_t sst;
    uint32_t sd;
    uint8_t snssai_present;
} pfcp_session_establishment_request_t;

/* ============== PFCP Session Establishment Response ============== */

typedef struct {
    pfcp_node_id_t up_node_id;
    pfcp_cause_t cause;
    
    /* F-SEID */
    uint8_t up_f_seid_present;
    pfcp_f_seid_t up_f_seid;
    
    /* Created PDRs (with F-TEIDs) */
    uint8_t num_created_pdr;
    uint16_t created_pdr_ids[PFCP_MAX_PDR_PER_SESSION];
    pfcp_f_teid_t created_pdr_teids[PFCP_MAX_PDR_PER_SESSION];
} pfcp_session_establishment_response_t;

/* ============== PFCP Session Modification Request ============== */

typedef struct {
    pfcp_f_seid_t cp_f_seid;
    
    /* PDRs to Update */
    uint8_t num_update_pdr;
    pfcp_pdr_t update_pdr[PFCP_MAX_PDR_PER_SESSION];
    
    /* PDRs to Remove */
    uint8_t num_remove_pdr;
    uint16_t remove_pdr_ids[PFCP_MAX_PDR_PER_SESSION];
    
    /* FARs to Update */
    uint8_t num_update_far;
    pfcp_far_t update_far[PFCP_MAX_FAR_PER_SESSION];
    
    /* FARs to Remove */
    uint8_t num_remove_far;
    uint32_t remove_far_ids[PFCP_MAX_FAR_PER_SESSION];
    
    /* QERs to Update */
    uint8_t num_update_qer;
    pfcp_qer_t update_qer[PFCP_MAX_QER_PER_SESSION];
    
    /* QERs to Remove */
    uint8_t num_remove_qer;
    uint32_t remove_qer_ids[PFCP_MAX_QER_PER_SESSION];
} pfcp_session_modification_request_t;

/* ============== PFCP Session Modification Response ============== */

typedef struct {
    pfcp_cause_t cause;
    
    /* Created PDRs */
    uint8_t num_created_pdr;
    uint16_t created_pdr_ids[PFCP_MAX_PDR_PER_SESSION];
    pfcp_f_teid_t created_pdr_teids[PFCP_MAX_PDR_PER_SESSION];
} pfcp_session_modification_response_t;

/* ============== PFCP Session Deletion Request ============== */

typedef struct {
    pfcp_f_seid_t cp_f_seid;
} pfcp_session_deletion_request_t;

/* ============== PFCP Session Deletion Response ============== */

typedef struct {
    pfcp_cause_t cause;
    
    /* Usage Reports */
    uint8_t num_usage_reports;
    pfcp_usage_report_t usage_reports[PFCP_MAX_URR_PER_SESSION];
} pfcp_session_deletion_response_t;

/* ============== PFCP Association Setup Request ============== */

typedef struct {
    pfcp_node_id_t node_id;
    uint64_t recovery_time_stamp;
    uint8_t cp_function_features;
    uint8_t up_function_features_present;
    uint8_t up_function_features;
} pfcp_association_setup_request_t;

/* ============== PFCP Association Setup Response ============== */

typedef struct {
    pfcp_node_id_t node_id;
    pfcp_cause_t cause;
    uint64_t recovery_time_stamp;
    uint8_t up_function_features_present;
    uint8_t up_function_features;
    uint8_t cp_function_features;
} pfcp_association_setup_response_t;

/* ============== PFCP Session Report Request ============== */

typedef struct {
    pfcp_f_seid_t up_f_seid;
    
    /* Usage Reports */
    uint8_t num_usage_reports;
    pfcp_usage_report_t usage_reports[PFCP_MAX_URR_PER_SESSION];
    
    /* Downlink Data Report */
    uint8_t downlink_data_report_present;
    uint16_t pdr_id;
    uint8_t data_status;
} pfcp_session_report_request_t;

/* ============== PFCP Session Report Response ============== */

typedef struct {
    pfcp_cause_t cause;
} pfcp_session_report_response_t;

/* ============== PFCP Heartbeat ============== */

typedef struct {
    uint64_t recovery_time_stamp;
} pfcp_heartbeat_t;

/* ============== PFCP Message Union ============== */

typedef struct {
    pfcp_message_type_t message_type;
    uint32_t sequence_number;
    uint16_t message_length;
    uint8_t version;            /* PFCP version, typically 1 */
    uint8_t mp;                 /* Message Priority flag */
    uint8_t s;                  /* SEID present flag */
    uint64_t seid;              /* Session Endpoint ID */
    
    union {
        pfcp_heartbeat_t heartbeat;
        pfcp_association_setup_request_t association_setup_request;
        pfcp_association_setup_response_t association_setup_response;
        pfcp_session_establishment_request_t session_est_request;
        pfcp_session_establishment_response_t session_est_response;
        pfcp_session_modification_request_t session_mod_request;
        pfcp_session_modification_response_t session_mod_response;
        pfcp_session_deletion_request_t session_del_request;
        pfcp_session_deletion_response_t session_del_response;
        pfcp_session_report_request_t session_report_request;
        pfcp_session_report_response_t session_report_response;
    } payload;
} pfcp_message_t;

/* ============== PFCP API Functions ============== */

/* Message encoding/decoding */
int pfcp_encode_message(const pfcp_message_t* msg, uint8_t** buffer, size_t* length);
int pfcp_decode_message(const uint8_t* buffer, size_t length, pfcp_message_t* msg);
void pfcp_free_message(pfcp_message_t* msg);

/* Message type utilities */
const char* pfcp_message_type_to_string(pfcp_message_type_t type);
const char* pfcp_cause_to_string(pfcp_cause_t cause);

/* Message initialization helpers */
void pfcp_init_association_setup_request(pfcp_message_t* msg);
void pfcp_init_association_setup_response(pfcp_message_t* msg);
void pfcp_init_session_establishment_request(pfcp_message_t* msg);
void pfcp_init_session_establishment_response(pfcp_message_t* msg);
void pfcp_init_session_deletion_request(pfcp_message_t* msg);

/* IE helpers */
void pfcp_set_f_teid(pfcp_f_teid_t* teid, uint32_t teid_val, uint32_t ipv4);
void pfcp_set_ue_ip(pfcp_ue_ip_address_t* ue_ip, uint32_t ipv4);

#endif /* PFCP_MESSAGES_H */