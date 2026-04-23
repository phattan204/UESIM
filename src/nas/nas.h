/*
 * 5G UE Simulation Application
 * NAS (Non-Access Stratum) Layer Header
 */

#ifndef NAS_H
#define NAS_H

#include "../uesim.h"
#include "../protocol/mac.h"
#include <stdint.h>
#include <stdbool.h>

// NAS Constants
#define NAS_MAX_MESSAGE_SIZE        4096
#define NAS_MAX_PDU_SESSIONS        16
#define NAS_MAX_EPS_BEARERS         11
#define NAS_MAX_SECURITY_CONTEXTS   8
#define NAS_MAX_AUTHENTICATION_VECTOR 5
#define NAS_DEFAULT_T3412           540  // 9 minutes in seconds
#define NAS_DEFAULT_T3422           12   // 12 seconds
#define NAS_DEFAULT_T3450           6    // 6 seconds

// NAS Message Types
typedef enum {
    // 5GMM (5G Mobility Management) Messages
    NAS_MSG_TYPE_REGISTRATION_REQUEST = 0x41,
    NAS_MSG_TYPE_REGISTRATION_ACCEPT = 0x42,
    NAS_MSG_TYPE_REGISTRATION_COMPLETE = 0x43,
    NAS_MSG_TYPE_REGISTRATION_REJECT = 0x44,
    NAS_MSG_TYPE_SERVICE_REQUEST = 0x4c,
    NAS_MSG_TYPE_SERVICE_REJECT = 0x4e,
    NAS_MSG_TYPE_SERVICE_ACCEPT = 0x4f,
    NAS_MSG_TYPE_AUTHENTICATION_REQUEST = 0x56,
    NAS_MSG_TYPE_AUTHENTICATION_RESPONSE = 0x57,
    NAS_MSG_TYPE_AUTHENTICATION_REJECT = 0x58,
    NAS_MSG_TYPE_AUTHENTICATION_FAILURE = 0x59,
    NAS_MSG_TYPE_AUTHENTICATION_RESULT = 0x5a,
    NAS_MSG_TYPE_IDENTITY_REQUEST = 0x5c,
    NAS_MSG_TYPE_IDENTITY_RESPONSE = 0x5d,
    NAS_MSG_TYPE_SECURITY_MODE_COMMAND = 0x5e,
    NAS_MSG_TYPE_SECURITY_MODE_COMPLETE = 0x5f,
    NAS_MSG_TYPE_SECURITY_MODE_REJECT = 0x60,
    NAS_MSG_TYPE_5GMM_STATUS = 0x64,
    NAS_MSG_TYPE_NOTIFICATION = 0x65,
    NAS_MSG_TYPE_NOTIFICATION_RESPONSE = 0x66,
    NAS_MSG_TYPE_UL_NAS_TRANSPORT = 0x67,
    NAS_MSG_TYPE_DL_NAS_TRANSPORT = 0x68,
    
    // 5GSM (5G Session Management) Messages
    NAS_MSG_TYPE_PDU_SESSION_ESTABLISHMENT_REQUEST = 0xc1,
    NAS_MSG_TYPE_PDU_SESSION_ESTABLISHMENT_ACCEPT = 0xc2,
    NAS_MSG_TYPE_PDU_SESSION_ESTABLISHMENT_REJECT = 0xc3,
    NAS_MSG_TYPE_PDU_SESSION_AUTHENTICATION_COMMAND = 0xc5,
    NAS_MSG_TYPE_PDU_SESSION_AUTHENTICATION_COMPLETE = 0xc6,
    NAS_MSG_TYPE_PDU_SESSION_AUTHENTICATION_RESULT = 0xc7,
    NAS_MSG_TYPE_PDU_SESSION_MODIFICATION_REQUEST = 0xc9,
    NAS_MSG_TYPE_PDU_SESSION_MODIFICATION_REJECT = 0xca,
    NAS_MSG_TYPE_PDU_SESSION_MODIFICATION_COMMAND = 0xcb,
    NAS_MSG_TYPE_PDU_SESSION_MODIFICATION_COMPLETE = 0xcc,
    NAS_MSG_TYPE_PDU_SESSION_MODIFICATION_COMMAND_REJECT = 0xcd,
    NAS_MSG_TYPE_PDU_SESSION_RELEASE_REQUEST = 0xd1,
    NAS_MSG_TYPE_PDU_SESSION_RELEASE_REJECT = 0xd2,
    NAS_MSG_TYPE_PDU_SESSION_RELEASE_COMMAND = 0xd3,
    NAS_MSG_TYPE_PDU_SESSION_RELEASE_COMPLETE = 0xd4,
    NAS_MSG_TYPE_5GSM_STATUS = 0xd6
} nas_message_type_t;

