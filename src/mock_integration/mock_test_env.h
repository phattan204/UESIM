/*
 * 5G UE Simulation Application
 * Mock Test Environment - Integration testing infrastructure
 * 
 * This module provides a complete test environment that orchestrates:
 * - Mock Core Network (AMF, SMF, UPF, CU-CP, DU, CU-UP, XnAP)
 * - Mock gNB Server
 * - UESim UE instances
 * 
 * Usage:
 *   mock_test_env_t* env = mock_test_env_create(&config);
 *   mock_test_env_start(env);
 *   // Run tests...
 *   mock_test_env_stop(env);
 *   mock_test_env_destroy(env);
 */

#ifndef MOCK_TEST_ENV_H
#define MOCK_TEST_ENV_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* Include component headers for type definitions */
#include "../mock_core/mock_core.h"
#include "../mock_gnb/mock_gnb_server.h"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <pthread.h>
#endif

/* Forward declarations - these types are defined in their respective headers */
/* Note: Include uesim.h before this header for ue_context_t */

/* ============== Constants ============== */

#define MOCK_TEST_MAX_UES           1024
#define MOCK_TEST_MAX_GNBS           16
#define MOCK_TEST_MAX_NAME_LEN       64
#define MOCK_TEST_MAX_IP_LEN         46

/* ============== Error Codes ============== */

typedef enum {
    MOCK_TEST_SUCCESS = 0,
    MOCK_TEST_ERROR_INVALID_PARAM = -1,
    MOCK_TEST_ERROR_MEMORY = -2,
    MOCK_TEST_ERROR_SOCKET = -3,
    MOCK_TEST_ERROR_THREAD = -4,
    MOCK_TEST_ERROR_TIMEOUT = -5,
    MOCK_TEST_ERROR_NOT_INITIALIZED = -6,
    MOCK_TEST_ERROR_ALREADY_RUNNING = -7,
    MOCK_TEST_ERROR_NOT_RUNNING = -8,
    MOCK_TEST_ERROR_COMPONENT = -9,
    MOCK_TEST_ERROR_PROTOCOL = -10,
    MOCK_TEST_ERROR_CAPACITY = -11,
    MOCK_TEST_ERROR_NOT_FOUND = -12
} mock_test_error_t;

/* ============== Component States ============== */

typedef enum {
    MOCK_TEST_STATE_IDLE = 0,
    MOCK_TEST_STATE_INITIALIZING,
    MOCK_TEST_STATE_STARTING_CORE,
    MOCK_TEST_STATE_STARTING_GNB,
    MOCK_TEST_STATE_CONNECTING,
    MOCK_TEST_STATE_READY,
    MOCK_TEST_STATE_RUNNING_TESTS,
    MOCK_TEST_STATE_STOPPING,
    MOCK_TEST_STATE_ERROR,
    MOCK_TEST_STATE_MAX
} mock_test_state_t;

/* ============== Component Configuration ============== */

typedef struct {
    bool enabled;
    char bind_ip[MOCK_TEST_MAX_IP_LEN];
    uint16_t port;
    bool log_messages;
} mock_test_component_config_t;

typedef struct {
    /* Core Network */
    mock_test_component_config_t amf;
    mock_test_component_config_t smf;
    mock_test_component_config_t upf;
    
    /* gNB Components */
    mock_test_component_config_t cu_cp;
    mock_test_component_config_t du;
    mock_test_component_config_t cu_up;
    mock_test_component_config_t xnap;
    
    /* gNB Server */
    mock_test_component_config_t gnb_server;
    uint16_t gnb_gtpu_port;
    
    /* gNB Cell Configuration */
    uint32_t gnb_id;
    char gnb_name[MOCK_TEST_MAX_NAME_LEN];
    uint16_t tac;
    uint16_t pci;
    uint32_t cell_id;
    
    /* Test Configuration */
    uint32_t max_ues;
    uint32_t response_delay_ms;
    bool auto_respond;
    bool capture_pcap;
    char pcap_file[256];
    
    /* Logging */
    bool verbose;
    bool log_to_console;
    char log_file[256];
} mock_test_env_config_t;

/* ============== Component Statistics ============== */

typedef struct {
    uint32_t messages_sent;
    uint32_t messages_received;
    uint32_t errors;
    uint32_t active_connections;
} mock_test_component_stats_t;

