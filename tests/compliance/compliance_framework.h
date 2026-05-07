/*
 * 5G UE Simulation Application
 * 3GPP Compliance Test Framework Header
 * 
 * Provides infrastructure for testing compliance with 3GPP specifications:
 * - TS 38.331 (RRC)
 * - TS 38.323 (PDCP)
 * - TS 38.322 (RLC)
 * - TS 38.321 (MAC)
 * - TS 24.501 (NAS)
 */

#ifndef COMPLIANCE_FRAMEWORK_H
#define COMPLIANCE_FRAMEWORK_H

#include "../../src/uesim.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Compliance test result codes
typedef enum {
    COMPLIANCE_PASS = 0,
    COMPLIANCE_FAIL = 1,
    COMPLIANCE_SKIP = 2,
    COMPLIANCE_ERROR = 3,
    COMPLIANCE_NOT_IMPLEMENTED = 4
} compliance_result_t;

// 3GPP specification identifiers
typedef enum {
    SPEC_TS_38_331 = 0,    // RRC
    SPEC_TS_38_323 = 1,    // PDCP
    SPEC_TS_38_322 = 2,    // RLC
    SPEC_TS_38_321 = 3,    // MAC
    SPEC_TS_24_501 = 4,    // NAS
    SPEC_TS_38_300 = 5,    // Overall NR
    SPEC_TS_38_401 = 6,    // NG-RAN
    SPEC_MAX
} compliance_spec_t;

// Test severity levels
typedef enum {
    SEVERITY_MANDATORY = 0,    // Must pass for compliance
    SEVERITY_CONDITIONAL = 1,   // Required if feature supported
    SEVERITY_OPTIONAL = 2       // Recommended but not required
} compliance_severity_t;

// Test category
typedef enum {
    CATEGORY_PROTOCOL = 0,     // Protocol behavior
    CATEGORY_ENCODING = 1,     // Message encoding/decoding
    CATEGORY_STATE = 2,        // State machine behavior
    CATEGORY_TIMER = 3,        // Timer behavior
    CATEGORY_SECURITY = 4,     // Security procedures
    CATEGORY_ERROR = 5         // Error handling
} compliance_category_t;

// Individual test case
typedef struct {
    const char* test_id;           // Unique test identifier (e.g., "RRC-001")
    const char* test_name;         // Human-readable test name
    const char* description;       // Detailed test description
    compliance_spec_t specification; // 3GPP specification
    compliance_severity_t severity;  // Test severity
    compliance_category_t category;  // Test category
    const char* section_ref;       // 3GPP spec section reference
    compliance_result_t (*test_func)(void* context);  // Test function
} compliance_test_case_t;

// Test execution result
typedef struct {
    const char* test_id;
    compliance_result_t result;
    const char* message;          // Result message
    uint32_t duration_ms;         // Execution time
    time_t timestamp;             // When test was executed
    const char* details;          // Additional details
} compliance_test_result_t;

// Test suite statistics
typedef struct {
    uint32_t total_tests;
    uint32_t passed;
    uint32_t failed;
    uint32_t skipped;
    uint32_t errors;
    uint32_t not_implemented;
    uint32_t duration_ms;
} compliance_stats_t;

// Test suite configuration
typedef struct {
    compliance_spec_t specification;  // Filter by spec (SPEC_MAX = all)
    compliance_severity_t min_severity; // Minimum severity to run
    compliance_category_t category;    // Filter by category (all if -1)
    bool stop_on_failure;             // Stop suite on first failure
    bool verbose;                     // Verbose output
    const char* output_file;          // Output file for results
} compliance_config_t;

// Test suite context
typedef struct {
    compliance_config_t config;
    compliance_test_case_t* test_cases;
    uint32_t num_test_cases;
    compliance_test_result_t* results;
    compliance_stats_t stats;
    void* user_context;              // User-provided context
} compliance_suite_t;

// Framework initialization
uesim_error_t compliance_framework_init(void);
void compliance_framework_cleanup(void);

// Test suite management
uesim_error_t compliance_create_suite(compliance_suite_t** suite, 
                                      const compliance_config_t* config);
uesim_error_t compliance_destroy_suite(compliance_suite_t* suite);
uesim_error_t compliance_register_test(compliance_suite_t* suite,
                                       const compliance_test_case_t* test_case);
uesim_error_t compliance_run_suite(compliance_suite_t* suite);
uesim_error_t compliance_run_single_test(compliance_suite_t* suite,
                                         const char* test_id);

// Result reporting
uesim_error_t compliance_generate_report(compliance_suite_t* suite,
                                        const char* format,  // "text", "json", "html"
                                        const char* output_path);
uesim_error_t compliance_print_results(compliance_suite_t* suite);
uesim_error_t compliance_get_stats(compliance_suite_t* suite,
                                   compliance_stats_t* stats);

// Utility functions
const char* compliance_result_to_string(compliance_result_t result);
const char* compliance_spec_to_string(compliance_spec_t spec);
const char* compliance_severity_to_string(compliance_severity_t severity);
const char* compliance_category_to_string(compliance_category_t category);

// Assertion macros for test implementations
#define COMPLIANCE_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            snprintf(compliance_message_buffer, sizeof(compliance_message_buffer), \
                     "Assertion failed: %s at %s:%d", message, __FILE__, __LINE__); \
            return COMPLIANCE_FAIL; \
        } \
    } while(0)

#define COMPLIANCE_ASSERT_EQ(expected, actual, message) \
    do { \
        if ((expected) != (actual)) { \
            snprintf(compliance_message_buffer, sizeof(compliance_message_buffer), \
                     "Expected %d, got %d: %s at %s:%d", (int)(expected), (int)(actual), \
                     message, __FILE__, __LINE__); \
            return COMPLIANCE_FAIL; \
        } \
    } while(0)

#define COMPLIANCE_ASSERT_NE(not_expected, actual, message) \
    do { \
        if ((not_expected) == (actual)) { \
            snprintf(compliance_message_buffer, sizeof(compliance_message_buffer), \
                     "Value should not be %d: %s at %s:%d", (int)(actual), \
                     message, __FILE__, __LINE__); \
            return COMPLIANCE_FAIL; \
        } \
    } while(0)

#define COMPLIANCE_ASSERT_NULL(ptr, message) \
    do { \
        if ((ptr) != NULL) { \
            snprintf(compliance_message_buffer, sizeof(compliance_message_buffer), \
                     "Expected NULL: %s at %s:%d", message, __FILE__, __LINE__); \
            return COMPLIANCE_FAIL; \
        } \
    } while(0)

#define COMPLIANCE_ASSERT_NOT_NULL(ptr, message) \
    do { \
        if ((ptr) == NULL) { \
            snprintf(compliance_message_buffer, sizeof(compliance_message_buffer), \
                     "Expected non-NULL: %s at %s:%d", message, __FILE__, __LINE__); \
            return COMPLIANCE_FAIL; \
        } \
    } while(0)

// Global message buffer for assertions
extern char compliance_message_buffer[1024];

#endif // COMPLIANCE_FRAMEWORK_H