// NAS Security Header Types
typedef enum {
    NAS_SECURITY_HEADER_PLAIN = 0x00,
    NAS_SECURITY_HEADER_INTEGRITY_PROTECTED = 0x01,
    NAS_SECURITY_HEADER_INTEGRITY_PROTECTED_CIPHERED = 0x02,
    NAS_SECURITY_HEADER_INTEGRITY_PROTECTED_NEW = 0x03,
    NAS_SECURITY_HEADER_INTEGRITY_PROTECTED_CIPHERED_NEW = 0x04,
    NAS_SECURITY_HEADER_SERVICE_REQUEST = 0x0c
} nas_security_header_type_t;

// NAS Registration Types
typedef enum {
    NAS_REGISTRATION_TYPE_INITIAL = 0x01,
    NAS_REGISTRATION_TYPE_MOBILITY_UPDATING = 0x02,
    NAS_REGISTRATION_TYPE_PERIODIC_UPDATING = 0x03,
    NAS_REGISTRATION_TYPE_EMERGENCY = 0x04,
    NAS_REGISTRATION_TYPE_DEREGISTRATION = 0x05
} nas_registration_type_t;

// NAS Identity Types
typedef enum {
    NAS_IDENTITY_TYPE_SUCI = 0x01,
    NAS_IDENTITY_TYPE_GUTI = 0x02,
    NAS_IDENTITY_TYPE_IMEI = 0x03,
    NAS_IDENTITY_TYPE_IMEISV = 0x04,
    NAS_IDENTITY_TYPE_TMSI = 0x05
} nas_identity_type_t;

// NAS Authentication Types
typedef enum {
    NAS_AUTHENTICATION_TYPE_5G_AKA = 0x01,
    NAS_AUTHENTICATION_TYPE_EAP = 0x02,
    NAS_AUTHENTICATION_TYPE_5G_AKA_PRIME = 0x03
} nas_authentication_type_t;

// NAS Ciphering Algorithms
typedef enum {
    NAS_CIPHERING_ALG_NEA0 = 0x00,  // NULL
    NAS_CIPHERING_ALG_NEA1 = 0x01,  // 128-NEA1
    NAS_CIPHERING_ALG_NEA2 = 0x02,  // 128-NEA2
    NAS_CIPHERING_ALG_NEA3 = 0x03   // 128-NEA3
} nas_ciphering_algorithm_t;

// NAS Integrity Algorithms
typedef enum {
    NAS_INTEGRITY_ALG_NIA0 = 0x00,  // NULL
    NAS_INTEGRITY_ALG_NIA1 = 0x01,  // 128-NIA1
    NAS_INTEGRITY_ALG_NIA2 = 0x02,  // 128-NIA2
    NAS_INTEGRITY_ALG_NIA3 = 0x03   // 128-NIA3
} nas_integrity_algorithm_t;

// NAS PDU Session Types
typedef enum {
    NAS_PDU_SESSION_TYPE_IPV4 = 0x01,
    NAS_PDU_SESSION_TYPE_IPV6 = 0x02,
    NAS_PDU_SESSION_TYPE_IPV4V6 = 0x03,
    NAS_PDU_SESSION_TYPE_UNSTRUCTURED = 0x04,
    NAS_PDU_SESSION_TYPE_ETHERNET = 0x05
} nas_pdu_session_type_t;

// NAS SSSC Modes
typedef enum {
    NAS_SSC_MODE_1 = 0x01,
    NAS_SSC_MODE_2 = 0x02,
    NAS_SSC_MODE_3 = 0x03
} nas_ssc_mode_t;

