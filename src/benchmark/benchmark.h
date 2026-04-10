/*
 * 5G UE Simulation Application
 * Performance Benchmarking Header
 */

#ifndef BENCHMARK_H
#define BENCHMARK_H

#include "../uesim.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Benchmark constants
#define BENCHMARK_MAX_NAME_LEN      64
#define BENCHMARK_MAX_ITERATIONS    1000000
#define BENCHMARK_DEFAULT_ITERATIONS 10000
#define BENCHMARK_MAX_THREADS       32

// Benchmark types
typedef enum {
    BENCHMARK_TYPE_LATENCY = 0,
    BENCHMARK_TYPE_THROUGHPUT = 1,
    BENCHMARK_TYPE_MEMORY = 2,
    BENCHMARK_TYPE_CPU = 3,
    BENCHMARK_TYPE_CONCURRENCY = 4,
    BENCHMARK_TYPE_PROTOCOL = 5,
    BENCHMARK_TYPE_SECURITY = 6,
    BENCHMARK_TYPE_MAX
} benchmark_type_t;

// Benchmark categories
typedef enum {
    BENCHMARK_CATEGORY_PDCP = 0,
    BENCHMARK_CATEGORY_RLC = 1,
    BENCHMARK_CATEGORY_MAC = 2,
    BENCHMARK_CATEGORY_NAS = 3,
    BENCHMARK_CATEGORY_RRC = 4,
    BENCHMARK_CATEGORY_SOCKET = 5,
    BENCHMARK_CATEGORY_MEMORY = 6,
    BENCHMARK_CATEGORY_THREAD = 7,
    BENCHMARK_CATEGORY_CRYPTO = 8,
    BENCHMARK_CATEGORY_MAX
} benchmark_category_t;

// Benchmark metrics
typedef struct {
    uint64_t min_time;              // Minimum execution time (ns)
    uint64_t max_time;              // Maximum execution time (ns)
    uint64_t avg_time;              // Average execution time (ns)
    uint64_t total_time;            // Total execution time (ns)
    uint64_t iterations;            // Number of iterations
    double throughput;              // Operations per second
    uint64_t memory_allocated;      // Memory allocated (bytes)
    uint64_t memory_peak;           // Peak memory usage (bytes)
    double cpu_utilization;         // CPU utilization (%)
    uint64_t errors;                // Number of errors
} benchmark_metrics_t;

// Benchmark configuration
typedef struct {
    benchmark_type_t type;          // Benchmark type
    benchmark_category_t category;  // Benchmark category
    char name[BENCHMARK_MAX_NAME_LEN]; // Benchmark name
    uint64_t iterations;            // Number of iterations
    uint32_t thread_count;          // Number of threads
    bool warmup;                    // Enable warmup runs
    uint32_t warmup_iterations;     // Warmup iterations
    bool measure_memory;            // Measure memory usage
    bool measure_cpu;               // Measure CPU usage
    uint32_t duration_seconds;      // Duration for time-based benchmarks
    bool time_based;                // Time-based vs iteration-based
    uint32_t packet_size;           // Packet size for network benchmarks
    uint32_t buffer_size;           // Buffer size for memory benchmarks
} benchmark_config_t;

// Benchmark result
typedef struct {
    benchmark_config_t config;      // Benchmark configuration
    benchmark_metrics_t metrics;    // Benchmark metrics
    time_t start_time;              // Start time
    time_t end_time;                // End time
    char description[256];          // Benchmark description
    bool success;                   // Benchmark success
    char error_message[256];        // Error message if failed
} benchmark_result_t;

// Benchmark suite
typedef struct {
    char name[BENCHMARK_MAX_NAME_LEN];  // Suite name
    benchmark_result_t* results;        // Array of results
    size_t result_count;                // Number of results
    size_t max_results;                 // Maximum results
    time_t start_time;                  // Suite start time
    time_t end_time;                    // Suite end time
    bool success;                       // Suite success
} benchmark_suite_t;

// Function pointer for benchmark functions
typedef uesim_error_t (*benchmark_func_t)(benchmark_config_t* config, benchmark_metrics_t* metrics);

// Function prototypes
uesim_error_t benchmark_init(void);
void benchmark_cleanup(void);