typedef struct {
    /* Core Statistics */
    mock_test_component_stats_t amf;
    mock_test_component_stats_t smf;
    mock_test_component_stats_t upf;
    
    /* gNB Statistics */
    mock_test_component_stats_t cu_cp;
    mock_test_component_stats_t du;
    mock_test_component_stats_t cu_up;
    mock_test_component_stats_t xnap;
    mock_test_component_stats_t gnb_server;
    
    /* Test Statistics */
    uint32_t total_tests;
    uint32_t passed_tests;
    uint32_t failed_tests;
    uint32_t active_ues;
    
    /* Timing */
    time_t start_time;
    uint64_t duration_ms;
} mock_test_stats_t;

/* ============== Test Environment Context ============== */

typedef struct {
    /* Configuration */
    mock_test_env_config_t config;
    
    /* State */
    mock_test_state_t state;
    char last_error[256];
    
    /* Core Components */
    amf_server_t* amf;
    smf_server_t* smf;
    upf_server_t* upf;
    
    /* gNB Components */
    cu_cp_server_t* cu_cp;
    du_server_t* du;
    cu_up_server_t* cu_up;
    xnap_server_t* xnap;
    
    /* gNB Server */
    mock_gnb_server_t* gnb_server;
    
    /* UE Instances */
    ue_context_t* ue_instances[MOCK_TEST_MAX_UES];
    uint32_t num_active_ues;
    
    /* Statistics */
    mock_test_stats_t stats;
    
    /* Threading */
#ifdef _WIN32
    volatile LONG running;
    HANDLE monitor_thread;
#else
    atomic_bool running;
    pthread_t monitor_thread;
#endif
    
} mock_test_env_t;

/* ============== API Functions ============== */

/**
 * Get default test environment configuration
 * @param config Configuration structure to fill
 */
void mock_test_env_get_default_config(mock_test_env_config_t* config);

/**
 * Create test environment
 * @param config Configuration (NULL for defaults)
 * @return Test environment or NULL on failure
 */
mock_test_env_t* mock_test_env_create(const mock_test_env_config_t* config);

/**
 * Destroy test environment
 * @param env Test environment
 */
void mock_test_env_destroy(mock_test_env_t* env);

/**
 * Start test environment (starts all components)
 * @param env Test environment
 * @return MOCK_TEST_SUCCESS or error code
 */
mock_test_error_t mock_test_env_start(mock_test_env_t* env);

/**
 * Stop test environment (stops all components)
 * @param env Test environment
 */
void mock_test_env_stop(mock_test_env_t* env);

/**
 * Check if test environment is running
 * @param env Test environment
 * @return true if running, false otherwise
 */
bool mock_test_env_is_running(const mock_test_env_t* env);

/**
 * Get test environment state
 * @param env Test environment
 * @return Current state
 */
mock_test_state_t mock_test_env_get_state(const mock_test_env_t* env);

/**
 * Get test environment statistics
 * @param env Test environment
 * @param stats Statistics structure to fill
 * @return MOCK_TEST_SUCCESS or error code
 */
mock_test_error_t mock_test_env_get_stats(const mock_test_env_t* env, 
                                          mock_test_stats_t* stats);

/**
 * Get last error message
 * @param env Test environment
 * @return Error message string
 */
const char* mock_test_env_get_last_error(const mock_test_env_t* env);

/* ============== Component Control ============== */

/**
 * Start core network components only
 * @param env Test environment
 * @return MOCK_TEST_SUCCESS or error code
 */
mock_test_error_t mock_test_env_start_core(mock_test_env_t* env);

/**
 * Start gNB components only
 * @param env Test environment
 * @return MOCK_TEST_SUCCESS or error code
 */
mock_test_error_t mock_test_env_start_gnb(mock_test_env_t* env);

/**
 * Connect components (F1 Setup, E1 Setup, NG Setup)
 * @param env Test environment
 * @return MOCK_TEST_SUCCESS or error code
 */
mock_test_error_t mock_test_env_connect_components(mock_test_env_t* env);

/**
 * Stop core network components
 * @param env Test environment
 */
void mock_test_env_stop_core(mock_test_env_t* env);

/**
 * Stop gNB components
 * @param env Test environment
 */
void mock_test_env_stop_gnb(mock_test_env_t* env);

/* ============== UE Management ============== */

/**
 * Register UE instance with test environment
 * @param env Test environment
 * @param ue UE context
 * @return MOCK_TEST_SUCCESS or error code
 */
mock_test_error_t mock_test_env_register_ue(mock_test_env_t* env, ue_context_t* ue);

/**
 * Unregister UE instance from test environment
 * @param env Test environment
 * @param ue_id UE identifier
 * @return MOCK_TEST_SUCCESS or error code
 */
mock_test_error_t mock_test_env_unregister_ue(mock_test_env_t* env, uint32_t ue_id);

/**
 * Find UE by index
 * @param env Test environment
 * @param index UE index
 * @return UE context or NULL
 */