// NAS 5GMM States
typedef enum {
    NAS_5GMM_NULL = 0,
    NAS_5GMM_DEREGISTERED = 1,
    NAS_5GMM_REGISTERED_INITIATED = 2,
    NAS_5GMM_REGISTERED = 3,
    NAS_5GMM_SERVICE_REQUEST_INITIATED = 4,
    NAS_5GMM_DEREGISTERED_INITIATED = 5
} nas_5gmm_state_t;

// NAS 5GSM States
typedef enum {
    NAS_5GSM_PDU_SESSION_INACTIVE = 0,
    NAS_5GSM_PDU_SESSION_ACTIVE_PENDING = 1,
    NAS_5GSM_PDU_SESSION_ACTIVE = 2,
    NAS_5GSM_PDU_SESSION_MODIFICATION_PENDING = 3,
    NAS_5GSM_PDU_SESSION_RELEASED_PENDING = 4
} nas_5gsm_state_t;

// NAS Security Context
typedef struct {
    uint8_t ksi;                    // Key Set Identifier
    nas_ciphering_algorithm_t ciphering_alg;  // Selected ciphering algorithm
    nas_integrity_algorithm_t integrity_alg;  // Selected integrity algorithm
    uint8_t knas_enc[16];          // NAS encryption key
    uint8_t knas_int[16];          // NAS integrity protection key
    uint32_t downlink_count;       // Downlink NAS COUNT
    uint32_t uplink_count;         // Uplink NAS COUNT
    bool security_context_valid;   // Security context validity
    pthread_mutex_t security_mutex; // Security context protection
} nas_security_context_t;

// NAS Authentication Vector
typedef struct {
    uint8_t rand[16];              // Random challenge
    uint8_t xres[16];              // Expected response
    uint8_t autn[16];              // Authentication token
    uint8_t kasme[32];             // Master key
    bool used;                     // Vector usage status
} nas_auth_vector_t;

// NAS Authentication Context
typedef struct {
    nas_authentication_type_t auth_type;  // Authentication type
    nas_auth_vector_t vectors[NAS_MAX_AUTHENTICATION_VECTOR];  // Auth vectors
    uint8_t num_vectors;           // Number of available vectors
    uint8_t current_vector;        // Current vector index
    bool authenticated;            // Authentication status
} nas_auth_context_t;

// NAS UE Identity
typedef struct {
    char suci[64];                 // Subscription Concealed Identifier
    char guti[64];                 // Globally Unique Temporary Identifier
    char imsi[16];                 // International Mobile Subscriber Identity
    char imei[16];                 // International Mobile Equipment Identity
    char msisdn[16];               // Mobile Station ISDN Number
} nas_ue_identity_t;

// NAS QoS Flow
typedef struct {
    uint8_t qfi;                   // QoS Flow Identifier (1-64)
    uint8_t arp;                   // Allocation and Retention Priority (1-15)
    uint8_t qci;                   // QoS Class Identifier
    uint16_t gbr_ul;              // Guaranteed Bit Rate Uplink (kbps)
    uint16_t gbr_dl;              // Guaranteed Bit Rate Downlink (kbps)
    uint16_t mbr_ul;              // Maximum Bit Rate Uplink (kbps)
    uint16_t mbr_dl;              // Maximum Bit Rate Downlink (kbps)
    bool active;                  // QoS flow active status
} nas_qos_flow_t;

// NAS PDU Session
typedef struct {
    uint8_t pdu_session_id;        // PDU Session ID (1-15)
    nas_pdu_session_type_t session_type;  // Session type
    nas_ssc_mode_t ssc_mode;       // SSC mode
    nas_5gsm_state_t state;        // Session state
    uint32_t pdu_address;          // PDU address (IPv4)
    uint8_t default_qfi;           // Default QoS Flow Identifier
    uint8_t num_qos_flows;         // Number of QoS flows
    nas_qos_flow_t qos_flows[8];   // QoS flows (max 8 per session)
    uint16_t session_ambr_ul;      // Session AMBR Uplink (kbps)
    uint16_t session_ambr_dl;      // Session AMBR Downlink (kbps)
    uint8_t ptis;                  // Procedure Transaction ID
    uint32_t ul_teid;              // Uplink TEID
    uint32_t dl_teid;              // Downlink TEID
    uint32_t upf_ip;               // UPF IP address
    bool active;                   // Session active status
    pthread_mutex_t session_mutex; // Session protection
} nas_pdu_session_t;

