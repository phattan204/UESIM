/*
 * 5G UE Simulation Application
 * Load Testing Framework Implementation
 */

#include "load_test.h"
#include "benchmark.h"
#include "../core/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef _WIN32
#include <sys/time.h>
#endif

/* Scenario definitions table */
static const load_test_scenario_def_t g_scenario_defs[] = {
    { LOAD_TEST_BURST_REGISTRATION, "burst_registration", "N UEs register simultaneously", 100, 0, 60 },
    { LOAD_TEST_RAMP_REGISTRATION, "ramp_registration", "Ramp from 1 to N UEs over time", 1000, 100, 120 },
    { LOAD_TEST_SESSION_FLOOD, "session_flood", "Rapid PDU session establishment", 100, 50, 60 },
    { LOAD_TEST_HANDOVER_STRESS, "handover_stress", "Repeated handover sequences", 50, 10, 120 },
    { LOAD_TEST_MIXED_WORKLOAD, "mixed_workload", "Realistic traffic mix", 500, 50, 300 }
};

/* Histogram bucket boundaries (microseconds) */
static const uint64_t g_histogram_bounds[LOAD_TEST_HISTOGRAM_BUCKETS] = {
    100, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000,
    500000, 1000000, 2000000, 5000000, 10000000, 20000000, 30000000, 60000000, 120000000, UINT64_MAX
};

static const char* g_state_strings[] = { "Idle", "Running", "Paused", "Completed", "Aborted", "Unknown" };
static bool g_load_test_initialized = false;

const char* load_test_scenario_type_str(load_test_scenario_type_t type) {
    return (type < LOAD_TEST_MAX_SCENARIOS) ? g_scenario_defs[type].name : "Unknown";
}

const char* load_test_state_str(load_test_state_t state) {
    return (state < LOAD_TEST_STATE_MAX) ? g_state_strings[state] : "Unknown";
}

const load_test_scenario_def_t* load_test_get_scenario_def(load_test_scenario_type_t type) {
    return (type < LOAD_TEST_MAX_SCENARIOS) ? &g_scenario_defs[type] : NULL;
}

uesim_error_t load_test_init(void) {
    if (!g_load_test_initialized) { g_load_test_initialized = true; printf("Load testing framework initialized\n"); }
    return UESIM_SUCCESS;
}

void load_test_cleanup(void) { g_load_test_initialized = false; printf("Load testing framework cleanup completed\n"); }

uesim_error_t load_test_set_default_config(load_test_config_t* config) {
    if (!config) return UESIM_ERROR_INVALID_PARAM;
    memset(config, 0, sizeof(load_test_config_t));
    config->scenario = LOAD_TEST_BURST_REGISTRATION; config->num_ues = 100; config->ramp_rate = 10;
    config->duration_seconds = 60; config->warmup_seconds = 5; config->cooldown_seconds = 5;
    config->collect_histograms = true; config->collect_memory = true; config->sample_interval_ms = 1000;
    strncpy(config->output_format, "text", 15); strncpy(config->output_path, "./load_test_report", 255);
    config->verbose = true; strncpy(config->name, "default_scenario", 63); strncpy(config->description, "Default load test scenario", 255);
    return UESIM_SUCCESS;
}

uesim_error_t load_test_set_scenario_config(load_test_config_t* config, load_test_scenario_type_t scenario) {
    if (!config || scenario >= LOAD_TEST_MAX_SCENARIOS) return UESIM_ERROR_INVALID_PARAM;
    const load_test_scenario_def_t* def = &g_scenario_defs[scenario];
    config->scenario = scenario; config->num_ues = def->default_num_ues;
    config->ramp_rate = def->default_ramp_rate; config->duration_seconds = def->default_duration;
    strncpy(config->name, def->name, 63); strncpy(config->description, def->description, 255);
    return UESIM_SUCCESS;
}

