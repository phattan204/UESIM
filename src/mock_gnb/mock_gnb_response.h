/*
 * 5G UE Simulation Application
 * Mock gNB Response Generator - Complete message handling for DU, CU, Core
 * 
 * This module provides response message generation for:
 * - DU Layer: MAC, RLC, PDCP responses
 * - CU Layer: RRC messages
 * - Core Layer: NAS messages (simulated AMF/SMF)
 */

#ifndef MOCK_GNB_RESPONSE_H
#define MOCK_GNB_RESPONSE_H

#include "mock_gnb_server.h"
#include <stdint.h>
#include <stdbool.h>

/* ============== Message Categories ============== */

typedef enum {
    MOCK_MSG_CAT_DU = 0,     /* DU Layer: MAC, RLC, PDCP */
    MOCK_MSG_CAT_CU = 1,     /* CU Layer: RRC, F1 */
    MOCK_MSG_CAT_CORE = 2,   /* Core Layer: NAS (AMF/SMF) */
    MOCK_MSG_CAT_MAX
} mock_msg_category_t;

/* ============== DU Layer Message Types ============== */

/* MAC Response Types */
typedef enum {
    MOCK_MAC_RAR = 0,                    /* Random Access Response */
    MOCK_MAC_UL_GRANT,                   /* Uplink Grant */
    MOCK_MAC_DL_ASSIGNMENT,              /* Downlink Assignment */
    MOCK_MAC_TIMING_ADVANCE,             /* Timing Advance Command */
    MOCK_MAC_CONTENTION_RESOLUTION,      /* Contention Resolution */
    MOCK_MAC_MAX
} mock_mac_response_type_t;

/* RLC Response Types */
typedef enum {
    MOCK_RLC_STATUS_PDU = 0,             /* Status PDU (ACK/NACK) */
    MOCK_RLC_REASSEMBLY_COMPLETE,        /* Reassembly Complete */
    MOCK_RLC_MAX
} mock_rlc_response_type_t;

/* PDCP Response Types */
typedef enum {
    MOCK_PDCP_SECURITY_CONFIG = 0,       /* Security Configuration */
    MOCK_PDCP_ROHC_FEEDBACK,              /* ROHC Feedback */
    MOCK_PDCP_STATUS_REPORT,             /* PDCP Status Report */
    MOCK_PDCP_MAX
} mock_pdcp_response_type_t;

/* ============== CU Layer Message Types (RRC) ============== */

typedef enum {
    MOCK_RRC_SETUP = 0,                  /* RRC Setup */
    MOCK_RRC_REJECT,                     /* RRC Reject */
    MOCK_RRC_REESTABLISHMENT,            /* RRC Reestablishment */
    MOCK_RRC_RECONFIGURATION,            /* RRC Reconfiguration */
    MOCK_RRC_RESUME,                     /* RRC Resume */
    MOCK_RRC_RELEASE,                    /* RRC Release */
    MOCK_RRC_SECURITY_MODE_COMMAND,      /* RRC Security Mode Command */
    MOCK_RRC_UE_CAPABILITY_ENQUIRY,      /* RRC UE Capability Enquiry */
    MOCK_RRC_UE_CAP_ENQUIRY = MOCK_RRC_UE_CAPABILITY_ENQUIRY, /* Alias */
    MOCK_RRC_MEAS_CONFIG,                /* RRC Measurement Configuration */
    MOCK_RRC_HANDOVER_COMMAND,           /* RRC Handover Command */
    MOCK_RRC_DL_INFO_TRANSFER,           /* RRC DL Information Transfer */
    MOCK_RRC_MAX
} mock_rrc_response_type_t;

/* ============== Core Layer Message Types (NAS) ============== */

/* 5GMM (Mobility Management) - Response types */
typedef enum {
    MOCK_NAS_RESP_REGISTRATION_ACCEPT = 0,    /* Registration Accept */
    MOCK_NAS_RESP_REGISTRATION_REJECT,         /* Registration Reject */
    MOCK_NAS_RESP_AUTHENTICATION_REQUEST,     /* Authentication Request */
    MOCK_NAS_RESP_AUTHENTICATION_REJECT,      /* Authentication Reject */
    MOCK_NAS_RESP_AUTHENTICATION_RESULT,      /* Authentication Result */
    MOCK_NAS_RESP_SECURITY_MODE_COMMAND,      /* Security Mode Command */
    MOCK_NAS_RESP_IDENTITY_REQUEST,           /* Identity Request */
    MOCK_NAS_RESP_DL_NAS_TRANSPORT,           /* DL NAS Transport */
    MOCK_NAS_RESP_SERVICE_ACCEPT,             /* Service Accept */
    MOCK_NAS_RESP_SERVICE_REJECT,             /* Service Reject */
    MOCK_NAS_RESP_NOTIFICATION,               /* Notification */
    MOCK_NAS_RESP_5GMM_STATUS,                /* 5GMM Status */
    MOCK_NAS_RESP_5GMM_MAX
} mock_nas_5gmm_resp_type_t;