// NAS Registration Request
typedef struct {
    nas_registration_type_t registration_type;  // Registration type
    bool ngksi_tsc;                // TSC flag
    uint8_t ngksi_value;           // NGKSI value
    nas_identity_type_t identity_type;  // Identity type
    char mobile_identity[64];      // Mobile identity
    bool follow_on_request;        // Follow-on request pending
    bool uplink_data_status;       // Uplink data status
    uint16_t allowed_pdu_session_status;  // Allowed PDU session status
    bool n1_mode_reg;              // N1 mode registration
    bool sms_requested;            // SMS requested
} nas_registration_request_t;

// NAS Registration Accept
typedef struct {
    uint8_t registration_result;   // Registration result
    char guti[64];                 // Assigned GUTI
    uint16_t equivalent_plmns[16]; // Equivalent PLMNs
    uint8_t num_equivalent_plmns;  // Number of equivalent PLMNs
    uint32_t tac;                  // Tracking Area Code
    uint16_t allowed_nssai[16];    // Allowed NSSAI
    uint8_t num_allowed_nssai;     // Number of allowed NSSAI items
    uint32_t t3412_value;          // T3412 timer value
    uint32_t t3402_value;          // T3402 timer value
    uint32_t t3423_value;          // T3423 timer value
} nas_registration_accept_t;

// NAS Security Mode Command
typedef struct {
    nas_ciphering_algorithm_t selected_nas_security_algorithms;  // Selected ciphering
    nas_integrity_algorithm_t selected_nas_integrity_algorithm;  // Selected integrity
    bool ngksi_tsc;                // TSC flag
    uint8_t ngksi_value;           // NGKSI value
    uint8_t replayed_ue_security_capabilities[3];  // Replayed security capabilities
    bool imeisv_request;           // IMEISV request
    bool nonce_ue_present;         // Nonce UE present
    uint32_t nonce_ue;             // Nonce UE value
    bool nonce_amf_present;        // Nonce AMF present
    uint32_t nonce_amf;            // Nonce AMF value
} nas_security_mode_command_t;

// NAS PDU Session Establishment Request
typedef struct {
    uint8_t pdu_session_id;        // PDU Session ID
    nas_pdu_session_type_t pdu_session_type;  // Session type
    nas_ssc_mode_t ssc_mode;       // SSC mode
    uint8_t ptis;                  // Procedure Transaction ID
    uint8_t pdu_session_establishment_request[256];  // Request data
    size_t request_length;         // Request data length
} nas_pdu_session_establishment_request_t;

// NAS Message Header
typedef struct {
    uint8_t extended_protocol_discriminator;  // Extended protocol discriminator
    nas_security_header_type_t security_header_type;  // Security header type
    uint8_t message_type;          // Message type
    uint32_t message_authentication_code;  // Message authentication code
    uint8_t sequence_number;       // Sequence number
} nas_message_header_t;

// NAS Message
typedef struct {
    nas_message_header_t header;   // Message header
    uint8_t* message_data;         // Message data
    size_t message_length;         // Message length
    uint32_t arrival_time;         // Message arrival time
    bool integrity_protected;      // Integrity protection status
    bool ciphered;                 // Ciphering status
} nas_message_t;

// NAS Statistics
typedef struct {
    uint64_t registration_requests;  // Registration requests sent
    uint64_t registration_accepts;   // Registration accepts received
    uint64_t authentication_requests; // Authentication requests received
    uint64_t authentication_responses; // Authentication responses sent
    uint64_t security_mode_commands;  // Security mode commands received
    uint64_t security_mode_completes; // Security mode completes sent
    uint64_t pdu_session_est_requests; // PDU session establishment requests
    uint64_t pdu_session_est_accepts;  // PDU session establishment accepts
    uint64_t messages_sent;           // Total messages sent
    uint64_t messages_received;       // Total messages received
    uint64_t authentication_success;  // Successful authentications
    uint64_t authentication_failures; // Failed authentications
} nas_stats_t;

