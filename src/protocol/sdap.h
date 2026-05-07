/*
 * 5G UE Simulation Application
 * SDAP (Service Data Adaptation Protocol) Layer Header
 * 
 * Implements 3GPP TS 37.324 SDAP specification
 */

#ifndef SDAP_H
#define SDAP_H

#include "../uesim.h"

/* SDAP Constants */
#define SDAP_MAX_PDU_SIZE       8192
#define SDAP_MAX_QOS_FLOWS      64
#define SDAP_MAX_DRB            32
#define SDAP_QFI_BITS           6
#define SDAP_RQ_BIT             0x01

/* SDAP PDU Types */
typedef enum {
    SDAP_PDU_DATA = 0,
    SDAP_PDU_CONTROL = 1
} sdap_pdu_type_t;

/* SDAP Header Structure (Data PDU) */
typedef struct {
    uint8_t qfi : 6;            /* QoS Flow Identifier */
    uint8_t rqi : 1;            /* Reflective QoS Indication */
    uint8_t dc : 1;             /* Data/Control field (0=Data, 1=Control) */
} sdap_data_header_t;

/* SDAP Control PDU Header */
typedef struct {
    uint8_t pdu_type : 4;       /* PDU type */
    uint8_t reserved : 4;       /* Reserved bits */
} sdap_control_header_t;

/* SDAP End Marker Control PDU */
typedef struct {
    sdap_control_header_t header;
    uint8_t qfi;                /* QoS Flow Identifier to end */
} sdap_end_marker_t;

/* SDAP Reflective QoS Flow Control PDU */
typedef struct {
    sdap_control_header_t header;
    uint8_t qfi;                /* QoS Flow Identifier */
    uint8_t drb_id;             /* DRB ID to map to */
} sdap_reflective_qos_t;

/* QoS Flow to DRB Mapping Entry */
typedef struct {
    uint8_t qfi;                /* QoS Flow Identifier */
    uint8_t drb_id;             /* Associated DRB ID */
    bool reflective_qos;        /* Reflective QoS enabled */
    bool active;                /* Flow is active */
} sdap_qos_mapping_t;

/* SDAP Entity Configuration */
typedef struct {
    uint32_t pdu_session_id;    /* PDU Session ID */
    uint8_t num_qos_flows;      /* Number of QoS flows */
    sdap_qos_mapping_t qos_mappings[SDAP_MAX_QOS_FLOWS];
    bool default_drb_configured;
    uint8_t default_drb_id;     /* Default DRB for unmapped flows */
} sdap_config_t;

/* SDAP Entity Statistics */
typedef struct {
    uint64_t tx_pdus;           /* Transmitted PDUs */
    uint64_t tx_bytes;          /* Transmitted bytes */
    uint64_t rx_pdus;           /* Received PDUs */
    uint64_t rx_bytes;          /* Received bytes */
    uint64_t tx_control_pdus;   /* Transmitted control PDUs */
    uint64_t rx_control_pdus;   /* Received control PDUs */
    uint64_t mapping_errors;    /* QoS flow mapping errors */
} sdap_stats_t;

/* SDAP Entity State */
typedef struct {
    uint32_t entity_id;         /* Entity identifier */
    uint32_t pdu_session_id;    /* Associated PDU Session */
    
    /* Configuration */
    sdap_config_t config;
    
    /* QoS Flow to DRB mapping table */
    sdap_qos_mapping_t qos_flow_map[SDAP_MAX_QOS_FLOWS];
    
    /* Statistics */
    sdap_stats_t stats;
    
    /* State */
    bool active;
    pthread_mutex_t entity_mutex;
    
} sdap_entity_t;

/* SDAP PDU Structure */
typedef struct sdap_pdu {
    uint8_t* data;              /* PDU data */
    size_t data_length;         /* Length of data */
    uint8_t qfi;                /* QoS Flow Identifier */
    bool is_control;            /* Control PDU flag */
    struct sdap_pdu* next;      /* Next PDU in chain */
} sdap_pdu_t;

