/*
 * 5G UE Simulation Application
 * F1AP Message Tests
 * 3GPP TS 38.473
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../src/protocol/f1ap_messages.h"
#include "../src/protocol/asn1_per.h"

/* Test counters */
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("Running test: %s...\n", #name); \
    test_##name(); \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("  FAILED: %s (line %d)\n", #cond, __LINE__); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("  FAILED: %s == %s (line %d)\n", #a, #b, __LINE__); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_STR_EQ(a, b) do { \
    if (strcmp((a), (b)) != 0) { \
        printf("  FAILED: \"%s\" == \"%s\" (line %d)\n", a, b, __LINE__); \
        tests_failed++; \
        return; \
    } \
} while(0)

/* ============== Unit Tests ============== */

TEST(f1ap_message_type_to_string) {
    const char* str;
    
    str = f1ap_message_type_to_string(F1AP_MSG_F1_SETUP_REQUEST);
    ASSERT_STR_EQ(str, "F1SetupRequest");
    
    str = f1ap_message_type_to_string(F1AP_MSG_F1_SETUP_RESPONSE);
    ASSERT_STR_EQ(str, "F1SetupResponse");
    
    str = f1ap_message_type_to_string(F1AP_MSG_UE_CONTEXT_SETUP_REQUEST);
    ASSERT_STR_EQ(str, "UEContextSetupRequest");
    
    str = f1ap_message_type_to_string(F1AP_MSG_UL_RRC_MESSAGE_TRANSFER);
    ASSERT_STR_EQ(str, "ULRRCMessageTransfer");
    
    str = f1ap_message_type_to_string(F1AP_MSG_MAX);
    ASSERT_STR_EQ(str, "Unknown");
    
    tests_passed++;
}

TEST(f1ap_cause_to_string) {
    f1ap_cause_t cause;
    const char* str;
    
    cause.cause_type = F1AP_CAUSE_RADIO_NETWORK;
    cause.cause_value = F1AP_CAUSE_RADIO_NORMAL_RELEASE;
    str = f1ap_cause_to_string(&cause);
    ASSERT(strstr(str, "RadioNetwork") != NULL);
    
    cause.cause_type = F1AP_CAUSE_TRANSPORT;
    cause.cause_value = F1AP_CAUSE_TRANSPORT_TRANSPORT_RESOURCE_UNAVAILABLE;
    str = f1ap_cause_to_string(&cause);
    ASSERT(strstr(str, "Transport") != NULL);
    
    cause.cause_type = F1AP_CAUSE_PROTOCOL;
    cause.cause_value = F1AP_CAUSE_PROTOCOL_TRANSFER_SYNTAX_ERROR;
    str = f1ap_cause_to_string(&cause);
    ASSERT(strstr(str, "Protocol") != NULL);
    
    cause.cause_type = F1AP_CAUSE_MISC;
    cause.cause_value = F1AP_CAUSE_MISC_HARDWARE_FAILURE;
    str = f1ap_cause_to_string(&cause);
    ASSERT(strstr(str, "Misc") != NULL);
    
    tests_passed++;
}

TEST(f1ap_set_cause) {
    f1ap_cause_t cause;
    
    f1ap_set_cause_radio(&cause, F1AP_CAUSE_RADIO_RLF_DETECTED);
    ASSERT_EQ(cause.cause_type, F1AP_CAUSE_RADIO_NETWORK);
    ASSERT_EQ(cause.cause_value, F1AP_CAUSE_RADIO_RLF_DETECTED);
    
    f1ap_set_cause_transport(&cause, F1AP_CAUSE_TRANSPORT_UNSPECIFIED);
    ASSERT_EQ(cause.cause_type, F1AP_CAUSE_TRANSPORT);
    ASSERT_EQ(cause.cause_value, F1AP_CAUSE_TRANSPORT_UNSPECIFIED);
    
    f1ap_set_cause_protocol(&cause, F1AP_CAUSE_PROTOCOL_SEMANTIC_ERROR);
    ASSERT_EQ(cause.cause_type, F1AP_CAUSE_PROTOCOL);
    ASSERT_EQ(cause.cause_value, F1AP_CAUSE_PROTOCOL_SEMANTIC_ERROR);
    
    f1ap_set_cause_misc(&cause, F1AP_CAUSE_MISC_OM_INTERVENTION);
    ASSERT_EQ(cause.cause_type, F1AP_CAUSE_MISC);
    ASSERT_EQ(cause.cause_value, F1AP_CAUSE_MISC_OM_INTERVENTION);
    
    tests_passed++;
}