/* 5GSM (Session Management) - Response types */
typedef enum {
    MOCK_NAS_RESP_PDU_SESSION_EST_ACCEPT = 0,   /* PDU Session Establishment Accept */
    MOCK_NAS_RESP_PDU_SESSION_EST_REJECT,       /* PDU Session Establishment Reject */
    MOCK_NAS_RESP_PDU_SESSION_AUTH_COMMAND,     /* PDU Session Authentication Command */
    MOCK_NAS_RESP_PDU_SESSION_MOD_COMMAND,      /* PDU Session Modification Command */
    MOCK_NAS_RESP_PDU_SESSION_RELEASE_COMMAND,   /* PDU Session Release Command */
    MOCK_NAS_RESP_PDU_SESSION_5GSM_STATUS,       /* 5GSM Status */
    MOCK_NAS_RESP_5GSM_MAX
} mock_nas_5gsm_resp_type_t;

/* ============== Response Context ============== */

typedef struct {
    /* UE Identity */
    uint64_t ran_ue_ngap_id;
    uint64_t amf_ue_ngap_id;
    char guti[24];
    char imsi[16];
    uint16_t c_rnti;
    
    /* Cell Info */
    uint16_t pci;
    uint32_t cell_id;
    uint16_t tac;
    uint32_t plmn_id;
    
    /* Security Context */
    bool security_enabled;
    uint8_t ciphering_alg;
    uint8_t integrity_alg;
    uint32_t nas_ul_count;
    uint32_t nas_dl_count;
    
    /* PDU Sessions */
    uint8_t pdu_session_id;
    uint32_t ue_ip_address;
    uint8_t qos_flow_id;
    uint8_t five_qi;
    
    /* Handover Info */
    uint16_t target_pci;
    uint32_t target_cell_id;
    
    /* Transaction */
    uint32_t transaction_id;
    uint8_t rrc_transaction_id;
    uint8_t nas_procedure_transaction_id;
} mock_response_context_t;

/* ============== Response Data Structures ============== */

/* MAC RAR (Random Access Response) */
typedef struct {
    uint8_t preamble_id;
    uint16_t timing_advance;
    uint16_t temp_c_rnti;
    uint32_t ul_grant;
    uint16_t backoff_indicator;
} mock_mac_rar_data_t;

/* RRC Setup */
typedef struct {
    uint8_t rrc_transaction_id;
    uint8_t radio_bearer_config[256];
    size_t radio_bearer_config_len;
    uint8_t master_cell_group[512];
    size_t master_cell_group_len;
    uint8_t dedicated_nas_pdu[256];
    size_t dedicated_nas_pdu_len;
} mock_rrc_setup_data_t;

/* RRC Reconfiguration */
typedef struct {
    uint8_t rrc_transaction_id;
    uint8_t radio_bearer_config[512];
    size_t radio_bearer_config_len;
    uint8_t secondary_cell_group[256];
    size_t secondary_cell_group_len;
    uint8_t meas_config[256];
    size_t meas_config_len;
    uint8_t dedicated_nas_pdu[256];
    size_t dedicated_nas_pdu_len;
} mock_rrc_reconfig_data_t;

/* RRC Handover Command */
typedef struct {
    uint8_t rrc_transaction_id;
    uint16_t target_pci;
    uint32_t target_cell_id;
    uint16_t new_c_rnti;
    uint8_t radio_bearer_config[512];
    size_t radio_bearer_config_len;
    uint8_t rach_config[64];
    size_t rach_config_len;
} mock_rrc_handover_data_t;

/* NAS Registration Accept */
typedef struct {
    uint8_t registration_result;
    char guti[24];
    uint16_t tac;
    uint32_t plmn_id;
    uint32_t t3412_value;       /* Registration timer */
    uint32_t t3402_value;       /* Periodic registration timer */
    uint8_t allowed_nssai[32];
    size_t allowed_nssai_len;
    uint8_t network_slicing[64];
    size_t network_slicing_len;
} mock_nas_reg_accept_data_t;

/* NAS Authentication Request */
typedef struct {
    uint8_t ngksi;
    uint8_t rand[16];
    uint8_t autn[16];
    uint8_t auth_type;
} mock_nas_auth_request_data_t;

