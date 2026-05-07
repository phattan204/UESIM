/*
 * 5G UE Simulation Application
 * 3GPP Compliance Test Framework Implementation
 */

#include "compliance_framework.h"
#include "../../src/core/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global message buffer for assertions
char compliance_message_buffer[1024] = {0};

// String conversion tables
static const char* result_strings[] = {
    "PASS",
    "FAIL",
    "SKIP",
    "ERROR",
    "NOT_IMPLEMENTED"
};

static const char* spec_strings[] = {
    "TS 38.331 (RRC)",
    "TS 38.323 (PDCP)",
    "TS 38.322 (RLC)",
    "TS 38.321 (MAC)",
    "TS 24.501 (NAS)",
    "TS 38.300 (NR)",
    "TS 38.401 (NG-RAN)"
};

static const char* severity_strings[] = {
    "MANDATORY",
    "CONDITIONAL",
    "OPTIONAL"
};

static const char* category_strings[] = {
    "PROTOCOL",
    "ENCODING",
    "STATE",
    "TIMER",
    "SECURITY",
    "ERROR"
};

uesim_error_t compliance_framework_init(void) {
    printf("3GPP Compliance Test Framework initialized\n");
    return UESIM_SUCCESS;
}

void compliance_framework_cleanup(void) {
    printf("3GPP Compliance Test Framework cleanup completed\n");
}

