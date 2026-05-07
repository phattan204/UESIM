/*
 * 5G UE Simulation Application
 * Conformance Test Harness Header
 * 
 * Provides infrastructure for GCF/PTCRB conformance testing
 */

#ifndef CONFORMANCE_HARNESS_H
#define CONFORMANCE_HARNESS_H

#include "../../src/uesim.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Conformance test result codes
typedef enum {
    CONF_RESULT_PASS = 0,
    CONF_RESULT_FAIL = 1,
    CONF_RESULT_INCONCLUSIVE = 2,
    CONF_RESULT_NOT_SUPPORTED = 3,
    CONF_RESULT_BLOCKED = 4
} conformance_result_t;

// Test execution status
typedef enum {
    CONF_STATUS_PENDING = 0,
    CONF_STATUS_RUNNING = 1,
    CONF_STATUS_COMPLETED = 2,
    CONF_STATUS_ABORTED = 3
} conformance_status_t;

// Test category
typedef enum {
    CONF_CAT_REGISTRATION = 0,
    CONF_CAT_AUTHENTICATION = 1,
    CONF_CAT_PDU_SESSION = 2,
    CONF_CAT_HANDOVER = 3,
    CONF_CAT_SECURITY = 4,
    CONF_CAT_IDLE_MODE = 5,
    CONF_CAT_CONNECTION_MANAGEMENT = 6
} conformance_category_t;

// Test priority
typedef enum {
    CONF_PRIORITY_HIGH = 0,
    CONF_PRIORITY_MEDIUM = 1,
    CONF_PRIORITY_LOW = 2
} conformance_priority_t;

// Test case definition
typedef struct {
    const char* tc_id;                    // Test case ID (e.g., "REG-001")
    const char* tc_name;                  // Test case name
    const char* description;               // Detailed description
    conformance_category_t category;      // Test category
    conformance_priority_t priority;      // Test priority
    const char* spec_ref;                  // 3GPP spec reference
    const char* pre_conditions;           // Pre-conditions for test
    const char* post_conditions;          // Expected post-conditions
    const char* test_steps;               // Test step description
    conformance_result_t (*execute)(void* context);  // Test function
} conformance_test_case_t;

// Test execution result
typedef struct {
    const char* tc_id;
    conformance_result_t result;
    conformance_status_t status;
    const char* message;
    uint32_t duration_ms;
    time_t start_time;
    time_t end_time;
    const char* failure_reason;
    uint32_t step_number;                 // Step where failure occurred
} conformance_test_result_t;

// Test suite statistics
typedef struct {
    uint32_t total_tests;
    uint32_t passed;
    uint32_t failed;
    uint32_t inconclusive;
    uint32_t not_supported;
    uint32_t blocked;
    uint32_t total_duration_ms;
    uint32_t pass_rate;                   // Percentage
} conformance_stats_t;

// Test harness configuration
typedef struct {
    conformance_category_t category;      // Filter by category (-1 for all)
    conformance_priority_t min_priority;  // Minimum priority to run
    bool stop_on_failure;                 // Stop on first failure
    bool verbose;                         // Verbose output
    bool generate_report;                 // Generate report file
    const char* report_path;              // Report output path
    const char* report_format;            // "json", "html", "text"
    uint32_t timeout_ms;                  // Test timeout in ms
} conformance_config_t;

// Test harness context
typedef struct {
    conformance_config_t config;
    conformance_test_case_t* test_cases;
    uint32_t num_test_cases;
    conformance_test_result_t* results;
    conformance_stats_t stats;
    void* user_context;
    conformance_status_t status;
    uint32_t current_test_index;
} conformance_harness_t;

// Test vector structure
typedef struct {
    const char* vector_id;
    const char* description;
    uint8_t* input_data;
    size_t input_length;
    uint8_t* expected_output;
    size_t expected_output_length;
    const char* spec_ref;
} conformance_test_vector_t;

// Harness initialization
uesim_error_t conformance_harness_init(void);
void conformance_harness_cleanup(void);

