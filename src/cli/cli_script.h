/*
 * 5G UE Simulation Application
 * CLI Script Execution Header
 */

#ifndef CLI_SCRIPT_H
#define CLI_SCRIPT_H

#include "../uesim.h"

/* Command history */
void cli_history_add(const char* command);
const char* cli_history_get(int offset);
void cli_history_clear(void);
void cli_history_print(void);

/* Script execution */
uesim_error_t cli_script_execute(const char* filename);

/* Command recording */
uesim_error_t cli_record_start(const char* filename);
void cli_record_command(const char* command);
uesim_error_t cli_record_stop(void);
bool cli_is_recording(void);

#endif /* CLI_SCRIPT_H */