/* NAS Security Mode Command */
typedef struct {
    uint8_t ngksi;
    uint8_t ciphering_alg;
    uint8_t integrity_alg;
    uint8_t ue_security_capability[4];
    size_t ue_security_capability_len;
} mock_nas_security_mode_data_t;

/* NAS PDU Session Establishment Accept */
typedef struct {
    uint8_t pdu_session_id;
    uint8_t pdu_session_type;
    uint32_t ue_ip_address;
    uint8_t default_qos_flow_id;
    uint8_t five_qi;
    uint16_t session_ambr_ul;
    uint16_t session_ambr_dl;
    uint8_t s_nssai[4];
    size_t s_nssai_len;
} mock_nas_pdu_session_accept_data_t;

/* ============== API Functions ============== */

/**
 * Initialize response generator
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_response_init(void);

/**
 * Cleanup response generator
 */
void mock_gnb_response_cleanup(void);

/* ============== DU Layer Response Generators ============== */

/**
 * Generate MAC Random Access Response
 * @param ctx Response context
 * @param rar_data RAR data
 * @param response Output buffer
 * @param len Output length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_mac_rar(const mock_response_context_t* ctx,
                                           const mock_mac_rar_data_t* rar_data,
                                           void** response, size_t* len);

/**
 * Generate MAC UL Grant
 * @param ctx Response context
 * @param tb_size Transport block size
 * @param mcs MCS index
 * @param response Output buffer
 * @param len Output length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_mac_ul_grant(const mock_response_context_t* ctx,
                                                uint16_t tb_size, uint8_t mcs,
                                                void** response, size_t* len);

/**
 * Generate RLC Status PDU
 * @param ctx Response context
 * @param ack_sn Last acknowledged SN
 * @param nack_sn_list List of NACK SNs
 * @param nack_count Number of NACKs
 * @param response Output buffer
 * @param len Output length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_rlc_status(const mock_response_context_t* ctx,
                                              uint16_t ack_sn,
                                              const uint16_t* nack_sn_list,
                                              uint8_t nack_count,
                                              void** response, size_t* len);

/* ============== CU Layer Response Generators (RRC) ============== */

/**
 * Generate RRC Setup
 * @param ctx Response context
 * @param rrc_data RRC setup data
 * @param response Output buffer
 * @param len Output length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_rrc_setup_full(const mock_response_context_t* ctx,
                                                   const mock_rrc_setup_data_t* rrc_data,
                                                   void** response, size_t* len);

/**
 * Generate RRC Reject
 * @param ctx Response context
 * @param wait_time Wait time in seconds
 * @param response Output buffer
 * @param len Output length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_rrc_reject(const mock_response_context_t* ctx,
                                              uint8_t wait_time,
                                              void** response, size_t* len);

/**
 * Generate RRC Reconfiguration
 * @param ctx Response context
 * @param reconfig_data Reconfiguration data
 * @param response Output buffer
 * @param len Output length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_rrc_reconfiguration(const mock_response_context_t* ctx,
                                                       const mock_rrc_reconfig_data_t* reconfig_data,
                                                       void** response, size_t* len);

/**
 * Generate RRC Security Mode Command
 * @param ctx Response context
 * @param ciphering_alg Ciphering algorithm
 * @param integrity_alg Integrity algorithm
 * @param response Output buffer
 * @param len Output length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_rrc_security_mode(const mock_response_context_t* ctx,
                                                     uint8_t ciphering_alg,
                                                     uint8_t integrity_alg,
                                                     void** response, size_t* len);

/**
 * Generate RRC Handover Command
 * @param ctx Response context
 * @param ho_data Handover data
 * @param response Output buffer
 * @param len Output length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_rrc_handover(const mock_response_context_t* ctx,
                                                 const mock_rrc_handover_data_t* ho_data,
                                                 void** response, size_t* len);

/**
 * Generate RRC Connection Release
 * @param ctx Response context
 * @param release_cause Release cause
 * @param response Output buffer
 * @param len Output length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_rrc_release(const mock_response_context_t* ctx,
                                               uint8_t release_cause,
                                               void** response, size_t* len);

/**
 * Generate RRC Measurement Configuration
 * @param ctx Response context
 * @param meas_config Measurement configuration
 * @param meas_config_len Length of measurement config
 * @param response Output buffer
 * @param len Output length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_rrc_meas_config(const mock_response_context_t* ctx,
                                                   const uint8_t* meas_config,
                                                   size_t meas_config_len,
                                                   void** response, size_t* len);

/* ============== Core Layer Response Generators (NAS) ============== */

