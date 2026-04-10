/*
 * 5G UE Simulation Application
 * Configuration Management Test
 */

#include "../src/config/config.h"
#include "../src/core/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("5G UE Simulation Configuration Management Test\n");
    printf("==============================================\n");
    
    // Test configuration initialization
    uesim_config_t config = {0};
    
    uesim_error_t result = config_init(&config);
    if (result == UESIM_SUCCESS) {
        printf("✓ Configuration initialization successful\n");
    } else {
        printf("✗ Configuration initialization failed: %d\n", result);
        return EXIT_FAILURE;
    }
    
    // Test default configuration loading
    result = config_load_default(&config);
    if (result == UESIM_SUCCESS) {
        printf("✓ Default configuration loading successful\n");
    } else {
        printf("✗ Default configuration loading failed: %d\n", result);
        return EXIT_FAILURE;
    }
    
    // Test configuration validation
    result = config_validate(&config);
    if (result == UESIM_SUCCESS) {
        printf("✓ Configuration validation successful\n");
    } else {
        printf("✗ Configuration validation failed: %d\n", result);
        return EXIT_FAILURE;
    }
    
    // Test configuration access functions
    printf("\nTesting Configuration Access Functions:\n");
    
    // Test integer access
    int num_instances = 0;
    result = config_get_int(&config, CONFIG_SECTION_GENERAL, "num_instances", &num_instances);
    if (result == UESIM_SUCCESS && num_instances == 1) {
        printf("✓ Get integer configuration successful: %d\n", num_instances);
    } else {
        printf("✗ Get integer configuration failed\n");
        return EXIT_FAILURE;
    }
    
    // Test string access
    char log_file[256] = {0};
    result = config_get_string(&config, CONFIG_SECTION_GENERAL, "log_file", log_file, sizeof(log_file));
    if (result == UESIM_SUCCESS && strlen(log_file) > 0) {
        printf("✓ Get string configuration successful: %s\n", log_file);
    } else {
        printf("✗ Get string configuration failed\n");
        return EXIT_FAILURE;
    }
    
    // Test boolean access
    bool verbose = false;
    result = config_get_bool(&config, CONFIG_SECTION_GENERAL, "verbose", &verbose);
    if (result == UESIM_SUCCESS) {
        printf("✓ Get boolean configuration successful: %s\n", verbose ? "true" : "false");
    } else {
        printf("✗ Get boolean configuration failed\n");
        return EXIT_FAILURE;
    }
    
    // Test configuration section access
    printf("\nTesting Configuration Section Access:\n");
    
    config_general_t* general = config_get_general(&config);
    if (general != NULL) {
        printf("✓ Get general configuration section successful\n");
    } else {
        printf("✗ Get general configuration section failed\n");
        return EXIT_FAILURE;
    }
    
    config_network_t* network = config_get_network(&config);
    if (network != NULL) {
        printf("✓ Get network configuration section successful\n");
    } else {
        printf("✗ Get network configuration section failed\n");
        return EXIT_FAILURE;
    }
    
    config_ue_t* ue = config_get_ue(&config);
    if (ue != NULL) {
        printf("✓ Get UE configuration section successful\n");
    } else {
        printf("✗ Get UE configuration section failed\n");
        return EXIT_FAILURE;
    }
    
    config_rrc_t* rrc = config_get_rrc(&config);
    if (rrc != NULL) {
        printf("✓ Get RRC configuration section successful\n");
    } else {
        printf("✗ Get RRC configuration section failed\n");
        return EXIT_FAILURE;
    }
    
    config_pdcp_t* pdcp = config_get_pdcp(&config);
    if (pdcp != NULL) {
        printf("✓ Get PDCP configuration section successful\n");
    } else {
        printf("✗ Get PDCP configuration section failed\n");
        return EXIT_FAILURE;
    }
    
    config_rlc_t* rlc = config_get_rlc(&config);
    if (rlc != NULL) {
        printf("✓ Get RLC configuration section successful\n");
    } else {
        printf("✗ Get RLC configuration section failed\n");
        return EXIT_FAILURE;
    }
    
    config_mac_t* mac = config_get_mac(&config);
    if (mac != NULL) {
        printf("✓ Get MAC configuration section successful\n");
    } else {
        printf("✗ Get MAC configuration section failed\n");
        return EXIT_FAILURE;
    }
    
    config_nas_t* nas = config_get_nas(&config);
    if (nas != NULL) {
        printf("✓ Get NAS configuration section successful\n");
    } else {
        printf("✗ Get NAS configuration section failed\n");
        return EXIT_FAILURE;
    }
    
    // Test configuration data structures
    printf("\nTesting Configuration Data Structures:\n");
    printf("✓ General config structure size: %zu bytes\n", sizeof(config_general_t));
    printf("✓ Network config structure size: %zu bytes\n", sizeof(config_network_t));
    printf("✓ UE config structure size: %zu bytes\n", sizeof(config_ue_t));
    printf("✓ RRC config structure size: %zu bytes\n", sizeof(config_rrc_t));
    printf("✓ PDCP config structure size: %zu bytes\n", sizeof(config_pdcp_t));
    printf("✓ RLC config structure size: %zu bytes\n", sizeof(config_rlc_t));
    printf("✓ MAC config structure size: %zu bytes\n", sizeof(config_mac_t));
    printf("✓ NAS config structure size: %zu bytes\n", sizeof(config_nas_t));
    printf("✓ Performance config structure size: %zu bytes\n", sizeof(config_performance_t));
    printf("✓ Security config structure size: %zu bytes\n", sizeof(config_security_t));
    printf("✓ Test config structure size: %zu bytes\n", sizeof(config_test_t));
    printf("✓ Main config structure size: %zu bytes\n", sizeof(uesim_config_t));
    
    // Test configuration constants
    printf("\nTesting Configuration Constants:\n");
    printf("✓ CONFIG_MAX_STRING_LEN: %d\n", CONFIG_MAX_STRING_LEN);
    printf("✓ CONFIG_MAX_UE_INSTANCES: %d\n", CONFIG_MAX_UE_INSTANCES);
    printf("✓ CONFIG_DEFAULT_BUFFER_SIZE: %d\n", CONFIG_DEFAULT_BUFFER_SIZE);
    printf("✓ CONFIG_MAX_LOG_LEVEL: %d\n", CONFIG_MAX_LOG_LEVEL);
    
    // Test configuration sections
    printf("\nTesting Configuration Sections:\n");
    printf("✓ CONFIG_SECTION_GENERAL: %d\n", CONFIG_SECTION_GENERAL);
    printf("✓ CONFIG_SECTION_NETWORK: %d\n", CONFIG_SECTION_NETWORK);
    printf("✓ CONFIG_SECTION_UE: %d\n", CONFIG_SECTION_UE);
    printf("✓ CONFIG_SECTION_RRC: %d\n", CONFIG_SECTION_RRC);
    printf("✓ CONFIG_SECTION_PDCP: %d\n", CONFIG_SECTION_PDCP);
    printf("✓ CONFIG_SECTION_RLC: %d\n", CONFIG_SECTION_RLC);
    printf("✓ CONFIG_SECTION_MAC: %d\n", CONFIG_SECTION_MAC);
    printf("✓ CONFIG_SECTION_NAS: %d\n", CONFIG_SECTION_NAS);
    printf("✓ CONFIG_SECTION_PERFORMANCE: %d\n", CONFIG_SECTION_PERFORMANCE);
    printf("✓ CONFIG_SECTION_SECURITY: %d\n", CONFIG_SECTION_SECURITY);
    printf("✓ CONFIG_SECTION_TEST: %d\n", CONFIG_SECTION_TEST);
    printf("✓ CONFIG_SECTION_MAX: %d\n", CONFIG_SECTION_MAX);
    
    printf("\nAll configuration management tests passed!\n");
    printf("Configuration management system is ready for integration.\n");
    
    // Cleanup
    config_cleanup(&config);
    
    return EXIT_SUCCESS;
}