TEST(f1ap_init_f1_setup_request) {
    f1ap_message_t msg;
    
    f1ap_init_f1_setup_request(&msg);
    
    ASSERT_EQ(msg.message_type, F1AP_MSG_F1_SETUP_REQUEST);
    ASSERT_EQ(msg.procedure_code, F1AP_PROC_F1_SETUP);
    ASSERT_EQ(msg.criticality, 0);
    ASSERT_EQ(msg.payload.f1_setup_request.gnb_du_rrc_version[0], 15);
    
    tests_passed++;
}

TEST(f1ap_init_f1_setup_response) {
    f1ap_message_t msg;
    
    f1ap_init_f1_setup_response(&msg);
    
    ASSERT_EQ(msg.message_type, F1AP_MSG_F1_SETUP_RESPONSE);
    ASSERT_EQ(msg.procedure_code, F1AP_PROC_F1_SETUP);
    ASSERT_EQ(msg.criticality, 0);
    ASSERT_EQ(msg.payload.f1_setup_response.gnb_cu_rrc_version[0], 15);
    
    tests_passed++;
}

TEST(f1ap_init_ue_context_setup_request) {
    f1ap_message_t msg;
    
    f1ap_init_ue_context_setup_request(&msg);
    
    ASSERT_EQ(msg.message_type, F1AP_MSG_UE_CONTEXT_SETUP_REQUEST);
    ASSERT_EQ(msg.procedure_code, F1AP_PROC_UE_CONTEXT_SETUP);
    ASSERT_EQ(msg.criticality, 0);
    
    tests_passed++;
}

TEST(f1ap_init_dl_rrc_message_transfer) {
    f1ap_message_t msg;
    
    f1ap_init_dl_rrc_message_transfer(&msg);
    
    ASSERT_EQ(msg.message_type, F1AP_MSG_DL_RRC_MESSAGE_TRANSFER);
    ASSERT_EQ(msg.procedure_code, F1AP_PROC_DL_RRC_MESSAGE_TRANSFER);
    ASSERT_EQ(msg.criticality, 1);
    
    tests_passed++;
}

TEST(f1ap_init_ul_rrc_message_transfer) {
    f1ap_message_t msg;
    
    f1ap_init_ul_rrc_message_transfer(&msg);
    
    ASSERT_EQ(msg.message_type, F1AP_MSG_UL_RRC_MESSAGE_TRANSFER);
    ASSERT_EQ(msg.procedure_code, F1AP_PROC_UL_RRC_MESSAGE_TRANSFER);
    ASSERT_EQ(msg.criticality, 1);
    
    tests_passed++;
}

