/*
 * 5G UE Simulation Application
 * Mock gNB Server - Simulates real gNB for testing
 * 
 * This module implements a mock gNB server that:
 * - Listens on real TCP/UDP sockets (NGAP/GTP-U)
 * - Implements 3GPP protocol state machines
 * - Generates proper responses to UE messages
 * - Supports PCAP logging for traffic capture
 */

#ifndef MOCK_GNB_SERVER_H
#define MOCK_GNB_SERVER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef HANDLE pthread_t;
typedef HANDLE pthread_mutex_t;
typedef HANDLE pthread_cond_t;
#else
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

/* ============== Constants ============== */

#define MOCK_GNB_DEFAULT_NGAP_PORT    38412
#define MOCK_GNB_DEFAULT_GTPU_PORT   2152
#define MOCK_GNB_MAX_UES             1024
#define MOCK_GNB_MAX_CONNECTIONS     64
#define MOCK_GNB_MAX_PDU_SESSIONS     16
#define MOCK_GNB_MAX_QOS_FLOWS        8
#define MOCK_GNB_MAX_BEARSERS         32
#define MOCK_GNB_BUFFER_SIZE          65536
#define MOCK_GNB_MAX_PCAP_FILE        256

/* ============== Error Codes ============== */

typedef enum {
    MOCK_GNB_SUCCESS = 0,
    MOCK_GNB_ERROR_INVALID_PARAM = -1,
    MOCK_GNB_ERROR_SOCKET = -2,
    MOCK_GNB_ERROR_MEMORY = -3,
    MOCK_GNB_ERROR_THREAD = -4,
    MOCK_GNB_ERROR_TIMEOUT = -5,
    MOCK_GNB_ERROR_PROTOCOL = -6,
    MOCK_GNB_ERROR_NOT_FOUND = -7,
    MOCK_GNB_ERROR_CAPACITY = -8,
    MOCK_GNB_ERROR_ALREADY_EXISTS = -9,
    MOCK_GNB_ERROR_FILE = -10,
    MOCK_GNB_ERROR_ENCODING = -11
} mock_gnb_error_t;

/* ============== Protocol States ============== */

typedef enum {
    MOCK_GNB_UE_STATE_IDLE = 0,
    MOCK_GNB_UE_STATE_CONNECTING,
    MOCK_GNB_UE_STATE_CONNECTED,
    MOCK_GNB_UE_STATE_REGISTERED,
    MOCK_GNB_UE_STATE_DEREGISTERING,
    MOCK_GNB_UE_STATE_HANDOVER_PREP,
    MOCK_GNB_UE_STATE_REESTABLISHING,
    MOCK_GNB_UE_STATE_RELEASED,
    MOCK_GNB_UE_STATE_MAX
} mock_gnb_ue_state_t;

typedef enum {
    MOCK_GNB_PDU_STATE_INACTIVE = 0,
    MOCK_GNB_PDU_STATE_ESTABLISHING,
    MOCK_GNB_PDU_STATE_ACTIVE,
    MOCK_GNB_PDU_STATE_RELEASING,
    MOCK_GNB_PDU_STATE_MAX
} mock_gnb_pdu_state_t;

/* ============== Message Types ============== */

