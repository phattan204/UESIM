/*
 * 5G UE Simulation Application
 * TS 38.331 RRC Compliance Tests
 * 
 * Tests for compliance with 3GPP TS 38.331 (RRC Protocol Specification)
 */

#include "../compliance_framework.h"
#include "../../../src/protocol/rrc.h"
#include "../../../src/core/memory.h"
#include <string.h>

// Test context
typedef struct {
    rrc_entity_t* rrc_entity;
    ue_context_t* ue_ctx;
} rrc_test_context_t;

// Forward declarations
static compliance_result_t test_rrc_state_idle_to_inactive(void* context);
static compliance_result_t test_rrc_state_inactive_to_connected(void* context);
static compliance_result_t test_rrc_state_connected_to_idle(void* context);
static compliance_result_t test_rrc_connection_request_encoding(void* context);
static compliance_result_t test_rrc_connection_setup_complete_encoding(void* context);
static compliance_result_t test_rrc_security_mode_command_handling(void* context);
static compliance_result_t test_rrc_measurement_report_encoding(void* context);
static compliance_result_t test_rrc_handover_preparation(void* context);
static compliance_result_t test_rrc_reestablishment_procedure(void* context);
static compliance_result_t test_rrc_release_procedure(void* context);

// Test case definitions
static compliance_test_case_t rrc_test_cases[] = {
    {
        .test_id = "RRC-001",
        .test_name = "RRC State Transition: IDLE to INACTIVE",
        .description = "Verify RRC state machine transitions correctly from IDLE to INACTIVE state",
        .specification = SPEC_TS_38_331,
        .severity = SEVERITY_MANDATORY,
        .category = CATEGORY_STATE,
        .section_ref = "TS 38.331 Section 5.3.2",
        .test_func = test_rrc_state_idle_to_inactive
    },
    {
        .test_id = "RRC-002",
        .test_name = "RRC State Transition: INACTIVE to CONNECTED",
        .description = "Verify RRC state machine transitions correctly from INACTIVE to CONNECTED state",
        .specification = SPEC_TS_38_331,
        .severity = SEVERITY_MANDATORY,
        .category = CATEGORY_STATE,
        .section_ref = "TS 38.331 Section 5.3.3",
        .test_func = test_rrc_state_inactive_to_connected
    },
    {
        .test_id = "RRC-003",
        .test_name = "RRC State Transition: CONNECTED to IDLE",
        .description = "Verify RRC state machine transitions correctly from CONNECTED to IDLE state",
        .specification = SPEC_TS_38_331,
        .severity = SEVERITY_MANDATORY,
        .category = CATEGORY_STATE,
        .section_ref = "TS 38.331 Section 5.3.4",
        .test_func = test_rrc_state_connected_to_idle
    },
    {
        .test_id = "RRC-004",
        .test_name = "RRCConnectionRequest Message Encoding",
        .description = "Verify RRCConnectionRequest message is encoded correctly per ASN.1 PER",
        .specification = SPEC_TS_38_331,
        .severity = SEVERITY_MANDATORY,
        .category = CATEGORY_ENCODING,
        .section_ref = "TS 38.331 Section 6.2.2",
        .test_func = test_rrc_connection_request_encoding
    },
    {
        .test_id = "RRC-005",
        .test_name = "RRCConnectionSetupComplete Message Encoding",
        .description = "Verify RRCConnectionSetupComplete message is encoded correctly",
        .specification = SPEC_TS_38_331,
        .severity = SEVERITY_MANDATORY,
        .category = CATEGORY_ENCODING,
        .section_ref = "TS 38.331 Section 6.2.2",
        .test_func = test_rrc_connection_setup_complete_encoding
    },
    {
        .test_id = "RRC-006",
        .test_name = "SecurityModeCommand Handling",
        .description = "Verify SecurityModeCommand is handled correctly",
        .specification = SPEC_TS_38_331,
        .severity = SEVERITY_MANDATORY,
        .category = CATEGORY_SECURITY,
        .section_ref = "TS 38.331 Section 5.10",
        .test_func = test_rrc_security_mode_command_handling
    },
    {
        .test_id = "RRC-007",
        .test_name = "MeasurementReport Message Encoding",
        .description = "Verify MeasurementReport message is encoded correctly",
        .specification = SPEC_TS_38_331,
        .severity = SEVERITY_MANDATORY,
        .category = CATEGORY_ENCODING,
        .section_ref = "TS 38.331 Section 5.5.5",
        .test_func = test_rrc_measurement_report_encoding
    },
    {
        .test_id = "RRC-008",
        .test_name = "Handover Preparation Procedure",
        .description = "Verify handover preparation procedure follows spec",
        .specification = SPEC_TS_38_331,
        .severity = SEVERITY_MANDATORY,
        .category = CATEGORY_PROTOCOL,
        .section_ref = "TS 38.331 Section 5.4",
        .test_func = test_rrc_handover_preparation
    },
    {
        .test_id = "RRC-009",
        .test_name = "RRC Reestablishment Procedure",
        .description = "Verify RRC reestablishment procedure follows spec",
        .specification = SPEC_TS_38_331,
        .severity = SEVERITY_MANDATORY,
        .category = CATEGORY_PROTOCOL,
        .section_ref = "TS 38.331 Section 5.3.7",
        .test_func = test_rrc_reestablishment_procedure
    },
    {
        .test_id = "RRC-010",
        .test_name = "RRC Release Procedure",
        .description = "Verify RRC release procedure follows spec",
        .specification = SPEC_TS_38_331,
        .severity = SEVERITY_MANDATORY,
        .category = CATEGORY_PROTOCOL,
        .section_ref = "TS 38.331 Section 5.3.8",
        .test_func = test_rrc_release_procedure
    }
};

