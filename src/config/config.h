/*
 * 5G UE Simulation Application
 * Enhanced Configuration Management Header
 */

#ifndef CONFIG_H
#define CONFIG_H

#include "../uesim.h"
#include <stdint.h>
#include <stdbool.h>

// Configuration constants
#define CONFIG_MAX_STRING_LEN       256
#define CONFIG_MAX_UE_INSTANCES     1024
#define CONFIG_DEFAULT_BUFFER_SIZE  65536
#define CONFIG_MAX_LOG_LEVEL        4

// Configuration sections
typedef enum {
    CONFIG_SECTION_GENERAL = 0,
    CONFIG_SECTION_NETWORK = 1,
    CONFIG_SECTION_UE = 2,
    CONFIG_SECTION_RRC = 3,
    CONFIG_SECTION_PERFORMANCE = 4,
    CONFIG_SECTION_SECURITY = 5,
    CONFIG_SECTION_TEST = 6,
    CONFIG_SECTION_PDCP = 7,
    CONFIG_SECTION_RLC = 8,
    CONFIG_SECTION_MAC = 9,
    CONFIG_SECTION_NAS = 10,
    CONFIG_SECTION_MAX
} config_section_t;

// General configuration
typedef struct {
    int num_instances;              // Number of UE instances
    int log_level;                  // Log level (0-4)
    bool verbose;                   // Verbose logging
    bool debug;                     // Debug mode
    char log_file[CONFIG_MAX_STRING_LEN]; // Log file path
    bool enable_syslog;             // Enable syslog output
    int max_log_file_size;          // Maximum log file size (MB)
    int log_file_count;             // Number of log files to keep
} config_general_t;

// Network configuration
typedef struct {
    char gnb_ip[CONFIG_MAX_STRING_LEN];     // gNB IP address
    int gnb_ngap_port;                      // gNB NGAP port
    int gnb_gtpu_port;                      // gNB GTP-U port
    char local_ip[CONFIG_MAX_STRING_LEN];   // Local bind address
    int local_ngap_port;                    // Local NGAP port
    int local_gtpu_port;                    // Local GTP-U port
    int connection_timeout;                 // Connection timeout (seconds)
    int keepalive_interval;                 // Keepalive interval (seconds)
    bool enable_ipv6;                       // Enable IPv6 support
    int max_connections;                    // Maximum connections
} config_network_t;

// UE configuration
typedef struct {
    char imsi_prefix[CONFIG_MAX_STRING_LEN];    // IMSI prefix
    uint32_t imsi_start;                        // IMSI start number
    char msisdn_prefix[CONFIG_MAX_STRING_LEN];  // MSISDN prefix
    uint32_t msisdn_start;                      // MSISDN start number
    uint32_t tac;                               // Tracking Area Code
    char mcc[8];                                // Mobile Country Code
    char mnc[8];                                // Mobile Network Code
    char imei[CONFIG_MAX_STRING_LEN];           // IMEI (if fixed)
    bool random_imei;                           // Generate random IMEI
    char apn[CONFIG_MAX_STRING_LEN];            // Access Point Name
    char apn_type[CONFIG_MAX_STRING_LEN];       // APN type
} config_ue_t;

// RRC configuration
typedef struct {
    int registration_timeout;           // Registration timeout (seconds)
    int establishment_timeout;          // Establishment timeout (seconds)
    int reestablishment_timeout;        // Re-establishment timeout (seconds)
    int handover_timeout;               // Handover timeout (seconds)
    bool enable_registration;           // Enable registration procedure
    bool enable_establishment;          // Enable establishment procedure
    bool enable_reestablishment;        // Enable re-establishment procedure
    bool enable_handover;               // Enable handover procedure
    int max_retransmissions;            // Maximum retransmissions
    int t300_value;                     // T300 timer value (ms)
    int t301_value;                     // T301 timer value (ms)
    int t302_value;                     // T302 timer value (ms)
    int t304_value;                     // T304 timer value (ms)
    int t310_value;                     // T310 timer value (ms)
    int t311_value;                     // T311 timer value (ms)
    int n310_value;                     // N310 counter value
    int n311_value;                     // N311 counter value
} config_rrc_t;

