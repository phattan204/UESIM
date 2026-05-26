/*
 * 5G UE Simulation Application
 * Connection Recovery Module Header
 * 
 * Features:
 * - Socket reconnection with exponential backoff
 * - Connection health monitoring
 * - gNB failover support
 * - Connection state machine
 */

#ifndef CONNECTION_RECOVERY_H
#define CONNECTION_RECOVERY_H

#include "../uesim.h"
#include "sctp_transport.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* ============== Constants ============== */

#define CONNECTION_MAX_RETRIES          5       /* Max reconnection attempts */
#define CONNECTION_BASE_BACKOFF_MS      1000    /* Initial backoff: 1 second */
#define CONNECTION_MAX_BACKOFF_MS       30000   /* Max backoff: 30 seconds */
#define CONNECTION_HEALTH_INTERVAL_MS   5000    /* Health check interval: 5s */
#define CONNECTION_TIMEOUT_MS           10000   /* Connection timeout: 10s */
#define CONNECTION_IDLE_TIMEOUT_MS      60000   /* Idle timeout: 60s */

/* ============== Connection States ============== */

typedef enum {
    CONN_STATE_DISCONNECTED = 0,    /* Not connected */
    CONN_STATE_CONNECTING,          /* Connection in progress */
    CONN_STATE_CONNECTED,           /* Active connection */
    CONN_STATE_RECONNECTING,        /* Reconnection in progress */
    CONN_STATE_FAILED,              /* Connection failed permanently */
    CONN_STATE_MAX
} connection_state_t;

/* ============== Connection Events ============== */

typedef enum {
    CONN_EVENT_UP = 0,              /* Connection established */
    CONN_EVENT_DOWN,                /* Connection lost */
    CONN_EVENT_TIMEOUT,             /* Connection timed out */
    CONN_EVENT_FAILOVER,            /* Failover to backup */
    CONN_EVENT_RECOVERED,           /* Connection recovered */
    CONN_EVENT_MAX
} connection_event_t;

/* ============== Recovery Configuration ============== */

typedef struct {
    uint32_t max_retries;           /* Maximum reconnection attempts */
    uint32_t base_backoff_ms;       /* Initial backoff duration */
    uint32_t max_backoff_ms;        /* Maximum backoff duration */
    uint32_t health_check_interval_ms;  /* Health check interval */
    uint32_t connection_timeout_ms; /* Connection timeout */
    uint32_t idle_timeout_ms;       /* Idle timeout before disconnect */
    bool enable_auto_reconnect;     /* Enable automatic reconnection */
    bool enable_failover;           /* Enable gNB failover */
} connection_recovery_config_t;

/* ============== Connection Statistics ============== */

typedef struct {
    uint32_t total_connections;     /* Total connection attempts */
    uint32_t successful_connections; /* Successful connections */
    uint32_t failed_connections;    /* Failed connection attempts */
    uint32_t reconnections;         /* Reconnection attempts */
    uint32_t timeouts;              /* Connection timeouts */
    uint32_t failovers;             /* Failover events */
    uint32_t recovery_successes;    /* Successful recoveries */
    uint64_t total_connected_time_ms;  /* Total connected time */
    uint64_t last_connected_time_ms;   /* Last connection timestamp */
    uint64_t last_disconnected_time_ms;/* Last disconnection timestamp */
    double avg_connection_time_ms;  /* Average connection time */
    double avg_backoff_ms;          /* Average backoff duration */
} connection_statistics_t;

/* ============== Connection Callback ============== */

typedef void (*connection_event_callback_t)(
    connection_state_t state,
    connection_event_t event,
    void* user_data
);

/* ============== Connection Recovery Context ============== */