// Benchmark suite management
uesim_error_t benchmark_suite_create(const char* name, benchmark_suite_t** suite);
uesim_error_t benchmark_suite_destroy(benchmark_suite_t* suite);
uesim_error_t benchmark_suite_add_result(benchmark_suite_t* suite, const benchmark_result_t* result);
uesim_error_t benchmark_suite_run(benchmark_suite_t* suite);
void benchmark_suite_print_results(const benchmark_suite_t* suite);

// Benchmark execution
uesim_error_t benchmark_run(const benchmark_config_t* config, benchmark_result_t* result);
uesim_error_t benchmark_run_function(benchmark_func_t func, const benchmark_config_t* config, 
                                    benchmark_result_t* result);

// Specific benchmark functions
uesim_error_t benchmark_pdcp_ciphering(benchmark_config_t* config, benchmark_metrics_t* metrics);
uesim_error_t benchmark_pdcp_integrity(benchmark_config_t* config, benchmark_metrics_t* metrics);
uesim_error_t benchmark_rlc_segmentation(benchmark_config_t* config, benchmark_metrics_t* metrics);
uesim_error_t benchmark_rlc_reassembly(benchmark_config_t* config, benchmark_metrics_t* metrics);
uesim_error_t benchmark_mac_harq(benchmark_config_t* config, benchmark_metrics_t* metrics);
uesim_error_t benchmark_nas_encoding(benchmark_config_t* config, benchmark_metrics_t* metrics);
uesim_error_t benchmark_nas_decoding(benchmark_config_t* config, benchmark_metrics_t* metrics);
uesim_error_t benchmark_rrc_procedures(benchmark_config_t* config, benchmark_metrics_t* metrics);
uesim_error_t benchmark_socket_operations(benchmark_config_t* config, benchmark_metrics_t* metrics);
uesim_error_t benchmark_memory_allocation(benchmark_config_t* config, benchmark_metrics_t* metrics);
uesim_error_t benchmark_thread_operations(benchmark_config_t* config, benchmark_metrics_t* metrics);
uesim_error_t benchmark_crypto_operations(benchmark_config_t* config, benchmark_metrics_t* metrics);

// Utility functions
uint64_t benchmark_get_time_ns(void);
uint64_t benchmark_get_memory_usage(void);
double benchmark_get_cpu_usage(void);
char* benchmark_format_time(uint64_t ns, char* buffer, size_t buffer_size);
char* benchmark_format_throughput(double ops_per_sec, char* buffer, size_t buffer_size);
char* benchmark_format_memory(uint64_t bytes, char* buffer, size_t buffer_size);

// Benchmark result management
uesim_error_t benchmark_result_init(benchmark_result_t* result);
uesim_error_t benchmark_result_copy(benchmark_result_t* dest, const benchmark_result_t* src);
uesim_error_t benchmark_result_merge(benchmark_result_t* dest, const benchmark_result_t* src);
void benchmark_result_print(const benchmark_result_t* result);
void benchmark_result_print_summary(const benchmark_result_t* result);

// Benchmark configuration helpers
uesim_error_t benchmark_config_init(benchmark_config_t* config);
uesim_error_t benchmark_config_set_defaults(benchmark_config_t* config);
uesim_error_t benchmark_config_validate(const benchmark_config_t* config);
void benchmark_config_print(const benchmark_config_t* config);

// Predefined benchmark configurations
benchmark_config_t benchmark_get_default_config(benchmark_type_t type, benchmark_category_t category);
benchmark_config_t benchmark_get_stress_config(benchmark_type_t type, benchmark_category_t category);
benchmark_config_t benchmark_get_performance_config(benchmark_type_t type, benchmark_category_t category);

// Benchmark categories
const char* benchmark_get_type_name(benchmark_type_t type);
const char* benchmark_get_category_name(benchmark_category_t category);
benchmark_category_t benchmark_get_category_from_string(const char* category_str);
benchmark_type_t benchmark_get_type_from_string(const char* type_str);

// Benchmark reporting
uesim_error_t benchmark_generate_report(const benchmark_suite_t* suite, const char* filename);
uesim_error_t benchmark_export_csv(const benchmark_suite_t* suite, const char* filename);
uesim_error_t benchmark_export_json(const benchmark_suite_t* suite, const char* filename);

#endif // BENCHMARK_H