uesim_error_t load_test_validate_config(const load_test_config_t* config) {
    if (!config) return UESIM_ERROR_INVALID_PARAM;
    if (config->num_ues == 0 || config->num_ues > LOAD_TEST_MAX_UE_INSTANCES) return UESIM_ERROR_INVALID_PARAM;
    if (config->duration_seconds == 0) return UESIM_ERROR_INVALID_PARAM;
    if (config->scenario >= LOAD_TEST_MAX_SCENARIOS) return UESIM_ERROR_INVALID_PARAM;
    return UESIM_SUCCESS;
}

uesim_error_t load_test_create_executor(load_test_executor_t** executor, const load_test_config_t* config) {
    if (!executor || !config) return UESIM_ERROR_INVALID_PARAM;
    uesim_error_t result = load_test_validate_config(config);
    if (result != UESIM_SUCCESS) return result;
    
    load_test_executor_t* exec = (load_test_executor_t*)uesim_calloc(1, sizeof(load_test_executor_t));
    if (!exec) return UESIM_ERROR_MEMORY;
    memcpy(&exec->config, config, sizeof(load_test_config_t));
    exec->ue_capacity = config->num_ues;
    exec->ue_instances = (ue_context_t**)uesim_calloc(config->num_ues, sizeof(ue_context_t*));
    if (!exec->ue_instances) { uesim_free(exec); return UESIM_ERROR_MEMORY; }
    
#ifdef _WIN32
    exec->metrics_mutex = CreateMutex(NULL, FALSE, NULL);
    exec->state_mutex = CreateMutex(NULL, FALSE, NULL);
    exec->state_cond = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!exec->metrics_mutex || !exec->state_mutex || !exec->state_cond) {
        if (exec->metrics_mutex) CloseHandle(exec->metrics_mutex);
        if (exec->state_mutex) CloseHandle(exec->state_mutex);
        if (exec->state_cond) CloseHandle(exec->state_cond);
        uesim_free(exec->ue_instances); uesim_free(exec); return UESIM_ERROR_THREAD;
    }
#else
    if (pthread_mutex_init(&exec->metrics_mutex, NULL) || pthread_mutex_init(&exec->state_mutex, NULL) || pthread_cond_init(&exec->state_cond, NULL)) {
        pthread_mutex_destroy(&exec->metrics_mutex); pthread_mutex_destroy(&exec->state_mutex);
        uesim_free(exec->ue_instances); uesim_free(exec); return UESIM_ERROR_THREAD;
    }
#endif
    exec->state = LOAD_TEST_STATE_IDLE; exec->abort_flag = 0;
    load_test_reset_metrics(exec);
    for (int i = 0; i < LOAD_TEST_HISTOGRAM_BUCKETS; i++) {
        exec->metrics.latency.histogram[i].upper_bound_us = g_histogram_bounds[i];
        exec->metrics.latency.histogram[i].count = 0;
    }
    *executor = exec;
    printf("Load test executor created (scenario: %s, UEs: %u)\n", load_test_scenario_type_str(config->scenario), config->num_ues);
    return UESIM_SUCCESS;
}

uesim_error_t load_test_destroy_executor(load_test_executor_t* executor) {
    if (!executor) return UESIM_ERROR_INVALID_PARAM;
    if (executor->state == LOAD_TEST_STATE_RUNNING) load_test_stop(executor);
    for (uint32_t i = 0; i < executor->config.num_ues; i++) {
        if (executor->ue_instances[i]) { uesim_stop_ue(executor->ue_instances[i]); executor->ue_instances[i] = NULL; }
    }
    uesim_free(executor->ue_instances);
#ifdef _WIN32
    CloseHandle(executor->metrics_mutex); CloseHandle(executor->state_mutex); CloseHandle(executor->state_cond);
#else
    pthread_mutex_destroy(&executor->metrics_mutex); pthread_mutex_destroy(&executor->state_mutex); pthread_cond_destroy(&executor->state_cond);
#endif
    uesim_free(executor);
    printf("Load test executor destroyed\n");
    return UESIM_SUCCESS;
}

