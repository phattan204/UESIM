/*
 * 5G UE Simulation Application
 * Load Testing Framework Header
 */

#ifndef LOAD_TEST_H
#define LOAD_TEST_H

#include "../uesim.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* Load test constants */
#define LOAD_TEST_MAX_UE_INSTANCES     10000
#define LOAD_TEST_MAX_SCENARIO_NAME    64
#define LOAD_TEST_MAX_DESCRIPTION      256
#define LOAD_TEST_MAX_LABELS           16
#define LOAD_TEST_HISTOGRAM_BUCKETS    20

/* Load test scenario types */
typedef enum {
    LOAD_TEST_BURST_REGISTRATION = 0,       /* N UEs register simultaneously */
    LOAD_TEST_RAMP_REGISTRATION,            /* Ramp from 1 to N UEs over time */
    LOAD_TEST_SESSION_FLOOD,                /* Rapid PDU session establishment */
    LOAD_TEST_HANDOVER_STRESS,              /* Repeated handover sequences */
    LOAD_TEST_MIXED_WORKLOAD,               /* Realistic traffic mix */
    LOAD_TEST_MAX_SCENARIOS
} load_test_scenario_type_t;

/* Load test state */
typedef enum {
    LOAD_TEST_STATE_IDLE = 0,
    LOAD_TEST_STATE_RUNNING,
    LOAD_TEST_STATE_PAUSED,
    LOAD_TEST_STATE_COMPLETED,
    LOAD_TEST_STATE_ABORTED,
    LOAD_TEST_STATE_MAX
} load_test_state_t;

/* Latency histogram bucket */
typedef struct {
    uint64_t upper_bound_us;    /* Upper bound in microseconds */
    uint64_t count;             /* Number of samples in this bucket */
} latency_bucket_t;

/* Latency statistics */
typedef struct {
    uint64_t min_us;                    /* Minimum latency (microseconds) */
    uint64_t max_us;                    /* Maximum latency (microseconds) */
    uint64_t mean_us;                   /* Mean latency */
    uint64_t variance_us;               /* Variance */
    uint64_t p50_us;                    /* 50th percentile */
    uint64_t p90_us;                    /* 90th percentile */
    uint64_t p95_us;                    /* 95th percentile */
    uint64_t p99_us;                    /* 99th percentile */
    uint64_t total_samples;             /* Total number of samples */
    latency_bucket_t histogram[LOAD_TEST_HISTOGRAM_BUCKETS];  /* Histogram */
} latency_stats_t;

/* Throughput statistics */
typedef struct {
    double procedures_per_second;       /* Procedures completed per second */
    double bytes_per_second;            /* Bytes transferred per second */
    uint64_t total_procedures;          /* Total procedures completed */
    uint64_t total_bytes;               /* Total bytes transferred */
    uint64_t successful_procedures;     /* Successful procedures */
    uint64_t failed_procedures;         /* Failed procedures */
} throughput_stats_t;

/* Memory statistics */
typedef struct {
    uint64_t total_allocated;           /* Total memory allocated */
    uint64_t per_ue_average;            /* Average memory per UE */
    uint64_t peak_usage;                /* Peak memory usage */
    uint64_t current_usage;             /* Current memory usage */
} memory_stats_t;

/* Failure statistics */
typedef struct {
    uint64_t timeout_failures;          /* Timeout failures */
    uint64_t reject_failures;           /* Reject failures */
    uint64_t error_failures;            /* Error failures */
    uint64_t connection_failures;       /* Connection failures */
    uint64_t protocol_failures;         /* Protocol failures */
    double failure_rate;                /* Failure rate (percentage) */
} failure_stats_t;

/* Load test metrics */
typedef struct {
    latency_stats_t latency;            /* Latency statistics */
    throughput_stats_t throughput;      /* Throughput statistics */
    memory_stats_t memory;              /* Memory statistics */
    failure_stats_t failures;           /* Failure statistics */
    time_t start_time;                  /* Test start time */
    time_t end_time;                    /* Test end time */
    uint64_t duration_seconds;          /* Test duration */
    uint32_t num_ues;                   /* Number of UEs in test */
    uint32_t active_ues;                /* Currently active UEs */
} load_test_metrics_t;

/* Load test configuration */
typedef struct {
    load_test_scenario_type_t scenario; /* Scenario type */
    char name[LOAD_TEST_MAX_SCENARIO_NAME];         /* Scenario name */
    char description[LOAD_TEST_MAX_DESCRIPTION];    /* Description */
    uint32_t num_ues;                   /* Number of UE instances */
    uint32_t ramp_rate;                 /* UEs per second for ramp */
    uint64_t duration_seconds;          /* Test duration */
    uint64_t warmup_seconds;            /* Warmup period */
    uint64_t cooldown_seconds;          /* Cooldown period */
    bool collect_histograms;            /* Collect latency histograms */
    bool collect_memory;                /* Collect memory stats */
    uint32_t sample_interval_ms;        /* Metrics sample interval */
    char output_format[16];             /* Output format: text, csv, json */
    char output_path[256];              /* Output file path */
    bool verbose;                       /* Verbose output */
} load_test_config_t;