typedef struct connection_recovery_ctx {
    /* Connection info */
    char primary_ip[64];
    uint16_t primary_port;
    char backup_ip[64];
    uint16_t backup_port;
    
    /* Socket handles */
    sctp_socket_t primary_socket;
    sctp_socket_t backup_socket;
    sctp_socket_t active_socket;
    
    /* State */
    connection_state_t state;
    connection_state_t previous_state;
    bool using_backup;
    uint32_t retry_count;
    uint32_t current_backoff_ms;
    
    /* Timing */
    uint64_t connect_start_time_ms;
    uint64_t last_activity_time_ms;
    uint64_t last_health_check_time_ms;
    time_t connect_time;
    
    /* Configuration */
    connection_recovery_config_t config;
    
    /* Statistics */
    connection_statistics_t stats;
    
    /* Callbacks */
    connection_event_callback_t event_callback;
    void* callback_user_data;
    
    /* Thread safety */
#ifdef _WIN32
    CRITICAL_SECTION mutex;
#else
    pthread_mutex_t mutex;
#endif
    
    /* Health check state */
    bool health_check_enabled;
    bool health_check_running;
#ifdef _WIN32
    HANDLE health_check_thread;
#else
    pthread_t health_check_thread;
#endif
    
    /* gNB failover list */
    struct {
        uint32_t gnb_id;
        char ip[64];
        uint16_t port;
        int32_t priority;  /* Higher = better */
        bool available;
    } failover_list[GNB_MAX_INSTANCES];
    uint8_t num_failover_entries;
    uint8_t current_failover_index;
} connection_recovery_ctx_t;

/* ============== Initialization & Cleanup ============== */

/**
 * Initialize connection recovery subsystem
 * @return UESIM_SUCCESS on success, error code otherwise
 */
int connection_recovery_init(void);

/**
 * Cleanup connection recovery subsystem
 */
void connection_recovery_cleanup(void);

/**
 * Create connection recovery context
 * @param config Recovery configuration
 * @return New context or NULL on failure
 */
connection_recovery_ctx_t* connection_recovery_create(
    const connection_recovery_config_t* config
);

/**
 * Destroy connection recovery context
 * @param ctx Context to destroy
 */
void connection_recovery_destroy(connection_recovery_ctx_t* ctx);

/**
 * Get default configuration
 * @param config Output configuration
 */
void connection_recovery_get_default_config(connection_recovery_config_t* config);

/* ============== Connection Lifecycle ============== */

/**
 * Connect to primary gNB
 * @param ctx Recovery context
 * @param ip Primary gNB IP address
 * @param port Primary gNB port
 * @return UESIM_SUCCESS on success, error code otherwise
 */
int connection_recovery_connect(connection_recovery_ctx_t* ctx,
                                const char* ip, uint16_t port);

/**
 * Disconnect from current gNB
 * @param ctx Recovery context
 */
void connection_recovery_disconnect(connection_recovery_ctx_t* ctx);

/**
 * Reconnect with exponential backoff
 * @param ctx Recovery context
 * @return UESIM_SUCCESS on success, error code otherwise
 */
int connection_recovery_reconnect(connection_recovery_ctx_t* ctx);

/**
 * Check connection status
 * @param ctx Recovery context
 * @return true if connected
 */
bool connection_recovery_is_connected(connection_recovery_ctx_t* ctx);

/**
 * Get current connection state
 * @param ctx Recovery context
 * @return Current state
 */
connection_state_t connection_recovery_get_state(connection_recovery_ctx_t* ctx);

/**
 * Get state as string
 * @param state Connection state
 * @return State string
 */
const char* connection_recovery_state_to_string(connection_state_t state);

/**
 * Get event as string
 * @param event Connection event
 * @return Event string
 */
const char* connection_recovery_event_to_string(connection_event_t event);

/* ============== Data Transfer ============== */

/**
 * Send data over active connection
 * @param ctx Recovery context
 * @param data Data buffer
 * @param len Data length
 * @param ppid Protocol Payload ID
 * @return UESIM_SUCCESS on success, error code otherwise
 */
int connection_recovery_send(connection_recovery_ctx_t* ctx,
                             const void* data, size_t len, uint32_t ppid);