static uesim_error_t load_test_set_state(load_test_executor_t* executor, load_test_state_t new_state) {
    if (!executor) return UESIM_ERROR_INVALID_PARAM;
#ifdef _WIN32
    WaitForSingleObject(executor->state_mutex, INFINITE);
#else
    pthread_mutex_lock(&executor->state_mutex);
#endif
    executor->state = new_state;
#ifdef _WIN32
    SetEvent(executor->state_cond); ReleaseMutex(executor->state_mutex);
#else
    pthread_cond_broadcast(&executor->state_cond); pthread_mutex_unlock(&executor->state_mutex);
#endif
    return UESIM_SUCCESS;
}

uesim_error_t load_test_start(load_test_executor_t* executor) {
    if (!executor) return UESIM_ERROR_INVALID_PARAM;
    if (executor->state == LOAD_TEST_STATE_RUNNING) return UESIM_SUCCESS;
    load_test_set_state(executor, LOAD_TEST_STATE_RUNNING);
    executor->metrics.start_time = time(NULL); executor->abort_flag = 0;
    printf("Load test started: %s\n", executor->config.name);
    return UESIM_SUCCESS;
}

uesim_error_t load_test_stop(load_test_executor_t* executor) {
    if (!executor) return UESIM_ERROR_INVALID_PARAM;
    if (executor->state != LOAD_TEST_STATE_RUNNING && executor->state != LOAD_TEST_STATE_PAUSED) return UESIM_SUCCESS;
    load_test_set_state(executor, LOAD_TEST_STATE_COMPLETED);
    executor->metrics.end_time = time(NULL);
    executor->metrics.duration_seconds = executor->metrics.end_time - executor->metrics.start_time;
    printf("Load test stopped after %lu seconds\n", executor->metrics.duration_seconds);
    return UESIM_SUCCESS;
}

uesim_error_t load_test_pause(load_test_executor_t* executor) {
    if (!executor || executor->state != LOAD_TEST_STATE_RUNNING) return UESIM_ERROR_INVALID_PARAM;
    load_test_set_state(executor, LOAD_TEST_STATE_PAUSED);
    printf("Load test paused\n");
    return UESIM_SUCCESS;
}

uesim_error_t load_test_resume(load_test_executor_t* executor) {
    if (!executor || executor->state != LOAD_TEST_STATE_PAUSED) return UESIM_ERROR_INVALID_PARAM;
    load_test_set_state(executor, LOAD_TEST_STATE_RUNNING);
    printf("Load test resumed\n");
    return UESIM_SUCCESS;
}

uesim_error_t load_test_abort(load_test_executor_t* executor) {
    if (!executor) return UESIM_ERROR_INVALID_PARAM;
    executor->abort_flag = 1; load_test_set_state(executor, LOAD_TEST_STATE_ABORTED);
    executor->metrics.end_time = time(NULL);
    printf("Load test aborted\n");
    return UESIM_SUCCESS;
}

uint64_t load_test_get_current_latency_us(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, ctr; QueryPerformanceFrequency(&freq); QueryPerformanceCounter(&ctr);
    return (uint64_t)((ctr.QuadPart * 1000000ULL) / freq.QuadPart);
#else
    struct timeval tv; gettimeofday(&tv, NULL); return (uint64_t)tv.tv_sec * 1000000ULL + tv.tv_usec;
#endif
}