ue_context_t* mock_test_env_get_ue(const mock_test_env_t* env, uint32_t index);

/**
 * Get active UE count
 * @param env Test environment
 * @return Number of active UEs
 */
uint32_t mock_test_env_get_ue_count(const mock_test_env_t* env);

/* ============== Test Flow Control ============== */

/**
 * Run complete registration flow for UE
 * @param env Test environment
 * @param ue_index UE index
 * @param timeout_ms Timeout in milliseconds
 * @return MOCK_TEST_SUCCESS or error code
 */
mock_test_error_t mock_test_env_run_registration(mock_test_env_t* env, 
                                                  uint32_t ue_index,
                                                  uint32_t timeout_ms);

/**
 * Run PDU session establishment flow for UE
 * @param env Test environment
 * @param ue_index UE index
 * @param pdu_session_id PDU session ID
 * @param timeout_ms Timeout in milliseconds
 * @return MOCK_TEST_SUCCESS or error code
 */
mock_test_error_t mock_test_env_run_pdu_session(mock_test_env_t* env,
                                                 uint32_t ue_index,
                                                 uint8_t pdu_session_id,
                                                 uint32_t timeout_ms);

/**
 * Run handover flow for UE
 * @param env Test environment
 * @param ue_index UE index
 * @param target_gnb_id Target gNB ID
 * @param target_cell_id Target cell ID
 * @param timeout_ms Timeout in milliseconds
 * @return MOCK_TEST_SUCCESS or error code
 */
mock_test_error_t mock_test_env_run_handover(mock_test_env_t* env,
                                              uint32_t ue_index,
                                              uint32_t target_gnb_id,
                                              uint32_t target_cell_id,
                                              uint32_t timeout_ms);

/**
 * Run deregistration flow for UE
 * @param env Test environment
 * @param ue_index UE index
 * @param timeout_ms Timeout in milliseconds
 * @return MOCK_TEST_SUCCESS or error code
 */
mock_test_error_t mock_test_env_run_deregistration(mock_test_env_t* env,
                                                    uint32_t ue_index,
                                                    uint32_t timeout_ms);

/* ============== Utility Functions ============== */

/**
 * Convert error code to string
 * @param error Error code
 * @return Error string
 */
const char* mock_test_error_to_string(mock_test_error_t error);

/**
 * Convert state to string
 * @param state State
 * @return State string
 */
const char* mock_test_state_to_string(mock_test_state_t state);

/**
 * Print test environment status
 * @param env Test environment
 */
void mock_test_env_print_status(const mock_test_env_t* env);

/**
 * Print test environment statistics
 * @param env Test environment
 */
void mock_test_env_print_stats(const mock_test_env_t* env);

/* ============== Component Health Check ============== */

/**
 * Component health status
 */
typedef enum {
    MOCK_TEST_HEALTH_UNKNOWN = 0,
    MOCK_TEST_HEALTH_HEALTHY = 1,
    MOCK_TEST_HEALTH_DEGRADED = 2,
    MOCK_TEST_HEALTH_UNHEALTHY = 3
} mock_test_health_status_t;

/**
 * Component health info
 */
typedef struct {
    mock_test_health_status_t status;
    uint32_t error_count;
    uint32_t last_error_code;
    char last_error_msg[128];
    uint64_t last_activity_ms;
} mock_test_component_health_t;

/**
 * Full health check result
 */
typedef struct {
    mock_test_component_health_t amf;
    mock_test_component_health_t smf;
    mock_test_component_health_t upf;
    mock_test_component_health_t cu_cp;
    mock_test_component_health_t du;
    mock_test_component_health_t cu_up;
    mock_test_component_health_t xnap;
    mock_test_component_health_t gnb_server;
    uint32_t total_errors;
    uint32_t unhealthy_count;
} mock_test_health_check_t;

/**
 * Perform health check on all components
 * @param env Test environment
 * @param health Health check result (output)
 * @return MOCK_TEST_SUCCESS or error code
 */
mock_test_error_t mock_test_env_health_check(const mock_test_env_t* env, 
                                              mock_test_health_check_t* health);

/**
 * Check if specific component is healthy
 * @param env Test environment
 * @param component Component name ("amf", "smf", "upf", "cu_cp", "du", "cu_up", "xnap", "gnb_server")
 * @return Health status
 */
mock_test_health_status_t mock_test_env_check_component_health(const mock_test_env_t* env,
                                                                 const char* component);

/**
 * Print health check results
 * @param health Health check result
 */
void mock_test_env_print_health_check(const mock_test_health_check_t* health);

#endif /* MOCK_TEST_ENV_H */
