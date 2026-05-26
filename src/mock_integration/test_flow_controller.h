/*
 * 5G UE Simulation Application
 * Test Flow Controller - Orchestrates test scenarios and flows
 * 
 * This module provides high-level test scenario execution:
 * - Scenario loading from JSON files
 * - Multi-UE test coordination
 * - Test result collection and reporting
 */

#ifndef TEST_FLOW_CONTROLLER_H
#define TEST_FLOW_CONTROLLER_H

#include "mock_test_env.h"
#include <stdint.h>
#include <stdbool.h>

/* ============== Constants ============== */

#define TEST_FLOW_MAX_SCENARIOS     64
#define TEST_FLOW_MAX_CLEANUP_CB    8
#define TEST_FLOW_MAX_STEPS         128
#define TEST_FLOW_MAX_NAME_LEN      64
#define TEST_FLOW_MAX_DESC_LEN      256
#define TEST_FLOW_MAX_PARAM_LEN     128

/* ============== Error Codes ============== */

typedef enum {
    TEST_FLOW_SUCCESS = 0,
    TEST_FLOW_ERROR_INVALID_PARAM = -1,
    TEST_FLOW_ERROR_MEMORY = -2,
    TEST_FLOW_ERROR_FILE = -3,
    TEST_FLOW_ERROR_PARSE = -4,
    TEST_FLOW_ERROR_NOT_FOUND = -5,
    TEST_FLOW_ERROR_TIMEOUT = -6,
    TEST_FLOW_ERROR_STEP_FAILED = -7,
    TEST_FLOW_ERROR_ENV_NOT_READY = -8,
    TEST_FLOW_ERROR_CAPACITY = -9
} test_flow_error_t;

/* ============== Step Types ============== */

typedef enum {
    TEST_STEP_TYPE_WAIT = 0,
    TEST_STEP_TYPE_REGISTRATION,
    TEST_STEP_TYPE_PDU_SESSION,
    TEST_STEP_TYPE_HANDOVER,
    TEST_STEP_TYPE_DEREGISTRATION,
    TEST_STEP_TYPE_DATA_TRANSFER,
    TEST_STEP_TYPE_VERIFY,
    TEST_STEP_TYPE_CUSTOM,
    TEST_STEP_TYPE_MAX
} test_step_type_t;

/* ============== Step Result ============== */

typedef enum {
    TEST_STEP_RESULT_PENDING = 0,
    TEST_STEP_RESULT_RUNNING,
    TEST_STEP_RESULT_PASSED,
    TEST_STEP_RESULT_FAILED,
    TEST_STEP_RESULT_SKIPPED,
    TEST_STEP_RESULT_TIMEOUT
} test_step_result_t;

/* ============== Test Step ============== */

typedef struct {
    test_step_type_t type;
    char name[TEST_FLOW_MAX_NAME_LEN];
    char description[TEST_FLOW_MAX_DESC_LEN];
    
    /* Parameters */
    uint32_t ue_index;
    uint32_t timeout_ms;
    uint32_t delay_ms;
    
    /* Type-specific parameters */
    union {
        struct {
            uint8_t pdu_session_id;
        } pdu_session;
        
        struct {
            uint32_t target_gnb_id;
            uint32_t target_cell_id;
        } handover;
        
        struct {
            uint32_t data_size;
            bool is_uplink;
        } data_transfer;
        
        struct {
            char param[TEST_FLOW_MAX_PARAM_LEN];
            char expected_value[TEST_FLOW_MAX_PARAM_LEN];
        } verify;
        
        struct {
            char function_name[TEST_FLOW_MAX_NAME_LEN];
            char params[4][TEST_FLOW_MAX_PARAM_LEN];
        } custom;
    } params;
    
    /* Result */
    test_step_result_t result;
    char result_message[TEST_FLOW_MAX_DESC_LEN];
    uint64_t duration_ms;
} test_step_t;

/* ============== Test Scenario ============== */

typedef struct {
    char name[TEST_FLOW_MAX_NAME_LEN];
    char description[TEST_FLOW_MAX_DESC_LEN];
    char version[16];
    
    /* Steps */
    test_step_t steps[TEST_FLOW_MAX_STEPS];
    uint32_t num_steps;
    
    /* Results */
    uint32_t passed_steps;
    uint32_t failed_steps;
    uint32_t skipped_steps;
    test_step_result_t overall_result;
    uint64_t total_duration_ms;
    
    /* Execution state */
    uint32_t current_step;
    bool is_running;
    bool is_complete;
} test_scenario_t;

/* ============== Cleanup Callback Type ============== */

/**
 * Cleanup callback function type for test failure cleanup
 * @param env Test environment
 * @param ue_index UE index that failed (MOCK_TEST_MAX_UES if not UE-specific)
 * @param step Step that triggered cleanup
 * @param user_data User-provided context
 */