uesim_error_t load_test_record_latency(load_test_executor_t* executor, uint64_t latency_us) {
    if (!executor) return UESIM_ERROR_INVALID_PARAM;
#ifdef _WIN32
    WaitForSingleObject(executor->metrics_mutex, INFINITE);
#else
    pthread_mutex_lock(&executor->metrics_mutex);
#endif
    latency_stats_t* stats = &executor->metrics.latency;
    if (stats->total_samples == 0 || latency_us < stats->min_us) stats->min_us = latency_us;
    if (latency_us > stats->max_us) stats->max_us = latency_us;
    stats->total_samples++;
    double delta = (double)latency_us - (double)stats->mean_us;
    stats->mean_us += (uint64_t)(delta / stats->total_samples);
    for (int i = 0; i < LOAD_TEST_HISTOGRAM_BUCKETS; i++) {
        if (latency_us <= stats->histogram[i].upper_bound_us) { stats->histogram[i].count++; break; }
    }
#ifdef _WIN32
    ReleaseMutex(executor->metrics_mutex);
#else
    pthread_mutex_unlock(&executor->metrics_mutex);
#endif
    return UESIM_SUCCESS;
}

uesim_error_t load_test_compute_percentiles(latency_stats_t* stats) {
    if (!stats || stats->total_samples == 0) return UESIM_ERROR_INVALID_PARAM;
    uint64_t cumulative = 0;
    uint64_t p50_target = stats->total_samples / 2, p90_target = (stats->total_samples * 90) / 100;
    uint64_t p95_target = (stats->total_samples * 95) / 100, p99_target = (stats->total_samples * 99) / 100;
    bool p50_found = false, p90_found = false, p95_found = false, p99_found = false;
    for (int i = 0; i < LOAD_TEST_HISTOGRAM_BUCKETS; i++) {
        cumulative += stats->histogram[i].count;
        if (!p50_found && cumulative >= p50_target) { stats->p50_us = stats->histogram[i].upper_bound_us; p50_found = true; }
        if (!p90_found && cumulative >= p90_target) { stats->p90_us = stats->histogram[i].upper_bound_us; p90_found = true; }
        if (!p95_found && cumulative >= p95_target) { stats->p95_us = stats->histogram[i].upper_bound_us; p95_found = true; }
        if (!p99_found && cumulative >= p99_target) { stats->p99_us = stats->histogram[i].upper_bound_us; p99_found = true; }
        if (p50_found && p90_found && p95_found && p99_found) break;
    }
    return UESIM_SUCCESS;
}

uesim_error_t load_test_reset_metrics(load_test_executor_t* executor) {
    if (!executor) return UESIM_ERROR_INVALID_PARAM;
    memset(&executor->metrics, 0, sizeof(load_test_metrics_t));
    executor->metrics.latency.min_us = UINT64_MAX;
    for (int i = 0; i < LOAD_TEST_HISTOGRAM_BUCKETS; i++) {
        executor->metrics.latency.histogram[i].upper_bound_us = g_histogram_bounds[i];
        executor->metrics.latency.histogram[i].count = 0;
    }
    return UESIM_SUCCESS;
}

uesim_error_t load_test_update_metrics(load_test_executor_t* executor, uint32_t ue_idx, uint64_t latency_us, bool success) {
    if (!executor) return UESIM_ERROR_INVALID_PARAM;
#ifdef _WIN32
    WaitForSingleObject(executor->metrics_mutex, INFINITE);
#else
    pthread_mutex_lock(&executor->metrics_mutex);
#endif
    executor->metrics.throughput.total_procedures++;
    if (success) { executor->metrics.throughput.successful_procedures++; load_test_record_latency(executor, latency_us); }
    else { executor->metrics.throughput.failed_procedures++; executor->metrics.failures.error_failures++; }
    if (executor->metrics.throughput.total_procedures > 0)
        executor->metrics.failures.failure_rate = (double)executor->metrics.throughput.failed_procedures / executor->metrics.throughput.total_procedures * 100.0;
#ifdef _WIN32
    ReleaseMutex(executor->metrics_mutex);
#else
    pthread_mutex_unlock(&executor->metrics_mutex);
#endif
    return UESIM_SUCCESS;
}

