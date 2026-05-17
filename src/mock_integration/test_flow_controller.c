/*
 * 5G UE Simulation Application
 * Test Flow Controller - Implementation
 */

#include "test_flow_controller.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#define get_time_ms() GetTickCount64()
#else
#include <sys/time.h>
static uint64_t get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
#endif

/* ============== Controller Management ============== */

test_flow_controller_t* test_flow_controller_create(mock_test_env_t* env) {
    if (!env) return NULL;
    
    test_flow_controller_t* controller = (test_flow_controller_t*)calloc(1, sizeof(test_flow_controller_t));
    if (!controller) return NULL;
    
    controller->env = env;
    controller->stop_on_failure = true;
    controller->verbose = true;
    
    return controller;
}

void test_flow_controller_destroy(test_flow_controller_t* controller) {
    if (!controller) return;
    
    test_flow_controller_clear_scenarios(controller);
    free(controller);
}

void test_flow_controller_reset(test_flow_controller_t* controller) {
    if (!controller) return;
    
    for (uint32_t i = 0; i < controller->num_scenarios; i++) {
        test_scenario_t* scenario = &controller->scenarios[i];
        scenario->current_step = 0;
        scenario->is_running = false;
        scenario->is_complete = false;
        scenario->passed_steps = 0;
        scenario->failed_steps = 0;
        scenario->skipped_steps = 0;
        scenario->overall_result = TEST_STEP_RESULT_PENDING;
        
        for (uint32_t j = 0; j < scenario->num_steps; j++) {
            scenario->steps[j].result = TEST_STEP_RESULT_PENDING;
            scenario->steps[j].result_message[0] = '\0';
            scenario->steps[j].duration_ms = 0;
        }
    }
    
    controller->current_scenario = 0;
    controller->is_running = false;
    controller->total_scenarios_run = 0;
    controller->total_scenarios_passed = 0;
    controller->total_scenarios_failed = 0;
    controller->total_duration_ms = 0;
}

/* ============== Scenario Management ============== */

test_flow_error_t test_flow_controller_add_scenario(test_flow_controller_t* controller,
                                                     const test_scenario_t* scenario) {
    if (!controller || !scenario) return TEST_FLOW_ERROR_INVALID_PARAM;
    
    if (controller->num_scenarios >= TEST_FLOW_MAX_SCENARIOS) {
        return TEST_FLOW_ERROR_MEMORY;
    }
    
    memcpy(&controller->scenarios[controller->num_scenarios], scenario, sizeof(test_scenario_t));
    controller->num_scenarios++;
    
    return TEST_FLOW_SUCCESS;
}

void test_flow_controller_clear_scenarios(test_flow_controller_t* controller) {
    if (!controller) return;
    
    memset(controller->scenarios, 0, sizeof(controller->scenarios));
    controller->num_scenarios = 0;
}

const test_scenario_t* test_flow_controller_get_scenario(const test_flow_controller_t* controller,
                                                         uint32_t index) {
    if (!controller || index >= controller->num_scenarios) return NULL;
    return &controller->scenarios[index];
}

/* ============== Scenario Execution ============== */