uesim_error_t compliance_create_suite(compliance_suite_t** suite, 
                                      const compliance_config_t* config) {
    if (suite == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    compliance_suite_t* new_suite = (compliance_suite_t*)uesim_calloc(1, sizeof(compliance_suite_t));
    if (new_suite == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    if (config != NULL) {
        new_suite->config = *config;
    } else {
        // Default configuration
        new_suite->config.specification = SPEC_MAX;
        new_suite->config.min_severity = SEVERITY_MANDATORY;
        new_suite->config.category = -1;
        new_suite->config.stop_on_failure = false;
        new_suite->config.verbose = true;
        new_suite->config.output_file = NULL;
    }
    
    new_suite->test_cases = NULL;
    new_suite->num_test_cases = 0;
    new_suite->results = NULL;
    memset(&new_suite->stats, 0, sizeof(compliance_stats_t));
    new_suite->user_context = NULL;
    
    *suite = new_suite;
    
    printf("Compliance test suite created\n");
    return UESIM_SUCCESS;
}

uesim_error_t compliance_destroy_suite(compliance_suite_t* suite) {
    if (suite == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (suite->test_cases != NULL) {
        uesim_free(suite->test_cases);
    }
    
    if (suite->results != NULL) {
        uesim_free(suite->results);
    }
    
    uesim_free(suite);
    
    return UESIM_SUCCESS;
}

uesim_error_t compliance_register_test(compliance_suite_t* suite,
                                       const compliance_test_case_t* test_case) {
    if (suite == NULL || test_case == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Reallocate test cases array
    uint32_t new_count = suite->num_test_cases + 1;
    compliance_test_case_t* new_cases = (compliance_test_case_t*)uesim_realloc(
        suite->test_cases, 
        new_count * sizeof(compliance_test_case_t));
    
    if (new_cases == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    suite->test_cases = new_cases;
    suite->test_cases[suite->num_test_cases] = *test_case;
    suite->num_test_cases = new_count;
    
    // Reallocate results array
    compliance_test_result_t* new_results = (compliance_test_result_t*)uesim_realloc(
        suite->results,
        new_count * sizeof(compliance_test_result_t));
    
    if (new_results == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    suite->results = new_results;
    memset(&suite->results[suite->num_test_cases - 1], 0, sizeof(compliance_test_result_t));
    
    return UESIM_SUCCESS;
}

uesim_error_t compliance_run_single_test(compliance_suite_t* suite,
                                         const char* test_id) {
    if (suite == NULL || test_id == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Find test case
    int32_t index = -1;
    for (uint32_t i = 0; i < suite->num_test_cases; i++) {
        if (strcmp(suite->test_cases[i].test_id, test_id) == 0) {
            index = i;
            break;
        }
    }
    
    if (index < 0) {
        printf("Test case not found: %s\n", test_id);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    compliance_test_case_t* tc = &suite->test_cases[index];
    compliance_test_result_t* result = &suite->results[index];
    
    // Check filters
    if (suite->config.specification != SPEC_MAX && 
        tc->specification != suite->config.specification) {
        result->result = COMPLIANCE_SKIP;
        result->message = "Skipped: specification filter";
        return UESIM_SUCCESS;
    }
    
    if (tc->severity < suite->config.min_severity) {
        result->result = COMPLIANCE_SKIP;
        result->message = "Skipped: severity filter";
        return UESIM_SUCCESS;
    }
    
    // Execute test
    result->test_id = tc->test_id;
    result->timestamp = time(NULL);
    compliance_message_buffer[0] = '\0';
    
    clock_t start = clock();
    result->result = tc->test_func(suite->user_context);
    clock_t end = clock();
    
    result->duration_ms = (uint32_t)((end - start) * 1000 / CLOCKS_PER_SEC);
    result->message = compliance_message_buffer[0] ? 
                      strdup(compliance_message_buffer) : "No message";
    
    // Update statistics
    suite->stats.total_tests++;
    switch (result->result) {
        case COMPLIANCE_PASS: suite->stats.passed++; break;
        case COMPLIANCE_FAIL: suite->stats.failed++; break;
        case COMPLIANCE_SKIP: suite->stats.skipped++; break;
        case COMPLIANCE_ERROR: suite->stats.errors++; break;
        case COMPLIANCE_NOT_IMPLEMENTED: suite->stats.not_implemented++; break;
    }
    suite->stats.duration_ms += result->duration_ms;
    
    // Print result
    if (suite->config.verbose) {
        printf("[%s] %s: %s (%u ms)\n", 
               compliance_result_to_string(result->result),
               tc->test_id, tc->test_name, result->duration_ms);
        if (result->result == COMPLIANCE_FAIL && result->message) {
            printf("    Message: %s\n", result->message);
        }
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t compliance_run_suite(compliance_suite_t* suite) {
    if (suite == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("\n========================================\n");
    printf("Running 3GPP Compliance Test Suite\n");
    printf("========================================\n");
    printf("Total test cases: %u\n\n", suite->num_test_cases);
    
    // Reset statistics
    memset(&suite->stats, 0, sizeof(compliance_stats_t));
    
    // Run all tests
    for (uint32_t i = 0; i < suite->num_test_cases; i++) {
        uesim_error_t result = compliance_run_single_test(suite, suite->test_cases[i].test_id);
        
        if (result != UESIM_SUCCESS) {
            printf("Error running test: %s\n", suite->test_cases[i].test_id);
            if (suite->config.stop_on_failure) {
                break;
            }
        }
        
        // Check for stop on failure
        if (suite->config.stop_on_failure && 
            suite->results[i].result == COMPLIANCE_FAIL) {
            printf("Stopping suite due to test failure\n");
            break;
        }
    }
    
    printf("\n========================================\n");
    printf("Test Suite Summary\n");
    printf("========================================\n");
    printf("Total:  %u\n", suite->stats.total_tests);
    printf("Passed: %u\n", suite->stats.passed);
    printf("Failed: %u\n", suite->stats.failed);
    printf("Skipped: %u\n", suite->stats.skipped);
    printf("Errors: %u\n", suite->stats.errors);
    printf("Not Implemented: %u\n", suite->stats.not_implemented);
    printf("Duration: %u ms\n", suite->stats.duration_ms);
    printf("========================================\n\n");
    
    return UESIM_SUCCESS;
}

uesim_error_t compliance_print_results(compliance_suite_t* suite) {
    if (suite == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("\nDetailed Test Results:\n");
    printf("----------------------\n");
    
    for (uint32_t i = 0; i < suite->num_test_cases; i++) {
        compliance_test_case_t* tc = &suite->test_cases[i];
        compliance_test_result_t* result = &suite->results[i];
        
        printf("\nTest ID: %s\n", tc->test_id);
        printf("  Name: %s\n", tc->test_name);
        printf("  Spec: %s\n", compliance_spec_to_string(tc->specification));
        printf("  Severity: %s\n", compliance_severity_to_string(tc->severity));
        printf("  Category: %s\n", compliance_category_to_string(tc->category));
        printf("  Section: %s\n", tc->section_ref);
        printf("  Result: %s\n", compliance_result_to_string(result->result));
        printf("  Duration: %u ms\n", result->duration_ms);
        if (result->message && result->message[0]) {
            printf("  Message: %s\n", result->message);
        }
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t compliance_generate_report(compliance_suite_t* suite,
                                        const char* format,
                                        const char* output_path) {
    if (suite == NULL || format == NULL || output_path == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    FILE* fp = fopen(output_path, "w");
    if (fp == NULL) {
        printf("Failed to open output file: %s\n", output_path);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (strcmp(format, "json") == 0) {
        fprintf(fp, "{\n");
        fprintf(fp, "  \"summary\": {\n");
        fprintf(fp, "    \"total\": %u,\n", suite->stats.total_tests);
        fprintf(fp, "    \"passed\": %u,\n", suite->stats.passed);
        fprintf(fp, "    \"failed\": %u,\n", suite->stats.failed);
        fprintf(fp, "    \"skipped\": %u,\n", suite->stats.skipped);
        fprintf(fp, "    \"errors\": %u,\n", suite->stats.errors);
        fprintf(fp, "    \"not_implemented\": %u,\n", suite->stats.not_implemented);
        fprintf(fp, "    \"duration_ms\": %u\n", suite->stats.duration_ms);
        fprintf(fp, "  },\n");
        fprintf(fp, "  \"results\": [\n");
        
        for (uint32_t i = 0; i < suite->num_test_cases; i++) {
            compliance_test_case_t* tc = &suite->test_cases[i];
            compliance_test_result_t* result = &suite->results[i];
            
            fprintf(fp, "    {\n");
            fprintf(fp, "      \"test_id\": \"%s\",\n", tc->test_id);
            fprintf(fp, "      \"name\": \"%s\",\n", tc->test_name);
            fprintf(fp, "      \"specification\": \"%s\",\n", compliance_spec_to_string(tc->specification));
            fprintf(fp, "      \"severity\": \"%s\",\n", compliance_severity_to_string(tc->severity));
            fprintf(fp, "      \"category\": \"%s\",\n", compliance_category_to_string(tc->category));
            fprintf(fp, "      \"section\": \"%s\",\n", tc->section_ref);
            fprintf(fp, "      \"result\": \"%s\",\n", compliance_result_to_string(result->result));
            fprintf(fp, "      \"duration_ms\": %u\n", result->duration_ms);
            fprintf(fp, "    }%s\n", (i < suite->num_test_cases - 1) ? "," : "");
        }
        
        fprintf(fp, "  ]\n");
        fprintf(fp, "}\n");
    } else {
        // Default to text format
        fprintf(fp, "3GPP Compliance Test Report\n");
        fprintf(fp, "==========================\n\n");
        fprintf(fp, "Summary:\n");
        fprintf(fp, "  Total: %u\n", suite->stats.total_tests);
        fprintf(fp, "  Passed: %u\n", suite->stats.passed);
        fprintf(fp, "  Failed: %u\n", suite->stats.failed);
        fprintf(fp, "  Skipped: %u\n", suite->stats.skipped);
        fprintf(fp, "  Errors: %u\n", suite->stats.errors);
        fprintf(fp, "  Not Implemented: %u\n", suite->stats.not_implemented);
        fprintf(fp, "  Duration: %u ms\n\n", suite->stats.duration_ms);
        
        fprintf(fp, "Detailed Results:\n");
        fprintf(fp, "-----------------\n");
        
        for (uint32_t i = 0; i < suite->num_test_cases; i++) {
            compliance_test_case_t* tc = &suite->test_cases[i];
            compliance_test_result_t* result = &suite->results[i];
            
            fprintf(fp, "\n[%s] %s\n", compliance_result_to_string(result->result), tc->test_id);
            fprintf(fp, "  Name: %s\n", tc->test_name);
            fprintf(fp, "  Spec: %s, Section: %s\n", compliance_spec_to_string(tc->specification), tc->section_ref);
            fprintf(fp, "  Duration: %u ms\n", result->duration_ms);
        }
    }
    
    fclose(fp);
    printf("Report generated: %s\n", output_path);
    
    return UESIM_SUCCESS;
}

uesim_error_t compliance_get_stats(compliance_suite_t* suite,
                                   compliance_stats_t* stats) {
    if (suite == NULL || stats == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    *stats = suite->stats;
    return UESIM_SUCCESS;
}

const char* compliance_result_to_string(compliance_result_t result) {
    if (result >= 0 && result < sizeof(result_strings) / sizeof(result_strings[0])) {
        return result_strings[result];
    }
    return "UNKNOWN";
}

const char* compliance_spec_to_string(compliance_spec_t spec) {
    if (spec >= 0 && spec < sizeof(spec_strings) / sizeof(spec_strings[0])) {
        return spec_strings[spec];
    }
    return "UNKNOWN";
}

const char* compliance_severity_to_string(compliance_severity_t severity) {
    if (severity >= 0 && severity < sizeof(severity_strings) / sizeof(severity_strings[0])) {
        return severity_strings[severity];
    }
    return "UNKNOWN";
}

const char* compliance_category_to_string(compliance_category_t category) {
    if (category >= 0 && category < sizeof(category_strings) / sizeof(category_strings[0])) {
        return category_strings[category];
    }
    return "UNKNOWN";
}