uesim_error_t load_test_get_metrics(load_test_executor_t* executor, load_test_metrics_t* metrics) {
    if (!executor || !metrics) return UESIM_ERROR_INVALID_PARAM;
#ifdef _WIN32
    WaitForSingleObject(executor->metrics_mutex, INFINITE);
#else
    pthread_mutex_lock(&executor->metrics_mutex);
#endif
    memcpy(metrics, &executor->metrics, sizeof(load_test_metrics_t));
#ifdef _WIN32
    ReleaseMutex(executor->metrics_mutex);
#else
    pthread_mutex_unlock(&executor->metrics_mutex);
#endif
    return UESIM_SUCCESS;
}

uesim_error_t load_test_collect_metrics(load_test_executor_t* executor) {
    if (!executor) return UESIM_ERROR_INVALID_PARAM;
    if (executor->metrics.duration_seconds > 0)
        executor->metrics.throughput.procedures_per_second = (double)executor->metrics.throughput.total_procedures / executor->metrics.duration_seconds;
    load_test_compute_percentiles(&executor->metrics.latency);
    return UESIM_SUCCESS;
}

uesim_error_t load_test_create_ue_batch(load_test_executor_t* executor, uint32_t start_idx, uint32_t count) {
    if (!executor || start_idx + count > executor->config.num_ues) return UESIM_ERROR_INVALID_PARAM;
    for (uint32_t i = start_idx; i < start_idx + count; i++) {
        if (executor->ue_instances[i]) continue;
        uesim_error_t result = uesim_create_ue_instance(&executor->ue_instances[i]);
        if (result != UESIM_SUCCESS) { fprintf(stderr, "Failed to create UE %u: %d\n", i, result); return result; }
        executor->ue_instances[i]->ue_id = i; uesim_start_ue(executor->ue_instances[i]);
    }
    executor->metrics.active_ues += count;
    if (executor->config.verbose) printf("Created batch of %u UEs (%u-%u)\n", count, start_idx, start_idx + count - 1);
    return UESIM_SUCCESS;
}

uesim_error_t load_test_execute_ue_procedure(load_test_executor_t* executor, uint32_t ue_idx, rrc_procedure_t procedure, uint64_t* latency_us) {
    if (!executor || ue_idx >= executor->config.num_ues || !latency_us) return UESIM_ERROR_INVALID_PARAM;
    if (!executor->ue_instances[ue_idx]) return UESIM_ERROR_NOT_INITIALIZED;
    uint64_t start = load_test_get_current_latency_us();
    uesim_error_t result = uesim_execute_procedure(executor->ue_instances[ue_idx], procedure);
    *latency_us = load_test_get_current_latency_us() - start;
    return result;
}

uesim_error_t load_test_run_burst_registration(load_test_executor_t* executor) {
    if (!executor) return UESIM_ERROR_INVALID_PARAM;
    printf("Running burst registration (%u UEs)\n", executor->config.num_ues);
    load_test_start(executor);
    uesim_error_t result = load_test_create_ue_batch(executor, 0, executor->config.num_ues);
    if (result != UESIM_SUCCESS) { load_test_abort(executor); return result; }
    for (uint32_t i = 0; i < executor->config.num_ues && !executor->abort_flag; i++) {
        uint64_t lat = 0; result = load_test_execute_ue_procedure(executor, i, RRC_PROC_REGISTRATION, &lat);
        load_test_update_metrics(executor, i, lat, result == UESIM_SUCCESS);
    }
    load_test_stop(executor); load_test_collect_metrics(executor);
    return UESIM_SUCCESS;
}

uesim_error_t load_test_run_ramp_registration(load_test_executor_t* executor) {
    if (!executor) return UESIM_ERROR_INVALID_PARAM;
    printf("Running ramp registration (%u UEs, %u/sec)\n", executor->config.num_ues, executor->config.ramp_rate);
    load_test_start(executor);
    uint32_t created = 0; uint32_t interval_us = 1000000 / executor->config.ramp_rate;
    while (created < executor->config.num_ues && !executor->abort_flag) {
        uesim_error_t result = load_test_create_ue_batch(executor, created, 1);
        if (result == UESIM_SUCCESS) {
            uint64_t lat = 0; result = load_test_execute_ue_procedure(executor, created, RRC_PROC_REGISTRATION, &lat);
            load_test_update_metrics(executor, created, lat, result == UESIM_SUCCESS);
        }
        created++;
#ifdef _WIN32
        Sleep(interval_us / 1000);
#else
        usleep(interval_us);
#endif
    }
    load_test_stop(executor); load_test_collect_metrics(executor);
    return UESIM_SUCCESS;
}