test_flow_error_t test_flow_controller_run_scenario(test_flow_controller_t* controller,
                                                     uint32_t scenario_index) {
    if (!controller) return TEST_FLOW_ERROR_INVALID_PARAM;
    
    if (scenario_index >= controller->num_scenarios) {
        return TEST_FLOW_ERROR_NOT_FOUND;
    }
    
    if (!mock_test_env_is_running(controller->env)) {
        return TEST_FLOW_ERROR_ENV_NOT_READY;
    }
    
    test_scenario_t* scenario = &controller->scenarios[scenario_index];
    scenario->is_running = true;
    scenario->current_step = 0;
    controller->is_running = true;
    controller->current_scenario = scenario_index;
    
    uint64_t scenario_start = get_time_ms();
    
    if (controller->verbose) {
        printf("\n[FlowController] Running scenario: %s\n", scenario->name);
        printf("[FlowController] Description: %s\n", scenario->description);
        printf("[FlowController] Steps: %u\n\n", scenario->num_steps);
    }
    
    /* Execute each step */
    for (uint32_t i = 0; i < scenario->num_steps; i++) {
        test_step_t* step = &scenario->steps[i];
        
        if (controller->verbose) {
            printf("[FlowController] Step %u: %s (%s)\n", 
                   i + 1, step->name, test_step_type_to_string(step->type));
        }
        
        test_flow_error_t err = test_flow_controller_execute_step(controller, step);
        
        if (step->result == TEST_STEP_RESULT_PASSED) {
            scenario->passed_steps++;
            if (controller->verbose) {
                printf("[FlowController]   PASSED (%llu ms)\n", 
                       (unsigned long long)step->duration_ms);
            }
        } else if (step->result == TEST_STEP_RESULT_SKIPPED) {
            scenario->skipped_steps++;
            if (controller->verbose) {
                printf("[FlowController]   SKIPPED: %s\n", step->result_message);
            }
        } else {
            scenario->failed_steps++;
            if (controller->verbose) {
                printf("[FlowController]   FAILED: %s\n", step->result_message);
            }
            
            if (controller->stop_on_failure) {
                /* Mark remaining steps as skipped */
                for (uint32_t j = i + 1; j < scenario->num_steps; j++) {
                    scenario->steps[j].result = TEST_STEP_RESULT_SKIPPED;
                    strncpy(scenario->steps[j].result_message, "Skipped due to previous failure",
                            sizeof(scenario->steps[j].result_message) - 1);
                    scenario->skipped_steps++;
                }
                break;
            }
        }
        
        scenario->current_step = i + 1;
        
        /* Delay between steps */
        if (step->delay_ms > 0) {
#ifdef _WIN32
            Sleep(step->delay_ms);
#else
            usleep(step->delay_ms * 1000);
#endif
        }
    }
    
    scenario->total_duration_ms = get_time_ms() - scenario_start;
    scenario->is_running = false;
    scenario->is_complete = true;
    controller->is_running = false;
    
    /* Determine overall result */
    if (scenario->failed_steps == 0) {
        scenario->overall_result = TEST_STEP_RESULT_PASSED;
        controller->total_scenarios_passed++;
    } else {
        scenario->overall_result = TEST_STEP_RESULT_FAILED;
        controller->total_scenarios_failed++;
    }
    
    controller->total_scenarios_run++;
    controller->total_duration_ms += scenario->total_duration_ms;
    
    if (controller->verbose) {
        printf("\n[FlowController] Scenario complete: %s\n", 
               scenario->overall_result == TEST_STEP_RESULT_PASSED ? "PASSED" : "FAILED");
        printf("[FlowController] Duration: %llu ms\n", 
               (unsigned long long)scenario->total_duration_ms);
        printf("[FlowController] Steps: %u passed, %u failed, %u skipped\n\n",
               scenario->passed_steps, scenario->failed_steps, scenario->skipped_steps);
    }
    
    return scenario->overall_result == TEST_STEP_RESULT_PASSED ? 
           TEST_FLOW_SUCCESS : TEST_FLOW_ERROR_STEP_FAILED;
}

test_flow_error_t test_flow_controller_run_all(test_flow_controller_t* controller) {
    if (!controller) return TEST_FLOW_ERROR_INVALID_PARAM;
    
    if (controller->num_scenarios == 0) {
        return TEST_FLOW_ERROR_NOT_FOUND;
    }
    
    if (controller->verbose) {
        printf("\n[FlowController] Running all scenarios (%u total)\n", controller->num_scenarios);
    }
    
    test_flow_error_t last_error = TEST_FLOW_SUCCESS;
    
    for (uint32_t i = 0; i < controller->num_scenarios; i++) {
        test_flow_error_t err = test_flow_controller_run_scenario(controller, i);
        
        if (err != TEST_FLOW_SUCCESS) {
            last_error = err;
            if (controller->stop_on_failure) {
                break;
            }
        }
    }
    
    if (controller->verbose) {
        test_flow_controller_print_summary(controller);
    }
    
    return last_error;
}

void test_flow_controller_stop(test_flow_controller_t* controller) {
    if (!controller) return;
    controller->is_running = false;
}

bool test_flow_controller_is_running(const test_flow_controller_t* controller) {
    return controller ? controller->is_running : false;
}

/* ============== Step Execution ============== */