// NAS UE Context
typedef struct {
    uint32_t ue_id;                // UE identifier
    nas_5gmm_state_t mm_state;     // 5GMM state
    nas_ue_identity_t identity;    // UE identity
    nas_security_context_t security_context;  // Security context
    nas_auth_context_t auth_context;  // Authentication context
    nas_pdu_session_t pdu_sessions[NAS_MAX_PDU_SESSIONS];  // PDU sessions
    uint8_t num_active_sessions;   // Number of active sessions
    uint32_t t3412_timer;          // T3412 timer value
    uint32_t t3422_timer;          // T3422 timer value
    uint32_t t3450_timer;          // T3450 timer value
    bool t3412_running;            // T3412 timer running
    bool t3422_running;            // T3422 timer running
    bool t3450_running;            // T3450 timer running
    atomic_uint message_counter;   // Message counter
    bool active;                   // UE context active
    pthread_mutex_t nas_mutex;     // NAS context protection
    pthread_cond_t nas_cond;       // NAS context signaling
    nas_stats_t stats;             /* NAS statistics */
} nas_ue_context_t;

// Function prototypes
uesim_error_t nas_init(ue_context_t* ue_ctx);
void nas_cleanup(ue_context_t* ue_ctx);

// NAS UE Context Management
uesim_error_t nas_create_ue_context(ue_context_t* ue_ctx, nas_ue_context_t** nas_ctx);
uesim_error_t nas_destroy_ue_context(ue_context_t* ue_ctx, nas_ue_context_t* nas_ctx);
uesim_error_t nas_activate_ue_context(nas_ue_context_t* nas_ctx);
uesim_error_t nas_deactivate_ue_context(nas_ue_context_t* nas_ctx);
uesim_error_t nas_update_5gmm_state(nas_ue_context_t* nas_ctx, nas_5gmm_state_t new_state);
uesim_error_t nas_update_5gsm_state(nas_ue_context_t* nas_ctx, uint8_t pdu_session_id, 
                                   nas_5gsm_state_t new_state);

// NAS Message Processing
uesim_error_t nas_process_message(nas_ue_context_t* nas_ctx, const nas_message_t* message);
uesim_error_t nas_send_message(nas_ue_context_t* nas_ctx, nas_message_t* message);
uesim_error_t nas_encode_message(const nas_message_t* message, uint8_t** encoded_data, 
                                size_t* encoded_length);
uesim_error_t nas_decode_message(const uint8_t* encoded_data, size_t encoded_length, 
                                nas_message_t* message);
uesim_error_t nas_create_message(nas_message_type_t msg_type, const void* msg_data, 
                                size_t msg_length, nas_message_t** message);
uesim_error_t nas_destroy_message(nas_message_t* message);

// NAS Registration Procedures
uesim_error_t nas_initiate_registration(nas_ue_context_t* nas_ctx, 
                                       nas_registration_type_t reg_type);
uesim_error_t nas_handle_registration_request(nas_ue_context_t* nas_ctx, 
                                             const nas_registration_request_t* request);
uesim_error_t nas_handle_registration_accept(nas_ue_context_t* nas_ctx, 
                                            const nas_registration_accept_t* accept);
uesim_error_t nas_send_registration_complete(nas_ue_context_t* nas_ctx);

// NAS Authentication Procedures
uesim_error_t nas_initiate_authentication(nas_ue_context_t* nas_ctx);
uesim_error_t nas_handle_authentication_request(nas_ue_context_t* nas_ctx, 
                                               const uint8_t* auth_data, size_t data_length);
uesim_error_t nas_send_authentication_response(nas_ue_context_t* nas_ctx, 
                                              const uint8_t* response_data, size_t data_length);
uesim_error_t nas_handle_authentication_result(nas_ue_context_t* nas_ctx, 
                                              const uint8_t* result_data, size_t data_length);

// NAS Security Mode Control
uesim_error_t nas_initiate_security_mode_control(nas_ue_context_t* nas_ctx);
uesim_error_t nas_handle_security_mode_command(nas_ue_context_t* nas_ctx, 
                                              const nas_security_mode_command_t* command);