uesim_error_t load_test_run_session_flood(load_test_executor_t* executor) {
    if (!executor) return UESIM_ERROR_INVALID_PARAM;
    printf("Running session flood (%u UEs)\n", executor->config.num_ues);
    load_test_start(executor);
    uesim_error_t result = load_test_create_ue_batch(executor, 0, executor->config.num_ues);
    if (result != UESIM_SUCCESS) { load_test_abort(executor); return result; }
    for (uint32_t i = 0; i < executor->config.num_ues && !executor->abort_flag; i++) {
        uint64_t lat = 0; result = load_test_execute_ue_procedure(executor, i, RRC_PROC_REGISTRATION, &lat);
        load_test_update_metrics(executor, i, lat, result == UESIM_SUCCESS);
    }
    time_t start = time(NULL);
    while (difftime(time(NULL), start) < executor->config.duration_seconds && !executor->abort_flag) {
        for (uint32_t i = 0; i < executor->config.num_ues && !executor->abort_flag; i++) {
            uint64_t lat = 0; result = load_test_execute_ue_procedure(executor, i, RRC_PROC_ESTABLISHMENT, &lat);
            load_test_update_metrics(executor, i, lat, result == UESIM_SUCCESS);
        }
    }
    load_test_stop(executor); load_test_collect_metrics(executor);
    return UESIM_SUCCESS;
}

uesim_error_t load_test_run_handover_stress(load_test_executor_t* executor) {
    if (!executor) return UESIM_ERROR_INVALID_PARAM;
    printf("Running handover stress (%u UEs)\n", executor->config.num_ues);
    load_test_start(executor);
    uesim_error_t result = load_test_create_ue_batch(executor, 0, executor->config.num_ues);
    if (result != UESIM_SUCCESS) { load_test_abort(executor); return result; }
    for (uint32_t i = 0; i < executor->config.num_ues && !executor->abort_flag; i++) {
        uint64_t lat = 0; result = load_test_execute_ue_procedure(executor, i, RRC_PROC_REGISTRATION, &lat);
        load_test_update_metrics(executor, i, lat, result == UESIM_SUCCESS);
    }
    time_t start = time(NULL);
    while (difftime(time(NULL), start) < executor->config.duration_seconds && !executor->abort_flag) {
        for (uint32_t i = 0; i < executor->config.num_ues && !executor->abort_flag; i++) {
            uint64_t lat = 0; result = load_test_execute_ue_procedure(executor, i, RRC_PROC_HANDOVER, &lat);
            load_test_update_metrics(executor, i, lat, result == UESIM_SUCCESS);
        }
    }
    load_test_stop(executor); load_test_collect_metrics(executor);
    return UESIM_SUCCESS;
}