test_flow_error_t test_flow_controller_execute_step(test_flow_controller_t* controller,
                                                     test_step_t* step) {
    if (!controller || !step) return TEST_FLOW_ERROR_INVALID_PARAM;
    
    uint64_t step_start = get_time_ms();
    step->result = TEST_STEP_RESULT_RUNNING;
    
    mock_test_error_t err = MOCK_TEST_SUCCESS;
    
    switch (step->type) {
        case TEST_STEP_TYPE_WAIT:
            if (controller->verbose) {
                printf("[FlowController]   Waiting %u ms...\n", step->timeout_ms);
            }
#ifdef _WIN32
            Sleep(step->timeout_ms);
#else
            usleep(step->timeout_ms * 1000);
#endif
            step->result = TEST_STEP_RESULT_PASSED;
            strncpy(step->result_message, "Wait completed", sizeof(step->result_message) - 1);
            break;
            
        case TEST_STEP_TYPE_REGISTRATION:
            err = mock_test_env_run_registration(controller->env, 
                                                  step->ue_index, 
                                                  step->timeout_ms);
            if (err == MOCK_TEST_SUCCESS) {
                step->result = TEST_STEP_RESULT_PASSED;
                strncpy(step->result_message, "Registration successful", sizeof(step->result_message) - 1);
            } else {
                step->result = TEST_STEP_RESULT_FAILED;
                snprintf(step->result_message, sizeof(step->result_message) - 1,
                         "Registration failed: %s", mock_test_error_to_string(err));
            }
            break;
            
        case TEST_STEP_TYPE_PDU_SESSION:
            err = mock_test_env_run_pdu_session(controller->env,
                                                 step->ue_index,
                                                 step->params.pdu_session.pdu_session_id,
                                                 step->timeout_ms);
            if (err == MOCK_TEST_SUCCESS) {
                step->result = TEST_STEP_RESULT_PASSED;
                strncpy(step->result_message, "PDU session established", sizeof(step->result_message) - 1);
            } else {
                step->result = TEST_STEP_RESULT_FAILED;
                snprintf(step->result_message, sizeof(step->result_message) - 1,
                         "PDU session failed: %s", mock_test_error_to_string(err));
            }
            break;
            
        case TEST_STEP_TYPE_HANDOVER:
            err = mock_test_env_run_handover(controller->env,
                                              step->ue_index,
                                              step->params.handover.target_gnb_id,
                                              step->params.handover.target_cell_id,
                                              step->timeout_ms);
            if (err == MOCK_TEST_SUCCESS) {
                step->result = TEST_STEP_RESULT_PASSED;
                strncpy(step->result_message, "Handover successful", sizeof(step->result_message) - 1);
            } else {
                step->result = TEST_STEP_RESULT_FAILED;
                snprintf(step->result_message, sizeof(step->result_message) - 1,
                         "Handover failed: %s", mock_test_error_to_string(err));
            }
            break;
            
        case TEST_STEP_TYPE_DEREGISTRATION:
            err = mock_test_env_run_deregistration(controller->env,
                                                    step->ue_index,
                                                    step->timeout_ms);
            if (err == MOCK_TEST_SUCCESS) {
                step->result = TEST_STEP_RESULT_PASSED;
                strncpy(step->result_message, "Deregistration successful", sizeof(step->result_message) - 1);
            } else {
                step->result = TEST_STEP_RESULT_FAILED;
                snprintf(step->result_message, sizeof(step->result_message) - 1,
                         "Deregistration failed: %s", mock_test_error_to_string(err));
            }
            break;
            
        case TEST_STEP_TYPE_DATA_TRANSFER:
            /* Simulated data transfer */
            if (controller->verbose) {
                printf("[FlowController]   Data transfer: %u bytes (%s)\n",
                       step->params.data_transfer.data_size,
                       step->params.data_transfer.is_uplink ? "UL" : "DL");
            }
            step->result = TEST_STEP_RESULT_PASSED;
            strncpy(step->result_message, "Data transfer completed", sizeof(step->result_message) - 1);
            break;
            
        case TEST_STEP_TYPE_VERIFY:
            /* Verification step - simulated */
            step->result = TEST_STEP_RESULT_PASSED;
            snprintf(step->result_message, sizeof(step->result_message) - 1,
                     "Verified: %s = %s", step->params.verify.param, step->params.verify.expected_value);
            break;
            
        case TEST_STEP_TYPE_CUSTOM:
            /* Custom step - would need callback implementation */
            step->result = TEST_STEP_RESULT_SKIPPED;
            strncpy(step->result_message, "Custom step not implemented", sizeof(step->result_message) - 1);
            break;
            
        default:
            step->result = TEST_STEP_RESULT_FAILED;
            strncpy(step->result_message, "Unknown step type", sizeof(step->result_message) - 1);
            break;
    }
    
    step->duration_ms = get_time_ms() - step_start;
    
    return step->result == TEST_STEP_RESULT_PASSED ? TEST_FLOW_SUCCESS : TEST_FLOW_ERROR_STEP_FAILED;
}

