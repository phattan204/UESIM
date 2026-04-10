/*
 * 5G UE Simulation Application
 * Command Line Interface implementation
 */

#include "cli.h"
#include "../protocol/rrc.h"
#include "../transport/socket_mgr.h"
#include "../config/config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

// External configuration reference
extern uesim_config_t g_config;

// Global variables
static atomic_bool g_cli_running = false;
static pthread_t g_cli_thread = 0;
static pthread_mutex_t g_cli_mutex = PTHREAD_MUTEX_INITIALIZER;

// Command string mappings
static const char* g_command_strings[] = {
    "start",
    "stop",
    "status",
    "config",
    "scenario",
    "help",
    "exit",
    "show",
    "set",
    "save",
    "load"
};

static const char* g_scenario_strings[] = {
    "registration",
    "establishment",
    "reestablishment",
    "handover"
};

uesim_error_t cli_init(void) {
    printf("CLI initialized successfully\n");
    return UESIM_SUCCESS;
}

void cli_cleanup(void) {
    cli_stop_interactive_mode();
    printf("CLI cleanup completed\n");
}

uesim_error_t cli_process_command(const char* input) {
    if (input == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Parse input into command structure
    cli_command_t command = {0};
    char* input_copy = strdup(input);
    if (input_copy == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    // Tokenize input
    char* token = strtok(input_copy, " \t\n");
    if (token == NULL) {
        free(input_copy);
        return UESIM_SUCCESS; // Empty command
    }
    
    // Find command type
    for (int i = 0; i < CLI_COMMAND_MAX; i++) {
        if (strcasecmp(token, g_command_strings[i]) == 0) {
            command.command_type = (cli_command_type_t)i;
            break;
        }
    }
    
    if (command.command_type >= CLI_COMMAND_MAX) {
        printf("Unknown command: %s\n", token);
        printf("Type 'help' for available commands\n");
        free(input_copy);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Parse arguments
    command.arg_count = 0;
    command.arg_values = NULL;
    
    while ((token = strtok(NULL, " \t\n")) != NULL) {
        command.arg_count++;
        char** new_args = realloc(command.arg_values, command.arg_count * sizeof(char*));
        if (new_args == NULL) {
            // Cleanup
            for (size_t j = 0; j < command.arg_count - 1; j++) {
                free(command.arg_values[j]);
            }
            free(command.arg_values);
            free(input_copy);
            return UESIM_ERROR_MEMORY;
        }
        command.arg_values = new_args;
        command.arg_values[command.arg_count - 1] = strdup(token);
    }
    
    command.arguments = strdup(input);
    
    // Execute command
    uesim_error_t result = cli_execute_command(&command);
    
    // Cleanup
    free(command.arguments);
    for (size_t i = 0; i < command.arg_count; i++) {
        free(command.arg_values[i]);
    }
    free(command.arg_values);
    free(input_copy);
    
    return result;
}

uesim_error_t cli_execute_command(cli_command_t* command) {
    if (command == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    uesim_error_t result = UESIM_SUCCESS;
    
    switch (command->command_type) {
        case CLI_COMMAND_START:
            result = cli_handle_start(command);
            break;
        case CLI_COMMAND_STOP:
            result = cli_handle_stop(command);
            break;
        case CLI_COMMAND_STATUS:
            result = cli_handle_status(command);
            break;
        case CLI_COMMAND_CONFIG:
            result = cli_handle_config(command);
            break;
        case CLI_COMMAND_SCENARIO:
            result = cli_handle_scenario(command);
            break;
        case CLI_COMMAND_HELP:
            result = cli_handle_help(command);
            break;
        case CLI_COMMAND_EXIT:
            result = cli_handle_exit(command);
            break;
        case 7: // show command
            result = cli_handle_show(command);
            break;
        case 8: // set command
            result = cli_handle_set(command);
            break;
        case 9: // save command
            result = cli_handle_save(command);
            break;
        case 10: // load command
            result = cli_handle_load(command);
            break;
        default:
            printf("Unknown command type: %d\n", command->command_type);
            result = UESIM_ERROR_INVALID_PARAM;
            break;
    }
    
    return result;
}

void cli_print_help(void) {
    printf("\n5G UE Simulation CLI Commands:\n");
    printf("================================\n");
    printf("  start     - Start UE simulation\n");
    printf("  stop      - Stop UE simulation\n");
    printf("  status    - Show UE status\n");
    printf("  config    - Configure UE parameters\n");
    printf("  scenario  - Execute RRC scenario\n");
    printf("  show      - Show configuration values\n");
    printf("  set       - Set configuration values\n");
    printf("  save      - Save configuration to file\n");
    printf("  load      - Load configuration from file\n");
    printf("  help      - Show this help message\n");
    printf("  exit      - Exit the application\n");
    printf("\nScenario types:\n");
    printf("  registration    - RRC registration procedure\n");
    printf("  establishment   - RRC establishment procedure\n");
    printf("  reestablishment - RRC re-establishment procedure\n");
    printf("  handover        - RRC handover procedure\n");
    printf("\n");
}

void cli_print_status(void) {
    printf("\nUE Simulation Status:\n");
    printf("=====================\n");
    printf("  CLI Status: %s\n", atomic_load(&g_cli_running) ? "Running" : "Stopped");
    printf("  Active UE Instances: %d\n", 0); // TODO: Get actual count
    printf("  RRC State: %s\n", "Not Connected"); // TODO: Get actual state
    printf("  Configuration Loaded: %s\n", config_is_loaded(&g_config) ? "Yes" : "No");
    if (config_is_loaded(&g_config)) {
        printf("  Config Load Time: %s", ctime(&g_config.load_time));
    }
    printf("\n");
}

// Command handlers
uesim_error_t cli_handle_start(cli_command_t* command) {
    printf("Starting UE simulation...\n");
    // TODO: Implement start logic
    return UESIM_SUCCESS;
}

uesim_error_t cli_handle_stop(cli_command_t* command) {
    printf("Stopping UE simulation...\n");
    // TODO: Implement stop logic
    return UESIM_SUCCESS;
}

uesim_error_t cli_handle_status(cli_command_t* command) {
    cli_print_status();
    return UESIM_SUCCESS;
}

uesim_error_t cli_handle_config(cli_command_t* command) {
    printf("Configuring UE parameters...\n");
    if (command->arg_count > 0) {
        printf("Configuration file: %s\n", command->arg_values[0]);
        // Load configuration file
        uesim_error_t result = config_load(&g_config, command->arg_values[0]);
        if (result == UESIM_SUCCESS) {
            printf("Configuration loaded successfully\n");
        } else {
            printf("Failed to load configuration: %d\n", result);
            return result;
        }
    } else {
        printf("No configuration file specified\n");
        printf("Usage: config <file_path>\n");
        return UESIM_ERROR_INVALID_PARAM;
    }
    return UESIM_SUCCESS;
}

uesim_error_t cli_handle_scenario(cli_command_t* command) {
    if (command->arg_count == 0) {
        printf("Usage: scenario <type>\n");
        printf("Available scenarios:\n");
        for (int i = 0; i < CLI_SCENARIO_MAX; i++) {
            printf("  %s\n", g_scenario_strings[i]);
        }
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Find scenario type
    cli_scenario_type_t scenario_type = CLI_SCENARIO_MAX;
    for (int i = 0; i < CLI_SCENARIO_MAX; i++) {
        if (strcasecmp(command->arg_values[0], g_scenario_strings[i]) == 0) {
            scenario_type = (cli_scenario_type_t)i;
            break;
        }
    }
    
    if (scenario_type >= CLI_SCENARIO_MAX) {
        printf("Unknown scenario: %s\n", command->arg_values[0]);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("Executing scenario: %s\n", g_scenario_strings[scenario_type]);
    
    // TODO: Implement scenario execution
    switch (scenario_type) {
        case CLI_SCENARIO_REGISTRATION:
            // Execute RRC registration
            break;
        case CLI_SCENARIO_ESTABLISHMENT:
            // Execute RRC establishment
            break;
        case CLI_SCENARIO_REESTABLISHMENT:
            // Execute RRC re-establishment
            break;
        case CLI_SCENARIO_HANDOVER:
            // Execute RRC handover
            break;
        default:
            break;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t cli_handle_help(cli_command_t* command) {
    cli_print_help();
    return UESIM_SUCCESS;
}

uesim_error_t cli_handle_exit(cli_command_t* command) {
    printf("Exiting UE simulation...\n");
    atomic_store(&g_cli_running, false);
    return UESIM_SUCCESS;
}

// Extended CLI command handlers
uesim_error_t cli_handle_show(cli_command_t* command) {
    if (command->arg_count == 0) {
        printf("Usage: show <section> [key]\n");
        printf("Available sections: general, network, ue, rrc, pdcp, rlc, mac, nas, performance, security, test\n");
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    const char* section_name = command->arg_values[0];
    config_section_t section = CONFIG_SECTION_GENERAL;
    bool section_found = false;
    
    // Map section name to enum
    struct section_map {
        const char* name;
        config_section_t section;
    } sections[] = {
        {"general", CONFIG_SECTION_GENERAL},
        {"network", CONFIG_SECTION_NETWORK},
        {"ue", CONFIG_SECTION_UE},
        {"rrc", CONFIG_SECTION_RRC},
        {"pdcp", CONFIG_SECTION_PDCP},
        {"rlc", CONFIG_SECTION_RLC},
        {"mac", CONFIG_SECTION_MAC},
        {"nas", CONFIG_SECTION_NAS},
        {"performance", CONFIG_SECTION_PERFORMANCE},
        {"security", CONFIG_SECTION_SECURITY},
        {"test", CONFIG_SECTION_TEST}
    };
    
    for (int i = 0; i < sizeof(sections) / sizeof(sections[0]); i++) {
        if (strcasecmp(section_name, sections[i].name) == 0) {
            section = sections[i].section;
            section_found = true;
            break;
        }
    }
    
    if (!section_found) {
        printf("Unknown section: %s\n", section_name);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Show configuration values
    printf("\nConfiguration Section: %s\n", section_name);
    printf("========================\n");
    
    // Show all values in section or specific key
    if (command->arg_count > 1) {
        const char* key = command->arg_values[1];
        char value_str[256] = {0};
        int value_int = 0;
        bool value_bool = false;
        uesim_error_t result;
        
        // Try to get as string first
        result = config_get_string(&g_config, section, key, value_str, sizeof(value_str));
        if (result == UESIM_SUCCESS && strlen(value_str) > 0) {
            printf("%s = %s\n", key, value_str);
            return UESIM_SUCCESS;
        }
        
        // Try to get as integer
        result = config_get_int(&g_config, section, key, &value_int);
        if (result == UESIM_SUCCESS) {
            printf("%s = %d\n", key, value_int);
            return UESIM_SUCCESS;
        }
        
        // Try to get as boolean
        result = config_get_bool(&g_config, section, key, &value_bool);
        if (result == UESIM_SUCCESS) {
            printf("%s = %s\n", key, value_bool ? "true" : "false");
            return UESIM_SUCCESS;
        }
        
        printf("Unknown key: %s\n", key);
        return UESIM_ERROR_INVALID_PARAM;
    } else {
        // Show all values in section (simplified display)
        printf("Section values displayed (full implementation would show all values)\n");
        return UESIM_SUCCESS;
    }
}

uesim_error_t cli_handle_set(cli_command_t* command) {
    if (command->arg_count < 3) {
        printf("Usage: set <section> <key> <value>\n");
        printf("Available sections: general, network, ue, rrc, pdcp, rlc, mac, nas, performance, security, test\n");
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    const char* section_name = command->arg_values[0];
    const char* key = command->arg_values[1];
    const char* value = command->arg_values[2];
    
    config_section_t section = CONFIG_SECTION_GENERAL;
    bool section_found = false;
    
    // Map section name to enum
    struct section_map {
        const char* name;
        config_section_t section;
    } sections[] = {
        {"general", CONFIG_SECTION_GENERAL},
        {"network", CONFIG_SECTION_NETWORK},
        {"ue", CONFIG_SECTION_UE},
        {"rrc", CONFIG_SECTION_RRC},
        {"pdcp", CONFIG_SECTION_PDCP},
        {"rlc", CONFIG_SECTION_RLC},
        {"mac", CONFIG_SECTION_MAC},
        {"nas", CONFIG_SECTION_NAS},
        {"performance", CONFIG_SECTION_PERFORMANCE},
        {"security", CONFIG_SECTION_SECURITY},
        {"test", CONFIG_SECTION_TEST}
    };
    
    for (int i = 0; i < sizeof(sections) / sizeof(sections[0]); i++) {
        if (strcasecmp(section_name, sections[i].name) == 0) {
            section = sections[i].section;
            section_found = true;
            break;
        }
    }
    
    if (!section_found) {
        printf("Unknown section: %s\n", section_name);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Set configuration value based on type
    if (strcasecmp(value, "true") == 0 || strcasecmp(value, "false") == 0) {
        bool bool_value = (strcasecmp(value, "true") == 0);
        uesim_error_t result = config_set_bool(&g_config, section, key, bool_value);
        if (result == UESIM_SUCCESS) {
            printf("Set %s.%s = %s\n", section_name, key, value);
        } else {
            printf("Failed to set %s.%s: %d\n", section_name, key, result);
            return result;
        }
    } else if (strspn(value, "0123456789") == strlen(value)) {
        int int_value = atoi(value);
        uesim_error_t result = config_set_int(&g_config, section, key, int_value);
        if (result == UESIM_SUCCESS) {
            printf("Set %s.%s = %d\n", section_name, key, int_value);
        } else {
            printf("Failed to set %s.%s: %d\n", section_name, key, result);
            return result;
        }
    } else {
        uesim_error_t result = config_set_string(&g_config, section, key, value);
        if (result == UESIM_SUCCESS) {
            printf("Set %s.%s = %s\n", section_name, key, value);
        } else {
            printf("Failed to set %s.%s: %d\n", section_name, key, result);
            return result;
        }
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t cli_handle_save(cli_command_t* command) {
    if (command->arg_count == 0) {
        printf("Usage: save <file_path>\n");
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    const char* file_path = command->arg_values[0];
    uesim_error_t result = config_save(&g_config, file_path);
    if (result == UESIM_SUCCESS) {
        printf("Configuration saved to %s\n", file_path);
    } else {
        printf("Failed to save configuration: %d\n", result);
        return result;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t cli_handle_load(cli_command_t* command) {
    if (command->arg_count == 0) {
        printf("Usage: load <file_path>\n");
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    const char* file_path = command->arg_values[0];
    uesim_error_t result = config_load(&g_config, file_path);
    if (result == UESIM_SUCCESS) {
        printf("Configuration loaded from %s\n", file_path);
    } else {
        printf("Failed to load configuration: %d\n", result);
        return result;
    }
    
    return UESIM_SUCCESS;
}

// Interactive mode
uesim_error_t cli_start_interactive_mode(void) {
    if (atomic_load(&g_cli_running)) {
        return UESIM_SUCCESS;
    }
    
    atomic_store(&g_cli_running, true);
    
    printf("\n5G UE Simulation CLI\n");
    printf("Type 'help' for available commands\n");
    printf("Type 'exit' to quit\n\n");
    
    char input[1024];
    
    while (atomic_load(&g_cli_running)) {
        printf("uesim> ");
        fflush(stdout);
        
        if (fgets(input, sizeof(input), stdin) == NULL) {
            if (feof(stdin)) {
                // EOF reached (Ctrl+D)
                printf("\n");
                break;
            }
            continue;
        }
        
        // Remove trailing newline
        input[strcspn(input, "\n")] = 0;
        
        // Skip empty lines
        if (strlen(input) == 0) {
            continue;
        }
        
        // Process command
        cli_process_command(input);
    }
    
    return UESIM_SUCCESS;
}

void cli_stop_interactive_mode(void) {
    atomic_store(&g_cli_running, false);
    
    if (g_cli_thread != 0) {
        pthread_join(g_cli_thread, NULL);
        g_cli_thread = 0;
    }
}
