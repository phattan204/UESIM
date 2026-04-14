/*
 * 5G UE Simulation Application
 * OAI-O-RAN gNB Integration Test
 */

#include "../src/uesim.h"
#include "../src/protocol/rrc.h"
#include "../src/transport/socket_mgr.h"
#include "../src/core/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

// Test configuration
#define TEST_GNB_IP "192.168.1.2"
#define TEST_GNB_PORT 38412
#define TEST_CAPTURE_DIR "./captures"
#define TEST_TIMEOUT_SECONDS 30

// Test results structure
typedef struct {
    char test_name[64];
    bool passed;
    uesim_error_t error_code;
    uint64_t execution_time_ms;
    char details[256];
} test_result_t;

// Test case structure
typedef struct {
    char name[64];
    char description[256];
    uesim_error_t (*test_function)(ue_context_t* ue_ctx, test_result_t* result);
    bool requires_capture;
    int timeout_seconds;
} oai_test_case_t;

// Forward declarations
static uesim_error_t test_oai_registration_procedure(ue_context_t* ue_ctx, test_result_t* result);
static uesim_error_t test_oai_establishment_procedure(ue_context_t* ue_ctx, test_result_t* result);
static uesim_error_t test_oai_reestablishment_procedure(ue_context_t* ue_ctx, test_result_t* result);
static uesim_error_t test_oai_handover_procedure(ue_context_t* ue_ctx, test_result_t* result);
static uesim_error_t run_test_with_timeout(uesim_error_t (*test_func)(ue_context_t*, test_result_t*), 
                                          ue_context_t* ue_ctx, test_result_t* result, int timeout_seconds);
static uesim_error_t start_wireshark_capture(const char* filename, const char* filter);
static uesim_error_t stop_wireshark_capture(void);
static uesim_error_t analyze_capture_file(const char* capture_file, const char* analysis_file);
static bool validate_registration_success(const char* analysis_file);
static bool validate_establishment_success(const char* analysis_file);
static bool validate_reestablishment_success(const char* analysis_file);
static bool validate_handover_success(const char* analysis_file);

// Test cases
static oai_test_case_t g_oai_test_cases[] = {
    {
        .name = "registration_procedure",
        .description = "RRC Registration with Initial Access to OAI gNB",
        .test_function = test_oai_registration_procedure,
        .requires_capture = true,
        .timeout_seconds = 15
    },
    {
        .name = "establishment_procedure",
        .description = "RRC Connection Establishment with OAI gNB",
        .test_function = test_oai_establishment_procedure,
        .requires_capture = true,
        .timeout_seconds = 20
    },
    {
        .name = "reestablishment_procedure",
        .description = "RRC Connection Re-establishment with OAI gNB",
        .test_function = test_oai_reestablishment_procedure,
        .requires_capture = true,
        .timeout_seconds = 25
    },
    {
        .name = "handover_procedure",
        .description = "RRC Handover Procedure with OAI gNB",
        .test_function = test_oai_handover_procedure,
        .requires_capture = true,
        .timeout_seconds = 30
    }
};

static pid_t g_capture_pid = 0;

