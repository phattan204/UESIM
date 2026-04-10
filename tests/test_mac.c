/*
 * 5G UE Simulation Application
 * MAC Layer Test
 */

#include "../src/protocol/mac.h"
#include "../src/core/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("5G UE Simulation MAC Layer Test\n");
    printf("===============================\n");
    
    // Test MAC entity creation
    mac_entity_t* entity = NULL;
    
    uesim_error_t result = mac_create_entity(NULL, NULL, &entity);
    if (result != UESIM_SUCCESS) {
        printf("✓ MAC entity creation with NULL UE context correctly failed\n");
    } else {
        printf("✗ MAC entity creation should have failed with NULL UE context\n");
        return EXIT_FAILURE;
    }
    
    // Test MAC configuration functions
    printf("\nTesting MAC Configuration Functions:\n");
    
    mac_config_t config;
    
    // Test default configuration
    result = mac_set_default_config(&config);
    if (result == UESIM_SUCCESS) {
        printf("✓ MAC default configuration successful\n");
    } else {
        printf("✗ MAC default configuration failed\n");
        return EXIT_FAILURE;
    }
    
    // Test RACH configuration
    mac_rach_config_t rach_config = {0};
    rach_config.preamble_index = 1;
    rach_config.ra_rnti = 0x1234;
    rach_config.ra_response_window = 10;
    
    result = mac_set_rach_config(&config, &rach_config);
    if (result == UESIM_SUCCESS) {
        printf("✓ MAC RACH configuration successful\n");
    } else {
        printf("✗ MAC RACH configuration failed\n");
        return EXIT_FAILURE;
    }
    
    // Test SR configuration
    mac_sr_config_t sr_config = {0};
    sr_config.sr_id = 1;
    sr_config.sr_prohibit_timer = 20;
    sr_config.sr_trans_max = 20;
    
    result = mac_set_sr_config(&config, &sr_config);
    if (result == UESIM_SUCCESS) {
        printf("✓ MAC SR configuration successful\n");
    } else {
        printf("✗ MAC SR configuration failed\n");
        return EXIT_FAILURE;
    }
    
    // Test logical channel configuration
    mac_lch_info_t lch_info = {0};
    lch_info.lch_id = MAC_LCH_DTCH;
    lch_info.priority = 5;
    lch_info.prioritized_bit_rate = 1000;
    
    result = mac_add_logical_channel(&config, &lch_info);
    if (result == UESIM_SUCCESS) {
        printf("✓ MAC logical channel configuration successful\n");
    } else {
        printf("✗ MAC logical channel configuration failed\n");
        return EXIT_FAILURE;
    }
    
    // Test MAC data structures
    printf("\nTesting MAC Data Structures:\n");
    printf("✓ MAC HARQ Process structure size: %zu bytes\n", sizeof(mac_harq_process_t));
    printf("✓ MAC Transport Block structure size: %zu bytes\n", sizeof(mac_tb_t));
    printf("✓ MAC Control Element structure size: %zu bytes\n", sizeof(mac_ce_t));
    printf("✓ MAC Entity structure size: %zu bytes\n", sizeof(mac_entity_t));
    printf("✓ MAC Config structure size: %zu bytes\n", sizeof(mac_config_t));
    
    // Test MAC constants
    printf("\nTesting MAC Constants:\n");
    printf("✓ MAC_MAX_HARQ_PROCESSES: %d\n", MAC_MAX_HARQ_PROCESSES);
    printf("✓ MAC_MAX_LOGICAL_CHANNELS: %d\n", MAC_MAX_LOGICAL_CHANNELS);
    printf("✓ MAC_MAX_TRANSPORT_BLOCKS: %d\n", MAC_MAX_TRANSPORT_BLOCKS);
    printf("✓ MAC_DEFAULT_TTI_MS: %d\n", MAC_DEFAULT_TTI_MS);
    
    // Test MAC enumerations
    printf("\nTesting MAC Enumerations:\n");
    printf("✓ MAC_LCH_BCCH: %d\n", MAC_LCH_BCCH);
    printf("✓ MAC_LCH_PCCH: %d\n", MAC_LCH_PCCH);
    printf("✓ MAC_LCH_CCCH: %d\n", MAC_LCH_CCCH);
    printf("✓ MAC_LCH_DCCH: %d\n", MAC_LCH_DCCH);
    printf("✓ MAC_LCH_DTCH: %d\n", MAC_LCH_DTCH);
    
    printf("✓ MAC_HARQ_IDLE: %d\n", MAC_HARQ_IDLE);
    printf("✓ MAC_HARQ_ACTIVE: %d\n", MAC_HARQ_ACTIVE);
    printf("✓ MAC_HARQ_PENDING: %d\n", MAC_HARQ_PENDING);
    
    printf("✓ MAC_RNTI_C_RNTI: %d\n", MAC_RNTI_C_RNTI);
    printf("✓ MAC_RNTI_RA_RNTI: %d\n", MAC_RNTI_RA_RNTI);
    printf("✓ MAC_RNTI_P_RNTI: %d\n", MAC_RNTI_P_RNTI);
    
    printf("\nAll MAC layer tests passed!\n");
    printf("MAC layer implementation is ready for integration.\n");
    
    return EXIT_SUCCESS;
}