TEST(f1ap_encode_decode_f1_setup_request) {
    f1ap_message_t msg_out, msg_in;
    uint8_t* buffer = NULL;
    size_t length = 0;
    
    /* Setup message */
    f1ap_init_f1_setup_request(&msg_out);
    
    f1ap_f1_setup_request_t* req = &msg_out.payload.f1_setup_request;
    req->gnb_du_id.gnb_du_id = 0x12345678;
    strncpy((char*)req->gnb_du_id.gnb_du_name, "Test-DU-01", 
            sizeof(req->gnb_du_id.gnb_du_name) - 1);
    
    req->served_cells.num_cells = 1;
    req->served_cells.cells[0].nr_cell_id.nr_cell_id = 0xABCDEF123ULL;
    req->served_cells.cells[0].pci.pci = 42;
    req->served_cells.cells[0].tac.tac = 100;
    req->served_cells.cells[0].num_plmns = 1;
    req->served_cells.cells[0].plmns[0].mcc[0] = 1;
    req->served_cells.cells[0].plmns[0].mcc[1] = 2;
    req->served_cells.cells[0].plmns[0].mcc[2] = 3;
    req->served_cells.cells[0].plmns[0].mnc[0] = 4;
    req->served_cells.cells[0].plmns[0].mnc[1] = 5;
    req->served_cells.cells[0].plmns[0].mnc_length = 2;
    req->served_cells.cells[0].ngran_duplex_mode = 1;
    
    req->ranac = 10;
    
    /* Encode */
    int ret = f1ap_encode_message(&msg_out, &buffer, &length);
    ASSERT_EQ(ret, 0);
    ASSERT(buffer != NULL);
    ASSERT(length > 0);
    
    printf("  Encoded F1 Setup Request: %zu bytes\n", length);
    
    /* Decode */
    ret = f1ap_decode_message(buffer, length, &msg_in);
    ASSERT_EQ(ret, 0);
    
    /* Verify */
    ASSERT_EQ(msg_in.message_type, F1AP_MSG_F1_SETUP_REQUEST);
    ASSERT_EQ(msg_in.procedure_code, F1AP_PROC_F1_SETUP);
    
    f1ap_f1_setup_request_t* decoded_req = &msg_in.payload.f1_setup_request;
    ASSERT_EQ(decoded_req->gnb_du_id.gnb_du_id, 0x12345678);
    ASSERT_STR_EQ((char*)decoded_req->gnb_du_id.gnb_du_name, "Test-DU-01");
    ASSERT_EQ(decoded_req->served_cells.num_cells, 1);
    ASSERT_EQ(decoded_req->served_cells.cells[0].nr_cell_id.nr_cell_id, 0xABCDEF123ULL);
    ASSERT_EQ(decoded_req->served_cells.cells[0].pci.pci, 42);
    ASSERT_EQ(decoded_req->served_cells.cells[0].tac.tac, 100);
    ASSERT_EQ(decoded_req->ranac, 10);
    
    free(buffer);
    tests_passed++;
}

TEST(f1ap_encode_decode_f1_setup_response) {
    f1ap_message_t msg_out, msg_in;
    uint8_t* buffer = NULL;
    size_t length = 0;
    
    /* Setup message */
    f1ap_init_f1_setup_response(&msg_out);
    
    f1ap_f1_setup_response_t* resp = &msg_out.payload.f1_setup_response;
    resp->gnb_cu_id.gnb_cu_id = 0xABCDEF12345ULL;
    strncpy((char*)resp->gnb_cu_id.gnb_cu_name, "Test-CU-CP-01",
            sizeof(resp->gnb_cu_id.gnb_cu_name) - 1);
    
    resp->num_cells_to_activate = 1;
    resp->cells_to_activate[0].nr_cell_id.nr_cell_id = 0xABCDEF123ULL;
    resp->cells_to_activate[0].pci.pci = 42;
    resp->cells_to_activate[0].tac.tac = 100;
    
    resp->transport_layer_address = 0x7F000001;  /* 127.0.0.1 */
    
    /* Encode */
    int ret = f1ap_encode_message(&msg_out, &buffer, &length);
    ASSERT_EQ(ret, 0);
    ASSERT(buffer != NULL);
    ASSERT(length > 0);
    
    printf("  Encoded F1 Setup Response: %zu bytes\n", length);
    
    /* Decode */
    ret = f1ap_decode_message(buffer, length, &msg_in);
    ASSERT_EQ(ret, 0);
    
    /* Verify */
    ASSERT_EQ(msg_in.message_type, F1AP_MSG_F1_SETUP_RESPONSE);
    
    f1ap_f1_setup_response_t* decoded_resp = &msg_in.payload.f1_setup_response;
    ASSERT_EQ(decoded_resp->gnb_cu_id.gnb_cu_id, 0xABCDEF12345ULL);
    ASSERT_STR_EQ((char*)decoded_resp->gnb_cu_id.gnb_cu_name, "Test-CU-CP-01");
    ASSERT_EQ(decoded_resp->num_cells_to_activate, 1);
    ASSERT_EQ(decoded_resp->transport_layer_address, 0x7F000001);
    
    free(buffer);
    tests_passed++;
}