typedef void (*test_flow_cleanup_cb_t)(mock_test_env_t* env, uint32_t ue_index,
                                        const test_step_t* step, void* user_data);

/* ============== Custom Step Handler Type ============== */

/**
 * Custom step handler function type for user-defined test steps
 * @param step Step being executed (contains parameters and result fields)
 * @param context User-provided context data
 * @return TEST_FLOW_SUCCESS or error code
 */
typedef test_flow_error_t (*custom_step_handler_t)(test_step_t* step, void* context);

/* ============== Custom Step Registration ============== */

typedef struct {
    char step_name[TEST_FLOW_MAX_NAME_LEN];
    custom_step_handler_t handler;
    void* context;
    bool is_default;  /* true if this is the default handler for all CUSTOM steps */
} custom_step_registration_t;

#define TEST_FLOW_MAX_CUSTOM_HANDLERS   16

/* ============== Test Flow Controller ============== */

typedef struct {
    /* Environment */
    mock_test_env_t* env;
    
    /* Scenarios */
    test_scenario_t scenarios[TEST_FLOW_MAX_SCENARIOS];
    uint32_t num_scenarios;
    
    /* Current execution */
    uint32_t current_scenario;
    bool is_running;
    
    /* Statistics */
    uint32_t total_scenarios_run;
    uint32_t total_scenarios_passed;
    uint32_t total_scenarios_failed;
    uint64_t total_duration_ms;
    
    /* Configuration */
    bool stop_on_failure;
    bool verbose;
    bool collect_pcap;
    char report_file[256];
    
    /* Cleanup callbacks */
    test_flow_cleanup_cb_t cleanup_callbacks[TEST_FLOW_MAX_CLEANUP_CB];
    void* cleanup_user_data[TEST_FLOW_MAX_CLEANUP_CB];
    uint32_t num_cleanup_callbacks;
    
    /* Custom step handlers */
    custom_step_registration_t custom_handlers[TEST_FLOW_MAX_CUSTOM_HANDLERS];
    uint32_t num_custom_handlers;
    custom_step_handler_t default_custom_handler;
    void* default_custom_context;
} test_flow_controller_t;

/* ============== Controller Management ============== */

/**
 * Create test flow controller
 * @param env Test environment (must be started)
 * @return Controller or NULL on failure
 */
test_flow_controller_t* test_flow_controller_create(mock_test_env_t* env);

/**
 * Destroy test flow controller
 * @param controller Controller to destroy
 */
void test_flow_controller_destroy(test_flow_controller_t* controller);

/**
 * Reset controller state
 * @param controller Controller
 */
void test_flow_controller_reset(test_flow_controller_t* controller);

/* ============== Scenario Management ============== */

/**
 * Load scenario from JSON file
 * @param controller Controller
 * @param filename JSON file path
 * @return TEST_FLOW_SUCCESS or error code
 */
test_flow_error_t test_flow_controller_load_scenario(test_flow_controller_t* controller,
                                                      const char* filename);

/**
 * Add scenario programmatically
 * @param controller Controller
 * @param scenario Scenario to add
 * @return TEST_FLOW_SUCCESS or error code
 */
test_flow_error_t test_flow_controller_add_scenario(test_flow_controller_t* controller,
                                                     const test_scenario_t* scenario);

/**
 * Clear all scenarios
 * @param controller Controller
 */
void test_flow_controller_clear_scenarios(test_flow_controller_t* controller);

/**
 * Get scenario by index
 * @param controller Controller
 * @param index Scenario index
 * @return Scenario or NULL
 */
const test_scenario_t* test_flow_controller_get_scenario(const test_flow_controller_t* controller,
                                                         uint32_t index);

/* ============== Scenario Execution ============== */

/**
 * Run single scenario
 * @param controller Controller
 * @param scenario_index Scenario index
 * @return TEST_FLOW_SUCCESS or error code
 */
test_flow_error_t test_flow_controller_run_scenario(test_flow_controller_t* controller,
                                                     uint32_t scenario_index);

/**
 * Run all scenarios
 * @param controller Controller
 * @return TEST_FLOW_SUCCESS or error code
 */
test_flow_error_t test_flow_controller_run_all(test_flow_controller_t* controller);

/**
 * Stop current execution
 * @param controller Controller
 */
void test_flow_controller_stop(test_flow_controller_t* controller);

/**
 * Check if controller is running
 * @param controller Controller
 * @return true if running
 */
bool test_flow_controller_is_running(const test_flow_controller_t* controller);

/* ============== Step Execution ============== */

/**
 * Execute single step
 * @param controller Controller
 * @param step Step to execute
 * @return TEST_FLOW_SUCCESS or error code
 */
test_flow_error_t test_flow_controller_execute_step(test_flow_controller_t* controller,
                                                     test_step_t* step);

/* ============== Results and Reporting ============== */

