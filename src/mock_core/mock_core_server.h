/*
 * 5G UE Simulation Application
 * Mock Core Network Server - Standalone server for testing
 * 
 * This module provides a standalone server that runs all mock core components:
 * - AMF (NGAP port 38412)
 * - SMF (PFCP port 8805)
 * - UPF (GTP-U port 2152)
 * - CU-CP (F1AP port 38472)
 * - DU (F1AP client)
 * - CU-UP (E1AP port 38470)
 * - XnAP (XnAP port 38422)
 * 
 * Usage:
 *   ./mock_core_server [options]
 *   ./mock_core_server --amf-port 38412 --upf-port 2152
 */

#ifndef MOCK_CORE_SERVER_H
#define MOCK_CORE_SERVER_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
typedef HANDLE pthread_t;
typedef HANDLE pthread_mutex_t;
#else
#include <pthread.h>
#endif

/* ============== Constants ============== */

#define MOCK_CORE_SERVER_MAX_NAME_LEN    64
#define MOCK_CORE_SERVER_MAX_IP_LEN      46
#define MOCK_CORE_SERVER_MAX_PCAP_FILE   256

/* ============== Error Codes ============== */

typedef enum {
    MOCK_CORE_SERVER_SUCCESS = 0,
    MOCK_CORE_SERVER_ERROR_INVALID_PARAM = -1,
    MOCK_CORE_SERVER_ERROR_MEMORY = -2,
    MOCK_CORE_SERVER_ERROR_SOCKET = -3,
    MOCK_CORE_SERVER_ERROR_THREAD = -4,
    MOCK_CORE_SERVER_ERROR_TIMEOUT = -5,
    MOCK_CORE_SERVER_ERROR_NOT_INITIALIZED = -6,
    MOCK_CORE_SERVER_ERROR_ALREADY_RUNNING = -7,
    MOCK_CORE_SERVER_ERROR_NOT_RUNNING = -8,
    MOCK_CORE_SERVER_ERROR_COMPONENT = -9
} mock_core_server_error_t;

/* ============== Server States ============== */

typedef enum {
    MOCK_CORE_SERVER_STATE_IDLE = 0,
    MOCK_CORE_SERVER_STATE_INITIALIZING,
    MOCK_CORE_SERVER_STATE_STARTING,
    MOCK_CORE_SERVER_STATE_RUNNING,
    MOCK_CORE_SERVER_STATE_STOPPING,
    MOCK_CORE_SERVER_STATE_ERROR,
    MOCK_CORE_SERVER_STATE_MAX
} mock_core_server_state_t;

/* ============== Component Configuration ============== */

typedef struct {
    bool enabled;
    char bind_ip[MOCK_CORE_SERVER_MAX_IP_LEN];
    uint16_t port;
    bool log_messages;
} mock_core_component_cfg_t;

/* ============== Server Configuration ============== */

typedef struct {
    /* Core Network Components */
    mock_core_component_cfg_t amf;
    mock_core_component_cfg_t smf;
    mock_core_component_cfg_t upf;
    
    /* gNB Components */
    mock_core_component_cfg_t cu_cp;
    mock_core_component_cfg_t du;
    mock_core_component_cfg_t cu_up;
    mock_core_component_cfg_t xnap;
    
    /* AMF Configuration */
    uint32_t amf_id;
    char amf_name[MOCK_CORE_SERVER_MAX_NAME_LEN];
    uint32_t tac;
    
    /* Behavior */
    bool auto_respond;
    uint32_t response_delay_ms;
    bool verbose;
    
    /* Logging */
    bool log_to_console;
    char log_file[MOCK_CORE_SERVER_MAX_PCAP_FILE];
    char pcap_file[MOCK_CORE_SERVER_MAX_PCAP_FILE];
} mock_core_server_config_t;

/* ============== Component Statistics ============== */

typedef struct {
    uint32_t messages_sent;
    uint32_t messages_received;
    uint32_t errors;
    uint32_t active_connections;
} mock_core_component_stats_t;

