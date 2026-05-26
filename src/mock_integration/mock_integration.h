/*
 * 5G UE Simulation Application
 * Mock Integration Layer - Connects mock_gnb with mock_core
 * 
 * Architecture:
 *   uesim ──(SCTP)──► mock_gnb ──(SCTP)──► mock_core (AMF)
 *                              │
 *                              └──(UDP)──► mock_core (UPF)
 */

#ifndef MOCK_INTEGRATION_H
#define MOCK_INTEGRATION_H

#include "../transport/sctp_transport.h"
#include "../mock_gnb/mock_gnb_server.h"
#include "../mock_core/mock_core.h"
#include <stdint.h>
#include <stdbool.h>

/* ============== Constants ============== */

#define MOCK_INT_DEFAULT_GNB_NGAP_PORT    48412   /* Avoid conflict with AMF */
#define MOCK_INT_DEFAULT_AMF_NGAP_PORT    38412   /* Standard AMF port */
#define MOCK_INT_DEFAULT_GNB_GTPU_PORT    2152
#define MOCK_INT_DEFAULT_UPF_GTPU_PORT    2153    /* Avoid conflict */
#define MOCK_INT_MAX_CONNECTIONS          64

/* ============== Integration Configuration ============== */

typedef struct {
    /* Mock gNB configuration */
    char gnb_bind_ip[46];
    uint16_t gnb_ngap_port;
    uint16_t gnb_gtpu_port;
    uint32_t gnb_id;
    char gnb_name[64];
    uint16_t tac;
    uint16_t pci;
    
    /* AMF (mock_core) configuration */
    char amf_ip[46];
    uint16_t amf_port;
    
    /* UPF configuration */
    char upf_ip[46];
    uint16_t upf_port;
    
    /* Behavior */
    bool auto_connect_amf;
    bool auto_forward_nas;
    bool log_messages;
    uint32_t response_delay_ms;
    
    /* PCAP */
    char pcap_file[256];
} mock_integration_config_t;

/* ============== Integration Context ============== */

typedef struct {
    mock_integration_config_t config;
    
    /* Components */
    mock_gnb_server_t* gnb_server;
    amf_server_t* amf_server;
    upf_server_t* upf_server;
    
    /* AMF Connection */
    sctp_socket_t amf_socket;
    bool amf_connected;
    bool amf_setup_complete;
    
    /* UE-to-AMF mapping */
    struct {
        uint32_t ran_ue_ngap_id;
        uint64_t amf_ue_ngap_id;
        int ue_socket;
        bool active;
    } ue_mapping[MOCK_GNB_MAX_UES];
    
    /* State */
#ifdef _WIN32
    volatile LONG running;
#else
    atomic_bool running;
#endif
    
    /* Statistics */
    uint64_t messages_to_amf;
    uint64_t messages_from_amf;
    uint64_t messages_to_ue;
    uint64_t messages_from_ue;
    
} mock_integration_ctx_t;

/* ============== Error Codes ============== */

typedef enum {
    MOCK_INT_SUCCESS = 0,
    MOCK_INT_ERROR_INVALID_PARAM = -1,
    MOCK_INT_ERROR_MEMORY = -2,
    MOCK_INT_ERROR_SOCKET = -3,
    MOCK_INT_ERROR_CONNECT = -4,
    MOCK_INT_ERROR_AMF_SETUP = -5,
    MOCK_INT_ERROR_NOT_CONNECTED = -6,
    MOCK_INT_ERROR_PROTOCOL = -7,
    MOCK_INT_ERROR_TIMEOUT = -8,
    MOCK_INT_ERROR_CAPACITY = -9
} mock_integration_error_t;

/* ============== API Functions ============== */

/**
 * Get default integration configuration
 * @param config Output configuration
 */
void mock_integration_get_default_config(mock_integration_config_t* config);

/**
 * Create integration context
 * @param config Configuration (NULL for defaults)
 * @return Integration context or NULL on failure
 */
mock_integration_ctx_t* mock_integration_create(const mock_integration_config_t* config);

/**
 * Destroy integration context
 * @param ctx Integration context
 */
void mock_integration_destroy(mock_integration_ctx_t* ctx);

/**
 * Start integration layer (starts gNB server and connects to AMF)
 * @param ctx Integration context
 * @return MOCK_INT_SUCCESS or error code
 */