/* NGAP Message Types */
typedef enum {
    MOCK_NGAP_NG_SETUP_REQUEST = 0,
    MOCK_NGAP_NG_SETUP_RESPONSE,
    MOCK_NGAP_NG_SETUP_FAILURE,
    MOCK_NGAP_INITIAL_UE_MESSAGE,
    MOCK_NGAP_INITIAL_CONTEXT_SETUP_REQUEST,
    MOCK_NGAP_INITIAL_CONTEXT_SETUP_RESPONSE,
    MOCK_NGAP_INITIAL_CONTEXT_SETUP_FAILURE,
    MOCK_NGAP_UE_CONTEXT_RELEASE_REQUEST,
    MOCK_NGAP_UE_CONTEXT_RELEASE_COMMAND,
    MOCK_NGAP_UE_CONTEXT_RELEASE_COMPLETE,
    MOCK_NGAP_PDU_SESSION_SETUP_REQUEST,
    MOCK_NGAP_PDU_SESSION_SETUP_RESPONSE,
    MOCK_NGAP_PDU_SESSION_RELEASE_COMMAND,
    MOCK_NGAP_PDU_SESSION_RELEASE_COMPLETE,
    MOCK_NGAP_HANDOVER_PREPARATION,
    MOCK_NGAP_HANDOVER_REQUEST,
    MOCK_NGAP_HANDOVER_COMMAND,
    MOCK_NGAP_HANDOVER_NOTIFY,
    MOCK_NGAP_PATH_SWITCH_REQUEST,
    MOCK_NGAP_UPLINK_NAS_TRANSPORT,
    MOCK_NGAP_DOWNLINK_NAS_TRANSPORT,
    MOCK_NGAP_ERROR_INDICATION,
    MOCK_NGAP_MAX
} mock_ngap_message_type_t;

/* NAS Message Types */
typedef enum {
    MOCK_NAS_REGISTRATION_REQUEST = 0,
    MOCK_NAS_REGISTRATION_ACCEPT,
    MOCK_NAS_REGISTRATION_REJECT,
    MOCK_NAS_REGISTRATION_COMPLETE,
    MOCK_NAS_AUTHENTICATION_REQUEST,
    MOCK_NAS_AUTHENTICATION_RESPONSE,
    MOCK_NAS_AUTHENTICATION_REJECT,
    MOCK_NAS_SECURITY_MODE_COMMAND,
    MOCK_NAS_SECURITY_MODE_COMPLETE,
    MOCK_NAS_SECURITY_MODE_REJECT,
    MOCK_NAS_UL_NAS_TRANSPORT,
    MOCK_NAS_DL_NAS_TRANSPORT,
    MOCK_NAS_PDU_SESSION_ESTABLISHMENT_REQUEST,
    MOCK_NAS_PDU_SESSION_ESTABLISHMENT_ACCEPT,
    MOCK_NAS_PDU_SESSION_ESTABLISHMENT_REJECT,
    MOCK_NAS_PDU_SESSION_RELEASE_REQUEST,
    MOCK_NAS_PDU_SESSION_RELEASE_COMMAND,
    MOCK_NAS_PDU_SESSION_RELEASE_COMPLETE,
    MOCK_NAS_MAX
} mock_nas_message_type_t;

/* ============== UE Context ============== */

typedef struct {
    uint8_t pdu_session_id;
    mock_gnb_pdu_state_t state;
    uint8_t pdu_session_type;  /* IPv4, IPv6, IPv4v6 */
    uint32_t ue_ip_address;
    uint8_t qos_flow_id;
    uint8_t five_qi;
    uint16_t upf_teid;
    struct sockaddr_in upf_addr;
    time_t establish_time;
} mock_gnb_pdu_session_t;

typedef struct {
    uint32_t ran_ue_ngap_id;          /* gNB-side UE ID */
    uint64_t amf_ue_ngap_id;          /* AMF-side UE ID */
    uint64_t ue_identity;             /* UE identity from RRCSetupRequest (39-bit random value) */
    char imsi[16];
    char guti[24];
    uint16_t rnti;
    mock_gnb_ue_state_t state;
    
    /* Connection info */
    int ngap_socket;
    struct sockaddr_in ue_addr;
    time_t connect_time;
    time_t last_activity;
    
    /* PDU Sessions */
    mock_gnb_pdu_session_t pdu_sessions[MOCK_GNB_MAX_PDU_SESSIONS];
    uint8_t num_active_sessions;
    
    /* Security context */
    bool security_context_valid;
    uint8_t ciphering_alg;
    uint8_t integrity_alg;
    uint32_t uplink_count;
    uint32_t downlink_count;
    
    /* Handover info */
    bool handover_prepared;
    uint32_t target_gnb_id;
    
    /* Statistics */
    uint64_t tx_bytes;
    uint64_t rx_bytes;
    uint32_t tx_packets;
    uint32_t rx_packets;
} mock_gnb_ue_context_t;