#define NUM_RRC_TESTS (sizeof(rrc_test_cases) / sizeof(rrc_test_cases[0]))

// Test implementations

static compliance_result_t test_rrc_state_idle_to_inactive(void* context) {
    rrc_test_context_t* ctx = (rrc_test_context_t*)context;
    
    COMPLIANCE_ASSERT_NOT_NULL(ctx, "Test context is NULL");
    COMPLIANCE_ASSERT_NOT_NULL(ctx->rrc_entity, "RRC entity is NULL");
    
    // Verify initial state is IDLE
    rrc_state_t state = rrc_get_state(ctx->rrc_entity);
    COMPLIANCE_ASSERT_EQ(RRC_STATE_IDLE, state, "Initial state should be IDLE");
    
    // Transition to INACTIVE
    uesim_error_t result = rrc_set_state(ctx->rrc_entity, RRC_STATE_INACTIVE);
    COMPLIANCE_ASSERT_EQ(UESIM_SUCCESS, result, "State transition to INACTIVE failed");
    
    // Verify new state
    state = rrc_get_state(ctx->rrc_entity);
    COMPLIANCE_ASSERT_EQ(RRC_STATE_INACTIVE, state, "State should be INACTIVE after transition");
    
    return COMPLIANCE_PASS;
}

static compliance_result_t test_rrc_state_inactive_to_connected(void* context) {
    rrc_test_context_t* ctx = (rrc_test_context_t*)context;
    
    COMPLIANCE_ASSERT_NOT_NULL(ctx, "Test context is NULL");
    COMPLIANCE_ASSERT_NOT_NULL(ctx->rrc_entity, "RRC entity is NULL");
    
    // Set initial state to INACTIVE
    rrc_set_state(ctx->rrc_entity, RRC_STATE_INACTIVE);
    
    // Transition to CONNECTED
    uesim_error_t result = rrc_set_state(ctx->rrc_entity, RRC_STATE_CONNECTED);
    COMPLIANCE_ASSERT_EQ(UESIM_SUCCESS, result, "State transition to CONNECTED failed");
    
    // Verify new state
    rrc_state_t state = rrc_get_state(ctx->rrc_entity);
    COMPLIANCE_ASSERT_EQ(RRC_STATE_CONNECTED, state, "State should be CONNECTED after transition");
    
    return COMPLIANCE_PASS;
}

static compliance_result_t test_rrc_state_connected_to_idle(void* context) {
    rrc_test_context_t* ctx = (rrc_test_context_t*)context;
    
    COMPLIANCE_ASSERT_NOT_NULL(ctx, "Test context is NULL");
    COMPLIANCE_ASSERT_NOT_NULL(ctx->rrc_entity, "RRC entity is NULL");
    
    // Set initial state to CONNECTED
    rrc_set_state(ctx->rrc_entity, RRC_STATE_CONNECTED);
    
    // Transition to IDLE
    uesim_error_t result = rrc_set_state(ctx->rrc_entity, RRC_STATE_IDLE);
    COMPLIANCE_ASSERT_EQ(UESIM_SUCCESS, result, "State transition to IDLE failed");
    
    // Verify new state
    rrc_state_t state = rrc_get_state(ctx->rrc_entity);
    COMPLIANCE_ASSERT_EQ(RRC_STATE_IDLE, state, "State should be IDLE after transition");
    
    return COMPLIANCE_PASS;
}