// Harness management
uesim_error_t conformance_create_harness(conformance_harness_t** harness,
                                        const conformance_config_t* config);
uesim_error_t conformance_destroy_harness(conformance_harness_t* harness);
uesim_error_t conformance_register_test(conformance_harness_t* harness,
                                        const conformance_test_case_t* test_case);
uesim_error_t conformance_register_tests(conformance_harness_t* harness,
                                         const conformance_test_case_t* test_cases,
                                         uint32_t count);

// Test execution
uesim_error_t conformance_run_all_tests(conformance_harness_t* harness);
uesim_error_t conformance_run_test(conformance_harness_t* harness,
                                   const char* tc_id);
uesim_error_t conformance_run_category(conformance_harness_t* harness,
                                       conformance_category_t category);
uesim_error_t conformance_abort(conformance_harness_t* harness);

// Result reporting
uesim_error_t conformance_generate_report(conformance_harness_t* harness);
uesim_error_t conformance_print_results(conformance_harness_t* harness);
uesim_error_t conformance_print_summary(conformance_harness_t* harness);
uesim_error_t conformance_get_stats(conformance_harness_t* harness,
                                   conformance_stats_t* stats);

// Test vector management
uesim_error_t conformance_load_test_vectors(const char* path,
                                           conformance_test_vector_t** vectors,
                                           uint32_t* count);
uesim_error_t conformance_free_test_vectors(conformance_test_vector_t* vectors,
                                           uint32_t count);

// Utility functions
const char* conformance_result_to_string(conformance_result_t result);
const char* conformance_status_to_string(conformance_status_t status);
const char* conformance_category_to_string(conformance_category_t category);
const char* conformance_priority_to_string(conformance_priority_t priority);

// Assertion macros for test implementations
extern char conformance_message_buffer[2048];

#define CONF_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            snprintf(conformance_message_buffer, sizeof(conformance_message_buffer), \
                     "Assertion failed: %s at %s:%d", message, __FILE__, __LINE__); \
            return CONF_RESULT_FAIL; \
        } \
    } while(0)

#define CONF_ASSERT_EQ(expected, actual, message) \
    do { \
        if ((expected) != (actual)) { \
            snprintf(conformance_message_buffer, sizeof(conformance_message_buffer), \
                     "Expected %d, got %d: %s at %s:%d", (int)(expected), (int)(actual), \
                     message, __FILE__, __LINE__); \
            return CONF_RESULT_FAIL; \
        } \
    } while(0)

#define CONF_ASSERT_STR_EQ(expected, actual, message) \
    do { \
        if (strcmp((expected), (actual)) != 0) { \
            snprintf(conformance_message_buffer, sizeof(conformance_message_buffer), \
                     "Expected '%s', got '%s': %s at %s:%d", expected, actual, \
                     message, __FILE__, __LINE__); \
            return CONF_RESULT_FAIL; \
        } \
    } while(0)

#define CONF_ASSERT_NOT_NULL(ptr, message) \
    do { \
        if ((ptr) == NULL) { \
            snprintf(conformance_message_buffer, sizeof(conformance_message_buffer), \
                     "Expected non-NULL: %s at %s:%d", message, __FILE__, __LINE__); \
            return CONF_RESULT_FAIL; \
        } \
    } while(0)

#define CONF_ASSERT_MEM_EQ(expected, actual, len, message) \
    do { \
        if (memcmp((expected), (actual), (len)) != 0) { \
            snprintf(conformance_message_buffer, sizeof(conformance_message_buffer), \
                     "Memory comparison failed: %s at %s:%d", message, __FILE__, __LINE__); \
            return CONF_RESULT_FAIL; \
        } \
    } while(0)

#define CONF_LOG_STEP(step_num, message) \
    printf("  Step %u: %s\n", step_num, message)

#define CONF_LOG_INFO(message) \
    printf("  INFO: %s\n", message)

#endif // CONFORMANCE_HARNESS_H