/* Load test executor */
typedef struct {
    load_test_config_t config;          /* Configuration */
    load_test_state_t state;            /* Current state */
    load_test_metrics_t metrics;        /* Current metrics */
    ue_context_t** ue_instances;        /* UE instance array */
    uint32_t ue_capacity;               /* Array capacity */
    pthread_t executor_thread;          /* Executor thread */
    pthread_mutex_t metrics_mutex;      /* Metrics protection */
    pthread_mutex_t state_mutex;        /* State protection */
    pthread_cond_t state_cond;          /* State signaling */
#ifdef _WIN32
    volatile LONG abort_flag;           /* Abort flag */
#else
    atomic_bool abort_flag;             /* Abort flag */
#endif
} load_test_executor_t;

/* Scenario definition */
typedef struct {
    load_test_scenario_type_t type;
    const char* name;
    const char* description;
    uint32_t default_num_ues;
    uint32_t default_ramp_rate;
    uint64_t default_duration;
} load_test_scenario_def_t;

/* Report output formats */
typedef enum {
    LOAD_TEST_REPORT_TEXT = 0,
    LOAD_TEST_REPORT_CSV,
    LOAD_TEST_REPORT_JSON,
    LOAD_TEST_REPORT_MAX
} load_test_report_format_t;

/* Function prototypes */

/* Initialization and cleanup */
uesim_error_t load_test_init(void);
void load_test_cleanup(void);

/* Executor management */
uesim_error_t load_test_create_executor(load_test_executor_t** executor, 
                                        const load_test_config_t* config);
uesim_error_t load_test_destroy_executor(load_test_executor_t* executor);
uesim_error_t load_test_start(load_test_executor_t* executor);
uesim_error_t load_test_stop(load_test_executor_t* executor);
uesim_error_t load_test_pause(load_test_executor_t* executor);
uesim_error_t load_test_resume(load_test_executor_t* executor);
uesim_error_t load_test_abort(load_test_executor_t* executor);

/* Configuration */
uesim_error_t load_test_set_default_config(load_test_config_t* config);
uesim_error_t load_test_set_scenario_config(load_test_config_t* config,
                                           load_test_scenario_type_t scenario);
uesim_error_t load_test_validate_config(const load_test_config_t* config);

/* Scenario execution */
uesim_error_t load_test_run_scenario(load_test_executor_t* executor,
                                     load_test_scenario_type_t scenario,
                                     uint32_t num_ues,
                                     uint64_t duration_seconds);
uesim_error_t load_test_run_burst_registration(load_test_executor_t* executor);
uesim_error_t load_test_run_ramp_registration(load_test_executor_t* executor);
uesim_error_t load_test_run_session_flood(load_test_executor_t* executor);
uesim_error_t load_test_run_handover_stress(load_test_executor_t* executor);
uesim_error_t load_test_run_mixed_workload(load_test_executor_t* executor);

/* Metrics collection */
uesim_error_t load_test_collect_metrics(load_test_executor_t* executor);
uesim_error_t load_test_update_metrics(load_test_executor_t* executor,
                                       uint32_t ue_idx,
                                       uint64_t latency_us,
                                       bool success);
uesim_error_t load_test_get_metrics(load_test_executor_t* executor,
                                   load_test_metrics_t* metrics);
uesim_error_t load_test_reset_metrics(load_test_executor_t* executor);

/* Latency statistics */
uesim_error_t load_test_record_latency(load_test_executor_t* executor,
                                      uint64_t latency_us);
uesim_error_t load_test_compute_percentiles(latency_stats_t* stats);
uint64_t load_test_get_current_latency_us(void);

/* Reporting */
uesim_error_t load_test_generate_report(load_test_executor_t* executor,
                                       load_test_report_format_t format,
                                       const char* output_path);
uesim_error_t load_test_print_report(load_test_executor_t* executor);
uesim_error_t load_test_write_csv_report(load_test_executor_t* executor,
                                        const char* path);
uesim_error_t load_test_write_json_report(load_test_executor_t* executor,
                                         const char* path);

/* Scenario definitions */
const load_test_scenario_def_t* load_test_get_scenario_def(load_test_scenario_type_t type);
const char* load_test_scenario_type_str(load_test_scenario_type_t type);
const char* load_test_state_str(load_test_state_t state);

/* Utility functions */
uesim_error_t load_test_create_ue_batch(load_test_executor_t* executor,
                                       uint32_t start_idx,
                                       uint32_t count);
uesim_error_t load_test_destroy_ue_batch(load_test_executor_t* executor,
                                        uint32_t start_idx,
                                        uint32_t count);
uesim_error_t load_test_execute_ue_procedure(load_test_executor_t* executor,
                                            uint32_t ue_idx,
                                            rrc_procedure_t procedure,
                                            uint64_t* latency_us);

#endif /* LOAD_TEST_H */