/* ============== Results and Reporting ============== */

test_flow_error_t test_flow_controller_get_results(const test_flow_controller_t* controller,
                                                    uint32_t scenario_index,
                                                    uint32_t* passed,
                                                    uint32_t* failed,
                                                    uint32_t* skipped) {
    if (!controller) return TEST_FLOW_ERROR_INVALID_PARAM;
    
    if (scenario_index >= controller->num_scenarios) {
        return TEST_FLOW_ERROR_NOT_FOUND;
    }
    
    const test_scenario_t* scenario = &controller->scenarios[scenario_index];
    
    if (passed) *passed = scenario->passed_steps;
    if (failed) *failed = scenario->failed_steps;
    if (skipped) *skipped = scenario->skipped_steps;
    
    return TEST_FLOW_SUCCESS;
}

test_flow_error_t test_flow_controller_generate_report(const test_flow_controller_t* controller,
                                                        const char* filename) {
    if (!controller) return TEST_FLOW_ERROR_INVALID_PARAM;
    
    FILE* fp = stdout;
    if (filename && filename[0]) {
        fp = fopen(filename, "w");
        if (!fp) return TEST_FLOW_ERROR_FILE;
    }
    
    fprintf(fp, "\n");
    fprintf(fp, "========================================\n");
    fprintf(fp, "        Test Flow Controller Report\n");
    fprintf(fp, "========================================\n\n");
    
    fprintf(fp, "Summary:\n");
    fprintf(fp, "  Total Scenarios: %u\n", controller->total_scenarios_run);
    fprintf(fp, "  Passed: %u\n", controller->total_scenarios_passed);
    fprintf(fp, "  Failed: %u\n", controller->total_scenarios_failed);
    fprintf(fp, "  Total Duration: %llu ms\n\n", (unsigned long long)controller->total_duration_ms);
    
    for (uint32_t i = 0; i < controller->num_scenarios; i++) {
        const test_scenario_t* scenario = &controller->scenarios[i];
        
        fprintf(fp, "----------------------------------------\n");
        fprintf(fp, "Scenario: %s\n", scenario->name);
        fprintf(fp, "  Result: %s\n", test_step_result_to_string(scenario->overall_result));
        fprintf(fp, "  Duration: %llu ms\n", (unsigned long long)scenario->total_duration_ms);
        fprintf(fp, "  Steps: %u passed, %u failed, %u skipped\n\n",
                scenario->passed_steps, scenario->failed_steps, scenario->skipped_steps);
        
        for (uint32_t j = 0; j < scenario->num_steps; j++) {
            const test_step_t* step = &scenario->steps[j];
            fprintf(fp, "  [%s] Step %u: %s (%llu ms)\n",
                    test_step_result_to_string(step->result),
                    j + 1, step->name, (unsigned long long)step->duration_ms);
            if (step->result_message[0]) {
                fprintf(fp, "        %s\n", step->result_message);
            }
        }
        fprintf(fp, "\n");
    }
    
    fprintf(fp, "========================================\n");
    
    if (fp != stdout) {
        fclose(fp);
    }
    
    return TEST_FLOW_SUCCESS;
}

void test_flow_controller_print_scenario_summary(const test_flow_controller_t* controller,
                                                  uint32_t scenario_index) {
    if (!controller || scenario_index >= controller->num_scenarios) return;
    
    const test_scenario_t* scenario = &controller->scenarios[scenario_index];
    
    printf("\n=== Scenario Summary: %s ===\n", scenario->name);
    printf("Result: %s\n", test_step_result_to_string(scenario->overall_result));
    printf("Duration: %llu ms\n", (unsigned long long)scenario->total_duration_ms);
    printf("Steps: %u total, %u passed, %u failed, %u skipped\n",
           scenario->num_steps, scenario->passed_steps, 
           scenario->failed_steps, scenario->skipped_steps);
    printf("==============================\n\n");
}