/* SDAP SDU Structure */
typedef struct sdap_sdu {
    uint8_t* data;              /* SDU data */
    size_t data_length;         /* Length of data */
    uint8_t qfi;                /* QoS Flow Identifier */
    uint8_t drb_id;             /* Target DRB ID */
    struct sdap_sdu* next;      /* Next SDU in chain */
} sdap_sdu_t;

/* Function Prototypes */

/* Initialization and Cleanup */
uesim_error_t sdap_init(ue_context_t* ue_ctx);
void sdap_cleanup(ue_context_t* ue_ctx);

/* Entity Management */
uesim_error_t sdap_create_entity(ue_context_t* ue_ctx, uint32_t pdu_session_id,
                                 const sdap_config_t* config, sdap_entity_t** entity);
uesim_error_t sdap_destroy_entity(ue_context_t* ue_ctx, sdap_entity_t* entity);
uesim_error_t sdap_configure_entity(sdap_entity_t* entity, const sdap_config_t* config);
uesim_error_t sdap_activate_entity(sdap_entity_t* entity);
uesim_error_t sdap_deactivate_entity(sdap_entity_t* entity);

/* QoS Flow Management */
uesim_error_t sdap_map_qos_flow(sdap_entity_t* entity, uint8_t qfi, uint8_t drb_id,
                                bool reflective_qos);
uesim_error_t sdap_unmap_qos_flow(sdap_entity_t* entity, uint8_t qfi);
uesim_error_t sdap_update_qos_mapping(sdap_entity_t* entity, uint8_t qfi,
                                      uint8_t new_drb_id);
int8_t sdap_get_drb_for_qos(sdap_entity_t* entity, uint8_t qfi);
bool sdap_is_reflective_qos(sdap_entity_t* entity, uint8_t qfi);

/* Data Processing - Uplink */
uesim_error_t sdap_process_ul_sdu(sdap_entity_t* entity, const uint8_t* data,
                                  size_t length, uint8_t qfi, sdap_pdu_t** pdu);
uesim_error_t sdap_process_ul_pdu(sdap_entity_t* entity, const sdap_pdu_t* pdu,
                                  uint8_t* drb_id);

/* Data Processing - Downlink */
uesim_error_t sdap_process_dl_pdu(sdap_entity_t* entity, const uint8_t* pdu_data,
                                  size_t pdu_length, uint8_t drb_id,
                                  sdap_sdu_t** sdu);
uesim_error_t sdap_process_dl_sdu(sdap_entity_t* entity, const sdap_sdu_t* sdu);

/* Control PDU Handling */
uesim_error_t sdap_send_end_marker(sdap_entity_t* entity, uint8_t qfi,
                                   sdap_pdu_t** pdu);
uesim_error_t sdap_send_reflective_qos(sdap_entity_t* entity, uint8_t qfi,
                                       uint8_t drb_id, sdap_pdu_t** pdu);
uesim_error_t sdap_handle_control_pdu(sdap_entity_t* entity, const uint8_t* data,
                                      size_t length);

/* PDU Construction */
uesim_error_t sdap_build_data_pdu(sdap_entity_t* entity, const uint8_t* sdu_data,
                                  size_t sdu_length, uint8_t qfi, bool rqi,
                                  sdap_pdu_t** pdu);
uesim_error_t sdap_parse_data_pdu(sdap_entity_t* entity, const uint8_t* pdu_data,
                                  size_t pdu_length, uint8_t* qfi, bool* rqi,
                                  uint8_t** sdu_data, size_t* sdu_length);

/* SDU/PDU Memory Management */
uesim_error_t sdap_create_pdu(size_t data_length, sdap_pdu_t** pdu);
void sdap_destroy_pdu(sdap_pdu_t* pdu);
uesim_error_t sdap_create_sdu(size_t data_length, sdap_sdu_t** sdu);
void sdap_destroy_sdu(sdap_sdu_t* sdu);

/* Statistics */
uesim_error_t sdap_get_stats(sdap_entity_t* entity, sdap_stats_t* stats);
uesim_error_t sdap_reset_stats(sdap_entity_t* entity);

/* Utility Functions */
const char* sdap_pdu_type_to_string(sdap_pdu_type_t type);

#endif /* SDAP_H */