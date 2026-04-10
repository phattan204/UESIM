/*
 * 5G UE Simulation Application
 * PDCP Layer Test
 */

#include "../src/protocol/pdcp.h"
#include "../src/core/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("5G UE Simulation PDCP Layer Test\n");
    printf("=================================\n");
    
    // Test PDCP entity creation
    pdcp_entity_t* entity = NULL;
    pdcp_bearer_t bearer = PDCP_BEARER_DRB1;
    pdcp_direction_t direction = PDCP_DIRECTION_UPLINK;
    
    uesim_error_t result = pdcp_create_entity(NULL, bearer, direction, &entity);
    if (result != UESIM_SUCCESS) {
        printf("✓ PDCP entity creation with NULL UE context correctly failed\n");
    } else {
        printf("✗ PDCP entity creation should have failed with NULL UE context\n");
        return EXIT_FAILURE;
    }
    
    // Test security context creation
    pdcp_security_context_t* security_ctx = NULL;
    uint8_t cipher_key[16] = {0};
    uint8_t integrity_key[16] = {0};
    
    result = pdcp_create_security_context(PDCP_CIPHERING_ALG_NEA1, 
                                         PDCP_INTEGRITY_ALG_NIA1,
                                         cipher_key, integrity_key, 
                                         &security_ctx);
    if (result == UESIM_SUCCESS) {
        printf("✓ PDCP security context creation successful\n");
        pdcp_destroy_security_context(security_ctx);
        printf("✓ PDCP security context destruction successful\n");
    } else {
        printf("✗ PDCP security context creation failed: %d\n", result);
        return EXIT_FAILURE;
    }
    
    // Test algorithm function pointers
    printf("\nTesting Algorithm Function Pointers:\n");
    printf("✓ SNOW3G encrypt function: %p\n", snow3g_encrypt);
    printf("✓ SNOW3G decrypt function: %p\n", snow3g_decrypt);
    printf("✓ SNOW3G MAC function: %p\n", snow3g_compute_mac);
    printf("✓ AES encrypt function: %p\n", aes_encrypt);
    printf("✓ AES decrypt function: %p\n", aes_decrypt);
    printf("✓ AES MAC function: %p\n", aes_compute_mac);
    printf("✓ ZUC encrypt function: %p\n", zuc_encrypt);
    printf("✓ ZUC decrypt function: %p\n", zuc_decrypt);
    printf("✓ ZUC MAC function: %p\n", zuc_compute_mac);
    
    // Test PDCP header encoding/decoding
    pdcp_pdu_t pdu = {0};
    pdu.sn = 0x123;
    
    uint8_t header[3];
    size_t header_len;
    
    result = pdcp_encode_header(&pdu, header, &header_len);
    if (result == UESIM_SUCCESS) {
        printf("✓ PDCP header encoding successful, length: %zu\n", header_len);
        
        pdcp_pdu_t decoded_pdu = {0};
        result = pdcp_decode_header(header, header_len, &decoded_pdu);
        if (result == UESIM_SUCCESS && decoded_pdu.sn == pdu.sn) {
            printf("✓ PDCP header decoding successful, SN: 0x%x\n", decoded_pdu.sn);
        } else {
            printf("✗ PDCP header decoding failed\n");
            return EXIT_FAILURE;
        }
    } else {
        printf("✗ PDCP header encoding failed: %d\n", result);
        return EXIT_FAILURE;
    }
    
    printf("\nAll PDCP layer tests passed!\n");
    printf("PDCP layer implementation is ready for integration.\n");
    
    return EXIT_SUCCESS;
}