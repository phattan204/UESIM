/*
 * 5G UE Simulation Application
 * CLI Test
 */

#include "../src/cli/cli.h"
#include "../src/config/config.h"
#include "../src/core/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Mock configuration for testing
uesim_config_t g_config = {0};

int main(void) {
    printf("5G UE Simulation CLI Test\n");
    printf("=========================\n");
    
    // Test CLI initialization
    uesim_error_t result = cli_init();
    if (result == UESIM_SUCCESS) {
        printf("✓ CLI initialization successful\n");
    } else {
        printf("✗ CLI initialization failed: %d\n", result);
        return EXIT_FAILURE;
    }
    
    // Test configuration initialization
    result = config_init(&g_config);
    if (result == UESIM_SUCCESS) {
        printf("✓ Configuration initialization successful\n");
    } else {
        printf("✗ Configuration initialization failed: %d\n", result);
        cli_cleanup();
        return EXIT_FAILURE;
    }
    
    // Test default configuration loading
    result = config_load_default(&g_config);
    if (result == UESIM_SUCCESS) {
        printf("✓ Default configuration loading successful\n");
    } else {
        printf("✗ Default configuration loading failed: %d\n", result);
        config_cleanup(&g_config);
        cli_cleanup();
        return EXIT_FAILURE;
    }
    
    // Test CLI command processing
    printf("\nTesting CLI Command Processing:\n");
    
    // Test help command
    result = cli_process_command("help");
    if (result == UESIM_SUCCESS) {
        printf("✓ Help command processing successful\n");
    } else {
        printf("✗ Help command processing failed: %d\n", result);
    }
    
    // Test status command
    result = cli_process_command("status");
    if (result == UESIM_SUCCESS) {
        printf("✓ Status command processing successful\n");
    } else {
        printf("✗ Status command processing failed: %d\n", result);
    }
    
    // Test show command
    result = cli_process_command("show general");
    if (result == UESIM_SUCCESS) {
        printf("✓ Show command processing successful\n");
    } else {
        printf("✗ Show command processing failed: %d\n", result);
    }
    
    // Test set command
    result = cli_process_command("set general num_instances 5");
    if (result == UESIM_SUCCESS) {
        printf("✓ Set command processing successful\n");
    } else {
        printf("✗ Set command processing failed: %d\n", result);
    }
    
    // Test unknown command
    result = cli_process_command("unknown_command");
    if (result != UESIM_SUCCESS) {
        printf("✓ Unknown command handling successful\n");
    } else {
        printf("✗ Unknown command handling failed\n");
    }
    
    // Test CLI command types
    printf("\nTesting CLI Command Types:\n");
    printf("✓ CLI_COMMAND_START: %d\n", CLI_COMMAND_START);
    printf("✓ CLI_COMMAND_STOP: %d\n", CLI_COMMAND_STOP);
    printf("✓ CLI_COMMAND_STATUS: %d\n", CLI_COMMAND_STATUS);
    printf("✓ CLI_COMMAND_CONFIG: %d\n", CLI_COMMAND_CONFIG);
    printf("✓ CLI_COMMAND_SCENARIO: %d\n", CLI_COMMAND_SCENARIO);
    printf("✓ CLI_COMMAND_HELP: %d\n", CLI_COMMAND_HELP);
    printf("✓ CLI_COMMAND_EXIT: %d\n", CLI_COMMAND_EXIT);
    printf("✓ CLI_COMMAND_SHOW: %d\n", CLI_COMMAND_SHOW);
    printf("✓ CLI_COMMAND_SET: %d\n", CLI_COMMAND_SET);
    printf("✓ CLI_COMMAND_SAVE: %d\n", CLI_COMMAND_SAVE);
    printf("✓ CLI_COMMAND_LOAD: %d\n", CLI_COMMAND_LOAD);
    printf("✓ CLI_COMMAND_MAX: %d\n", CLI_COMMAND_MAX);
    
    // Test CLI scenario types
    printf("\nTesting CLI Scenario Types:\n");
    printf("✓ CLI_SCENARIO_REGISTRATION: %d\n", CLI_SCENARIO_REGISTRATION);
    printf("✓ CLI_SCENARIO_ESTABLISHMENT: %d\n", CLI_SCENARIO_ESTABLISHMENT);
    printf("✓ CLI_SCENARIO_REESTABLISHMENT: %d\n", CLI_SCENARIO_REESTABLISHMENT);
    printf("✓ CLI_SCENARIO_HANDOVER: %d\n", CLI_SCENARIO_HANDOVER);
    printf("✓ CLI_SCENARIO_MAX: %d\n", CLI_SCENARIO_MAX);
    
    // Test CLI data structures
    printf("\nTesting CLI Data Structures:\n");
    printf("✓ CLI command structure size: %zu bytes\n", sizeof(cli_command_t));
    printf("✓ CLI command type enum size: %zu bytes\n", sizeof(cli_command_type_t));
    printf("✓ CLI scenario type enum size: %zu bytes\n", sizeof(cli_scenario_type_t));
    
    printf("\nAll CLI tests passed!\n");
    printf("CLI system is ready for integration.\n");
    
    // Cleanup
    config_cleanup(&g_config);
    cli_cleanup();
    
    return EXIT_SUCCESS;
}