/* ============== gNB Context ============== */

typedef struct {
    uint32_t gnb_id;
    char gnb_name[64];
    uint16_t tac;
    uint32_t plmn_id;  /* MCC + MNC encoded */
    
    /* Cell info */
    uint16_t pci;
    uint32_t cell_id;
    uint32_t arfcn;
    int32_t rsrp_offset;  /* Simulated RSRP offset */
    int32_t rsrq_offset;  /* Simulated RSRQ offset */
    
    /* Capabilities */
    bool supports_xn_handover;
    bool supports_n2_handover;
    uint32_t max_ues_supported;
} mock_gnb_cell_config_t;

/* ============== Server Configuration ============== */

typedef struct {
    char bind_ip[46];
    uint16_t ngap_port;
    uint16_t gtpu_port;
    uint32_t max_ues;
    uint32_t max_connections;
    
    /* Behavior configuration */
    bool auto_respond;           /* Auto-generate responses */
    uint32_t response_delay_ms;  /* Simulated processing delay */
    bool accept_all_connections; /* Accept all incoming connections */
    
    /* Logging */
    bool log_messages;
    bool log_to_console;
    char pcap_file[MOCK_GNB_MAX_PCAP_FILE];
    char log_file[MOCK_GNB_MAX_PCAP_FILE];
    
    /* Cell configuration */
    mock_gnb_cell_config_t cell_config;
} mock_gnb_config_t;

/* ============== Server Statistics ============== */

typedef struct {
    uint32_t total_connections;
    uint32_t active_connections;
    uint32_t total_registrations;
    uint32_t successful_registrations;
    uint32_t failed_registrations;
    uint32_t total_handovers;
    uint32_t successful_handovers;
    uint32_t failed_handovers;
    uint64_t total_tx_bytes;
    uint64_t total_rx_bytes;
    uint32_t ngap_messages_sent;
    uint32_t ngap_messages_received;
    uint32_t gtpu_packets_sent;
    uint32_t gtpu_packets_received;
    time_t start_time;
} mock_gnb_stats_t;

/* ============== PCAP Context ============== */

typedef struct {
    FILE* pcap_file;
    uint32_t packet_count;
    pthread_mutex_t pcap_mutex;
    bool enabled;
} mock_gnb_pcap_t;

/* ============== Server Context ============== */

typedef struct {
    /* Configuration */
    mock_gnb_config_t config;
    
    /* Sockets */
    int ngap_listen_socket;
    int gtpu_socket;
    
    /* UE contexts */
    mock_gnb_ue_context_t* ue_contexts[MOCK_GNB_MAX_UES];
    uint32_t num_active_ues;
    pthread_mutex_t ue_mutex;
    
    /* Server state */
#ifdef _WIN32
    volatile LONG running;
#else
    atomic_bool running;
#endif
    pthread_t ngap_thread;
    pthread_t gtpu_thread;
    
    /* PCAP logging */
    mock_gnb_pcap_t pcap;
    
    /* Statistics */
    mock_gnb_stats_t stats;
    pthread_mutex_t stats_mutex;
    
} mock_gnb_server_t;

/* ============== API Functions ============== */

/**
 * Initialize mock gNB server with configuration
 * @param config Server configuration
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_server_init(const mock_gnb_config_t* config);

/**
 * Start mock gNB server (blocking call)
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_server_start(void);

/**
 * Stop mock gNB server
 */
void mock_gnb_server_stop(void);

/**
 * Get server statistics
 * @param stats Pointer to stats structure to fill
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_server_get_stats(mock_gnb_stats_t* stats);

/**
 * Check if server is running
 * @return true if running, false otherwise
 */
bool mock_gnb_server_is_running(void);

/**
 * Get default configuration
 * @param config Pointer to config structure to fill
 */
void mock_gnb_get_default_config(mock_gnb_config_t* config);

/* ============== Message Handling Functions ============== */

