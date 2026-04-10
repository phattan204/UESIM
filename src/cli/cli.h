/*
 * 5G UE Simulation Application
 * Command Line Interface header
 */

#ifndef CLI_H
#define CLI_H

#include "../uesim.h"

// CLI command types
typedef enum {
    CLI_COMMAND_START = 0,
    CLI_COMMAND_STOP = 1,
    CLI_COMMAND_STATUS = 2,
    CLI_COMMAND_CONFIG = 3,
    CLI_COMMAND_SCENARIO = 4,
    CLI_COMMAND_HELP = 5,
    CLI_COMMAND_EXIT = 6,
    CLI_COMMAND_SHOW = 7,
    CLI_COMMAND_SET = 8,
    CLI_COMMAND_SAVE = 9,
    CLI_COMMAND_LOAD = 10,
    CLI_COMMAND_MAX
} cli_command_type_t;

// CLI scenario types
typedef enum {
    CLI_SCENARIO_REGISTRATION = 0,
    CLI_SCENARIO_ESTABLISHMENT = 1,
    CLI_SCENARIO_REESTABLISHMENT = 2,
    CLI_SCENARIO_HANDOVER = 3,
    CLI_SCENARIO_MAX
} cli_scenario_type_t;

// CLI command structure
typedef struct {
    cli_command_type_t command_type;
    char* arguments;
    size_t arg_count;
    char** arg_values;
} cli_command_t;

// Function prototypes
uesim_error_t cli_init(void);
void cli_cleanup(void);
uesim_error_t cli_process_command(const char* input);
uesim_error_t cli_execute_command(cli_command_t* command);
void cli_print_help(void);
void cli_print_status(void);

// Command handlers
uesim_error_t cli_handle_start(cli_command_t* command);
uesim_error_t cli_handle_stop(cli_command_t* command);
uesim_error_t cli_handle_status(cli_command_t* command);
uesim_error_t cli_handle_config(cli_command_t* command);
uesim_error_t cli_handle_scenario(cli_command_t* command);
uesim_error_t cli_handle_help(cli_command_t* command);
uesim_error_t cli_handle_exit(cli_command_t* command);
uesim_error_t cli_handle_show(cli_command_t* command);
uesim_error_t cli_handle_set(cli_command_t* command);
uesim_error_t cli_handle_save(cli_command_t* command);
uesim_error_t cli_handle_load(cli_command_t* command);

// Interactive mode
uesim_error_t cli_start_interactive_mode(void);
void cli_stop_interactive_mode(void);

#endif // CLI_H