uesim_error_t load_test_run_mixed_workload(load_test_executor_t* executor) {
    if (!executor) return UESIM_ERROR_INVALID_PARAM;
    printf("Running mixed workload (%u UEs)\n", executor->config.num_ues);
    load_test_start(executor);
    uint32_t created = 0; uint32_t interval_us = 1000000 / executor->config.ramp_rate;
    while (created < executor->config.num_ues && !executor->abort_flag) {
        uesim_error_t result = load_test_create_ue_batch(executor, created, 1);
        if (result == UESIM_SUCCESS) {
            uint64_t lat = 0; result = load_test_execute_ue_procedure(executor, created, RRC_PROC_REGISTRATION, &lat);
            load_test_update_metrics(executor, created, lat, result == UESIM_SUCCESS);
        }
        created++;
#ifdef _WIN32
        Sleep(interval_us / 1000);
#else
        usleep(interval_us);
#endif
    }
    time_t start = time(NULL);
    rrc_procedure_t procedures[] = { RRC_PROC_REGISTRATION, RRC_PROC_ESTABLISHMENT, RRC_PROC_HANDOVER };
    while (difftime(time(NULL), start) < executor->config.duration_seconds && !executor->abort_flag) {
        for (uint32_t i = 0; i < executor->config.num_ues && !executor->abort_flag; i++) {
            uint64_t lat = 0;
            rrc_procedure_t proc = procedures[rand() % 3];
            uesim_error_t result = load_test_execute_ue_procedure(executor, i, proc, &lat);
            load_test_update_metrics(executor, i, lat, result == UESIM_SUCCESS);
        }
    }
    load_test_stop(executor); load_test_collect_metrics(executor);
    return UESIM_SUCCESS;
}

uesim_error_t load_test_run_scenario(load_test_executor_t* executor, load_test_scenario_type_t scenario, uint32_t num_ues, uint64_t duration_seconds) {
    if (!executor) return UESIM_ERROR_INVALID_PARAM;
    executor->config.scenario = scenario; executor->config.num_ues = num_ues; executor->config.duration_seconds = duration_seconds;
    switch (scenario) {
        case LOAD_TEST_BURST_REGISTRATION: return load_test_run_burst_registration(executor);
        case LOAD_TEST_RAMP_REGISTRATION: return load_test_run_ramp_registration(executor);
        case LOAD_TEST_SESSION_FLOOD: return load_test_run_session_flood(executor);
        case LOAD_TEST_HANDOVER_STRESS: return load_test_run_handover_stress(executor);
        case LOAD_TEST_MIXED_WORKLOAD: return load_test_run_mixed_workload(executor);
        default: return UESIM_ERROR_INVALID_PARAM;
    }
}

uesim_error_t load_test_print_report(load_test_executor_t* executor) {
    if (!executor) return UESIM_ERROR_INVALID_PARAM;
    printf("\n===== Load Test Report =====\n");
    printf("Scenario: %s\n", executor->config.name);
    printf("Duration: %lu seconds\n", executor->metrics.duration_seconds);
    printf("UEs: %u\n", executor->config.num_ues);
    printf("\n--- Throughput ---\n");
    printf("Total procedures: %lu\n", executor->metrics.throughput.total_procedures);
    printf("Successful: %lu\n", executor->metrics.throughput.successful_procedures);
    printf("Failed: %lu\n", executor->metrics.throughput.failed_procedures);
    printf("Procedures/sec: %.2f\n", executor->metrics.throughput.procedures_per_second);
    printf("\n--- Latency ---\n");
    printf("Min: %lu us\n", executor->metrics.latency.min_us);
    printf("Max: %lu us\n", executor->metrics.latency.max_us);
    printf("Mean: %lu us\n", executor->metrics.latency.mean_us);
    printf("P50: %lu us\n", executor->metrics.latency.p50_us);
    printf("P95: %lu us\n", executor->metrics.latency.p95_us);
    printf("P99: %lu us\n", executor->metrics.latency.p99_us);
    printf("\n--- Failures ---\n");
    printf("Failure rate: %.2f%%\n", executor->metrics.failures.failure_rate);
    printf("============================\n");
    return UESIM_SUCCESS;
}

uesim_error_t load_test_generate_report(load_test_executor_t* executor, load_test_report_format_t format, const char* output_path) {
    if (!executor) return UESIM_ERROR_INVALID_PARAM;
    if (format == LOAD_TEST_REPORT_TEXT) return load_test_print_report(executor);
    if (format == LOAD_TEST_REPORT_CSV) return load_test_write_csv_report(executor, output_path);
    if (format == LOAD_TEST_REPORT_JSON) return load_test_write_json_report(executor, output_path);
    return load_test_print_report(executor);
}