/**
 * Handle incoming NGAP message
 * @param socket Client socket
 * @param data Message data
 * @param len Message length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_handle_ngap_message(int socket, const void* data, size_t len);

/**
 * Handle incoming GTP-U packet
 * @param socket GTP-U socket
 * @param data Packet data
 * @param len Packet length
 * @param src_addr Source address
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_handle_gtpu_packet(int socket, const void* data, size_t len,
                                              const struct sockaddr_in* src_addr);

/* ============== Response Generation Functions ============== */

/**
 * Generate NG Setup Response
 * @param response Output buffer
 * @param len Output length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_ng_setup_response(void** response, size_t* len);

/**
 * Generate RRC Setup message
 * @param transaction_id Transaction ID
 * @param response Output buffer
 * @param len Output length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_rrc_setup(uint32_t transaction_id, void** response, size_t* len);

/**
 * Generate Registration Accept
 * @param ue_ctx UE context
 * @param response Output buffer
 * @param len Output length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_registration_accept(mock_gnb_ue_context_t* ue_ctx,
                                                        void** response, size_t* len);

/**
 * Generate PDU Session Establishment Accept
 * @param ue_ctx UE context
 * @param session_id PDU session ID
 * @param response Output buffer
 * @param len Output length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_pdu_session_accept(mock_gnb_ue_context_t* ue_ctx,
                                                       uint8_t session_id,
                                                       void** response, size_t* len);

/**
 * Generate Handover Command
 * @param ue_ctx UE context
 * @param target_pci Target cell PCI
 * @param response Output buffer
 * @param len Output length
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_generate_handover_command(mock_gnb_ue_context_t* ue_ctx,
                                                    uint16_t target_pci,
                                                    void** response, size_t* len);

/* ============== UE Context Management ============== */

/**
 * Create UE context
 * @param socket Client socket
 * @return UE context or NULL on failure
 */
mock_gnb_ue_context_t* mock_gnb_create_ue_context(int socket);

/**
 * Find UE context by socket
 * @param socket Client socket
 * @return UE context or NULL if not found
 */
mock_gnb_ue_context_t* mock_gnb_find_ue_by_socket(int socket);

/**
 * Find UE context by RAN-UE-NGAP-ID
 * @param ran_ue_ngap_id RAN-UE-NGAP-ID
 * @return UE context or NULL if not found
 */
mock_gnb_ue_context_t* mock_gnb_find_ue_by_ran_id(uint32_t ran_ue_ngap_id);

/**
 * Remove UE context
 * @param ue_ctx UE context to remove
 */
void mock_gnb_remove_ue_context(mock_gnb_ue_context_t* ue_ctx);

/* ============== PCAP Functions ============== */

/**
 * Initialize PCAP logging
 * @param filename Output file name
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_pcap_init(const char* filename);

/**
 * Write packet to PCAP file
 * @param data Packet data
 * @param len Packet length
 * @param src_addr Source address
 * @param dst_addr Destination address
 * @param protocol Protocol (6=TCP, 17=UDP)
 * @return MOCK_GNB_SUCCESS or error code
 */
mock_gnb_error_t mock_gnb_pcap_write_packet(const void* data, size_t len,
                                            const struct sockaddr_in* src_addr,
                                            const struct sockaddr_in* dst_addr,
                                            uint8_t protocol);

/**
 * Close PCAP file
 */
void mock_gnb_pcap_close(void);

/* ============== Utility Functions ============== */

/**
 * Convert UE state to string
 * @param state UE state
 * @return State string
 */
const char* mock_gnb_ue_state_to_string(mock_gnb_ue_state_t state);

/**
 * Convert NGAP message type to string
 * @param type Message type
 * @return Type string
 */
const char* mock_ngap_message_type_to_string(mock_ngap_message_type_t type);

/**
 * Convert NAS message type to string
 * @param type Message type
 * @return Type string
 */
const char* mock_nas_message_type_to_string(mock_nas_message_type_t type);

/**
 * Get current timestamp in milliseconds
 * @return Timestamp in ms
 */
uint64_t mock_gnb_get_time_ms(void);

#endif /* MOCK_GNB_SERVER_H */