// PDCP configuration
typedef struct {
    int max_pdu_size;                   // Maximum PDU size
    int discard_timer;                  // Discard timer (ms)
    int status_report_timer;            // Status report timer (ms)
    int ciphering_algorithm;            // Default ciphering algorithm
    int integrity_algorithm;            // Default integrity algorithm
    bool enable_ciphering;              // Enable ciphering
    bool enable_integrity;              // Enable integrity protection
    int max_retransmissions;            // Maximum retransmissions
    int polling_pdu;                    // Polling PDU count
    int polling_byte;                   // Polling byte count
} config_pdcp_t;

// RLC configuration
typedef struct {
    int am_window_size;                 // AM window size
    int um_window_size;                 // UM window size
    int poll_retransmit_timer;          // Poll retransmit timer (ms)
    int reassembly_timer;               // Reassembly timer (ms)
    int status_prohibit_timer;          // Status prohibit timer (ms)
    int max_retransmissions;            // Maximum retransmissions
    bool enable_arq;                    // Enable ARQ
    int buffer_size;                    // RLC buffer size
    int max_pdu_size;                   // Maximum PDU size
    bool enable_segmentation;           // Enable segmentation
} config_rlc_t;

// MAC configuration
typedef struct {
    int harq_processes;                 // Number of HARQ processes
    int max_harq_retransmissions;       // Maximum HARQ retransmissions
    int tti_length;                     // TTI length (ms)
    int rach_preambles;                 // Number of RACH preambles
    int rach_response_window;           // RACH response window (slots)
    int max_rach_transmissions;         // Maximum RACH transmissions
    int scheduling_requests;            // Number of scheduling requests
    int ul_grants;                      // Number of UL grants
    int dl_grants;                      // Number of DL grants
    bool enable_harq;                   // Enable HARQ
    bool enable_rach;                   // Enable RACH
    int tb_size;                        // Default TB size
    int mcs;                            // Default MCS
} config_mac_t;

// NAS configuration
typedef struct {
    int registration_timer;             // Registration timer (seconds)
    int authentication_timer;           // Authentication timer (seconds)
    int security_timer;                 // Security mode timer (seconds)
    int pdu_session_timer;              // PDU session timer (seconds)
    bool enable_periodic_registration;  // Enable periodic registration
    int periodic_registration_time;     // Periodic registration time (seconds)
    int max_registration_attempts;      // Maximum registration attempts
    int max_authentication_attempts;    // Maximum authentication attempts
    int ciphering_algorithm;            // Default ciphering algorithm
    int integrity_algorithm;            // Default integrity algorithm
    bool enable_5g_features;            // Enable 5G features
    int max_pdu_sessions;               // Maximum PDU sessions
    bool enable_sms;                    // Enable SMS support
    bool enable_voice;                  // Enable voice support
} config_nas_t;

// Performance configuration
typedef struct {
    int thread_pool_size;               // Thread pool size (0 = auto)
    int rx_buffer_size;                 // RX buffer size
    int tx_buffer_size;                 // TX buffer size
    int memory_pool_size;               // Memory pool size (bytes)
    bool use_memory_pool;               // Use memory pool
    int max_queue_size;                 // Maximum queue size
    int worker_threads;                 // Number of worker threads
    int io_threads;                     // Number of I/O threads
    bool enable_thread_affinity;        // Enable thread affinity
    int cpu_affinity_mask;              // CPU affinity mask
    bool enable_async_io;               // Enable async I/O
    int io_priority;                    // I/O priority
} config_performance_t;

// Security configuration
typedef struct {
    bool stack_protection;              // Enable stack protection
    bool aslr;                          // Enable ASLR
    bool secure_flags;                  // Enable secure compilation flags
    bool enable_encryption;             // Enable encryption
    bool enable_integrity_protection;   // Enable integrity protection
    char key_file[CONFIG_MAX_STRING_LEN]; // Key file path
    bool enable_certificate_validation; // Enable certificate validation
    char cert_file[CONFIG_MAX_STRING_LEN]; // Certificate file path
    char ca_file[CONFIG_MAX_STRING_LEN];   // CA file path
    bool enable_tls;                    // Enable TLS
    int tls_version;                    // TLS version
    bool enable_pki;                    // Enable PKI
} config_security_t;

