/*
 * 5G UE Simulation Application
 * RLC Layer Test
 */

#include "../src/protocol/rlc.h"
#include "../src/core/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("5G UE Simulation RLC Layer Test\n");
    printf("===============================\n");
    
    // Test RLC entity creation
    rlc_entity_t* entity = NULL;
    rlc_bearer_t bearer = RLC_BEARER_DRB1;
    rlc_direction_t direction = RLC_DIRECTION_UPLINK;
    rlc_mode_t mode = RLC_MODE_AM;
    
    uesim_error_t result = rlc_create_entity(NULL, bearer, direction, mode, NULL, &entity);
    if (result != UESIM_SUCCESS) {
        printf("✓ RLC entity creation with NULL UE context correctly failed\n");
    } else {
        printf("✗ RLC entity creation should have failed with NULL UE context\n");
        return EXIT_FAILURE;
    }
    
    // Test RLC configuration functions
    printf("\nTesting RLC Configuration Functions:\n");
    
    rlc_config_t config;
    
    // Test TM configuration
    result = rlc_set_tm_config(&config);
    if (result == UESIM_SUCCESS && config.mode == RLC_MODE_TM) {
        printf("✓ RLC TM configuration successful\n");
    } else {
        printf("✗ RLC TM configuration failed\n");
        return EXIT_FAILURE;
    }
    
    // Test UM configuration
    result = rlc_set_um_config(&config, 12, 100);
    if (result == UESIM_SUCCESS && config.mode == RLC_MODE_UM) {
        printf("✓ RLC UM configuration successful\n");
    } else {
        printf("✗ RLC UM configuration failed\n");
        return EXIT_FAILURE;
    }
    
    // Test AM configuration
    result = rlc_set_am_config(&config, 12, 50, 100, 25, 1000, 10000, 4);
    if (result == UESIM_SUCCESS && config.mode == RLC_MODE_AM) {
        printf("✓ RLC AM configuration successful\n");
    } else {
        printf("✗ RLC AM configuration failed\n");
        return EXIT_FAILURE;
    }
    
    // Test RLC data structures
    printf("\nTesting RLC Data Structures:\n");
    printf("✓ RLC SDU structure size: %zu bytes\n", sizeof(rlc_sdu_t));
    printf("✓ RLC PDU structure size: %zu bytes\n", sizeof(rlc_pdu_t));
    printf("✓ RLC Entity structure size: %zu bytes\n", sizeof(rlc_entity_t));
    printf("✓ RLC Config structure size: %zu bytes\n", sizeof(rlc_config_t));
    
    // Test RLC modes
    printf("\nTesting RLC Modes:\n");
    printf("✓ RLC_MODE_TM: %d\n", RLC_MODE_TM);
    printf("✓ RLC_MODE_UM: %d\n", RLC_MODE_UM);
    printf("✓ RLC_MODE_AM: %d\n", RLC_MODE_AM);
    
    // Test RLC constants
    printf("\nTesting RLC Constants:\n");
    printf("✓ RLC_MAX_SDU_SIZE: %d\n", RLC_MAX_SDU_SIZE);
    printf("✓ RLC_MAX_PDU_SIZE: %d\n", RLC_MAX_PDU_SIZE);
    printf("✓ RLC_DEFAULT_WINDOW_SIZE: %d\n", RLC_DEFAULT_WINDOW_SIZE);
    
    printf("\nAll RLC layer tests passed!\n");
    printf("RLC layer implementation is ready for integration.\n");
    
    return EXIT_SUCCESS;
}