/**
 * Receive data from active connection
 * @param ctx Recovery context
 * @param buf Output buffer
 * @param buf_size Buffer size
 * @param bytes_received Output bytes received
 * @param ppid Output PPID
 * @return UESIM_SUCCESS on success, error code otherwise
 */
int connection_recovery_recv(connection_recovery_ctx_t* ctx,
                             void* buf, size_t buf_size,
                             size_t* bytes_received, uint32_t* ppid);

/**
 * Update last activity time
 * @param ctx Recovery context
 */
void connection_recovery_update_activity(connection_recovery_ctx_t* ctx);

/* ============== Health Monitoring ============== */

/**
 * Start health check monitoring
 * @param ctx Recovery context
 * @return UESIM_SUCCESS on success
 */
int connection_recovery_start_health_check(connection_recovery_ctx_t* ctx);

/**
 * Stop health check monitoring
 * @param ctx Recovery context
 */
void connection_recovery_stop_health_check(connection_recovery_ctx_t* ctx);

/**
 * Perform health check
 * @param ctx Recovery context
 * @return true if healthy
 */
bool connection_recovery_health_check(connection_recovery_ctx_t* ctx);

/* ============== Failover Management ============== */

/**
 * Add failover gNB to list
 * @param ctx Recovery context
 * @param gnb_id gNB identifier
 * @param ip gNB IP address
 * @param port gNB port
 * @param priority Failover priority (higher = better)
 * @return UESIM_SUCCESS on success
 */
int connection_recovery_add_failover(connection_recovery_ctx_t* ctx,
                                     uint32_t gnb_id,
                                     const char* ip, uint16_t port,
                                     int32_t priority);

/**
 * Remove failover gNB from list
 * @param ctx Recovery context
 * @param gnb_id gNB identifier
 * @return UESIM_SUCCESS on success
 */
int connection_recovery_remove_failover(connection_recovery_ctx_t* ctx,
                                        uint32_t gnb_id);

/**
 * Execute failover to backup gNB
 * @param ctx Recovery context
 * @return UESIM_SUCCESS on success
 */
int connection_recovery_failover(connection_recovery_ctx_t* ctx);

/**
 * Get best available failover target
 * @param ctx Recovery context
 * @param out_ip Output IP address
 * @param out_port Output port
 * @return UESIM_SUCCESS if found
 */
int connection_recovery_get_best_failover(connection_recovery_ctx_t* ctx,
                                          char* out_ip, uint16_t* out_port);

/* ============== Callbacks ============== */

/**
 * Set connection event callback
 * @param ctx Recovery context
 * @param callback Event callback function
 * @param user_data User data for callback
 */
void connection_recovery_set_callback(connection_recovery_ctx_t* ctx,
                                      connection_event_callback_t callback,
                                      void* user_data);

/* ============== Statistics ============== */

/**
 * Get connection statistics
 * @param ctx Recovery context
 * @return Pointer to statistics structure
 */
const connection_statistics_t* connection_recovery_get_stats(
    connection_recovery_ctx_t* ctx);

/**
 * Reset connection statistics
 * @param ctx Recovery context
 */
void connection_recovery_reset_stats(connection_recovery_ctx_t* ctx);

/**
 * Print connection statistics
 * @param ctx Recovery context
 * @param output Output file stream
 */
void connection_recovery_print_stats(connection_recovery_ctx_t* ctx,
                                     FILE* output);

/* ============== Utility ============== */

/**
 * Calculate exponential backoff
 * @param base_ms Base backoff in milliseconds
 * @param retry_count Current retry count
 * @param max_ms Maximum backoff
 * @return Backoff duration in milliseconds
 */
uint32_t connection_recovery_calc_backoff(uint32_t base_ms, 
                                          uint32_t retry_count,
                                          uint32_t max_ms);

/**
 * Check if reconnection should be attempted
 * @param ctx Recovery context
 * @return true if should retry
 */
bool connection_recovery_should_retry(connection_recovery_ctx_t* ctx);

#endif /* CONNECTION_RECOVERY_H */