/**
 * Get scenario results
 * @param controller Controller
 * @param scenario_index Scenario index
 * @param passed Output: passed steps
 * @param failed Output: failed steps
 * @param skipped Output: skipped steps
 * @return TEST_FLOW_SUCCESS or error code
 */
test_flow_error_t test_flow_controller_get_results(const test_flow_controller_t* controller,
                                                    uint32_t scenario_index,
                                                    uint32_t* passed,
                                                    uint32_t* failed,
                                                    uint32_t* skipped);

/**
 * Generate test report
 * @param controller Controller
 * @param filename Output file (NULL for console)
 * @return TEST_FLOW_SUCCESS or error code
 */
test_flow_error_t test_flow_controller_generate_report(const test_flow_controller_t* controller,
                                                        const char* filename);

/**
 * Print scenario summary
 * @param controller Controller
 * @param scenario_index Scenario index
 */
void test_flow_controller_print_scenario_summary(const test_flow_controller_t* controller,
                                                  uint32_t scenario_index);

/**
 * Print overall summary
 * @param controller Controller
 */
void test_flow_controller_print_summary(const test_flow_controller_t* controller);

/* ============== Built-in Scenarios ============== */

/**
 * Create registration scenario
 * @param scenario Scenario to fill
 * @param num_ues Number of UEs
 * @return TEST_FLOW_SUCCESS or error code
 */
test_flow_error_t test_flow_controller_create_registration_scenario(test_scenario_t* scenario,
                                                                     uint32_t num_ues);

/**
 * Create PDU session scenario
 * @param scenario Scenario to fill
 * @param num_ues Number of UEs
 * @param sessions_per_ue Sessions per UE
 * @return TEST_FLOW_SUCCESS or error code
 */
test_flow_error_t test_flow_controller_create_pdu_session_scenario(test_scenario_t* scenario,
                                                                     uint32_t num_ues,
                                                                     uint8_t sessions_per_ue);

/**
 * Create handover scenario
 * @param scenario Scenario to fill
 * @param num_ues Number of UEs
 * @param target_gnb_id Target gNB ID
 * @param target_cell_id Target cell ID
 * @return TEST_FLOW_SUCCESS or error code
 */
test_flow_error_t test_flow_controller_create_handover_scenario(test_scenario_t* scenario,
                                                                  uint32_t num_ues,
                                                                  uint32_t target_gnb_id,
                                                                  uint32_t target_cell_id);

/**
 * Create complete test scenario (registration + PDU session + data)
 * @param scenario Scenario to fill
 * @param num_ues Number of UEs
 * @return TEST_FLOW_SUCCESS or error code
 */
test_flow_error_t test_flow_controller_create_complete_scenario(test_scenario_t* scenario,
                                                                  uint32_t num_ues);

/* ============== Cleanup Callback Management ============== */

/**
 * Register cleanup callback for test failure
 * @param controller Controller
 * @param callback Cleanup callback function
 * @param user_data User data passed to callback
 * @return TEST_FLOW_SUCCESS or error code
 */
test_flow_error_t test_flow_controller_register_cleanup(test_flow_controller_t* controller,
                                                         test_flow_cleanup_cb_t callback,
                                                         void* user_data);

/**
 * Clear all cleanup callbacks
 * @param controller Controller
 */
void test_flow_controller_clear_cleanup_callbacks(test_flow_controller_t* controller);

/* ============== Custom Step Handler Management ============== */

/**
 * Register a custom step handler by name
 * @param controller Controller
 * @param step_name Name of the step to handle (matches step->name)
 * @param handler Custom handler function
 * @param context User context passed to handler
 * @return TEST_FLOW_SUCCESS or error code
 */
test_flow_error_t test_flow_register_step_handler(test_flow_controller_t* controller,
                                                   const char* step_name,
                                                   custom_step_handler_t handler,
                                                   void* context);

/**
 * Set default handler for all CUSTOM steps
 * @param controller Controller
 * @param handler Default handler function (NULL to clear)
 * @param context User context passed to handler
 * @return TEST_FLOW_SUCCESS or error code
 */
test_flow_error_t test_flow_set_default_custom_handler(test_flow_controller_t* controller,
                                                        custom_step_handler_t handler,
                                                        void* context);

/**
 * Clear all custom step handlers
 * @param controller Controller
 */
void test_flow_clear_custom_handlers(test_flow_controller_t* controller);

/* ============== Utility Functions ============== */

/**
 * Convert step type to string
 * @param type Step type
 * @return Type string
 */
const char* test_step_type_to_string(test_step_type_t type);

/**
 * Convert step result to string
 * @param result Step result
 * @return Result string
 */
const char* test_step_result_to_string(test_step_result_t result);

/**
 * Convert error code to string
 * @param error Error code
 * @return Error string
 */
const char* test_flow_error_to_string(test_flow_error_t error);

#endif /* TEST_FLOW_CONTROLLER_H */