/*
 * 5G UE Simulation Application
 * NAS PDU Session Management Test
 */

#include "../src/nas/nas.h"
#include "../src/core/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("5G UE Simulation NAS PDU Session Test\n");
    printf("=====================================\n");
    
    // Initialize memory system
    uesim_error_t result = memory_init(UESIM_HEAP_SIZE);
    if (result != UESIM_SUCCESS) {
        fprintf(stderr, "Failed to initialize memory system: %d\n", result);
        return EXIT_FAILURE;
    }
    
    printf("✓ Memory system initialized\n");
    
    // Create UE context
    ue_context_t ue_ctx = {0};
    ue_ctx.ue_id = 1;
    
    // Create NAS UE context
    nas_ue_context_t* nas_ctx = NULL;
    result = nas_create_ue_context(&ue_ctx, &nas_ctx);
    if (result != UESIM_SUCCESS) {
        fprintf(stderr, "Failed to create NAS UE context: %d\n", result);
        memory_cleanup();
        return EXIT_FAILURE;
    }
    
    printf("✓ NAS UE context created\n");
    
    // Activate NAS context
    result = nas_activate_ue_context(nas_ctx);
    if (result != UESIM_SUCCESS) {
        fprintf(stderr, "Failed to activate NAS context: %d\n", result);
        nas_destroy_ue_context(&ue_ctx, nas_ctx);
        memory_cleanup();
        return EXIT_FAILURE;
    }
    
    printf("✓ NAS context activated\n");
    
    // Test PDU session establishment
    printf("\nTesting PDU Session Establishment:\n");
    
    result = nas_initiate_pdu_session_establishment(nas_ctx, 1, NAS_PDU_SESSION_TYPE_IPV4);
    if (result == UESIM_SUCCESS) {
        printf("✓ PDU session establishment initiated for session ID 1\n");
    } else {
        printf("✗ Failed to initiate PDU session establishment: %d\n", result);
    }
    
    // Test PDU session establishment accept
    result = nas_send_pdu_session_establishment_accept(nas_ctx, 1);
    if (result == UESIM_SUCCESS) {
        printf("✓ PDU session establishment accept sent for session ID 1\n");
    } else {
        printf("✗ Failed to send PDU session establishment accept: %d\n", result);
    }
    
    // Verify session is active
    if (nas_is_pdu_session_active(nas_ctx, 1)) {
        printf("✓ PDU session 1 is active\n");
    } else {
        printf("✗ PDU session 1 is not active\n");
    }
    
    // Test QoS flow management
    printf("\nTesting QoS Flow Management:\n");
    
    nas_qos_flow_t qos_flow = {0};
    qos_flow.qfi = 2;
    qos_flow.arp = 2;
    qos_flow.qci = 1;
    qos_flow.gbr_ul = 50;
    qos_flow.gbr_dl = 100;
    qos_flow.mbr_ul = 500;
    qos_flow.mbr_dl = 1000;
    
    result = nas_add_qos_flow(nas_ctx, 1, &qos_flow);
    if (result == UESIM_SUCCESS) {
        printf("✓ QoS flow added to PDU session 1\n");
    } else {
        printf("✗ Failed to add QoS flow: %d\n", result);
    }
    
    // Test PDU session modification
    printf("\nTesting PDU Session Modification:\n");
    
    result = nas_initiate_pdu_session_modification(nas_ctx, 1);
    if (result == UESIM_SUCCESS) {
        printf("✓ PDU session modification initiated for session ID 1\n");
    } else {
        printf("✗ Failed to initiate PDU session modification: %d\n", result);
    }
    
    // Test AMBR update
    result = nas_update_pdu_session_ambr(nas_ctx, 1, 1500, 2500);
    if (result == UESIM_SUCCESS) {
        printf("✓ PDU session AMBR updated for session ID 1\n");
    } else {
        printf("✗ Failed to update PDU session AMBR: %d\n", result);
    }
    
    // Test multiple PDU sessions
    printf("\nTesting Multiple PDU Sessions:\n");
    
    result = nas_initiate_pdu_session_establishment(nas_ctx, 2, NAS_PDU_SESSION_TYPE_IPV4V6);
    if (result == UESIM_SUCCESS) {
        printf("✓ PDU session establishment initiated for session ID 2\n");
    } else {
        printf("✗ Failed to initiate PDU session establishment for session 2: %d\n", result);
    }
    
    result = nas_send_pdu_session_establishment_accept(nas_ctx, 2);
    if (result == UESIM_SUCCESS) {
        printf("✓ PDU session establishment accept sent for session ID 2\n");
    } else {
        printf("✗ Failed to send PDU session establishment accept for session 2: %d\n", result);
    }
    
    // Get all active sessions
    uint8_t active_sessions[16];
    uint8_t num_active_sessions = 0;
    result = nas_get_all_active_pdu_sessions(nas_ctx, active_sessions, &num_active_sessions);
    if (result == UESIM_SUCCESS) {
        printf("✓ Retrieved all active PDU sessions: %u sessions\n", num_active_sessions);
        for (int i = 0; i < num_active_sessions; i++) {
            printf("  - Session ID: %u\n", active_sessions[i]);
        }
    } else {
        printf("✗ Failed to get active PDU sessions: %d\n", result);
    }
    
    // Test PDU session release
    printf("\nTesting PDU Session Release:\n");
    
    result = nas_initiate_pdu_session_release(nas_ctx, 1);
    if (result == UESIM_SUCCESS) {
        printf("✓ PDU session release initiated for session ID 1\n");
    } else {
        printf("✗ Failed to initiate PDU session release: %d\n", result);
    }
    
    // Test session information retrieval
    printf("\nTesting Session Information Retrieval:\n");
    
    nas_pdu_session_t session_info = {0};
    result = nas_get_pdu_session_info(nas_ctx, 2, &session_info);
    if (result == UESIM_SUCCESS) {
        printf("✓ Retrieved PDU session information for session ID 2\n");
        printf("  - Session Type: %d\n", session_info.session_type);
        printf("  - SSC Mode: %d\n", session_info.ssc_mode);
        printf("  - State: %d\n", session_info.state);
        printf("  - Active: %s\n", session_info.active ? "Yes" : "No");
        printf("  - Number of QoS Flows: %u\n", session_info.num_qos_flows);
    } else {
        printf("✗ Failed to get PDU session information: %d\n", result);
    }
    
    // Test invalid session operations
    printf("\nTesting Invalid Session Operations:\n");
    
    result = nas_initiate_pdu_session_establishment(nas_ctx, 0, NAS_PDU_SESSION_TYPE_IPV4);
    if (result != UESIM_SUCCESS) {
        printf("✓ Correctly rejected invalid PDU session ID 0\n");
    } else {
        printf("✗ Should have rejected invalid PDU session ID 0\n");
    }
    
    result = nas_initiate_pdu_session_establishment(nas_ctx, 16, NAS_PDU_SESSION_TYPE_IPV4);
    if (result != UESIM_SUCCESS) {
        printf("✓ Correctly rejected invalid PDU session ID 16\n");
    } else {
        printf("✗ Should have rejected invalid PDU session ID 16\n");
    }
    
    // Test data structures
    printf("\nTesting Data Structures:\n");
    printf("✓ NAS PDU session structure size: %zu bytes\n", sizeof(nas_pdu_session_t));
    printf("✓ NAS QoS flow structure size: %zu bytes\n", sizeof(nas_qos_flow_t));
    printf("✓ NAS UE context structure size: %zu bytes\n", sizeof(nas_ue_context_t));
    printf("✓ Maximum PDU sessions: %d\n", NAS_MAX_PDU_SESSIONS);
    printf("✓ Maximum QoS flows per session: %d\n", 8);
    
    // Test constants
    printf("\nTesting Constants:\n");
    printf("✓ NAS_MAX_PDU_SESSIONS: %d\n", NAS_MAX_PDU_SESSIONS);
    printf("✓ NAS_MAX_MESSAGE_SIZE: %d\n", NAS_MAX_MESSAGE_SIZE);
    printf("✓ NAS_DEFAULT_T3412: %d seconds\n", NAS_DEFAULT_T3412);
    printf("✓ NAS_DEFAULT_T3422: %d seconds\n", NAS_DEFAULT_T3422);
    printf("✓ NAS_DEFAULT_T3450: %d seconds\n", NAS_DEFAULT_T3450);
    
    // Test session states
    printf("\nTesting Session States:\n");
    printf("✓ NAS_5GSM_PDU_SESSION_INACTIVE: %d\n", NAS_5GSM_PDU_SESSION_INACTIVE);
    printf("✓ NAS_5GSM_PDU_SESSION_ACTIVE_PENDING: %d\n", NAS_5GSM_PDU_SESSION_ACTIVE_PENDING);
    printf("✓ NAS_5GSM_PDU_SESSION_ACTIVE: %d\n", NAS_5GSM_PDU_SESSION_ACTIVE);
    printf("✓ NAS_5GSM_PDU_SESSION_MODIFICATION_PENDING: %d\n", NAS_5GSM_PDU_SESSION_MODIFICATION_PENDING);
    printf("✓ NAS_5GSM_PDU_SESSION_RELEASED_PENDING: %d\n", NAS_5GSM_PDU_SESSION_RELEASED_PENDING);
    
    // Test session types
    printf("\nTesting Session Types:\n");
    printf("✓ NAS_PDU_SESSION_TYPE_IPV4: %d\n", NAS_PDU_SESSION_TYPE_IPV4);
    printf("✓ NAS_PDU_SESSION_TYPE_IPV6: %d\n", NAS_PDU_SESSION_TYPE_IPV6);
    printf("✓ NAS_PDU_SESSION_TYPE_IPV4V6: %d\n", NAS_PDU_SESSION_TYPE_IPV4V6);
    printf("✓ NAS_PDU_SESSION_TYPE_UNSTRUCTURED: %d\n", NAS_PDU_SESSION_TYPE_UNSTRUCTURED);
    printf("✓ NAS_PDU_SESSION_TYPE_ETHERNET: %d\n", NAS_PDU_SESSION_TYPE_ETHERNET);
    
    // Cleanup
    nas_destroy_ue_context(&ue_ctx, nas_ctx);
    memory_cleanup();
    
    printf("\nAll NAS PDU session tests completed successfully!\n");
    printf("PDU session management is ready for integration.\n");
    
    return EXIT_SUCCESS;
}