uesim_error_t load_test_write_csv_report(load_test_executor_t* executor, const char* path) {
    if (!executor || !path) return UESIM_ERROR_INVALID_PARAM;
    FILE* f = fopen(path, "w");
    if (!f) return UESIM_ERROR_FILE;
    fprintf(f, "metric,value\n");
    fprintf(f, "scenario,%s\n", executor->config.name);
    fprintf(f, "duration_seconds,%lu\n", executor->metrics.duration_seconds);
    fprintf(f, "num_ues,%u\n", executor->config.num_ues);
    fprintf(f, "total_procedures,%lu\n", executor->metrics.throughput.total_procedures);
    fprintf(f, "successful_procedures,%lu\n", executor->metrics.throughput.successful_procedures);
    fprintf(f, "failed_procedures,%lu\n", executor->metrics.throughput.failed_procedures);
    fprintf(f, "procedures_per_second,%.2f\n", executor->metrics.throughput.procedures_per_second);
    fprintf(f, "latency_min_us,%lu\n", executor->metrics.latency.min_us);
    fprintf(f, "latency_max_us,%lu\n", executor->metrics.latency.max_us);
    fprintf(f, "latency_mean_us,%lu\n", executor->metrics.latency.mean_us);
    fprintf(f, "latency_p50_us,%lu\n", executor->metrics.latency.p50_us);
    fprintf(f, "latency_p95_us,%lu\n", executor->metrics.latency.p95_us);
    fprintf(f, "latency_p99_us,%lu\n", executor->metrics.latency.p99_us);
    fprintf(f, "failure_rate,%.2f\n", executor->metrics.failures.failure_rate);
    fclose(f);
    printf("CSV report written to: %s\n", path);
    return UESIM_SUCCESS;
}

uesim_error_t load_test_write_json_report(load_test_executor_t* executor, const char* path) {
    if (!executor || !path) return UESIM_ERROR_INVALID_PARAM;
    FILE* f = fopen(path, "w");
    if (!f) return UESIM_ERROR_FILE;
    fprintf(f, "{\n");
    fprintf(f, "  \"scenario\": \"%s\",\n", executor->config.name);
    fprintf(f, "  \"duration_seconds\": %lu,\n", executor->metrics.duration_seconds);
    fprintf(f, "  \"num_ues\": %u,\n", executor->config.num_ues);
    fprintf(f, "  \"throughput\": {\n");
    fprintf(f, "    \"total_procedures\": %lu,\n", executor->metrics.throughput.total_procedures);
    fprintf(f, "    \"successful\": %lu,\n", executor->metrics.throughput.successful_procedures);
    fprintf(f, "    \"failed\": %lu,\n", executor->metrics.throughput.failed_procedures);
    fprintf(f, "    \"procedures_per_second\": %.2f\n", executor->metrics.throughput.procedures_per_second);
    fprintf(f, "  },\n");
    fprintf(f, "  \"latency\": {\n");
    fprintf(f, "    \"min_us\": %lu,\n", executor->metrics.latency.min_us);
    fprintf(f, "    \"max_us\": %lu,\n", executor->metrics.latency.max_us);
    fprintf(f, "    \"mean_us\": %lu,\n", executor->metrics.latency.mean_us);
    fprintf(f, "    \"p50_us\": %lu,\n", executor->metrics.latency.p50_us);
    fprintf(f, "    \"p95_us\": %lu,\n", executor->metrics.latency.p95_us);
    fprintf(f, "    \"p99_us\": %lu\n", executor->metrics.latency.p99_us);
    fprintf(f, "  },\n");
    fprintf(f, "  \"failures\": {\n");
    fprintf(f, "    \"failure_rate\": %.2f\n", executor->metrics.failures.failure_rate);
    fprintf(f, "  }\n");
    fprintf(f, "}\n");
    fclose(f);
    printf("JSON report written to: %s\n", path);
    return UESIM_SUCCESS;
}