TEST(f1ap_encode_decode_ue_context_setup_request) {
    f1ap_message_t msg_out, msg_in;
    uint8_t* buffer = NULL;
    size_t length = 0;
    
    /* Setup message */
    f1ap_init_ue_context_setup_request(&msg_out);
    
    f1ap_ue_context_setup_request_t* req = &msg_out.payload.ue_context_setup_request;
    req->ue_ids.gnb_cu_ue_f1ap_id = 1;
    req->ue_ids.gnb_du_ue_f1ap_id = 0;
    req->ran_ue_id.ran_ue_id = 0x123456789AULL;
    
    req->plmn.mcc[0] = 0;
    req->plmn.mcc[1] = 0;
    req->plmn.mcc[2] = 1;
    req->plmn.mnc[0] = 0;
    req->plmn.mnc[1] = 1;
    req->plmn.mnc_length = 2;
    
    req->num_drbs_to_setup = 2;
    req->drbs_to_setup[0].drb_id = 1;
    req->drbs_to_setup[0].rlc_mode = 1;  /* AM */
    req->drbs_to_setup[0].num_qos_flows = 1;
    req->drbs_to_setup[0].qos_flows[0].qfi = 1;
    req->drbs_to_setup[0].five_qi[0] = 9;
    
    req->drbs_to_setup[1].drb_id = 2;
    req->drbs_to_setup[1].rlc_mode = 2;  /* UM */
    req->drbs_to_setup[1].num_qos_flows = 1;
    req->drbs_to_setup[1].qos_flows[0].qfi = 2;
    req->drbs_to_setup[1].five_qi[0] = 7;
    
    req->num_srbs_to_setup = 1;
    req->srbs_to_setup[0].srb_id = 1;
    req->srbs_to_setup[0].rlc_mode = 1;
    
    req->ue_ambr_dl = 1000000000ULL;
    req->ue_ambr_ul = 500000000ULL;
    
    req->nr_cell_id.nr_cell_id = 0x123456789ABULL;
    req->serv_cell_idx = 0;
    req->sst = 1;
    req->sd = 0;
    
    /* Encode */
    int ret = f1ap_encode_message(&msg_out, &buffer, &length);
    ASSERT_EQ(ret, 0);
    ASSERT(buffer != NULL);
    ASSERT(length > 0);
    
    printf("  Encoded UE Context Setup Request: %zu bytes\n", length);
    
    /* Decode */
    ret = f1ap_decode_message(buffer, length, &msg_in);
    ASSERT_EQ(ret, 0);
    
    /* Verify */
    ASSERT_EQ(msg_in.message_type, F1AP_MSG_UE_CONTEXT_SETUP_REQUEST);
    
    f1ap_ue_context_setup_request_t* decoded_req = &msg_in.payload.ue_context_setup_request;
    ASSERT_EQ(decoded_req->ue_ids.gnb_cu_ue_f1ap_id, 1);
    ASSERT_EQ(decoded_req->ran_ue_id.ran_ue_id, 0x123456789AULL);
    ASSERT_EQ(decoded_req->num_drbs_to_setup, 2);
    ASSERT_EQ(decoded_req->drbs_to_setup[0].drb_id, 1);
    ASSERT_EQ(decoded_req->drbs_to_setup[0].rlc_mode, 1);
    ASSERT_EQ(decoded_req->drbs_to_setup[1].drb_id, 2);
    ASSERT_EQ(decoded_req->drbs_to_setup[1].rlc_mode, 2);
    ASSERT_EQ(decoded_req->num_srbs_to_setup, 1);
    
    free(buffer);
    tests_passed++;
}