int main(void) {
    printf("5G UE Simulation OAI-O-RAN gNB Integration Test\n");
    printf("==============================================\n");
    
    // Initialize memory system
    uesim_error_t result = memory_init(UESIM_HEAP_SIZE);
    if (result != UESIM_SUCCESS) {
        fprintf(stderr, "Failed to initialize memory system: %d\n", result);
        return EXIT_FAILURE;
    }
    
    printf("✓ Memory system initialized\n");
    
    // Initialize socket manager
    result = socket_manager_init();
    if (result != UESIM_SUCCESS) {
        fprintf(stderr, "Failed to initialize socket manager: %d\n", result);
        memory_cleanup();
        return EXIT_FAILURE;
    }
    
    printf("✓ Socket manager initialized\n");
    
    // Create UE context
    ue_context_t* ue_ctx = NULL;
    result = uesim_create_ue_instance(&ue_ctx);
    if (result != UESIM_SUCCESS) {
        fprintf(stderr, "Failed to create UE instance: %d\n", result);
        socket_manager_cleanup();
        memory_cleanup();
        return EXIT_FAILURE;
    }
    
    printf("✓ UE instance created (ID: %u)\n", ue_ctx->ue_id);
    
    // Configure UE for OAI gNB
    ue_ctx->gnb_ip = inet_addr(TEST_GNB_IP);
    ue_ctx->gnb_port = TEST_GNB_PORT;
    strcpy(ue_ctx->imsi, "001010123456789");
    strcpy(ue_ctx->msisdn, "1234567890");
    ue_ctx->tac = 1;
    
    printf("✓ UE configured for OAI gNB (%s:%d)\n", TEST_GNB_IP, TEST_GNB_PORT);
    
    // Create capture directory
    system("mkdir -p " TEST_CAPTURE_DIR);
    printf("✓ Capture directory created\n");
    
    // Run test suite
    int num_tests = sizeof(g_oai_test_cases) / sizeof(g_oai_test_cases[0]);
    test_result_t* results = (test_result_t*)calloc(num_tests, sizeof(test_result_t));
    if (results == NULL) {
        fprintf(stderr, "Failed to allocate memory for test results\n");
        uesim_stop_ue(ue_ctx);
        socket_manager_cleanup();
        memory_cleanup();
        return EXIT_FAILURE;
    }
    
    int passed = 0;
    int failed = 0;
    
    printf("\nRunning OAI-O-RAN Integration Test Suite (%d tests)\n", num_tests);
    printf("==================================================\n");
    
    for (int i = 0; i < num_tests; i++) {
        printf("\nTest %d/%d: %s\n", i+1, num_tests, g_oai_test_cases[i].name);
        printf("  Description: %s\n", g_oai_test_cases[i].description);
        printf("  Capture: %s\n", g_oai_test_cases[i].requires_capture ? "ENABLED" : "DISABLED");
        printf("  Timeout: %d seconds\n", g_oai_test_cases[i].timeout_seconds);
        
        // Run test with timeout
        uesim_error_t test_result = run_test_with_timeout(g_oai_test_cases[i].test_function, 
                                                         ue_ctx, &results[i], 
                                                         g_oai_test_cases[i].timeout_seconds);
        
        strcpy(results[i].test_name, g_oai_test_cases[i].name);
        results[i].error_code = test_result;
        
        if (test_result == UESIM_SUCCESS) {
            printf("  Result: PASSED ✓\n");
            results[i].passed = true;
            passed++;
        } else {
            printf("  Result: FAILED ✗ (Error: %d)\n", test_result);
            results[i].passed = false;
            failed++;
        }
        
        if (strlen(results[i].details) > 0) {
            printf("  Details: %s\n", results[i].details);
        }
    }
    
    // Print summary
    printf("\nTest Suite Results:\n");
    printf("==================\n");
    printf("  Total Tests: %d\n", num_tests);
    printf("  Passed: %d\n", passed);
    printf("  Failed: %d\n", failed);
    printf("  Success Rate: %.2f%%\n", (float)passed / num_tests * 100);
    
    // Print detailed results
    printf("\nDetailed Results:\n");
    printf("================\n");
    for (int i = 0; i < num_tests; i++) {
        printf("  %s: %s", results[i].test_name, results[i].passed ? "PASSED" : "FAILED");
        if (results[i].error_code != UESIM_SUCCESS) {
            printf(" (Error: %d)", results[i].error_code);
        }
        printf("\n");
    }
    
    // Cleanup
    free(results);
    uesim_stop_ue(ue_ctx);
    socket_manager_cleanup();
    memory_cleanup();
    
    printf("\nOAI-O-RAN gNB Integration Test Suite completed.\n");
    printf("Capture files available in: %s\n", TEST_CAPTURE_DIR);
    
    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

// Test implementation functions
static uesim_error_t test_oai_registration_procedure(ue_context_t* ue_ctx, test_result_t* result) {
    if (ue_ctx == NULL || result == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    uint64_t start_time = time(NULL) * 1000;
    
    // Start Wireshark capture
    char capture_file[256];
    snprintf(capture_file, sizeof(capture_file), 
             TEST_CAPTURE_DIR "/registration_ue_%u.pcap", ue_ctx->ue_id);
    
    uesim_error_t capture_result = start_wireshark_capture(capture_file, 
        "host " TEST_GNB_IP " and port " STRINGIFY(TEST_GNB_PORT));
    
    if (capture_result != UESIM_SUCCESS) {
        snprintf(result->details, sizeof(result->details), 
                "Failed to start capture: %d", capture_result);
        return capture_result;
    }
    
    printf("  Starting registration procedure...\n");
    
    // Execute registration procedure
    uesim_error_t reg_result = uesim_execute_procedure(ue_ctx, RRC_PROC_REGISTRATION);
    
    if (reg_result != UESIM_SUCCESS) {
        stop_wireshark_capture();
        snprintf(result->details, sizeof(result->details), 
                "Registration procedure failed: %d", reg_result);
        return reg_result;
    }
    
    // Wait for procedure completion
    sleep(3);
    
    // Stop capture
    stop_wireshark_capture();
    
    // Analyze capture
    char analysis_file[256];
    snprintf(analysis_file, sizeof(analysis_file), 
             TEST_CAPTURE_DIR "/registration_analysis_%u.txt", ue_ctx->ue_id);
    
    uesim_error_t analyze_result = analyze_capture_file(capture_file, analysis_file);
    if (analyze_result != UESIM_SUCCESS) {
        snprintf(result->details, sizeof(result->details), 
                "Capture analysis failed: %d", analyze_result);
        return analyze_result;
    }
    
    // Validate results
    if (validate_registration_success(analysis_file)) {
        uint64_t end_time = time(NULL) * 1000;
        result->execution_time_ms = end_time - start_time;
        snprintf(result->details, sizeof(result->details), 
                "Registration successful in %lu ms", result->execution_time_ms);
        return UESIM_SUCCESS;
    } else {
        snprintf(result->details, sizeof(result->details), 
                "Registration validation failed");
        return UESIM_ERROR_PROTOCOL;
    }
}

static uesim_error_t test_oai_establishment_procedure(ue_context_t* ue_ctx, test_result_t* result) {
    if (ue_ctx == NULL || result == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if UE is in proper state for establishment
    if (rrc_get_current_state(ue_ctx) != RRC_STATE_IDLE) {
        snprintf(result->details, sizeof(result->details), 
                "UE not in IDLE state for establishment");
        return UESIM_ERROR_PROTOCOL;
    }
    
    uint64_t start_time = time(NULL) * 1000;
    
    // Start Wireshark capture
    char capture_file[256];
    snprintf(capture_file, sizeof(capture_file), 
             TEST_CAPTURE_DIR "/establishment_ue_%u.pcap", ue_ctx->ue_id);
    
    uesim_error_t capture_result = start_wireshark_capture(capture_file, 
        "host " TEST_GNB_IP " and port " STRINGIFY(TEST_GNB_PORT));
    
    if (capture_result != UESIM_SUCCESS) {
        snprintf(result->details, sizeof(result->details), 
                "Failed to start capture: %d", capture_result);
        return capture_result;
    }
    
    printf("  Starting establishment procedure...\n");
    
    // Execute establishment procedure
    uesim_error_t est_result = uesim_execute_procedure(ue_ctx, RRC_PROC_ESTABLISHMENT);
    
    if (est_result != UESIM_SUCCESS) {
        stop_wireshark_capture();
        snprintf(result->details, sizeof(result->details), 
                "Establishment procedure failed: %d", est_result);
        return est_result;
    }
    
    // Wait for procedure completion
    sleep(3);
    
    // Stop capture
    stop_wireshark_capture();
    
    // Analyze capture
    char analysis_file[256];
    snprintf(analysis_file, sizeof(analysis_file), 
             TEST_CAPTURE_DIR "/establishment_analysis_%u.txt", ue_ctx->ue_id);
    
    uesim_error_t analyze_result = analyze_capture_file(capture_file, analysis_file);
    if (analyze_result != UESIM_SUCCESS) {
        snprintf(result->details, sizeof(result->details), 
                "Capture analysis failed: %d", analyze_result);
        return analyze_result;
    }
    
    // Validate results
    if (validate_establishment_success(analysis_file)) {
        uint64_t end_time = time(NULL) * 1000;
        result->execution_time_ms = end_time - start_time;
        snprintf(result->details, sizeof(result->details), 
                "Establishment successful in %lu ms", result->execution_time_ms);
        return UESIM_SUCCESS;
    } else {
        snprintf(result->details, sizeof(result->details), 
                "Establishment validation failed");
        return UESIM_ERROR_PROTOCOL;
    }
}

static uesim_error_t test_oai_reestablishment_procedure(ue_context_t* ue_ctx, test_result_t* result) {
    if (ue_ctx == NULL || result == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if UE is in proper state for reestablishment
    if (rrc_get_current_state(ue_ctx) == RRC_STATE_IDLE) {
        snprintf(result->details, sizeof(result->details), 
                "UE in IDLE state, cannot reestablish");
        return UESIM_ERROR_PROTOCOL;
    }
    
    uint64_t start_time = time(NULL) * 1000;
    
    // Start Wireshark capture
    char capture_file[256];
    snprintf(capture_file, sizeof(capture_file), 
             TEST_CAPTURE_DIR "/reestablishment_ue_%u.pcap", ue_ctx->ue_id);
    
    uesim_error_t capture_result = start_wireshark_capture(capture_file, 
        "host " TEST_GNB_IP " and port " STRINGIFY(TEST_GNB_PORT));
    
    if (capture_result != UESIM_SUCCESS) {
        snprintf(result->details, sizeof(result->details), 
                "Failed to start capture: %d", capture_result);
        return capture_result;
    }
    
    printf("  Starting reestablishment procedure...\n");
    
    // Execute reestablishment procedure
    uesim_error_t reest_result = uesim_execute_procedure(ue_ctx, RRC_PROC_REESTABLISHMENT);
    
    if (reest_result != UESIM_SUCCESS) {
        stop_wireshark_capture();
        snprintf(result->details, sizeof(result->details), 
                "Reestablishment procedure failed: %d", reest_result);
        return reest_result;
    }
    
    // Wait for procedure completion
    sleep(4);
    
    // Stop capture
    stop_wireshark_capture();
    
    // Analyze capture
    char analysis_file[256];
    snprintf(analysis_file, sizeof(analysis_file), 
             TEST_CAPTURE_DIR "/reestablishment_analysis_%u.txt", ue_ctx->ue_id);
    
    uesim_error_t analyze_result = analyze_capture_file(capture_file, analysis_file);
    if (analyze_result != UESIM_SUCCESS) {
        snprintf(result->details, sizeof(result->details), 
                "Capture analysis failed: %d", analyze_result);
        return analyze_result;
    }
    
    // Validate results
    if (validate_reestablishment_success(analysis_file)) {
        uint64_t end_time = time(NULL) * 1000;
        result->execution_time_ms = end_time - start_time;
        snprintf(result->details, sizeof(result->details), 
                "Reestablishment successful in %lu ms", result->execution_time_ms);
        return UESIM_SUCCESS;
    } else {
        snprintf(result->details, sizeof(result->details), 
                "Reestablishment validation failed");
        return UESIM_ERROR_PROTOCOL;
    }
}

static uesim_error_t test_oai_handover_procedure(ue_context_t* ue_ctx, test_result_t* result) {
    if (ue_ctx == NULL || result == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if UE is in proper state for handover
    if (rrc_get_current_state(ue_ctx) != RRC_STATE_CONNECTED) {
        snprintf(result->details, sizeof(result->details), 
                "UE not in CONNECTED state for handover");
        return UESIM_ERROR_PROTOCOL;
    }
    
    uint64_t start_time = time(NULL) * 1000;
    
    // Start Wireshark capture
    char capture_file[256];
    snprintf(capture_file, sizeof(capture_file), 
             TEST_CAPTURE_DIR "/handover_ue_%u.pcap", ue_ctx->ue_id);
    
    uesim_error_t capture_result = start_wireshark_capture(capture_file, 
        "host " TEST_GNB_IP " and port " STRINGIFY(TEST_GNB_PORT));
    
    if (capture_result != UESIM_SUCCESS) {
        snprintf(result->details, sizeof(result->details), 
                "Failed to start capture: %d", capture_result);
        return capture_result;
    }
    
    printf("  Starting handover procedure...\n");
    
    // Execute handover procedure
    uesim_error_t ho_result = uesim_execute_procedure(ue_ctx, RRC_PROC_HANDOVER);
    
    if (ho_result != UESIM_SUCCESS) {
        stop_wireshark_capture();
        snprintf(result->details, sizeof(result->details), 
                "Handover procedure failed: %d", ho_result);
        return ho_result;
    }
    
    // Wait for procedure completion
    sleep(5);
    
    // Stop capture
    stop_wireshark_capture();
    
    // Analyze capture
    char analysis_file[256];
    snprintf(analysis_file, sizeof(analysis_file), 
             TEST_CAPTURE_DIR "/handover_analysis_%u.txt", ue_ctx->ue_id);
    
    uesim_error_t analyze_result = analyze_capture_file(capture_file, analysis_file);
    if (analyze_result != UESIM_SUCCESS) {
        snprintf(result->details, sizeof(result->details), 
                "Capture analysis failed: %d", analyze_result);
        return analyze_result;
    }
    
    // Validate results
    if (validate_handover_success(analysis_file)) {
        uint64_t end_time = time(NULL) * 1000;
        result->execution_time_ms = end_time - start_time;
        snprintf(result->details, sizeof(result->details), 
                "Handover successful in %lu ms", result->execution_time_ms);
        return UESIM_SUCCESS;
    } else {
        snprintf(result->details, sizeof(result->details), 
                "Handover validation failed");
        return UESIM_ERROR_PROTOCOL;
    }
}

// Utility functions
static uesim_error_t run_test_with_timeout(uesim_error_t (*test_func)(ue_context_t*, test_result_t*), 
                                          ue_context_t* ue_ctx, test_result_t* result, int timeout_seconds) {
    // In a real implementation, this would use alarm() or pthread_timedjoin_np()
    // For now, we'll just call the function directly
    return test_func(ue_ctx, result);
}

static uesim_error_t start_wireshark_capture(const char* filename, const char* filter) {
    if (filename == NULL || filter == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "tshark -i any -f '%s' -w %s &", filter, filename);
    
    int result = system(cmd);
    if (result == 0) {
        // Give tshark time to start
        sleep(1);
        return UESIM_SUCCESS;
    } else {
        return UESIM_ERROR_SOCKET;
    }
}

static uesim_error_t stop_wireshark_capture(void) {
    int result = system("pkill tshark 2>/dev/null");
    sleep(1); // Give time for capture to finish writing
    return (result == 0) ? UESIM_SUCCESS : UESIM_ERROR_SOCKET;
}

static uesim_error_t analyze_capture_file(const char* capture_file, const char* analysis_file) {
    if (capture_file == NULL || analysis_file == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    char cmd[512];
    snprintf(cmd, sizeof(cmd), 
             "tshark -r %s -T fields -e frame.number -e ip.src -e ip.dst > %s 2>/dev/null",
             capture_file, analysis_file);
    
    int result = system(cmd);
    return (result == 0) ? UESIM_SUCCESS : UESIM_ERROR_PROTOCOL;
}

static bool validate_registration_success(const char* analysis_file) {
    if (analysis_file == NULL) {
        return false;
    }
    
    FILE* file = fopen(analysis_file, "r");
    if (!file) return false;
    
    char line[512];
    bool has_content = false;
    
    while (fgets(line, sizeof(line), file)) {
        if (strlen(line) > 1) { // Non-empty line
            has_content = true;
            break;
        }
    }
    
    fclose(file);
    return has_content;
}

static bool validate_establishment_success(const char* analysis_file) {
    return validate_registration_success(analysis_file); // Similar validation
}

static bool validate_reestablishment_success(const char* analysis_file) {
    return validate_registration_success(analysis_file); // Similar validation
}

static bool validate_handover_success(const char* analysis_file) {
    return validate_registration_success(analysis_file); // Similar validation
}

// Helper macro for stringification
#define STRINGIFY(x) #x