mock_integration_error_t mock_integration_start(mock_integration_ctx_t* ctx);

/**
 * Stop integration layer
 * @param ctx Integration context
 */
void mock_integration_stop(mock_integration_ctx_t* ctx);

/**
 * Check if integration is running
 * @param ctx Integration context
 * @return true if running
 */
bool mock_integration_is_running(mock_integration_ctx_t* ctx);

/**
 * Check if connected to AMF
 * @param ctx Integration context
 * @return true if AMF is connected
 */
bool mock_integration_amf_connected(mock_integration_ctx_t* ctx);

/* ============== Message Forwarding ============== */

/**
 * Forward NGAP message to AMF
 * @param ctx Integration context
 * @param ue_ctx UE context (for routing)
 * @param data Message data
 * @param len Message length
 * @return MOCK_INT_SUCCESS or error code
 */
mock_integration_error_t mock_integration_forward_to_amf(mock_integration_ctx_t* ctx,
                                                          mock_gnb_ue_context_t* ue_ctx,
                                                          const void* data, size_t len);

/**
 * Process message received from AMF
 * @param ctx Integration context
 * @param data Message data
 * @param len Message length
 * @return MOCK_INT_SUCCESS or error code
 */
mock_integration_error_t mock_integration_process_amf_message(mock_integration_ctx_t* ctx,
                                                               const void* data, size_t len);

/**
 * Register UE with AMF (create mapping)
 * @param ctx Integration context
 * @param ran_ue_ngap_id RAN UE NGAP ID
 * @param ue_socket UE socket
 * @return MOCK_INT_SUCCESS or error code
 */
mock_integration_error_t mock_integration_register_ue(mock_integration_ctx_t* ctx,
                                                       uint32_t ran_ue_ngap_id,
                                                       int ue_socket);

/**
 * Unregister UE from AMF
 * @param ctx Integration context
 * @param ran_ue_ngap_id RAN UE NGAP ID
 */
void mock_integration_unregister_ue(mock_integration_ctx_t* ctx, uint32_t ran_ue_ngap_id);

/**
 * Find UE mapping by RAN UE ID
 * @param ctx Integration context
 * @param ran_ue_ngap_id RAN UE NGAP ID
 * @return UE mapping index or -1 if not found
 */
int mock_integration_find_ue_mapping(mock_integration_ctx_t* ctx, uint32_t ran_ue_ngap_id);

/* ============== NGAP Setup ============== */

/**
 * Perform NG Setup with AMF
 * @param ctx Integration context
 * @return MOCK_INT_SUCCESS or error code
 */
mock_integration_error_t mock_integration_ng_setup(mock_integration_ctx_t* ctx);

/* ============== Utility Functions ============== */

/**
 * Convert error code to string
 * @param error Error code
 * @return Error string
 */
const char* mock_integration_error_to_string(mock_integration_error_t error);

/**
 * Print integration statistics
 * @param ctx Integration context
 */
void mock_integration_print_stats(const mock_integration_ctx_t* ctx);

/* ============== GTP-U Tunnel Management ============== */

/**
 * Create GTP-U tunnel for user plane data
 * @param ctx Integration context
 * @param teid Tunnel endpoint identifier
 * @param ue_ip UE IP address
 * @param upf_ip UPF IP address
 * @param upf_port UPF GTP-U port
 * @param uplink True for uplink tunnel, false for downlink
 * @return MOCK_INT_SUCCESS or error code
 */
mock_integration_error_t mock_integration_create_gtpu_tunnel(
    mock_integration_ctx_t* ctx,
    uint32_t teid,
    uint32_t ue_ip,
    uint32_t upf_ip,
    uint16_t upf_port,
    bool uplink);

/**
 * Delete GTP-U tunnel
 * @param teid Tunnel endpoint identifier
 */
void mock_integration_delete_gtpu_tunnel(uint32_t teid);

/* ============== GTP-U Data Transfer ============== */

/**
 * Send data over GTP-U tunnel to UPF
 * @param ctx Integration context
 * @param teid Tunnel endpoint identifier
 * @param data User data
 * @param len Data length
 * @return MOCK_INT_SUCCESS or error code
 */
mock_integration_error_t mock_integration_send_gtpu_uplink(
    mock_integration_ctx_t* ctx,
    uint32_t teid,
    const void* data,
    size_t len);

#endif /* MOCK_INTEGRATION_H */
