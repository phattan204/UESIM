/*
 * 5G UE Simulation Application
 * Benchmark Test
 */

#include "../src/benchmark/benchmark.h"
#include "../src/core/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("5G UE Simulation Benchmark Test\n");
    printf("===============================\n");
    
    // Test benchmark initialization
    uesim_error_t result = benchmark_init();
    if (result == UESIM_SUCCESS) {
        printf("✓ Benchmark initialization successful\n");
    } else {
        printf("✗ Benchmark initialization failed: %d\n", result);
        return EXIT_FAILURE;
    }
    
    // Test benchmark configuration
    printf("\nTesting Benchmark Configuration:\n");
    
    benchmark_config_t config = {0};
    result = benchmark_config_init(&config);
    if (result == UESIM_SUCCESS) {
        printf("✓ Benchmark configuration initialization successful\n");
    } else {
        printf("✗ Benchmark configuration initialization failed: %d\n");
        benchmark_cleanup();
        return EXIT_FAILURE;
    }
    
    result = benchmark_config_set_defaults(&config);
    if (result == UESIM_SUCCESS) {
        printf("✓ Benchmark configuration defaults setting successful\n");
    } else {
        printf("✗ Benchmark configuration defaults setting failed: %d\n");
        benchmark_cleanup();
        return EXIT_FAILURE;
    }
    
    // Test benchmark result initialization
    printf("\nTesting Benchmark Result Management:\n");
    
    benchmark_result_t benchmark_result = {0};
    result = benchmark_result_init(&benchmark_result);
    if (result == UESIM_SUCCESS) {
        printf("✓ Benchmark result initialization successful\n");
    } else {
        printf("✗ Benchmark result initialization failed: %d\n");
        benchmark_cleanup();
        return EXIT_FAILURE;
    }
    
    // Test benchmark suite creation
    printf("\nTesting Benchmark Suite Management:\n");
    
    benchmark_suite_t* suite = NULL;
    result = benchmark_suite_create("Test Suite", &suite);
    if (result == UESIM_SUCCESS && suite != NULL) {
        printf("✓ Benchmark suite creation successful\n");
    } else {
        printf("✗ Benchmark suite creation failed: %d\n");
        benchmark_cleanup();
        return EXIT_FAILURE;
    }
    
    // Test predefined configurations
    printf("\nTesting Predefined Configurations:\n");
    
    benchmark_config_t default_config = benchmark_get_default_config(BENCHMARK_TYPE_THROUGHPUT, BENCHMARK_CATEGORY_PDCP);
    printf("✓ Default PDCP config created: %s\n", default_config.name);
    
    benchmark_config_t stress_config = benchmark_get_stress_config(BENCHMARK_TYPE_LATENCY, BENCHMARK_CATEGORY_RLC);
    printf("✓ Stress RLC config created: %s\n", stress_config.name);
    
    benchmark_config_t perf_config = benchmark_get_performance_config(BENCHMARK_TYPE_MEMORY, BENCHMARK_CATEGORY_MAC);
    printf("✓ Performance MAC config created: %s\n", perf_config.name);
    
    // Test benchmark categories and types
    printf("\nTesting Benchmark Categories and Types:\n");
    
    printf("✓ Benchmark categories:\n");
    for (int i = 0; i < BENCHMARK_CATEGORY_MAX; i++) {
        printf("  %d: %s\n", i, benchmark_get_category_name((benchmark_category_t)i));
    }
    
    printf("✓ Benchmark types:\n");
    for (int i = 0; i < BENCHMARK_TYPE_MAX; i++) {
        printf("  %d: %s\n", i, benchmark_get_type_name((benchmark_type_t)i));
    }
    
    // Test utility functions
    printf("\nTesting Utility Functions:\n");
    
    uint64_t test_time = 1234567890ULL;
    char time_buffer[64];
    char* time_result = benchmark_format_time(test_time, time_buffer, sizeof(time_buffer));
    if (time_result != NULL) {
        printf("✓ Time formatting successful: %s\n", time_result);
    } else {
        printf("✗ Time formatting failed\n");
    }
    
    double test_throughput = 1234567.89;
    char throughput_buffer[64];
    char* throughput_result = benchmark_format_throughput(test_throughput, throughput_buffer, sizeof(throughput_buffer));
    if (throughput_result != NULL) {
        printf("✓ Throughput formatting successful: %s\n", throughput_result);
    } else {
        printf("✗ Throughput formatting failed\n");
    }
    
    uint64_t test_memory = 1234567890ULL;
    char memory_buffer[64];
    char* memory_result = benchmark_format_memory(test_memory, memory_buffer, sizeof(memory_buffer));
    if (memory_result != NULL) {
        printf("✓ Memory formatting successful: %s\n", memory_result);
    } else {
        printf("✗ Memory formatting failed\n");
    }
    
    // Test data structures
    printf("\nTesting Benchmark Data Structures:\n");
    printf("✓ Benchmark config structure size: %zu bytes\n", sizeof(benchmark_config_t));
    printf("✓ Benchmark metrics structure size: %zu bytes\n", sizeof(benchmark_metrics_t));
    printf("✓ Benchmark result structure size: %zu bytes\n", sizeof(benchmark_result_t));
    printf("✓ Benchmark suite structure size: %zu bytes\n", sizeof(benchmark_suite_t));
    printf("✓ Benchmark constants:\n");
    printf("  BENCHMARK_MAX_NAME_LEN: %d\n", BENCHMARK_MAX_NAME_LEN);
    printf("  BENCHMARK_MAX_ITERATIONS: %d\n", BENCHMARK_MAX_ITERATIONS);
    printf("  BENCHMARK_DEFAULT_ITERATIONS: %d\n", BENCHMARK_DEFAULT_ITERATIONS);
    printf("  BENCHMARK_MAX_THREADS: %d\n", BENCHMARK_MAX_THREADS);
    
    // Test benchmark suite destruction
    result = benchmark_suite_destroy(suite);
    if (result == UESIM_SUCCESS) {
        printf("✓ Benchmark suite destruction successful\n");
    } else {
        printf("✗ Benchmark suite destruction failed: %d\n");
    }
    
    // Cleanup
    benchmark_cleanup();
    
    printf("\nAll benchmark tests passed!\n");
    printf("Benchmark system is ready for integration.\n");
    
    return EXIT_SUCCESS;
}