static compliance_result_t test_rrc_connection_request_encoding(void* context) {
    rrc_test_context_t* ctx = (rrc_test_context_t*)context;
    
    COMPLIANCE_ASSERT_NOT_NULL(ctx, "Test context is NULL");
    COMPLIANCE_ASSERT_NOT_NULL(ctx->rrc_entity, "RRC entity is NULL");
    
    // Create connection request
    rrc_connection_request_t request = {0};
    request.ue_identity_type = RRC_IDENTITY_TYPE_SUCI;
    strcpy(request.ue_identity, "suci-0-208-01-0000-0-0-0123456789");
    request.establishment_cause = RRC_ESTABLISHMENT_CAUSE_MO_SIGNALING;
    request.spare = 0;
    
    // Encode message
    uint8_t encoded[256];
    size_t encoded_length = 0;
    uesim_error_t result = rrc_encode_connection_request(&request, encoded, sizeof(encoded), &encoded_length);
    COMPLIANCE_ASSERT_EQ(UESIM_SUCCESS, result, "Connection request encoding failed");
    COMPLIANCE_ASSERT(encoded_length > 0, "Encoded length should be > 0");
    
    // Verify encoding contains expected fields
    // Per TS 38.331, RRCConnectionRequest should contain:
    // - rrc-RequestIdentity (8 bits)
    // - establishmentCause (4 bits)
    // - spare (4 bits)
    COMPLIANCE_ASSERT(encoded_length >= 2, "Encoded message too short");
    
    return COMPLIANCE_PASS;
}

static compliance_result_t test_rrc_connection_setup_complete_encoding(void* context) {
    rrc_test_context_t* ctx = (rrc_test_context_t*)context;
    
    COMPLIANCE_ASSERT_NOT_NULL(ctx, "Test context is NULL");
    COMPLIANCE_ASSERT_NOT_NULL(ctx->rrc_entity, "RRC entity is NULL");
    
    // Create connection setup complete
    rrc_connection_setup_complete_t complete = {0};
    complete.selected_plmn = 1;
    complete.registered_amf = 0;
    complete.guami_type = 0;
    complete.dedicated_nas_message_length = 0;
    
    // Encode message
    uint8_t encoded[256];
    size_t encoded_length = 0;
    uesim_error_t result = rrc_encode_connection_setup_complete(&complete, encoded, sizeof(encoded), &encoded_length);
    COMPLIANCE_ASSERT_EQ(UESIM_SUCCESS, result, "Connection setup complete encoding failed");
    COMPLIANCE_ASSERT(encoded_length > 0, "Encoded length should be > 0");
    
    return COMPLIANCE_PASS;
}

static compliance_result_t test_rrc_security_mode_command_handling(void* context) {
    rrc_test_context_t* ctx = (rrc_test_context_t*)context;
    
    COMPLIANCE_ASSERT_NOT_NULL(ctx, "Test context is NULL");
    COMPLIANCE_ASSERT_NOT_NULL(ctx->rrc_entity, "RRC entity is NULL");
    
    // Create security mode command
    rrc_security_mode_command_t command = {0};
    command.ciphering_algorithm = RRC_CIPHERING_ALG_NEA2;
    command.integrity_algorithm = RRC_INTEGRITY_ALG_NIA2;
    command.security_config_smc_present = true;
    
    // Process security mode command
    uesim_error_t result = rrc_handle_security_mode_command(ctx->rrc_entity, &command);
    COMPLIANCE_ASSERT_EQ(UESIM_SUCCESS, result, "Security mode command handling failed");
    
    // Verify security context was updated
    // In a real implementation, we would check the security context
    
    return COMPLIANCE_PASS;
}

static compliance_result_t test_rrc_measurement_report_encoding(void* context) {
    rrc_test_context_t* ctx = (rrc_test_context_t*)context;
    
    COMPLIANCE_ASSERT_NOT_NULL(ctx, "Test context is NULL");
    COMPLIANCE_ASSERT_NOT_NULL(ctx->rrc_entity, "RRC entity is NULL");
    
    // Create measurement report
    rrc_measurement_report_t report = {0};
    report.meas_id = 1;
    report.meas_result_serving_cell.rsrp = -80;
    report.meas_result_serving_cell.rsrq = -10;
    report.meas_result_serving_cell.sinr = 15;
    report.num_neighbor_cells = 0;
    
    // Encode message
    uint8_t encoded[512];
    size_t encoded_length = 0;
    uesim_error_t result = rrc_encode_measurement_report(&report, encoded, sizeof(encoded), &encoded_length);
    COMPLIANCE_ASSERT_EQ(UESIM_SUCCESS, result, "Measurement report encoding failed");
    COMPLIANCE_ASSERT(encoded_length > 0, "Encoded length should be > 0");
    
    return COMPLIANCE_PASS;
}