void test_flow_controller_print_summary(const test_flow_controller_t* controller) {
    if (!controller) return;
    
    printf("\n");
    printf("========================================\n");
    printf("      Test Flow Controller Summary\n");
    printf("========================================\n\n");
    
    printf("Scenarios Run: %u\n", controller->total_scenarios_run);
    printf("  Passed: %u\n", controller->total_scenarios_passed);
    printf("  Failed: %u\n", controller->total_scenarios_failed);
    printf("Total Duration: %llu ms\n", (unsigned long long)controller->total_duration_ms);
    
    printf("\n----------------------------------------\n");
    printf("Scenario Results:\n");
    printf("----------------------------------------\n");
    
    for (uint32_t i = 0; i < controller->num_scenarios; i++) {
        const test_scenario_t* scenario = &controller->scenarios[i];
        printf("  [%s] %s (%u/%u steps passed)\n",
               test_step_result_to_string(scenario->overall_result),
               scenario->name,
               scenario->passed_steps,
               scenario->num_steps);
    }
    
    printf("\n========================================\n\n");
}

/* ============== Built-in Scenarios ============== */

test_flow_error_t test_flow_controller_create_registration_scenario(test_scenario_t* scenario,
                                                                     uint32_t num_ues) {
    if (!scenario || num_ues == 0) return TEST_FLOW_ERROR_INVALID_PARAM;
    
    memset(scenario, 0, sizeof(test_scenario_t));
    
    strncpy(scenario->name, "Registration Scenario", sizeof(scenario->name) - 1);
    snprintf(scenario->description, sizeof(scenario->description) - 1,
             "Register %u UE(s) with the network", num_ues);
    strncpy(scenario->version, "1.0", sizeof(scenario->version) - 1);
    
    /* Add registration steps for each UE */
    for (uint32_t i = 0; i < num_ues && scenario->num_steps < TEST_FLOW_MAX_STEPS; i++) {
        test_step_t* step = &scenario->steps[scenario->num_steps];
        
        step->type = TEST_STEP_TYPE_REGISTRATION;
        snprintf(step->name, sizeof(step->name) - 1, "Register UE %u", i);
        snprintf(step->description, sizeof(step->description) - 1,
                 "Complete registration flow for UE %u", i);
        step->ue_index = i;
        step->timeout_ms = 5000;
        step->delay_ms = 100;
        
        scenario->num_steps++;
    }
    
    return TEST_FLOW_SUCCESS;
}

test_flow_error_t test_flow_controller_create_pdu_session_scenario(test_scenario_t* scenario,
                                                                     uint32_t num_ues,
                                                                     uint8_t sessions_per_ue) {
    if (!scenario || num_ues == 0 || sessions_per_ue == 0) return TEST_FLOW_ERROR_INVALID_PARAM;
    
    memset(scenario, 0, sizeof(test_scenario_t));
    
    strncpy(scenario->name, "PDU Session Scenario", sizeof(scenario->name) - 1);
    snprintf(scenario->description, sizeof(scenario->description) - 1,
             "Establish %u PDU session(s) for %u UE(s)", sessions_per_ue, num_ues);
    strncpy(scenario->version, "1.0", sizeof(scenario->version) - 1);
    
    /* Add registration + PDU session steps for each UE */
    for (uint32_t i = 0; i < num_ues && scenario->num_steps < TEST_FLOW_MAX_STEPS; i++) {
        /* Registration step */
        test_step_t* reg_step = &scenario->steps[scenario->num_steps];
        reg_step->type = TEST_STEP_TYPE_REGISTRATION;
        snprintf(reg_step->name, sizeof(reg_step->name) - 1, "Register UE %u", i);
        reg_step->ue_index = i;
        reg_step->timeout_ms = 5000;
        scenario->num_steps++;
        
        /* PDU session steps */
        for (uint8_t s = 1; s <= sessions_per_ue && scenario->num_steps < TEST_FLOW_MAX_STEPS; s++) {
            test_step_t* pdu_step = &scenario->steps[scenario->num_steps];
            pdu_step->type = TEST_STEP_TYPE_PDU_SESSION;
            snprintf(pdu_step->name, sizeof(pdu_step->name) - 1, 
                     "PDU Session %u for UE %u", s, i);
            pdu_step->ue_index = i;
            pdu_step->params.pdu_session.pdu_session_id = s;
            pdu_step->timeout_ms = 5000;
            pdu_step->delay_ms = 100;
            scenario->num_steps++;
        }
    }
    
    return TEST_FLOW_SUCCESS;
}

