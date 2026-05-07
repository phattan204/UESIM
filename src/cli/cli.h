/*
 * 5G UE Simulation Application
 * Command Line Interface Header
 */

#ifndef CLI_H
#define CLI_H

#include "../uesim.h"

/* Legacy command types (for backward compatibility) */
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
    CLI_COMMAND_QOS = 11,
    CLI_COMMAND_SESSION = 12,
    CLI_COMMAND_LOADTEST = 13,
    CLI_COMMAND_GNB = 14,
    CLI_COMMAND_MAX
} cli_command_type_t;

/* Legacy CLI command structure */
typedef struct {
    cli_command_type_t command_type;
    char* arguments;
    size_t arg_count;
    char** arg_values;
} cli_command_t;

/* Function prototypes */
uesim_error_t cli_init(void);
void cli_cleanup(void);
uesim_error_t cli_process_command(const char* input);
uesim_error_t cli_execute_command(cli_command_t* command);
void cli_print_main_help(void);
void cli_print_system_status(void);

/* Interactive mode */
uesim_error_t cli_start_interactive_mode(void);
void cli_stop_interactive_mode(void);

#endif /* CLI_H */