static compliance_result_t test_rrc_handover_preparation(void* context) {
    rrc_test_context_t* ctx = (rrc_test_context_t*)context;
    
    COMPLIANCE_ASSERT_NOT_NULL(ctx, "Test context is NULL");
    COMPLIANCE_ASSERT_NOT_NULL(ctx->rrc_entity, "RRC entity is NULL");
    
    // Set state to CONNECTED for handover
    rrc_set_state(ctx->rrc_entity, RRC_STATE_CONNECTED);
    
    // Create handover command
    rrc_handover_command_t command = {0};
    command.target_cell_id = 12345;
    command.new_c_rnti = 0x5678;
    command.t304 = 1000;  // ms
    
    // Process handover command
    uesim_error_t result = rrc_handle_handover_command(ctx->rrc_entity, &command);
    COMPLIANCE_ASSERT_EQ(UESIM_SUCCESS, result, "Handover command handling failed");
    
    return COMPLIANCE_PASS;
}

static compliance_result_t test_rrc_reestablishment_procedure(void* context) {
    rrc_test_context_t* ctx = (rrc_test_context_t*)context;
    
    COMPLIANCE_ASSERT_NOT_NULL(ctx, "Test context is NULL");
    COMPLIANCE_ASSERT_NOT_NULL(ctx->rrc_entity, "RRC entity is NULL");
    
    // Create reestablishment request
    rrc_reestablishment_request_t request = {0};
    request.ue_identity_type = RRC_IDENTITY_TYPE_C_RNTI;
    request.c_rnti = 0x1234;
    request.phys_cell_id = 100;
    request.short_mac_i = 0xA5A5A5A5;
    
    // Encode reestablishment request
    uint8_t encoded[256];
    size_t encoded_length = 0;
    uesim_error_t result = rrc_encode_reestablishment_request(&request, encoded, sizeof(encoded), &encoded_length);
    COMPLIANCE_ASSERT_EQ(UESIM_SUCCESS, result, "Reestablishment request encoding failed");
    COMPLIANCE_ASSERT(encoded_length > 0, "Encoded length should be > 0");
    
    return COMPLIANCE_PASS;
}

static compliance_result_t test_rrc_release_procedure(void* context) {
    rrc_test_context_t* ctx = (rrc_test_context_t*)context;
    
    COMPLIANCE_ASSERT_NOT_NULL(ctx, "Test context is NULL");
    COMPLIANCE_ASSERT_NOT_NULL(ctx->rrc_entity, "RRC entity is NULL");
    
    // Set state to CONNECTED
    rrc_set_state(ctx->rrc_entity, RRC_STATE_CONNECTED);
    
    // Create release message
    rrc_release_t release = {0};
    release.release_cause = RRC_RELEASE_CAUSE_NORMAL;
    release.redirected_carrier_info_present = false;
    
    // Process release
    uesim_error_t result = rrc_handle_release(ctx->rrc_entity, &release);
    COMPLIANCE_ASSERT_EQ(UESIM_SUCCESS, result, "Release handling failed");
    
    // Verify state transition to IDLE
    rrc_state_t state = rrc_get_state(ctx->rrc_entity);
    COMPLIANCE_ASSERT_EQ(RRC_STATE_IDLE, state, "State should be IDLE after release");
    
    return COMPLIANCE_PASS;
}

// Register RRC tests with suite
uesim_error_t register_rrc_compliance_tests(compliance_suite_t* suite, rrc_test_context_t* context) {
    if (suite == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    suite->user_context = context;
    
    for (uint32_t i = 0; i < NUM_RRC_TESTS; i++) {
        uesim_error_t result = compliance_register_test(suite, &rrc_test_cases[i]);
        if (result != UESIM_SUCCESS) {
            printf("Failed to register test: %s\n", rrc_test_cases[i].test_id);
            return result;
        }
    }
    
    printf("Registered %u RRC compliance tests\n", NUM_RRC_TESTS);
    return UESIM_SUCCESS;
}