/**
 * Generate NAS Registration Accept
 * @param ctx Response context
 * @param reg_data Registration accept data
 * @param response Output buffer
 * @param len Output length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_nas_reg_accept(const mock_response_context_t* ctx,
                                                   const mock_nas_reg_accept_data_t* reg_data,
                                                   void** response, size_t* len);

/**
 * Generate NAS Registration Reject
 * @param ctx Response context
 * @param reject_cause Reject cause value
 * @param response Output buffer
 * @param len Output length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_nas_reg_reject(const mock_response_context_t* ctx,
                                                   uint8_t reject_cause,
                                                   void** response, size_t* len);

/**
 * Generate NAS Authentication Request
 * @param ctx Response context
 * @param auth_data Authentication data
 * @param response Output buffer
 * @param len Output length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_nas_auth_request(const mock_response_context_t* ctx,
                                                     const mock_nas_auth_request_data_t* auth_data,
                                                     void** response, size_t* len);

/**
 * Generate NAS Security Mode Command
 * @param ctx Response context
 * @param security_data Security mode data
 * @param response Output buffer
 * @param len Output length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_nas_security_mode(const mock_response_context_t* ctx,
                                                      const mock_nas_security_mode_data_t* security_data,
                                                      void** response, size_t* len);

/**
 * Generate NAS PDU Session Establishment Accept
 * @param ctx Response context
 * @param pdu_data PDU session data
 * @param response Output buffer
 * @param len Output length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_nas_pdu_session_accept(const mock_response_context_t* ctx,
                                                           const mock_nas_pdu_session_accept_data_t* pdu_data,
                                                           void** response, size_t* len);

/**
 * Generate NAS PDU Session Release Command
 * @param ctx Response context
 * @param pdu_session_id PDU session ID
 * @param release_cause Release cause
 * @param response Output buffer
 * @param len Output length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_nas_pdu_session_release(const mock_response_context_t* ctx,
                                                            uint8_t pdu_session_id,
                                                            uint8_t release_cause,
                                                            void** response, size_t* len);

/**
 * Generate NAS DL Transport (for encapsulated NAS messages)
 * @param ctx Response context
 * @param nas_pdu NAS PDU data
 * @param nas_pdu_len NAS PDU length
 * @param response Output buffer
 * @param len Output length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_nas_dl_transport(const mock_response_context_t* ctx,
                                                     const uint8_t* nas_pdu,
                                                     size_t nas_pdu_len,
                                                     void** response, size_t* len);

/* ============== Complete Procedure Response Chains ============== */

/**
 * Generate complete registration procedure responses
 * Includes: Authentication, Security Mode, Registration Accept
 * @param ue_ctx UE context
 * @param responses Array of response buffers
 * @param lengths Array of response lengths
 * @param count Number of responses generated
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_registration_procedure(mock_gnb_ue_context_t* ue_ctx,
                                                          void** responses,
                                                          size_t* lengths,
                                                          uint8_t* count);

/**
 * Generate complete PDU session establishment responses
 * Includes: PDU Session Establishment Accept
 * @param ue_ctx UE context
 * @param session_id PDU session ID
 * @param response Output buffer
 * @param len Output length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_pdu_session_procedure(mock_gnb_ue_context_t* ue_ctx,
                                                         uint8_t session_id,
                                                         void** response, size_t* len);

/**
 * Generate complete handover procedure response
 * Includes: RRC Handover Command
 * @param ue_ctx UE context
 * @param target_pci Target PCI
 * @param response Output buffer
 * @param len Output length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_handover_procedure(mock_gnb_ue_context_t* ue_ctx,
                                                      uint16_t target_pci,
                                                      void** response, size_t* len);

/* ============== Utility Functions ============== */

/**
 * Build response context from UE context
 * @param ue_ctx UE context
 * @param response_ctx Output response context
 */
void mock_gnb_build_response_context(const mock_gnb_ue_context_t* ue_ctx,
                                     mock_response_context_t* response_ctx);

/**
 * Get default RRC Setup data
 * @param rrc_data Output RRC setup data
 */
void mock_gnb_get_default_rrc_setup(mock_rrc_setup_data_t* rrc_data);

/**
 * Get default Registration Accept data
 * @param reg_data Output registration accept data
 */
void mock_gnb_get_default_reg_accept(mock_nas_reg_accept_data_t* reg_data);

/**
 * Get default PDU Session Accept data
 * @param pdu_data Output PDU session accept data
 */
void mock_gnb_get_default_pdu_session_accept(mock_nas_pdu_session_accept_data_t* pdu_data);

