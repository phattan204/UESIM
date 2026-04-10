/*
 * 5G UE Simulation Application
 * Performance Benchmarking Implementation
 */

#include "benchmark.h"
#include "../core/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#ifdef __linux__
#include <sys/resource.h>
#endif

// Global variables
static bool g_benchmark_initialized = false;
static pthread_mutex_t g_benchmark_mutex = PTHREAD_MUTEX_INITIALIZER;

uesim_error_t benchmark_init(void) {
    if (g_benchmark_initialized) {
        return UESIM_SUCCESS;
    }
    
    // Initialize mutex
    if (pthread_mutex_init(&g_benchmark_mutex, NULL) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    g_benchmark_initialized = true;
    printf("Benchmark system initialized\n");
    return UESIM_SUCCESS;
}

void benchmark_cleanup(void) {
    if (!g_benchmark_initialized) {
        return;
    }
    
    pthread_mutex_destroy(&g_benchmark_mutex);
    g_benchmark_initialized = false;
    printf("Benchmark system cleaned up\n");
}

// Benchmark suite management
uesim_error_t benchmark_suite_create(const char* name, benchmark_suite_t** suite) {
    if (name == NULL || suite == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (!g_benchmark_initialized) {
        return UESIM_ERROR_NOT_INITIALIZED;
    }
    
    benchmark_suite_t* new_suite = (benchmark_suite_t*)uesim_calloc(1, sizeof(benchmark_suite_t));
    if (new_suite == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    strncpy(new_suite->name, name, BENCHMARK_MAX_NAME_LEN - 1);
    new_suite->name[BENCHMARK_MAX_NAME_LEN - 1] = '\0';
    new_suite->max_results = 32; // Initial capacity
    
    new_suite->results = (benchmark_result_t*)uesim_calloc(new_suite->max_results, 
                                                          sizeof(benchmark_result_t));
    if (new_suite->results == NULL) {
        uesim_free(new_suite);
        return UESIM_ERROR_MEMORY;
    }
    
    *suite = new_suite;
    return UESIM_SUCCESS;
}

uesim_error_t benchmark_suite_destroy(benchmark_suite_t* suite) {
    if (suite == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (suite->results != NULL) {
        uesim_free(suite->results);
    }
    
    uesim_free(suite);
    return UESIM_SUCCESS;
}

uesim_error_t benchmark_suite_add_result(benchmark_suite_t* suite, const benchmark_result_t* result) {
    if (suite == NULL || result == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if we need to expand the results array
    if (suite->result_count >= suite->max_results) {
        size_t new_max = suite->max_results * 2;
        benchmark_result_t* new_results = (benchmark_result_t*)uesim_realloc(suite->results, 
                                                                           new_max * sizeof(benchmark_result_t));
        if (new_results == NULL) {
            return UESIM_ERROR_MEMORY;
        }
        suite->results = new_results;
        suite->max_results = new_max;
    }
    
    // Copy the result
    suite->results[suite->result_count] = *result;
    suite->result_count++;
    
    return UESIM_SUCCESS;
}

uesim_error_t benchmark_suite_run(benchmark_suite_t* suite) {
    if (suite == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (!g_benchmark_initialized) {
        return UESIM_ERROR_NOT_INITIALIZED;
    }
    
    suite->start_time = time(NULL);
    suite->success = true;
    
    printf("Running benchmark suite: %s\n", suite->name);
    printf("Total benchmarks: %zu\n", suite->result_count);
    
    // Run each benchmark in the suite
    for (size_t i = 0; i < suite->result_count; i++) {
        benchmark_result_t* result = &suite->results[i];
        
        printf("\nRunning benchmark %zu/%zu: %s\n", i + 1, suite->result_count, result->config.name);
        
        // Run the benchmark
        uesim_error_t error = benchmark_run(&result->config, result);
        if (error != UESIM_SUCCESS) {
            printf("  Failed: %d\n", error);
            suite->success = false;
        } else {
            printf("  Completed successfully\n");
            benchmark_result_print_summary(result);
        }
    }
    
    suite->end_time = time(NULL);
    printf("\nBenchmark suite completed in %ld seconds\n", suite->end_time - suite->start_time);
    
    return UESIM_SUCCESS;
}

void benchmark_suite_print_results(const benchmark_suite_t* suite) {
    if (suite == NULL) {
        return;
    }
    
    printf("\n=== Benchmark Suite Results: %s ===\n", suite->name);
    printf("Start time: %s", ctime(&suite->start_time));
    printf("End time: %s", ctime(&suite->end_time));
    printf("Total benchmarks: %zu\n", suite->result_count);
    printf("Status: %s\n", suite->success ? "SUCCESS" : "FAILED");
    printf("==========================================\n");
    
    for (size_t i = 0; i < suite->result_count; i++) {
        printf("\n--- Benchmark %zu ---\n", i + 1);
        benchmark_result_print(&suite->results[i]);
    }
}

// Benchmark execution
uesim_error_t benchmark_run(const benchmark_config_t* config, benchmark_result_t* result) {
    if (config == NULL || result == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (!g_benchmark_initialized) {
        return UESIM_ERROR_NOT_INITIALIZED;
    }
    
    // Initialize result
    memset(result, 0, sizeof(benchmark_result_t));
    result->config = *config;
    result->start_time = time(NULL);
    
    // Select benchmark function based on category
    benchmark_func_t benchmark_func = NULL;
    
    switch (config->category) {
        case BENCHMARK_CATEGORY_PDCP:
            if (strstr(config->name, "ciphering") != NULL) {
                benchmark_func = benchmark_pdcp_ciphering;
            } else if (strstr(config->name, "integrity") != NULL) {
                benchmark_func = benchmark_pdcp_integrity;
            }
            break;
            
        case BENCHMARK_CATEGORY_RLC:
            if (strstr(config->name, "segmentation") != NULL) {
                benchmark_func = benchmark_rlc_segmentation;
            } else if (strstr(config->name, "reassembly") != NULL) {
                benchmark_func = benchmark_rlc_reassembly;
            }
            break;
            
        case BENCHMARK_CATEGORY_MAC:
            if (strstr(config->name, "harq") != NULL) {
                benchmark_func = benchmark_mac_harq;
            }
            break;
            
        case BENCHMARK_CATEGORY_NAS:
            if (strstr(config->name, "encoding") != NULL) {
                benchmark_func = benchmark_nas_encoding;
            } else if (strstr(config->name, "decoding") != NULL) {
                benchmark_func = benchmark_nas_decoding;
            }
            break;
            
        case BENCHMARK_CATEGORY_RRC:
            benchmark_func = benchmark_rrc_procedures;
            break;
            
        case BENCHMARK_CATEGORY_SOCKET:
            benchmark_func = benchmark_socket_operations;
            break;
            
        case BENCHMARK_CATEGORY_MEMORY:
            benchmark_func = benchmark_memory_allocation;
            break;
            
        case BENCHMARK_CATEGORY_THREAD:
            benchmark_func = benchmark_thread_operations;
            break;
            
        case BENCHMARK_CATEGORY_CRYPTO:
            benchmark_func = benchmark_crypto_operations;
            break;
            
        default:
            snprintf(result->error_message, sizeof(result->error_message), 
                    "Unknown benchmark category: %d", config->category);
            result->success = false;
            return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (benchmark_func == NULL) {
        snprintf(result->error_message, sizeof(result->error_message), 
                "No benchmark function found for category %d", config->category);
        result->success = false;
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Run the benchmark function
    uesim_error_t error = benchmark_func((benchmark_config_t*)config, &result->metrics);
    result->end_time = time(NULL);
    
    if (error == UESIM_SUCCESS) {
        result->success = true;
        snprintf(result->description, sizeof(result->description), 
                "Benchmark completed successfully");
    } else {
        result->success = false;
        snprintf(result->error_message, sizeof(result->error_message), 
                "Benchmark function failed: %d", error);
    }
    
    return error;
}

uesim_error_t benchmark_run_function(benchmark_func_t func, const benchmark_config_t* config, 
                                    benchmark_result_t* result) {
    if (func == NULL || config == NULL || result == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    return benchmark_run(config, result);
}

// Specific benchmark functions (simplified implementations)
uesim_error_t benchmark_pdcp_ciphering(benchmark_config_t* config, benchmark_metrics_t* metrics) {
    if (config == NULL || metrics == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("Running PDCP ciphering benchmark...\n");
    
    uint64_t iterations = config->iterations > 0 ? config->iterations : BENCHMARK_DEFAULT_ITERATIONS;
    uint32_t packet_size = config->packet_size > 0 ? config->packet_size : 1024;
    
    // Allocate test buffer
    uint8_t* buffer = (uint8_t*)uesim_malloc(packet_size);
    if (buffer == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    // Initialize buffer with test data
    for (uint32_t i = 0; i < packet_size; i++) {
        buffer[i] = (uint8_t)(i & 0xFF);
    }
    
    // Warmup runs
    if (config->warmup) {
        uint32_t warmup_iters = config->warmup_iterations > 0 ? config->warmup_iterations : 1000;
        for (uint32_t i = 0; i < warmup_iters; i++) {
            // Simulate ciphering operation
            for (uint32_t j = 0; j < packet_size; j++) {
                buffer[j] ^= 0xAA;
            }
        }
    }
    
    // Benchmark runs
    uint64_t start_time = benchmark_get_time_ns();
    metrics->min_time = UINT64_MAX;
    metrics->max_time = 0;
    metrics->total_time = 0;
    
    for (uint64_t i = 0; i < iterations; i++) {
        uint64_t iter_start = benchmark_get_time_ns();
        
        // Simulate ciphering operation (NEA2/AES)
        for (uint32_t j = 0; j < packet_size; j++) {
            buffer[j] ^= 0x55;
        }
        
        uint64_t iter_end = benchmark_get_time_ns();
        uint64_t iter_time = iter_end - iter_start;
        
        if (iter_time < metrics->min_time) metrics->min_time = iter_time;
        if (iter_time > metrics->max_time) metrics->max_time = iter_time;
        metrics->total_time += iter_time;
    }
    
    uint64_t end_time = benchmark_get_time_ns();
    uint64_t total_time = end_time - start_time;
    
    metrics->avg_time = metrics->total_time / iterations;
    metrics->iterations = iterations;
    metrics->throughput = (double)iterations / (total_time / 1000000000.0); // ops/sec
    metrics->memory_allocated = packet_size;
    
    uesim_free(buffer);
    
    printf("PDCP ciphering benchmark completed\n");
    return UESIM_SUCCESS;
}

uesim_error_t benchmark_pdcp_integrity(benchmark_config_t* config, benchmark_metrics_t* metrics) {
    if (config == NULL || metrics == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("Running PDCP integrity benchmark...\n");
    
    uint64_t iterations = config->iterations > 0 ? config->iterations : BENCHMARK_DEFAULT_ITERATIONS;
    uint32_t packet_size = config->packet_size > 0 ? config->packet_size : 1024;
    
    // Allocate test buffer
    uint8_t* buffer = (uint8_t*)uesim_malloc(packet_size);
    if (buffer == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    // Initialize buffer with test data
    for (uint32_t i = 0; i < packet_size; i++) {
        buffer[i] = (uint8_t)(i & 0xFF);
    }
    
    // Benchmark runs
    uint64_t start_time = benchmark_get_time_ns();
    metrics->min_time = UINT64_MAX;
    metrics->max_time = 0;
    metrics->total_time = 0;
    
    uint32_t mac_result = 0;
    
    for (uint64_t i = 0; i < iterations; i++) {
        uint64_t iter_start = benchmark_get_time_ns();
        
        // Simulate integrity operation (NIA2/AES-CMAC)
        mac_result = 0;
        for (uint32_t j = 0; j < packet_size; j++) {
            mac_result ^= (buffer[j] << (j % 32));
        }
        
        uint64_t iter_end = benchmark_get_time_ns();
        uint64_t iter_time = iter_end - iter_start;
        
        if (iter_time < metrics->min_time) metrics->min_time = iter_time;
        if (iter_time > metrics->max_time) metrics->max_time = iter_time;
        metrics->total_time += iter_time;
    }
    
    uint64_t end_time = benchmark_get_time_ns();
    uint64_t total_time = end_time - start_time;
    
    metrics->avg_time = metrics->total_time / iterations;
    metrics->iterations = iterations;
    metrics->throughput = (double)iterations / (total_time / 1000000000.0); // ops/sec
    metrics->memory_allocated = packet_size;
    
    uesim_free(buffer);
    
    printf("PDCP integrity benchmark completed\n");
    return UESIM_SUCCESS;
}

uesim_error_t benchmark_rlc_segmentation(benchmark_config_t* config, benchmark_metrics_t* metrics) {
    if (config == NULL || metrics == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("Running RLC segmentation benchmark...\n");
    
    uint64_t iterations = config->iterations > 0 ? config->iterations : BENCHMARK_DEFAULT_ITERATIONS;
    uint32_t packet_size = config->packet_size > 0 ? config->packet_size : 8192;
    uint32_t segment_size = 1024;
    
    // Allocate test buffers
    uint8_t* input_buffer = (uint8_t*)uesim_malloc(packet_size);
    uint8_t* output_buffer = (uint8_t*)uesim_malloc(segment_size);
    if (input_buffer == NULL || output_buffer == NULL) {
        if (input_buffer) uesim_free(input_buffer);
        if (output_buffer) uesim_free(output_buffer);
        return UESIM_ERROR_MEMORY;
    }
    
    // Initialize input buffer
    for (uint32_t i = 0; i < packet_size; i++) {
        input_buffer[i] = (uint8_t)(i & 0xFF);
    }
    
    // Benchmark runs
    uint64_t start_time = benchmark_get_time_ns();
    metrics->min_time = UINT64_MAX;
    metrics->max_time = 0;
    metrics->total_time = 0;
    uint64_t total_segments = 0;
    
    for (uint64_t i = 0; i < iterations; i++) {
        uint64_t iter_start = benchmark_get_time_ns();
        
        // Simulate segmentation
        uint32_t segments = packet_size / segment_size;
        if (packet_size % segment_size) segments++;
        
        for (uint32_t seg = 0; seg < segments; seg++) {
            uint32_t copy_size = (seg == segments - 1) ? 
                               (packet_size - seg * segment_size) : segment_size;
            memcpy(output_buffer, input_buffer + seg * segment_size, copy_size);
            total_segments++;
        }
        
        uint64_t iter_end = benchmark_get_time_ns();
        uint64_t iter_time = iter_end - iter_start;
        
        if (iter_time < metrics->min_time) metrics->min_time = iter_time;
        if (iter_time > metrics->max_time) metrics->max_time = iter_time;
        metrics->total_time += iter_time;
    }
    
    uint64_t end_time = benchmark_get_time_ns();
    uint64_t total_time = end_time - start_time;
    
    metrics->avg_time = metrics->total_time / iterations;
    metrics->iterations = iterations;
    metrics->throughput = (double)total_segments / (total_time / 1000000000.0); // segments/sec
    metrics->memory_allocated = packet_size + segment_size;
    
    uesim_free(input_buffer);
    uesim_free(output_buffer);
    
    printf("RLC segmentation benchmark completed\n");
    return UESIM_SUCCESS;
}

uesim_error_t benchmark_rlc_reassembly(benchmark_config_t* config, benchmark_metrics_t* metrics) {
    if (config == NULL || metrics == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("Running RLC reassembly benchmark...\n");
    
    uint64_t iterations = config->iterations > 0 ? config->iterations : BENCHMARK_DEFAULT_ITERATIONS;
    uint32_t packet_size = config->packet_size > 0 ? config->packet_size : 8192;
    uint32_t segment_size = 1024;
    
    // Allocate test buffers
    uint8_t* input_buffer = (uint8_t*)uesim_malloc(segment_size);
    uint8_t* output_buffer = (uint8_t*)uesim_malloc(packet_size);
    if (input_buffer == NULL || output_buffer == NULL) {
        if (input_buffer) uesim_free(input_buffer);
        if (output_buffer) uesim_free(output_buffer);
        return UESIM_ERROR_MEMORY;
    }
    
    // Initialize input buffer
    for (uint32_t i = 0; i < segment_size; i++) {
        input_buffer[i] = (uint8_t)(i & 0xFF);
    }
    
    // Benchmark runs
    uint64_t start_time = benchmark_get_time_ns();
    metrics->min_time = UINT64_MAX;
    metrics->max_time = 0;
    metrics->total_time = 0;
    uint64_t total_reassemblies = 0;
    
    for (uint64_t i = 0; i < iterations; i++) {
        uint64_t iter_start = benchmark_get_time_ns();
        
        // Simulate reassembly
        uint32_t segments = packet_size / segment_size;
        if (packet_size % segment_size) segments++;
        
        for (uint32_t seg = 0; seg < segments; seg++) {
            uint32_t copy_size = (seg == segments - 1) ? 
                               (packet_size - seg * segment_size) : segment_size;
            memcpy(output_buffer + seg * segment_size, input_buffer, copy_size);
        }
        total_reassemblies++;
        
        uint64_t iter_end = benchmark_get_time_ns();
        uint64_t iter_time = iter_end - iter_start;
        
        if (iter_time < metrics->min_time) metrics->min_time = iter_time;
        if (iter_time > metrics->max_time) metrics->max_time = iter_time;
        metrics->total_time += iter_time;
    }
    
    uint64_t end_time = benchmark_get_time_ns();
    uint64_t total_time = end_time - start_time;
    
    metrics->avg_time = metrics->total_time / iterations;
    metrics->iterations = iterations;
    metrics->throughput = (double)total_reassemblies / (total_time / 1000000000.0); // reassemblies/sec
    metrics->memory_allocated = packet_size + segment_size;
    
    uesim_free(input_buffer);
    uesim_free(output_buffer);
    
    printf("RLC reassembly benchmark completed\n");
    return UESIM_SUCCESS;
}

uesim_error_t benchmark_mac_harq(benchmark_config_t* config, benchmark_metrics_t* metrics) {
    if (config == NULL || metrics == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("Running MAC HARQ benchmark...\n");
    
    uint64_t iterations = config->iterations > 0 ? config->iterations : BENCHMARK_DEFAULT_ITERATIONS;
    uint32_t tb_size = config->packet_size > 0 ? config->packet_size : 1024;
    uint32_t max_retransmissions = 4;
    
    // Allocate test buffers
    uint8_t* tx_buffer = (uint8_t*)uesim_malloc(tb_size);
    uint8_t* rx_buffer = (uint8_t*)uesim_malloc(tb_size);
    if (tx_buffer == NULL || rx_buffer == NULL) {
        if (tx_buffer) uesim_free(tx_buffer);
        if (rx_buffer) uesim_free(rx_buffer);
        return UESIM_ERROR_MEMORY;
    }
    
    // Initialize buffers
    for (uint32_t i = 0; i < tb_size; i++) {
        tx_buffer[i] = (uint8_t)(i & 0xFF);
        rx_buffer[i] = 0;
    }
    
    // Benchmark runs
    uint64_t start_time = benchmark_get_time_ns();
    metrics->min_time = UINT64_MAX;
    metrics->max_time = 0;
    metrics->total_time = 0;
    uint64_t total_harq_processes = 0;
    
    for (uint64_t i = 0; i < iterations; i++) {
        uint64_t iter_start = benchmark_get_time_ns();
        
        // Simulate HARQ process
        for (uint32_t harq = 0; harq < 16; harq++) { // 16 HARQ processes
            // Simulate transmission and potential retransmissions
            for (uint32_t retx = 0; retx < max_retransmissions; retx++) {
                memcpy(rx_buffer, tx_buffer, tb_size);
                total_harq_processes++;
                
                // Simulate ACK/NACK decision (80% success rate)
                if ((rand() % 100) < 80) {
                    break; // ACK - process complete
                }
                // NACK - retransmit
            }
        }
        
        uint64_t iter_end = benchmark_get_time_ns();
        uint64_t iter_time = iter_end - iter_start;
        
        if (iter_time < metrics->min_time) metrics->min_time = iter_time;
        if (iter_time > metrics->max_time) metrics->max_time = iter_time;
        metrics->total_time += iter_time;
    }
    
    uint64_t end_time = benchmark_get_time_ns();
    uint64_t total_time = end_time - start_time;
    
    metrics->avg_time = metrics->total_time / iterations;
    metrics->iterations = iterations;
    metrics->throughput = (double)total_harq_processes / (total_time / 1000000000.0); // processes/sec
    metrics->memory_allocated = tb_size * 2;
    
    uesim_free(tx_buffer);
    uesim_free(rx_buffer);
    
    printf("MAC HARQ benchmark completed\n");
    return UESIM_SUCCESS;
}

uesim_error_t benchmark_nas_encoding(benchmark_config_t* config, benchmark_metrics_t* metrics) {
    if (config == NULL || metrics == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("Running NAS encoding benchmark...\n");
    
    uint64_t iterations = config->iterations > 0 ? config->iterations : BENCHMARK_DEFAULT_ITERATIONS;
    uint32_t message_size = config->packet_size > 0 ? config->packet_size : 256;
    
    // Allocate test buffers
    uint8_t* input_buffer = (uint8_t*)uesim_malloc(message_size);
    uint8_t* output_buffer = (uint8_t*)uesim_malloc(message_size * 2);
    if (input_buffer == NULL || output_buffer == NULL) {
        if (input_buffer) uesim_free(input_buffer);
        if (output_buffer) uesim_free(output_buffer);
        return UESIM_ERROR_MEMORY;
    }
    
    // Initialize input buffer
    for (uint32_t i = 0; i < message_size; i++) {
        input_buffer[i] = (uint8_t)(i & 0xFF);
    }
    
    // Benchmark runs
    uint64_t start_time = benchmark_get_time_ns();
    metrics->min_time = UINT64_MAX;
    metrics->max_time = 0;
    metrics->total_time = 0;
    
    for (uint64_t i = 0; i < iterations; i++) {
        uint64_t iter_start = benchmark_get_time_ns();
        
        // Simulate NAS message encoding
        uint32_t encoded_size = 0;
        output_buffer[encoded_size++] = 0x7E; // Extended protocol discriminator
        output_buffer[encoded_size++] = 0x00; // Security header
        output_buffer[encoded_size++] = 0x41; // Message type (Registration Request)
        
        // Copy message content
        memcpy(output_buffer + encoded_size, input_buffer, message_size);
        encoded_size += message_size;
        
        uint64_t iter_end = benchmark_get_time_ns();
        uint64_t iter_time = iter_end - iter_start;
        
        if (iter_time < metrics->min_time) metrics->min_time = iter_time;
        if (iter_time > metrics->max_time) metrics->max_time = iter_time;
        metrics->total_time += iter_time;
    }
    
    uint64_t end_time = benchmark_get_time_ns();
    uint64_t total_time = end_time - start_time;
    
    metrics->avg_time = metrics->total_time / iterations;
    metrics->iterations = iterations;
    metrics->throughput = (double)iterations / (total_time / 1000000000.0); // messages/sec
    metrics->memory_allocated = message_size + (message_size * 2);
    
    uesim_free(input_buffer);
    uesim_free(output_buffer);
    
    printf("NAS encoding benchmark completed\n");
    return UESIM_SUCCESS;
}

uesim_error_t benchmark_nas_decoding(benchmark_config_t* config, benchmark_metrics_t* metrics) {
    if (config == NULL || metrics == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("Running NAS decoding benchmark...\n");
    
    uint64_t iterations = config->iterations > 0 ? config->iterations : BENCHMARK_DEFAULT_ITERATIONS;
    uint32_t message_size = config->packet_size > 0 ? config->packet_size : 256;
    
    // Allocate test buffers
    uint8_t* input_buffer = (uint8_t*)uesim_malloc(message_size * 2);
    uint8_t* output_buffer = (uint8_t*)uesim_malloc(message_size);
    if (input_buffer == NULL || output_buffer == NULL) {
        if (input_buffer) uesim_free(input_buffer);
        if (output_buffer) uesim_free(output_buffer);
        return UESIM_ERROR_MEMORY;
    }
    
    // Initialize input buffer with encoded message
    input_buffer[0] = 0x7E; // Extended protocol discriminator
    input_buffer[1] = 0x00; // Security header
    input_buffer[2] = 0x41; // Message type (Registration Request)
    for (uint32_t i = 3; i < message_size + 3; i++) {
        input_buffer[i] = (uint8_t)(i & 0xFF);
    }
    
    // Benchmark runs
    uint64_t start_time = benchmark_get_time_ns();
    metrics->min_time = UINT64_MAX;
    metrics->max_time = 0;
    metrics->total_time = 0;
    
    for (uint64_t i = 0; i < iterations; i++) {
        uint64_t iter_start = benchmark_get_time_ns();
        
        // Simulate NAS message decoding
        if (input_buffer[0] == 0x7E && input_buffer[2] == 0x41) {
            // Valid NAS message - decode content
            memcpy(output_buffer, input_buffer + 3, message_size);
        }
        
        uint64_t iter_end = benchmark_get_time_ns();
        uint64_t iter_time = iter_end - iter_start;
        
        if (iter_time < metrics->min_time) metrics->min_time = iter_time;
        if (iter_time > metrics->max_time) metrics->max_time = iter_time;
        metrics->total_time += iter_time;
    }
    
    uint64_t end_time = benchmark_get_time_ns();
    uint64_t total_time = end_time - start_time;
    
    metrics->avg_time = metrics->total_time / iterations;
    metrics->iterations = iterations;
    metrics->throughput = (double)iterations / (total_time / 1000000000.0); // messages/sec
    metrics->memory_allocated = message_size + (message_size * 2);
    
    uesim_free(input_buffer);
    uesim_free(output_buffer);
    
    printf("NAS decoding benchmark completed\n");
    return UESIM_SUCCESS;
}

uesim_error_t benchmark_rrc_procedures(benchmark_config_t* config, benchmark_metrics_t* metrics) {
    if (config == NULL || metrics == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("Running RRC procedures benchmark...\n");
    
    uint64_t iterations = config->iterations > 0 ? config->iterations : BENCHMARK_DEFAULT_ITERATIONS;
    
    // Benchmark runs
    uint64_t start_time = benchmark_get_time_ns();
    metrics->min_time = UINT64_MAX;
    metrics->max_time = 0;
    metrics->total_time = 0;
    
    for (uint64_t i = 0; i < iterations; i++) {
        uint64_t iter_start = benchmark_get_time_ns();
        
        // Simulate RRC procedure execution
        // This is a simplified simulation of various RRC procedures
        switch (i % 4) {
            case 0: // Registration
                usleep(1000); // 1ms
                break;
            case 1: // Establishment
                usleep(2000); // 2ms
                break;
            case 2: // Re-establishment
                usleep(3000); // 3ms
                break;
            case 3: // Handover
                usleep(5000); // 5ms
                break;
        }
        
        uint64_t iter_end = benchmark_get_time_ns();
        uint64_t iter_time = iter_end - iter_start;
        
        if (iter_time < metrics->min_time) metrics->min_time = iter_time;
        if (iter_time > metrics->max_time) metrics->max_time = iter_time;
        metrics->total_time += iter_time;
    }
    
    uint64_t end_time = benchmark_get_time_ns();
    uint64_t total_time = end_time - start_time;
    
    metrics->avg_time = metrics->total_time / iterations;
    metrics->iterations = iterations;
    metrics->throughput = (double)iterations / (total_time / 1000000000.0); // procedures/sec
    
    printf("RRC procedures benchmark completed\n");
    return UESIM_SUCCESS;
}

uesim_error_t benchmark_socket_operations(benchmark_config_t* config, benchmark_metrics_t* metrics) {
    if (config == NULL || metrics == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("Running socket operations benchmark...\n");
    
    uint64_t iterations = config->iterations > 0 ? config->iterations : BENCHMARK_DEFAULT_ITERATIONS;
    uint32_t packet_size = config->packet_size > 0 ? config->packet_size : 1024;
    
    // Allocate test buffers
    uint8_t* send_buffer = (uint8_t*)uesim_malloc(packet_size);
    uint8_t* recv_buffer = (uint8_t*)uesim_malloc(packet_size);
    if (send_buffer == NULL || recv_buffer == NULL) {
        if (send_buffer) uesim_free(send_buffer);
        if (recv_buffer) uesim_free(recv_buffer);
        return UESIM_ERROR_MEMORY;
    }
    
    // Initialize send buffer
    for (uint32_t i = 0; i < packet_size; i++) {
        send_buffer[i] = (uint8_t)(i & 0xFF);
    }
    
    // Benchmark runs
    uint64_t start_time = benchmark_get_time_ns();
    metrics->min_time = UINT64_MAX;
    metrics->max_time = 0;
    metrics->total_time = 0;
    uint64_t total_packets = 0;
    
    for (uint64_t i = 0; i < iterations; i++) {
        uint64_t iter_start = benchmark_get_time_ns();
        
        // Simulate socket send/receive operations
        memcpy(recv_buffer, send_buffer, packet_size);
        total_packets++;
        
        uint64_t iter_end = benchmark_get_time_ns();
        uint64_t iter_time = iter_end - iter_start;
        
        if (iter_time < metrics->min_time) metrics->min_time = iter_time;
        if (iter_time > metrics->max_time) metrics->max_time = iter_time;
        metrics->total_time += iter_time;
    }
    
    uint64_t end_time = benchmark_get_time_ns();
    uint64_t total_time = end_time - start_time;
    
    metrics->avg_time = metrics->total_time / iterations;
    metrics->iterations = iterations;
    metrics->throughput = (double)total_packets / (total_time / 1000000000.0); // packets/sec
    metrics->memory_allocated = packet_size * 2;
    
    uesim_free(send_buffer);
    uesim_free(recv_buffer);
    
    printf("Socket operations benchmark completed\n");
    return UESIM_SUCCESS;
}

uesim_error_t benchmark_memory_allocation(benchmark_config_t* config, benchmark_metrics_t* metrics) {
    if (config == NULL || metrics == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("Running memory allocation benchmark...\n");
    
    uint64_t iterations = config->iterations > 0 ? config->iterations : BENCHMARK_DEFAULT_ITERATIONS;
    uint32_t buffer_size = config->buffer_size > 0 ? config->buffer_size : 1024;
    
    // Benchmark runs
    uint64_t start_time = benchmark_get_time_ns();
    metrics->min_time = UINT64_MAX;
    metrics->max_time = 0;
    metrics->total_time = 0;
    uint64_t total_allocations = 0;
    uint64_t peak_memory = 0;
    uint64_t current_memory = 0;
    
    void** buffers = (void**)uesim_malloc(iterations * sizeof(void*));
    if (buffers == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    for (uint64_t i = 0; i < iterations; i++) {
        uint64_t iter_start = benchmark_get_time_ns();
        
        // Simulate memory allocation
        buffers[i] = uesim_malloc(buffer_size);
        if (buffers[i] != NULL) {
            current_memory += buffer_size;
            if (current_memory > peak_memory) {
                peak_memory = current_memory;
            }
            total_allocations++;
            
            // Initialize allocated memory
            memset(buffers[i], 0xAA, buffer_size);
        }
        
        uint64_t iter_end = benchmark_get_time_ns();
        uint64_t iter_time = iter_end - iter_start;
        
        if (iter_time < metrics->min_time) metrics->min_time = iter_time;
        if (iter_time > metrics->max_time) metrics->max_time = iter_time;
        metrics->total_time += iter_time;
    }
    
    // Free allocated memory
    for (uint64_t i = 0; i < iterations && i < total_allocations; i++) {
        if (buffers[i] != NULL) {
            uesim_free(buffers[i]);
        }
    }
    
    uint64_t end_time = benchmark_get_time_ns();
    uint64_t total_time = end_time - start_time;
    
    metrics->avg_time = metrics->total_time / iterations;
    metrics->iterations = iterations;
    metrics->throughput = (double)total_allocations / (total_time / 1000000000.0); // allocations/sec
    metrics->memory_allocated = total_allocations * buffer_size;
    metrics->memory_peak = peak_memory;
    
    uesim_free(buffers);
    
    printf("Memory allocation benchmark completed\n");
    return UESIM_SUCCESS;
}

uesim_error_t benchmark_thread_operations(benchmark_config_t* config, benchmark_metrics_t* metrics) {
    if (config == NULL || metrics == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("Running thread operations benchmark...\n");
    
    uint64_t iterations = config->iterations > 0 ? config->iterations : BENCHMARK_DEFAULT_ITERATIONS;
    uint32_t thread_count = config->thread_count > 0 ? config->thread_count : 4;
    
    if (thread_count > BENCHMARK_MAX_THREADS) {
        thread_count = BENCHMARK_MAX_THREADS;
    }
    
    // Benchmark runs
    uint64_t start_time = benchmark_get_time_ns();
    metrics->min_time = UINT64_MAX;
    metrics->max_time = 0;
    metrics->total_time = 0;
    
    volatile atomic_int thread_counter = 0;
    
    for (uint64_t i = 0; i < iterations; i++) {
        uint64_t iter_start = benchmark_get_time_ns();
        
        // Simulate thread operations
        atomic_fetch_add(&thread_counter, thread_count);
        
        // Simulate thread synchronization
        for (uint32_t t = 0; t < thread_count; t++) {
            atomic_fetch_sub(&thread_counter, 1);
        }
        
        uint64_t iter_end = benchmark_get_time_ns();
        uint64_t iter_time = iter_end - iter_start;
        
        if (iter_time < metrics->min_time) metrics->min_time = iter_time;
        if (iter_time > metrics->max_time) metrics->max_time = iter_time;
        metrics->total_time += iter_time;
    }
    
    uint64_t end_time = benchmark_get_time_ns();
    uint64_t total_time = end_time - start_time;
    
    metrics->avg_time = metrics->total_time / iterations;
    metrics->iterations = iterations;
    metrics->throughput = (double)iterations / (total_time / 1000000000.0); // operations/sec
    
    printf("Thread operations benchmark completed\n");
    return UESIM_SUCCESS;
}

uesim_error_t benchmark_crypto_operations(benchmark_config_t* config, benchmark_metrics_t* metrics) {
    if (config == NULL || metrics == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("Running crypto operations benchmark...\n");
    
    uint64_t iterations = config->iterations > 0 ? config->iterations : BENCHMARK_DEFAULT_ITERATIONS;
    uint32_t data_size = config->packet_size > 0 ? config->packet_size : 128;
    
    // Allocate test buffers
    uint8_t* input_data = (uint8_t*)uesim_malloc(data_size);
    uint8_t* output_data = (uint8_t*)uesim_malloc(data_size);
    if (input_data == NULL || output_data == NULL) {
        if (input_data) uesim_free(input_data);
        if (output_data) uesim_free(output_data);
        return UESIM_ERROR_MEMORY;
    }
    
    // Initialize input data
    for (uint32_t i = 0; i < data_size; i++) {
        input_data[i] = (uint8_t)(i & 0xFF);
    }
    
    // Benchmark runs
    uint64_t start_time = benchmark_get_time_ns();
    metrics->min_time = UINT64_MAX;
    metrics->max_time = 0;
    metrics->total_time = 0;
    
    for (uint64_t i = 0; i < iterations; i++) {
        uint64_t iter_start = benchmark_get_time_ns();
        
        // Simulate crypto operations (AES, SHA, etc.)
        // This is a simplified simulation of cryptographic operations
        uint32_t crypto_result = 0;
        for (uint32_t j = 0; j < data_size; j++) {
            crypto_result ^= (input_data[j] << (j % 32));
            output_data[j] = input_data[j] ^ 0x55;
        }
        
        uint64_t iter_end = benchmark_get_time_ns();
        uint64_t iter_time = iter_end - iter_start;
        
        if (iter_time < metrics->min_time) metrics->min_time = iter_time;
        if (iter_time > metrics->max_time) metrics->max_time = iter_time;
        metrics->total_time += iter_time;
    }
    
    uint64_t end_time = benchmark_get_time_ns();
    uint64_t total_time = end_time - start_time;
    
    metrics->avg_time = metrics->total_time / iterations;
    metrics->iterations = iterations;
    metrics->throughput = (double)iterations / (total_time / 1000000000.0); // operations/sec
    metrics->memory_allocated = data_size * 2;
    
    uesim_free(input_data);
    uesim_free(output_data);
    
    printf("Crypto operations benchmark completed\n");
    return UESIM_SUCCESS;
}

// Utility functions
uint64_t benchmark_get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

uint64_t benchmark_get_memory_usage(void) {
#ifdef __linux__
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return (uint64_t)usage.ru_maxrss * 1024; // Convert KB to bytes
    }
#endif
    return 0;
}

double benchmark_get_cpu_usage(void) {
    // Simplified CPU usage estimation
    return 0.0;
}

char* benchmark_format_time(uint64_t ns, char* buffer, size_t buffer_size) {
    if (buffer == NULL || buffer_size == 0) {
        return NULL;
    }
    
    if (ns < 1000) {
        snprintf(buffer, buffer_size, "%lu ns", ns);
    } else if (ns < 1000000) {
        snprintf(buffer, buffer_size, "%.2f μs", ns / 1000.0);
    } else if (ns < 1000000000) {
        snprintf(buffer, buffer_size, "%.2f ms", ns / 1000000.0);
    } else {
        snprintf(buffer, buffer_size, "%.2f s", ns / 1000000000.0);
    }
    
    return buffer;
}

char* benchmark_format_throughput(double ops_per_sec, char* buffer, size_t buffer_size) {
    if (buffer == NULL || buffer_size == 0) {
        return NULL;
    }
    
    if (ops_per_sec < 1000) {
        snprintf(buffer, buffer_size, "%.2f ops/sec", ops_per_sec);
    } else if (ops_per_sec < 1000000) {
        snprintf(buffer, buffer_size, "%.2f Kops/sec", ops_per_sec / 1000.0);
    } else {
        snprintf(buffer, buffer_size, "%.2f Mops/sec", ops_per_sec / 1000000.0);
    }
    
    return buffer;
}

char* benchmark_format_memory(uint64_t bytes, char* buffer, size_t buffer_size) {
    if (buffer == NULL || buffer_size == 0) {
        return NULL;
    }
    
    if (bytes < 1024) {
        snprintf(buffer, buffer_size, "%lu B", bytes);
    } else if (bytes < 1048576) {
        snprintf(buffer, buffer_size, "%.2f KB", bytes / 1024.0);
    } else if (bytes < 1073741824) {
        snprintf(buffer, buffer_size, "%.2f MB", bytes / 1048576.0);
    } else {
        snprintf(buffer, buffer_size, "%.2f GB", bytes / 1073741824.0);
    }
    
    return buffer;
}

// Benchmark result management
uesim_error_t benchmark_result_init(benchmark_result_t* result) {
    if (result == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    memset(result, 0, sizeof(benchmark_result_t));
    return UESIM_SUCCESS;
}

uesim_error_t benchmark_result_copy(benchmark_result_t* dest, const benchmark_result_t* src) {
    if (dest == NULL || src == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    *dest = *src;
    return UESIM_SUCCESS;
}

uesim_error_t benchmark_result_merge(benchmark_result_t* dest, const benchmark_result_t* src) {
    if (dest == NULL || src == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Merge metrics (simplified)
    dest->metrics.min_time = (src->metrics.min_time < dest->metrics.min_time) ? 
                           src->metrics.min_time : dest->metrics.min_time;
    dest->metrics.max_time = (src->metrics.max_time > dest->metrics.max_time) ? 
                           src->metrics.max_time : dest->metrics.max_time;
    dest->metrics.total_time += src->metrics.total_time;
    dest->metrics.iterations += src->metrics.iterations;
    dest->metrics.memory_allocated += src->metrics.memory_allocated;
    dest->metrics.memory_peak = (src->metrics.memory_peak > dest->metrics.memory_peak) ? 
                              src->metrics.memory_peak : dest->metrics.memory_peak;
    
    return UESIM_SUCCESS;
}

void benchmark_result_print(const benchmark_result_t* result) {
    if (result == NULL) {
        return;
    }
    
    printf("Benchmark: %s\n", result->config.name);
    printf("Category: %s\n", benchmark_get_category_name(result->config.category));
    printf("Type: %s\n", benchmark_get_type_name(result->config.type));
    printf("Status: %s\n", result->success ? "SUCCESS" : "FAILED");
    
    if (!result->success) {
        printf("Error: %s\n", result->error_message);
        return;
    }
    
    printf("Iterations: %lu\n", result->metrics.iterations);
    printf("Duration: %lu seconds\n", result->end_time - result->start_time);
    
    char time_buffer[64];
    printf("Min Time: %s\n", benchmark_format_time(result->metrics.min_time, time_buffer, sizeof(time_buffer)));
    printf("Max Time: %s\n", benchmark_format_time(result->metrics.max_time, time_buffer, sizeof(time_buffer)));
    printf("Avg Time: %s\n", benchmark_format_time(result->metrics.avg_time, time_buffer, sizeof(time_buffer)));
    
    char throughput_buffer[64];
    printf("Throughput: %s\n", benchmark_format_throughput(result->metrics.throughput, throughput_buffer, sizeof(throughput_buffer)));
    
    char memory_buffer[64];
    printf("Memory Allocated: %s\n", benchmark_format_memory(result->metrics.memory_allocated, memory_buffer, sizeof(memory_buffer)));
    if (result->metrics.memory_peak > 0) {
        printf("Peak Memory: %s\n", benchmark_format_memory(result->metrics.memory_peak, memory_buffer, sizeof(memory_buffer)));
    }
    
    if (result->metrics.cpu_utilization > 0) {
        printf("CPU Utilization: %.2f%%\n", result->metrics.cpu_utilization);
    }
    
    if (result->metrics.errors > 0) {
        printf("Errors: %lu\n", result->metrics.errors);
    }
}

void benchmark_result_print_summary(const benchmark_result_t* result) {
    if (result == NULL || !result->success) {
        return;
    }
    
    char time_buffer[64];
    char throughput_buffer[64];
    printf("  Avg Time: %s, Throughput: %s\n", 
           benchmark_format_time(result->metrics.avg_time, time_buffer, sizeof(time_buffer)),
           benchmark_format_throughput(result->metrics.throughput, throughput_buffer, sizeof(throughput_buffer)));
}

// Benchmark configuration helpers
uesim_error_t benchmark_config_init(benchmark_config_t* config) {
    if (config == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    memset(config, 0, sizeof(benchmark_config_t));
    return UESIM_SUCCESS;
}

uesim_error_t benchmark_config_set_defaults(benchmark_config_t* config) {
    if (config == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    config->iterations = BENCHMARK_DEFAULT_ITERATIONS;
    config->thread_count = 1;
    config->warmup = true;
    config->warmup_iterations = 1000;
    config->measure_memory = true;
    config->measure_cpu = false;
    config->duration_seconds = 0;
    config->time_based = false;
    config->packet_size = 1024;
    config->buffer_size = 1024;
    
    return UESIM_SUCCESS;
}

uesim_error_t benchmark_config_validate(const benchmark_config_t* config) {
    if (config == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (config->iterations > BENCHMARK_MAX_ITERATIONS) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (config->thread_count > BENCHMARK_MAX_THREADS) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    return UESIM_SUCCESS;
}

void benchmark_config_print(const benchmark_config_t* config) {
    if (config == NULL) {
        return;
    }
    
    printf("Benchmark Configuration:\n");
    printf("  Name: %s\n", config->name);
    printf("  Category: %s\n", benchmark_get_category_name(config->category));
    printf("  Type: %s\n", benchmark_get_type_name(config->type));
    printf("  Iterations: %lu\n", config->iterations);
    printf("  Thread Count: %u\n", config->thread_count);
    printf("  Warmup: %s\n", config->warmup ? "Yes" : "No");
    printf("  Warmup Iterations: %u\n", config->warmup_iterations);
    printf("  Measure Memory: %s\n", config->measure_memory ? "Yes" : "No");
    printf("  Measure CPU: %s\n", config->measure_cpu ? "Yes" : "No");
    printf("  Time Based: %s\n", config->time_based ? "Yes" : "No");
    printf("  Duration: %u seconds\n", config->duration_seconds);
    printf("  Packet Size: %u bytes\n", config->packet_size);
    printf("  Buffer Size: %u bytes\n", config->buffer_size);
}

// Predefined benchmark configurations
benchmark_config_t benchmark_get_default_config(benchmark_type_t type, benchmark_category_t category) {
    benchmark_config_t config = {0};
    
    config.type = type;
    config.category = category;
    config.iterations = BENCHMARK_DEFAULT_ITERATIONS;
    config.thread_count = 1;
    config.warmup = true;
    config.warmup_iterations = 1000;
    config.measure_memory = true;
    config.measure_cpu = false;
    config.packet_size = 1024;
    config.buffer_size = 1024;
    
    switch (category) {
        case BENCHMARK_CATEGORY_PDCP:
            strncpy(config.name, "PDCP Default", BENCHMARK_MAX_NAME_LEN - 1);
            break;
        case BENCHMARK_CATEGORY_RLC:
            strncpy(config.name, "RLC Default", BENCHMARK_MAX_NAME_LEN - 1);
            break;
        case BENCHMARK_CATEGORY_MAC:
            strncpy(config.name, "MAC Default", BENCHMARK_MAX_NAME_LEN - 1);
            break;
        case BENCHMARK_CATEGORY_NAS:
            strncpy(config.name, "NAS Default", BENCHMARK_MAX_NAME_LEN - 1);
            break;
        case BENCHMARK_CATEGORY_RRC:
            strncpy(config.name, "RRC Default", BENCHMARK_MAX_NAME_LEN - 1);
            break;
        case BENCHMARK_CATEGORY_SOCKET:
            strncpy(config.name, "Socket Default", BENCHMARK_MAX_NAME_LEN - 1);
            break;
        case BENCHMARK_CATEGORY_MEMORY:
            strncpy(config.name, "Memory Default", BENCHMARK_MAX_NAME_LEN - 1);
            break;
        case BENCHMARK_CATEGORY_THREAD:
            strncpy(config.name, "Thread Default", BENCHMARK_MAX_NAME_LEN - 1);
            break;
        case BENCHMARK_CATEGORY_CRYPTO:
            strncpy(config.name, "Crypto Default", BENCHMARK_MAX_NAME_LEN - 1);
            break;
        default:
            strncpy(config.name, "Default", BENCHMARK_MAX_NAME_LEN - 1);
            break;
    }
    
    config.name[BENCHMARK_MAX_NAME_LEN - 1] = '\0';
    
    return config;
}

benchmark_config_t benchmark_get_stress_config(benchmark_type_t type, benchmark_category_t category) {
    benchmark_config_t config = benchmark_get_default_config(type, category);
    
    config.iterations = BENCHMARK_MAX_ITERATIONS;
    config.thread_count = 8;
    config.warmup_iterations = 10000;
    config.packet_size = 8192;
    config.buffer_size = 8192;
    
    // Append " Stress" to name
    char stress_suffix[] = " Stress";
    size_t name_len = strlen(config.name);
    if (name_len + strlen(stress_suffix) < BENCHMARK_MAX_NAME_LEN) {
        strcat(config.name, stress_suffix);
    }
    
    return config;
}

benchmark_config_t benchmark_get_performance_config(benchmark_type_t type, benchmark_category_t category) {
    benchmark_config_t config = benchmark_get_default_config(type, category);
    
    config.iterations = 100000;
    config.thread_count = 4;
    config.warmup_iterations = 5000;
    config.packet_size = 2048;
    config.buffer_size = 2048;
    
    // Append " Performance" to name
    char perf_suffix[] = " Performance";
    size_t name_len = strlen(config.name);
    if (name_len + strlen(perf_suffix) < BENCHMARK_MAX_NAME_LEN) {
        strcat(config.name, perf_suffix);
    }
    
    return config;
}

// Benchmark categories
const char* benchmark_get_type_name(benchmark_type_t type) {
    switch (type) {
        case BENCHMARK_TYPE_LATENCY: return "Latency";
        case BENCHMARK_TYPE_THROUGHPUT: return "Throughput";
        case BENCHMARK_TYPE_MEMORY: return "Memory";
        case BENCHMARK_TYPE_CPU: return "CPU";
        case BENCHMARK_TYPE_CONCURRENCY: return "Concurrency";
        case BENCHMARK_TYPE_PROTOCOL: return "Protocol";
        case BENCHMARK_TYPE_SECURITY: return "Security";
        default: return "Unknown";
    }
}

const char* benchmark_get_category_name(benchmark_category_t category) {
    switch (category) {
        case BENCHMARK_CATEGORY_PDCP: return "PDCP";
        case BENCHMARK_CATEGORY_RLC: return "RLC";
        case BENCHMARK_CATEGORY_MAC: return "MAC";
        case BENCHMARK_CATEGORY_NAS: return "NAS";
        case BENCHMARK_CATEGORY_RRC: return "RRC";
        case BENCHMARK_CATEGORY_SOCKET: return "Socket";
        case BENCHMARK_CATEGORY_MEMORY: return "Memory";
        case BENCHMARK_CATEGORY_THREAD: return "Thread";
        case BENCHMARK_CATEGORY_CRYPTO: return "Crypto";
        default: return "Unknown";
    }
}

benchmark_category_t benchmark_get_category_from_string(const char* category_str) {
    if (category_str == NULL) {
        return BENCHMARK_CATEGORY_MAX;
    }
    
    struct category_map {
        const char* name;
        benchmark_category_t category;
    } categories[] = {
        {"pdcp", BENCHMARK_CATEGORY_PDCP},
        {"rlc", BENCHMARK_CATEGORY_RLC},
        {"mac", BENCHMARK_CATEGORY_MAC},
        {"nas", BENCHMARK_CATEGORY_NAS},
        {"rrc", BENCHMARK_CATEGORY_RRC},
        {"socket", BENCHMARK_CATEGORY_SOCKET},
        {"memory", BENCHMARK_CATEGORY_MEMORY},
        {"thread", BENCHMARK_CATEGORY_THREAD},
        {"crypto", BENCHMARK_CATEGORY_CRYPTO}
    };
    
    for (int i = 0; i < sizeof(categories) / sizeof(categories[0]); i++) {
        if (strcasecmp(category_str, categories[i].name) == 0) {
            return categories[i].category;
        }
    }
    
    return BENCHMARK_CATEGORY_MAX;
}

benchmark_type_t benchmark_get_type_from_string(const char* type_str) {
    if (type_str == NULL) {
        return BENCHMARK_TYPE_MAX;
    }
    
    struct type_map {
        const char* name;
        benchmark_type_t type;
    } types[] = {
        {"latency", BENCHMARK_TYPE_LATENCY},
        {"throughput", BENCHMARK_TYPE_THROUGHPUT},
        {"memory", BENCHMARK_TYPE_MEMORY},
        {"cpu", BENCHMARK_TYPE_CPU},
        {"concurrency", BENCHMARK_TYPE_CONCURRENCY},
        {"protocol", BENCHMARK_TYPE_PROTOCOL},
        {"security", BENCHMARK_TYPE_SECURITY}
    };
    
    for (int i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        if (strcasecmp(type_str, types[i].name) == 0) {
            return types[i].type;
        }
    }
    
    return BENCHMARK_TYPE_MAX;
}

// Benchmark reporting
uesim_error_t benchmark_generate_report(const benchmark_suite_t* suite, const char* filename) {
    if (suite == NULL || filename == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        return UESIM_ERROR_FILE;
    }
    
    fprintf(file, "5G UE Simulation Benchmark Report\n");
    fprintf(file, "==================================\n");
    fprintf(file, "Suite: %s\n", suite->name);
    fprintf(file, "Start Time: %s", ctime(&suite->start_time));
    fprintf(file, "End Time: %s", ctime(&suite->end_time));
    fprintf(file, "Total Benchmarks: %zu\n", suite->result_count);
    fprintf(file, "Status: %s\n", suite->success ? "SUCCESS" : "FAILED");
    fprintf(file, "\n");
    
    for (size_t i = 0; i < suite->result_count; i++) {
        const benchmark_result_t* result = &suite->results[i];
        
        fprintf(file, "Benchmark %zu: %s\n", i + 1, result->config.name);
        fprintf(file, "  Category: %s\n", benchmark_get_category_name(result->config.category));
        fprintf(file, "  Type: %s\n", benchmark_get_type_name(result->config.type));
        fprintf(file, "  Status: %s\n", result->success ? "SUCCESS" : "FAILED");
        
        if (result->success) {
            char time_buffer[64];
            char throughput_buffer[64];
            char memory_buffer[64];
            
            fprintf(file, "  Iterations: %lu\n", result->metrics.iterations);
            fprintf(file, "  Duration: %lu seconds\n", result->end_time - result->start_time);
            fprintf(file, "  Min Time: %s\n", benchmark_format_time(result->metrics.min_time, time_buffer, sizeof(time_buffer)));
            fprintf(file, "  Max Time: %s\n", benchmark_format_time(result->metrics.max_time, time_buffer, sizeof(time_buffer)));
            fprintf(file, "  Avg Time: %s\n", benchmark_format_time(result->metrics.avg_time, time_buffer, sizeof(time_buffer)));
            fprintf(file, "  Throughput: %s\n", benchmark_format_throughput(result->metrics.throughput, throughput_buffer, sizeof(throughput_buffer)));
            fprintf(file, "  Memory Allocated: %s\n", benchmark_format_memory(result->metrics.memory_allocated, memory_buffer, sizeof(memory_buffer)));
            if (result->metrics.memory_peak > 0) {
                fprintf(file, "  Peak Memory: %s\n", benchmark_format_memory(result->metrics.memory_peak, memory_buffer, sizeof(memory_buffer)));
            }
        } else {
            fprintf(file, "  Error: %s\n", result->error_message);
        }
        
        fprintf(file, "\n");
    }
    
    fclose(file);
    printf("Benchmark report generated: %s\n", filename);
    return UESIM_SUCCESS;
}

uesim_error_t benchmark_export_csv(const benchmark_suite_t* suite, const char* filename) {
    if (suite == NULL || filename == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        return UESIM_ERROR_FILE;
    }
    
    // Write CSV header
    fprintf(file, "Benchmark,Category,Type,Status,Iterations,Duration,MinTime,MaxTime,AvgTime,Throughput,MemoryAllocated,PeakMemory,Errors\n");
    
    // Write benchmark results
    for (size_t i = 0; i < suite->result_count; i++) {
        const benchmark_result_t* result = &suite->results[i];
        
        fprintf(file, "\"%s\",", result->config.name);
        fprintf(file, "\"%s\",", benchmark_get_category_name(result->config.category));
        fprintf(file, "\"%s\",", benchmark_get_type_name(result->config.type));
        fprintf(file, "\"%s\",", result->success ? "SUCCESS" : "FAILED");
        
        if (result->success) {
            fprintf(file, "%lu,", result->metrics.iterations);
            fprintf(file, "%ld,", result->end_time - result->start_time);
            fprintf(file, "%lu,", result->metrics.min_time);
            fprintf(file, "%lu,", result->metrics.max_time);
            fprintf(file, "%lu,", result->metrics.avg_time);
            fprintf(file, "%.2f,", result->metrics.throughput);
            fprintf(file, "%lu,", result->metrics.memory_allocated);
            fprintf(file, "%lu,", result->metrics.memory_peak);
            fprintf(file, "%lu", result->metrics.errors);
        } else {
            fprintf(file, "0,0,0,0,0,0.0,0,0,0");
        }
        
        fprintf(file, "\n");
    }
    
    fclose(file);
    printf("CSV export generated: %s\n", filename);
    return UESIM_SUCCESS;
}

uesim_error_t benchmark_export_json(const benchmark_suite_t* suite, const char* filename) {
    if (suite == NULL || filename == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        return UESIM_ERROR_FILE;
    }
    
    // Write JSON header
    fprintf(file, "{\n");
    fprintf(file, "  \"suite\": \"%s\",\n", suite->name);
    fprintf(file, "  \"startTime\": %ld,\n", suite->start_time);
    fprintf(file, "  \"endTime\": %ld,\n", suite->end_time);
    fprintf(file, "  \"totalBenchmarks\": %zu,\n", suite->result_count);
    fprintf(file, "  \"success\": %s,\n", suite->success ? "true" : "false");
    fprintf(file, "  \"benchmarks\": [\n");
    
    // Write benchmark results
    for (size_t i = 0; i < suite->result_count; i++) {
        const benchmark_result_t* result = &suite->results[i];
        
        fprintf(file, "    {\n");
        fprintf(file, "      \"name\": \"%s\",\n", result->config.name);
        fprintf(file, "      \"category\": \"%s\",\n", benchmark_get_category_name(result->config.category));
        fprintf(file, "      \"type\": \"%s\",\n", benchmark_get_type_name(result->config.type));
        fprintf(file, "      \"success\": %s,\n", result->success ? "true" : "false");
        
        if (result->success) {
            fprintf(file, "      \"iterations\": %lu,\n", result->metrics.iterations);
            fprintf(file, "      \"duration\": %ld,\n", result->end_time - result->start_time);
            fprintf(file, "      \"minTime\": %lu,\n", result->metrics.min_time);
            fprintf(file, "      \"maxTime\": %lu,\n", result->metrics.max_time);
            fprintf(file, "      \"avgTime\": %lu,\n", result->metrics.avg_time);
            fprintf(file, "      \"throughput\": %.2f,\n", result->metrics.throughput);
            fprintf(file, "      \"memoryAllocated\": %lu,\n", result->metrics.memory_allocated);
            fprintf(file, "      \"memoryPeak\": %lu,\n", result->metrics.memory_peak);
            fprintf(file, "      \"errors\": %lu\n", result->metrics.errors);
        } else {
            fprintf(file, "      \"error\": \"%s\"\n", result->error_message);
        }
        
        fprintf(file, "    }%s\n", (i == suite->result_count - 1) ? "" : ",");
    }
    
    fprintf(file, "  ]\n");
    fprintf(file, "}\n");
    
    fclose(file);
    printf("JSON export generated: %s\n", filename);
    return UESIM_SUCCESS;
}