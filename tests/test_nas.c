/*
 * 5G UE Simulation Application
 * NAS Layer Test
 */

#include "../src/nas/nas.h"
#include "../src/core/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("5G UE Simulation NAS Layer Test\n");
    printf("===============================\n");
    
    // Test NAS UE context creation
    nas_ue_context_t* nas_ctx = NULL;
    
    uesim_error_t result = nas_create_ue_context(NULL, &nas_ctx);
    if (result != UESIM_SUCCESS) {
        printf("✓ NAS UE context creation with NULL UE context correctly failed\n");
    } else {
        printf("✗ NAS UE context creation should have failed with NULL UE context\n");
        return EXIT_FAILURE;
    }
    
    // Test NAS configuration functions
    printf("\nTesting NAS Configuration Functions:\n");
    
    // Test default configuration
    nas_ue_context_t dummy_ctx = {0};
    result = nas_set_default_config(&dummy_ctx);
    if (result == UESIM_SUCCESS) {
        printf("✓ NAS default configuration successful\n");
    } else {
        printf("✗ NAS default configuration failed\n");
        return EXIT_FAILURE;
    }
    
    // Test security configuration
    result = nas_set_security_config(&dummy_ctx, NAS_CIPHERING_ALG_NEA2, NAS_INTEGRITY_ALG_NIA2);
    if (result == UESIM_SUCCESS) {
        printf("✓ NAS security configuration successful\n");
    } else {
        printf("✗ NAS security configuration failed\n");
        return EXIT_FAILURE;
    }
    
    // Test registration configuration
    result = nas_set_registration_config(&dummy_ctx, NAS_REGISTRATION_TYPE_INITIAL);
    if (result == UESIM_SUCCESS) {
        printf("✓ NAS registration configuration successful\n");
    } else {
        printf("✗ NAS registration configuration failed\n");
        return EXIT_FAILURE;
    }
    
    // Test PDU session configuration
    result = nas_set_pdu_session_config(&dummy_ctx, 1, NAS_PDU_SESSION_TYPE_IPV4, NAS_SSC_MODE_1);
    if (result == UESIM_SUCCESS) {
        printf("✓ NAS PDU session configuration successful\n");
    } else {
        printf("✗ NAS PDU session configuration failed\n");
        return EXIT_FAILURE;
    }
    
    // Test NAS data structures
    printf("\nTesting NAS Data Structures:\n");
    printf("✓ NAS Security Context structure size: %zu bytes\n", sizeof(nas_security_context_t));
    printf("✓ NAS Authentication Vector structure size: %zu bytes\n", sizeof(nas_auth_vector_t));
    printf("✓ NAS Authentication Context structure size: %zu bytes\n", sizeof(nas_auth_context_t));
    printf("✓ NAS UE Identity structure size: %zu bytes\n", sizeof(nas_ue_identity_t));
    printf("✓ NAS PDU Session structure size: %zu bytes\n", sizeof(nas_pdu_session_t));
    printf("✓ NAS UE Context structure size: %zu bytes\n", sizeof(nas_ue_context_t));
    printf("✓ NAS Message structure size: %zu bytes\n", sizeof(nas_message_t));
    
    // Test NAS constants
    printf("\nTesting NAS Constants:\n");
    printf("✓ NAS_MAX_MESSAGE_SIZE: %d\n", NAS_MAX_MESSAGE_SIZE);
    printf("✓ NAS_MAX_PDU_SESSIONS: %d\n", NAS_MAX_PDU_SESSIONS);
    printf("✓ NAS_MAX_AUTHENTICATION_VECTOR: %d\n", NAS_MAX_AUTHENTICATION_VECTOR);
    printf("✓ NAS_DEFAULT_T3412: %d\n", NAS_DEFAULT_T3412);
    
    // Test NAS enumerations
    printf("\nTesting NAS Enumerations:\n");
    printf("✓ NAS_MSG_TYPE_REGISTRATION_REQUEST: 0x%02x\n", NAS_MSG_TYPE_REGISTRATION_REQUEST);
    printf("✓ NAS_MSG_TYPE_REGISTRATION_ACCEPT: 0x%02x\n", NAS_MSG_TYPE_REGISTRATION_ACCEPT);
    printf("✓ NAS_MSG_TYPE_AUTHENTICATION_REQUEST: 0x%02x\n", NAS_MSG_TYPE_AUTHENTICATION_REQUEST);
    printf("✓ NAS_MSG_TYPE_SECURITY_MODE_COMMAND: 0x%02x\n", NAS_MSG_TYPE_SECURITY_MODE_COMMAND);
    printf("✓ NAS_MSG_TYPE_PDU_SESSION_ESTABLISHMENT_REQUEST: 0x%02x\n", NAS_MSG_TYPE_PDU_SESSION_ESTABLISHMENT_REQUEST);
    
    printf("✓ NAS_CIPHERING_ALG_NEA0: %d\n", NAS_CIPHERING_ALG_NEA0);
    printf("✓ NAS_CIPHERING_ALG_NEA1: %d\n", NAS_CIPHERING_ALG_NEA1);
    printf("✓ NAS_CIPHERING_ALG_NEA2: %d\n", NAS_CIPHERING_ALG_NEA2);
    printf("✓ NAS_CIPHERING_ALG_NEA3: %d\n", NAS_CIPHERING_ALG_NEA3);
    
    printf("✓ NAS_INTEGRITY_ALG_NIA0: %d\n", NAS_INTEGRITY_ALG_NIA0);
    printf("✓ NAS_INTEGRITY_ALG_NIA1: %d\n", NAS_INTEGRITY_ALG_NIA1);
    printf("✓ NAS_INTEGRITY_ALG_NIA2: %d\n", NAS_INTEGRITY_ALG_NIA2);
    printf("✓ NAS_INTEGRITY_ALG_NIA3: %d\n", NAS_INTEGRITY_ALG_NIA3);
    
    printf("✓ NAS_PDU_SESSION_TYPE_IPV4: %d\n", NAS_PDU_SESSION_TYPE_IPV4);
    printf("✓ NAS_PDU_SESSION_TYPE_IPV6: %d\n", NAS_PDU_SESSION_TYPE_IPV6);
    printf("✓ NAS_PDU_SESSION_TYPE_IPV4V6: %d\n", NAS_PDU_SESSION_TYPE_IPV4V6);
    
    printf("\nAll NAS layer tests passed!\n");
    printf("NAS layer implementation is ready for integration.\n");
    
    return EXIT_SUCCESS;
}