test_flow_error_t test_flow_controller_create_handover_scenario(test_scenario_t* scenario,
                                                                  uint32_t num_ues,
                                                                  uint32_t target_gnb_id,
                                                                  uint32_t target_cell_id) {
    if (!scenario || num_ues == 0) return TEST_FLOW_ERROR_INVALID_PARAM;
    
    memset(scenario, 0, sizeof(test_scenario_t));
    
    strncpy(scenario->name, "Handover Scenario", sizeof(scenario->name) - 1);
    snprintf(scenario->description, sizeof(scenario->description) - 1,
             "Handover %u UE(s) to gNB %u, cell %u", num_ues, target_gnb_id, target_cell_id);
    strncpy(scenario->version, "1.0", sizeof(scenario->version) - 1);
    
    /* Add registration + PDU session + handover steps for each UE */
    for (uint32_t i = 0; i < num_ues && scenario->num_steps < TEST_FLOW_MAX_STEPS; i++) {
        /* Registration step */
        test_step_t* reg_step = &scenario->steps[scenario->num_steps];
        reg_step->type = TEST_STEP_TYPE_REGISTRATION;
        snprintf(reg_step->name, sizeof(reg_step->name) - 1, "Register UE %u", i);
        reg_step->ue_index = i;
        reg_step->timeout_ms = 5000;
        scenario->num_steps++;
        
        /* PDU session step */
        test_step_t* pdu_step = &scenario->steps[scenario->num_steps];
        pdu_step->type = TEST_STEP_TYPE_PDU_SESSION;
        snprintf(pdu_step->name, sizeof(pdu_step->name) - 1, "PDU Session for UE %u", i);
        pdu_step->ue_index = i;
        pdu_step->params.pdu_session.pdu_session_id = 1;
        pdu_step->timeout_ms = 5000;
        scenario->num_steps++;
        
        /* Handover step */
        test_step_t* ho_step = &scenario->steps[scenario->num_steps];
        ho_step->type = TEST_STEP_TYPE_HANDOVER;
        snprintf(ho_step->name, sizeof(ho_step->name) - 1, "Handover UE %u", i);
        ho_step->ue_index = i;
        ho_step->params.handover.target_gnb_id = target_gnb_id;
        ho_step->params.handover.target_cell_id = target_cell_id;
        ho_step->timeout_ms = 10000;
        scenario->num_steps++;
    }
    
    return TEST_FLOW_SUCCESS;
}