TEST(f1ap_encode_decode_ul_rrc_message_transfer) {
    f1ap_message_t msg_out, msg_in;
    uint8_t* buffer = NULL;
    size_t length = 0;
    
    /* Setup message */
    f1ap_init_ul_rrc_message_transfer(&msg_out);
    
    f1ap_ul_rrc_message_transfer_t* transfer = &msg_out.payload.ul_rrc_message_transfer;
    transfer->ue_ids.gnb_cu_ue_f1ap_id = 1;
    transfer->ue_ids.gnb_du_ue_f1ap_id = 1;
    transfer->srb_id = 1;
    
    /* Simulate RRC container with some data */
    uint8_t rrc_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    memcpy(transfer->rrc_container.rrc_container, rrc_data, sizeof(rrc_data));
    transfer->rrc_container.length = sizeof(rrc_data);
    
    /* Encode */
    int ret = f1ap_encode_message(&msg_out, &buffer, &length);
    ASSERT_EQ(ret, 0);
    ASSERT(buffer != NULL);
    ASSERT(length > 0);
    
    printf("  Encoded UL RRC Message Transfer: %zu bytes\n", length);
    
    /* Decode */
    ret = f1ap_decode_message(buffer, length, &msg_in);
    ASSERT_EQ(ret, 0);
    
    /* Verify */
    ASSERT_EQ(msg_in.message_type, F1AP_MSG_UL_RRC_MESSAGE_TRANSFER);
    
    f1ap_ul_rrc_message_transfer_t* decoded = &msg_in.payload.ul_rrc_message_transfer;
    ASSERT_EQ(decoded->ue_ids.gnb_cu_ue_f1ap_id, 1);
    ASSERT_EQ(decoded->ue_ids.gnb_du_ue_f1ap_id, 1);
    ASSERT_EQ(decoded->srb_id, 1);
    ASSERT_EQ(decoded->rrc_container.length, sizeof(rrc_data));
    ASSERT(memcmp(decoded->rrc_container.rrc_container, rrc_data, sizeof(rrc_data)) == 0);
    
    free(buffer);
    tests_passed++;
}

TEST(f1ap_encode_decode_error_indication) {
    f1ap_message_t msg_out, msg_in;
    uint8_t* buffer = NULL;
    size_t length = 0;
    
    /* Setup message */
    memset(&msg_out, 0, sizeof(msg_out));
    msg_out.message_type = F1AP_MSG_ERROR_INDICATION;
    msg_out.procedure_code = F1AP_PROC_ERROR_INDICATION;
    msg_out.criticality = 1;
    
    f1ap_error_indication_t* err = &msg_out.payload.error_indication;
    err->ue_ids_present = 1;
    err->ue_ids.gnb_cu_ue_f1ap_id = 1;
    err->ue_ids.gnb_du_ue_f1ap_id = 1;
    err->cause_present = 1;
    f1ap_set_cause_protocol(&err->cause, F1AP_CAUSE_PROTOCOL_TRANSFER_SYNTAX_ERROR);
    
    /* Encode */
    int ret = f1ap_encode_message(&msg_out, &buffer, &length);
    ASSERT_EQ(ret, 0);
    ASSERT(buffer != NULL);
    
    printf("  Encoded Error Indication: %zu bytes\n", length);
    
    /* Decode */
    ret = f1ap_decode_message(buffer, length, &msg_in);
    ASSERT_EQ(ret, 0);
    
    /* Verify */
    ASSERT_EQ(msg_in.message_type, F1AP_MSG_ERROR_INDICATION);
    
    f1ap_error_indication_t* decoded = &msg_in.payload.error_indication;
    ASSERT_EQ(decoded->ue_ids_present, 1);
    ASSERT_EQ(decoded->ue_ids.gnb_cu_ue_f1ap_id, 1);
    ASSERT_EQ(decoded->cause_present, 1);
    ASSERT_EQ(decoded->cause.cause_type, F1AP_CAUSE_PROTOCOL);
    
    free(buffer);
    tests_passed++;
}

/* ============== Test Runner ============== */

int main(void) {
    printf("\n=== F1AP Message Tests ===\n\n");
    
    /* Utility tests */
    RUN_TEST(f1ap_message_type_to_string);
    RUN_TEST(f1ap_cause_to_string);
    RUN_TEST(f1ap_set_cause);
    
    /* Initialization tests */
    RUN_TEST(f1ap_init_f1_setup_request);
    RUN_TEST(f1ap_init_f1_setup_response);
    RUN_TEST(f1ap_init_ue_context_setup_request);
    RUN_TEST(f1ap_init_dl_rrc_message_transfer);
    RUN_TEST(f1ap_init_ul_rrc_message_transfer);
    
    /* Encode/Decode tests */
    RUN_TEST(f1ap_encode_decode_f1_setup_request);
    RUN_TEST(f1ap_encode_decode_f1_setup_response);
    RUN_TEST(f1ap_encode_decode_ue_context_setup_request);
    RUN_TEST(f1ap_encode_decode_ul_rrc_message_transfer);
    RUN_TEST(f1ap_encode_decode_error_indication);
    
    /* Summary */
    printf("\n=== Test Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Total:  %d\n", tests_passed + tests_failed);
    
    return tests_failed > 0 ? 1 : 0;
}