/* ============== Server Statistics ============== */

typedef struct {
    /* Component Statistics */
    mock_core_component_stats_t amf;
    mock_core_component_stats_t smf;
    mock_core_component_stats_t upf;
    mock_core_component_stats_t cu_cp;
    mock_core_component_stats_t du;
    mock_core_component_stats_t cu_up;
    mock_core_component_stats_t xnap;
    
    /* Overall Statistics */
    uint32_t total_registrations;
    uint32_t successful_registrations;
    uint32_t total_pdu_sessions;
    uint32_t active_pdu_sessions;
    uint32_t total_ue_contexts;
    
    /* Timing */
    time_t start_time;
    uint64_t uptime_seconds;
} mock_core_server_stats_t;

/* ============== Server Context ============== */

typedef struct {
    /* Configuration */
    mock_core_server_config_t config;
    
    /* State */
    mock_core_server_state_t state;
    char last_error[256];
    
    /* Component Servers (forward declarations from mock_core.h) */
    struct amf_server_s* amf;
    struct smf_server_s* smf;
    struct upf_server_s* upf;
    struct cu_cp_server_s* cu_cp;
    struct du_server_s* du;
    struct cu_up_server_s* cu_up;
    struct xnap_server_s* xnap;
    
    /* Statistics */
    mock_core_server_stats_t stats;
    
    /* Threading */
#ifdef _WIN32
    volatile LONG running;
#else
    atomic_bool running;
#endif
} mock_core_server_ctx_t;

/* ============== API Functions ============== */

/**
 * Get default server configuration
 * @param config Configuration structure to fill
 */
void mock_core_server_get_default_config(mock_core_server_config_t* config);

/**
 * Create server context
 * @param config Configuration (NULL for defaults)
 * @return Server context or NULL on failure
 */
mock_core_server_ctx_t* mock_core_server_create(const mock_core_server_config_t* config);

/**
 * Destroy server context
 * @param ctx Server context
 */
void mock_core_server_destroy(mock_core_server_ctx_t* ctx);

/**
 * Start server (starts all enabled components)
 * @param ctx Server context
 * @return MOCK_CORE_SERVER_SUCCESS or error code
 */
mock_core_server_error_t mock_core_server_start(mock_core_server_ctx_t* ctx);

/**
 * Stop server (stops all components)
 * @param ctx Server context
 */
void mock_core_server_stop(mock_core_server_ctx_t* ctx);

/**
 * Check if server is running
 * @param ctx Server context
 * @return true if running
 */
bool mock_core_server_is_running(const mock_core_server_ctx_t* ctx);

/**
 * Get server state
 * @param ctx Server context
 * @return Current state
 */
mock_core_server_state_t mock_core_server_get_state(const mock_core_server_ctx_t* ctx);

/**
 * Get server statistics
 * @param ctx Server context
 * @param stats Statistics structure to fill
 * @return MOCK_CORE_SERVER_SUCCESS or error code
 */
mock_core_server_error_t mock_core_server_get_stats(mock_core_server_ctx_t* ctx,
                                                     mock_core_server_stats_t* stats);

/**
 * Get last error message
 * @param ctx Server context
 * @return Error message string
 */
const char* mock_core_server_get_last_error(const mock_core_server_ctx_t* ctx);

/**
 * Print server status
 * @param ctx Server context
 */
void mock_core_server_print_status(const mock_core_server_ctx_t* ctx);

/**
 * Print server statistics
 * @param ctx Server context
 */
void mock_core_server_print_stats(const mock_core_server_ctx_t* ctx);

/* ============== Utility Functions ============== */

/**
 * Convert error code to string
 * @param error Error code
 * @return Error string
 */
const char* mock_core_server_error_to_string(mock_core_server_error_t error);

/**
 * Convert state to string
 * @param state State
 * @return State string
 */
const char* mock_core_server_state_to_string(mock_core_server_state_t state);

#endif /* MOCK_CORE_SERVER_H */