/* ============== ASN.1 PER Encoded RRC Generators ============== */

/**
 * Generate RRC Setup with ASN.1 PER encoding
 * @param ctx Response context
 * @param rrc_transaction_id RRC transaction ID
 * @param radio_bearer_config Radio bearer config (optional)
 * @param config_len Config length
 * @param response Output buffer
 * @param len Output length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_rrc_setup_per(const mock_response_context_t* ctx,
                                                  uint8_t rrc_transaction_id,
                                                  const uint8_t* radio_bearer_config,
                                                  size_t config_len,
                                                  void** response, size_t* len);

/**
 * Generate RRC Reconfiguration with ASN.1 PER encoding
 * @param ctx Response context
 * @param rrc_transaction_id RRC transaction ID
 * @param radio_bearer_config Radio bearer config (optional)
 * @param config_len Config length
 * @param meas_config Measurement config (optional)
 * @param meas_config_len Measurement config length
 * @param response Output buffer
 * @param len Output length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_rrc_reconfig_per(const mock_response_context_t* ctx,
                                                    uint8_t rrc_transaction_id,
                                                    const uint8_t* radio_bearer_config,
                                                    size_t config_len,
                                                    const uint8_t* meas_config,
                                                    size_t meas_config_len,
                                                    void** response, size_t* len);

/**
 * Generate RRC Handover Command with ASN.1 PER encoding
 * @param ctx Response context
 * @param rrc_transaction_id RRC transaction ID
 * @param target_pci Target PCI
 * @param target_cell_id Target cell ID
 * @param new_c_rnti New C-RNTI
 * @param radio_bearer_config Radio bearer config (optional)
 * @param config_len Config length
 * @param response Output buffer
 * @param len Output length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_rrc_handover_per(const mock_response_context_t* ctx,
                                                    uint8_t rrc_transaction_id,
                                                    uint16_t target_pci,
                                                    uint32_t target_cell_id,
                                                    uint16_t new_c_rnti,
                                                    const uint8_t* radio_bearer_config,
                                                    size_t config_len,
                                                    void** response, size_t* len);

/**
 * Generate RRC Connection Release with ASN.1 PER encoding
 * @param ctx Response context
 * @param release_cause Release cause
 * @param redirect_carrier Whether to redirect carrier
 * @param redirect_earfcn Redirect EARFCN (if redirect_carrier is true)
 * @param response Output buffer
 * @param len Output length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_rrc_release_per(const mock_response_context_t* ctx,
                                                   uint8_t release_cause,
                                                   bool redirect_carrier,
                                                   uint16_t redirect_earfcn,
                                                   void** response, size_t* len);

/**
 * Generate RRC Measurement Report with ASN.1 PER encoding
 * @param ctx Response context
 * @param meas_id Measurement ID
 * @param rsrp RSRP value (-140 to -44 dBm)
 * @param rsrq RSRQ value (-20 to -3 dB)
 * @param pci PCI
 * @param cell_id Cell ID
 * @param response Output buffer
 * @param len Output length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_rrc_meas_report_per(const mock_response_context_t* ctx,
                                                       uint8_t meas_id,
                                                       int32_t rsrp,
                                                       int32_t rsrq,
                                                       uint16_t pci,
                                                       uint32_t cell_id,
                                                       void** response, size_t* len);

/**
 * Generate RRC Security Mode Command with ASN.1 PER encoding
 * @param ctx Response context
 * @param rrc_transaction_id RRC transaction ID
 * @param ciphering_alg Ciphering algorithm
 * @param integrity_alg Integrity algorithm
 * @param security_capabilities Security capabilities (optional)
 * @param capabilities_len Capabilities length
 * @param response Output buffer
 * @param len Output length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_rrc_security_mode_per(const mock_response_context_t* ctx,
                                                         uint8_t rrc_transaction_id,
                                                         uint8_t ciphering_alg,
                                                         uint8_t integrity_alg,
                                                         const uint8_t* security_capabilities,
                                                         size_t capabilities_len,
                                                         void** response, size_t* len);

/**
 * Generate RRC UE Capability Enquiry with ASN.1 PER encoding
 * @param ctx Response context
 * @param enquiry_id Enquiry ID
 * @param rat_types RAT types array
 * @param num_rat_types Number of RAT types
 * @param response Output buffer
 * @param len Output length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_rrc_ue_cap_enquiry_per(const mock_response_context_t* ctx,
                                                          uint8_t enquiry_id,
                                                          const uint8_t* rat_types,
                                                          size_t num_rat_types,
                                                          void** response, size_t* len);

#endif /* MOCK_GNB_RESPONSE_H */
