/*
 * 5G UE Simulation Application
 * CLI Script Execution Implementation
 */

#include "cli_parser.h"
#include "cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define strdup _strdup
#endif

/* Command history */
#define MAX_HISTORY 100

static char* g_command_history[MAX_HISTORY] = {NULL};
static int g_history_count = 0;
static int g_history_index = 0;

/* Add command to history */
void cli_history_add(const char* command) {
    if (!command || strlen(command) == 0) return;
    
    /* Don't add duplicate consecutive commands */
    if (g_history_count > 0) {
        int last_idx = (g_history_index - 1 + MAX_HISTORY) % MAX_HISTORY;
        if (g_command_history[last_idx] && 
            strcmp(g_command_history[last_idx], command) == 0) {
            return;
        }
    }
    
    /* Free old entry if exists */
    if (g_command_history[g_history_index] != NULL) {
        free(g_command_history[g_history_index]);
    }
    
    g_command_history[g_history_index] = strdup(command);
    g_history_index = (g_history_index + 1) % MAX_HISTORY;
    
    if (g_history_count < MAX_HISTORY) {
        g_history_count++;
    }
}

/* Get history by index (0 = most recent) */
const char* cli_history_get(int offset) {
    if (offset < 0 || offset >= g_history_count) {
        return NULL;
    }
    
    int idx = (g_history_index - 1 - offset + MAX_HISTORY) % MAX_HISTORY;
    return g_command_history[idx];
}

/* Clear history */
void cli_history_clear(void) {
    for (int i = 0; i < MAX_HISTORY; i++) {
        if (g_command_history[i] != NULL) {
            free(g_command_history[i]);
            g_command_history[i] = NULL;
        }
    }
    g_history_count = 0;
    g_history_index = 0;
}

/* Print history */
void cli_history_print(void) {
    printf("\nCommand History (%d commands):\n", g_history_count);
    printf("==============================\n");
    
    for (int i = g_history_count - 1; i >= 0; i--) {
        int idx = (g_history_index - 1 - i + MAX_HISTORY) % MAX_HISTORY;
        if (g_command_history[idx] != NULL) {
            printf("  %3d  %s\n", g_history_count - i, g_command_history[idx]);
        }
    }
    printf("\n");
}

/* Execute script from file */
uesim_error_t cli_script_execute(const char* filename) {
    if (!filename) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        printf("Error: Cannot open script file '%s'\n", filename);
        return UESIM_ERROR_FILE;
    }
    
    printf("Executing script: %s\n", filename);
    printf("------------------------------\n");
    
    char line[CLI_MAX_LINE_LEN];
    int line_num = 0;
    int success_count = 0;
    int error_count = 0;
    uesim_error_t result = UESIM_SUCCESS;
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        line_num++;
        
        /* Remove trailing newline */
        line[strcspn(line, "\n\r")] = 0;
        
        /* Skip empty lines and comments */
        char* trimmed = line;
        while (*trimmed && (*trimmed == ' ' || *trimmed == '\t')) trimmed++;
        
        if (*trimmed == '\0' || *trimmed == '#') {
            continue;
        }
        
        /* Add to history */
        cli_history_add(trimmed);
        
        /* Execute command */
        printf("[%d]> %s\n", line_num, trimmed);
        result = cli_process_command(trimmed);
        
        if (result == UESIM_SUCCESS) {
            success_count++;
        } else {
            error_count++;
            printf("Error at line %d: code %d\n", line_num, result);
            
            /* Stop on error (could make this configurable) */
            if (result == UESIM_ERROR_INVALID_PARAM) {
                printf("Stopping script due to invalid command.\n");
                break;
            }
        }
    }
    
    fclose(fp);
    
    printf("------------------------------\n");
    printf("Script completed: %d commands executed, %d errors\n", 
           success_count, error_count);
    
    return (error_count > 0) ? UESIM_ERROR_PROTOCOL : UESIM_SUCCESS;
}

/* Record commands to file */
typedef struct {
    FILE* fp;
    bool recording;
} cli_recorder_t;

static cli_recorder_t g_recorder = {NULL, false};

uesim_error_t cli_record_start(const char* filename) {
    if (!filename) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (g_recorder.recording) {
        printf("Already recording. Stop current recording first.\n");
        return UESIM_ERROR_ALREADY_EXISTS;
    }
    
    g_recorder.fp = fopen(filename, "w");
    if (!g_recorder.fp) {
        printf("Error: Cannot create recording file '%s'\n", filename);
        return UESIM_ERROR_FILE;
    }
    
    g_recorder.recording = true;
    fprintf(g_recorder.fp, "# UESIM Script Recording\n");
    fprintf(g_recorder.fp, "# Generated: %s\n", __DATE__);
    fprintf(g_recorder.fp, "# ==============================\n\n");
    
    printf("Recording commands to: %s\n", filename);
    printf("Use 'script record stop' to stop recording.\n");
    
    return UESIM_SUCCESS;
}

void cli_record_command(const char* command) {
    if (!g_recorder.recording || !g_recorder.fp || !command) {
        return;
    }
    
    fprintf(g_recorder.fp, "%s\n", command);
    fflush(g_recorder.fp);
}

uesim_error_t cli_record_stop(void) {
    if (!g_recorder.recording) {
        printf("Not currently recording.\n");
        return UESIM_ERROR_NOT_FOUND;
    }
    
    fprintf(g_recorder.fp, "\n# End of recording\n");
    fclose(g_recorder.fp);
    g_recorder.fp = NULL;
    g_recorder.recording = false;
    
    printf("Recording stopped.\n");
    return UESIM_SUCCESS;
}

bool cli_is_recording(void) {
    return g_recorder.recording;
}