uesim_error_t nas_send_security_mode_complete(nas_ue_context_t* nas_ctx);

// NAS PDU Session Management
uesim_error_t nas_initiate_pdu_session_establishment(nas_ue_context_t* nas_ctx, 
                                                    uint8_t pdu_session_id,
                                                    nas_pdu_session_type_t session_type);
uesim_error_t nas_handle_pdu_session_establishment_request(nas_ue_context_t* nas_ctx, 
                                                          const nas_pdu_session_establishment_request_t* request);
uesim_error_t nas_send_pdu_session_establishment_accept(nas_ue_context_t* nas_ctx, 
                                                       uint8_t pdu_session_id);
uesim_error_t nas_handle_pdu_session_establishment_reject(nas_ue_context_t* nas_ctx, 
                                                         uint8_t pdu_session_id, uint8_t cause);

// NAS Security Functions
uesim_error_t nas_create_security_context(nas_ue_context_t* nas_ctx, 
                                         nas_ciphering_algorithm_t cipher_alg,
                                         nas_integrity_algorithm_t integrity_alg);
uesim_error_t nas_destroy_security_context(nas_ue_context_t* nas_ctx);
uesim_error_t nas_update_security_context(nas_ue_context_t* nas_ctx, 
                                         nas_ciphering_algorithm_t new_cipher_alg,
                                         nas_integrity_algorithm_t new_integrity_alg);
uesim_error_t nas_integrity_protect_message(nas_ue_context_t* nas_ctx, 
                                           nas_message_t* message);
uesim_error_t nas_verify_integrity_protection(nas_ue_context_t* nas_ctx, 
                                             const nas_message_t* message, bool* valid);
uesim_error_t nas_cipher_message(nas_ue_context_t* nas_ctx, nas_message_t* message);
uesim_error_t nas_decipher_message(nas_ue_context_t* nas_ctx, nas_message_t* message);

// NAS Authentication Functions
uesim_error_t nas_generate_authentication_vector(nas_ue_context_t* nas_ctx, 
                                                nas_auth_vector_t* vector);
uesim_error_t nas_validate_authentication_response(nas_ue_context_t* nas_ctx, 
                                                  const uint8_t* response, size_t response_length,
                                                  bool* valid);
uesim_error_t nas_derive_nas_keys(nas_ue_context_t* nas_ctx, const uint8_t* kasme);

// NAS Timer Functions
uesim_error_t nas_start_timer(nas_ue_context_t* nas_ctx, uint16_t timer_id, uint32_t timeout_ms);
uesim_error_t nas_stop_timer(nas_ue_context_t* nas_ctx, uint16_t timer_id);
uesim_error_t nas_handle_timer_expiry(nas_ue_context_t* nas_ctx, uint16_t timer_id);

// NAS Utility Functions
uesim_error_t nas_update_ue_identity(nas_ue_context_t* nas_ctx, const nas_ue_identity_t* identity);
uesim_error_t nas_get_ue_identity(nas_ue_context_t* nas_ctx, nas_ue_identity_t* identity);
bool nas_is_ue_registered(nas_ue_context_t* nas_ctx);
bool nas_is_pdu_session_active(nas_ue_context_t* nas_ctx, uint8_t pdu_session_id);
uesim_error_t nas_get_statistics(nas_ue_context_t* nas_ctx, nas_stats_t* stats);
uesim_error_t nas_update_statistics(nas_ue_context_t* nas_ctx);

// NAS Configuration Functions
uesim_error_t nas_set_default_config(nas_ue_context_t* nas_ctx);
uesim_error_t nas_set_registration_config(nas_ue_context_t* nas_ctx, 
                                         nas_registration_type_t reg_type);
uesim_error_t nas_set_security_config(nas_ue_context_t* nas_ctx, 
                                     nas_ciphering_algorithm_t cipher_alg,
                                     nas_integrity_algorithm_t integrity_alg);
uesim_error_t nas_set_pdu_session_config(nas_ue_context_t* nas_ctx, uint8_t pdu_session_id,
                                        nas_pdu_session_type_t session_type,
                                        nas_ssc_mode_t ssc_mode);

#endif // NAS_H