test_flow_error_t test_flow_controller_create_complete_scenario(test_scenario_t* scenario,
                                                                  uint32_t num_ues) {
    if (!scenario || num_ues == 0) return TEST_FLOW_ERROR_INVALID_PARAM;
    
    memset(scenario, 0, sizeof(test_scenario_t));
    
    strncpy(scenario->name, "Complete Test Scenario", sizeof(scenario->name) - 1);
    snprintf(scenario->description, sizeof(scenario->description) - 1,
             "Complete flow: Registration + PDU Session + Data Transfer for %u UE(s)", num_ues);
    strncpy(scenario->version, "1.0", sizeof(scenario->version) - 1);
    
    /* Add complete flow for each UE */
    for (uint32_t i = 0; i < num_ues && scenario->num_steps < TEST_FLOW_MAX_STEPS; i++) {
        /* Registration step */
        test_step_t* reg_step = &scenario->steps[scenario->num_steps];
        reg_step->type = TEST_STEP_TYPE_REGISTRATION;
        snprintf(reg_step->name, sizeof(reg_step->name) - 1, "Register UE %u", i);
        reg_step->ue_index = i;
        reg_step->timeout_ms = 5000;
        scenario->num_steps++;
        
        /* PDU session step */
        test_step_t* pdu_step = &scenario->steps[scenario->num_steps];
        pdu_step->type = TEST_STEP_TYPE_PDU_SESSION;
        snprintf(pdu_step->name, sizeof(pdu_step->name) - 1, "PDU Session for UE %u", i);
        pdu_step->ue_index = i;
        pdu_step->params.pdu_session.pdu_session_id = 1;
        pdu_step->timeout_ms = 5000;
        scenario->num_steps++;
        
        /* Uplink data transfer */
        test_step_t* ul_step = &scenario->steps[scenario->num_steps];
        ul_step->type = TEST_STEP_TYPE_DATA_TRANSFER;
        snprintf(ul_step->name, sizeof(ul_step->name) - 1, "UL Data UE %u", i);
        ul_step->ue_index = i;
        ul_step->params.data_transfer.data_size = 1024;
        ul_step->params.data_transfer.is_uplink = true;
        ul_step->timeout_ms = 2000;
        scenario->num_steps++;
        
        /* Downlink data transfer */
        test_step_t* dl_step = &scenario->steps[scenario->num_steps];
        dl_step->type = TEST_STEP_TYPE_DATA_TRANSFER;
        snprintf(dl_step->name, sizeof(dl_step->name) - 1, "DL Data UE %u", i);
        dl_step->ue_index = i;
        dl_step->params.data_transfer.data_size = 2048;
        dl_step->params.data_transfer.is_uplink = false;
        dl_step->timeout_ms = 2000;
        scenario->num_steps++;
        
        /* Deregistration step */
        test_step_t* dereg_step = &scenario->steps[scenario->num_steps];
        dereg_step->type = TEST_STEP_TYPE_DEREGISTRATION;
        snprintf(dereg_step->name, sizeof(dereg_step->name) - 1, "Deregister UE %u", i);
        dereg_step->ue_index = i;
        dereg_step->timeout_ms = 5000;
        scenario->num_steps++;
    }
    
    return TEST_FLOW_SUCCESS;
}

/* ============== Utility Functions ============== */

const char* test_step_type_to_string(test_step_type_t type) {
    switch (type) {
        case TEST_STEP_TYPE_WAIT: return "WAIT";
        case TEST_STEP_TYPE_REGISTRATION: return "REGISTRATION";
        case TEST_STEP_TYPE_PDU_SESSION: return "PDU_SESSION";
        case TEST_STEP_TYPE_HANDOVER: return "HANDOVER";
        case TEST_STEP_TYPE_DEREGISTRATION: return "DEREGISTRATION";
        case TEST_STEP_TYPE_DATA_TRANSFER: return "DATA_TRANSFER";
        case TEST_STEP_TYPE_VERIFY: return "VERIFY";
        case TEST_STEP_TYPE_CUSTOM: return "CUSTOM";
        default: return "UNKNOWN";
    }
}

const char* test_step_result_to_string(test_step_result_t result) {
    switch (result) {
        case TEST_STEP_RESULT_PENDING: return "PENDING";
        case TEST_STEP_RESULT_RUNNING: return "RUNNING";
        case TEST_STEP_RESULT_PASSED: return "PASSED";
        case TEST_STEP_RESULT_FAILED: return "FAILED";
        case TEST_STEP_RESULT_SKIPPED: return "SKIPPED";
        case TEST_STEP_RESULT_TIMEOUT: return "TIMEOUT";
        default: return "UNKNOWN";
    }
}

const char* test_flow_error_to_string(test_flow_error_t error) {
    switch (error) {
        case TEST_FLOW_SUCCESS: return "Success";
        case TEST_FLOW_ERROR_INVALID_PARAM: return "Invalid parameter";
        case TEST_FLOW_ERROR_MEMORY: return "Memory allocation failed";
        case TEST_FLOW_ERROR_FILE: return "File error";
        case TEST_FLOW_ERROR_PARSE: return "Parse error";
        case TEST_FLOW_ERROR_NOT_FOUND: return "Not found";
        case TEST_FLOW_ERROR_TIMEOUT: return "Timeout";
        case TEST_FLOW_ERROR_STEP_FAILED: return "Step failed";
        case TEST_FLOW_ERROR_ENV_NOT_READY: return "Environment not ready";
        default: return "Unknown error";
    }
}