// Test configuration
typedef struct {
    bool enable_tests;                  // Enable unit tests
    int test_duration;                  // Test duration (seconds)
    char report_file[CONFIG_MAX_STRING_LEN]; // Test report file
    bool enable_performance_tests;      // Enable performance tests
    bool enable_stress_tests;           // Enable stress tests
    int stress_test_duration;           // Stress test duration (seconds)
    int stress_test_concurrency;        // Stress test concurrency
    bool enable_compliance_tests;       // Enable compliance tests
    char test_suite[CONFIG_MAX_STRING_LEN]; // Test suite name
    bool generate_test_report;          // Generate test report
    bool enable_coverage;               // Enable code coverage
} config_test_t;

// Main configuration structure
typedef struct {
    config_general_t general;           // General configuration
    config_network_t network;           // Network configuration
    config_ue_t ue;                     // UE configuration
    config_rrc_t rrc;                   // RRC configuration
    config_pdcp_t pdcp;                 // PDCP configuration
    config_rlc_t rlc;                   // RLC configuration
    config_mac_t mac;                   // MAC configuration
    config_nas_t nas;                   // NAS configuration
    config_performance_t performance;   // Performance configuration
    config_security_t security;         // Security configuration
    config_test_t test;                 // Test configuration
    bool loaded;                        // Configuration loaded flag
    char config_file[CONFIG_MAX_STRING_LEN]; // Configuration file path
    time_t load_time;                   // Configuration load time
} uesim_config_t;

// Function prototypes
uesim_error_t config_init(uesim_config_t* config);
uesim_error_t config_load(uesim_config_t* config, const char* config_file);
uesim_error_t config_load_default(uesim_config_t* config);
uesim_error_t config_save(uesim_config_t* config, const char* config_file);
uesim_error_t config_validate(uesim_config_t* config);
uesim_error_t config_cleanup(uesim_config_t* config);

// Configuration access functions
uesim_error_t config_get_string(uesim_config_t* config, config_section_t section,
                               const char* key, char* value, size_t value_size);
uesim_error_t config_get_int(uesim_config_t* config, config_section_t section,
                            const char* key, int* value);
uesim_error_t config_get_bool(uesim_config_t* config, config_section_t section,
                             const char* key, bool* value);
uesim_error_t config_set_string(uesim_config_t* config, config_section_t section,
                               const char* key, const char* value);
uesim_error_t config_set_int(uesim_config_t* config, config_section_t section,
                            const char* key, int value);
uesim_error_t config_set_bool(uesim_config_t* config, config_section_t section,
                             const char* key, bool value);

// Configuration section access
config_general_t* config_get_general(uesim_config_t* config);
config_network_t* config_get_network(uesim_config_t* config);
config_ue_t* config_get_ue(uesim_config_t* config);
config_rrc_t* config_get_rrc(uesim_config_t* config);
config_pdcp_t* config_get_pdcp(uesim_config_t* config);
config_rlc_t* config_get_rlc(uesim_config_t* config);
config_mac_t* config_get_mac(uesim_config_t* config);
config_nas_t* config_get_nas(uesim_config_t* config);
config_performance_t* config_get_performance(uesim_config_t* config);
config_security_t* config_get_security(uesim_config_t* config);
config_test_t* config_get_test(uesim_config_t* config);

// Configuration validation
uesim_error_t config_validate_general(config_general_t* general);
uesim_error_t config_validate_network(config_network_t* network);
uesim_error_t config_validate_ue(config_ue_t* ue);
uesim_error_t config_validate_rrc(config_rrc_t* rrc);
uesim_error_t config_validate_pdcp(config_pdcp_t* pdcp);
uesim_error_t config_validate_rlc(config_rlc_t* rlc);
uesim_error_t config_validate_mac(config_mac_t* mac);
uesim_error_t config_validate_nas(config_nas_t* nas);
uesim_error_t config_validate_performance(config_performance_t* performance);
uesim_error_t config_validate_security(config_security_t* security);
uesim_error_t config_validate_test(config_test_t* test);

// Configuration utilities
uesim_error_t config_parse_file(uesim_config_t* config, const char* filename);
uesim_error_t config_write_file(uesim_config_t* config, const char* filename);
uesim_error_t config_merge(uesim_config_t* dest, const uesim_config_t* src);
uesim_error_t config_copy(uesim_config_t* dest, const uesim_config_t* src);
uesim_error_t config_reset(uesim_config_t* config);
bool config_is_loaded(uesim_config_t* config);
time_t config_get_load_time(uesim_config_t* config);

#endif // CONFIG_H