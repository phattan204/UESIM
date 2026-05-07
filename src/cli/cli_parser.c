/*
 * 5G UE Simulation Application
 * CLI Parser Implementation - Hierarchical Command Parser
 */

#include "cli_parser.h"
#include "cli.h"
#include "../core/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define strtok_r strtok_s
#endif

/* Noun string mappings */
static const char* g_noun_strings[] = {
    "none", "ue", "gnb", "session", "qos", "scenario",
    "loadtest", "config", "system", "script", "meas", "nas"
};

/* Verb string mappings */
static const char* g_verb_strings[] = {
    "none", "start", "stop", "status", "list", "add", "remove",
    "connect", "disconnect", "create", "release", "modify", "run",
    "show", "set", "load", "save", "validate", "select", "history",
    "record", "info", "benchmark", "help", "exit"
};

/* Global context */
static cli_context_t g_cli_context;

/* External reference to active UE count */
extern uint32_t g_num_active_ues;

/* Parser initialization */
void cli_parser_init(void) {
    cli_context_init(&g_cli_context);
}

void cli_parser_cleanup(void) {
    /* Nothing to clean up currently */
}

/* Context functions */
void cli_context_init(cli_context_t* ctx) {
    if (ctx) {
        ctx->selected_ue_id = -1;
        ctx->selected_gnb_id[0] = '\0';
        ctx->in_context_mode = false;
        ctx->script_file[0] = '\0';
        ctx->script_depth = 0;
        ctx->recording = false;
        ctx->record_file[0] = '\0';
    }
}

void cli_context_set_ue(cli_context_t* ctx, int ue_id) {
    if (ctx) {
        ctx->selected_ue_id = ue_id;
        ctx->in_context_mode = (ue_id >= 0);
    }
}

void cli_context_clear(cli_context_t* ctx) {
    if (ctx) {
        ctx->selected_ue_id = -1;
        ctx->selected_gnb_id[0] = '\0';
        ctx->in_context_mode = false;
    }
}

const char* cli_context_prompt(cli_context_t* ctx) {
    static char prompt[128];
    if (ctx && ctx->in_context_mode && ctx->selected_ue_id >= 0) {
        snprintf(prompt, sizeof(prompt), "uesim(ue:%d)> ", ctx->selected_ue_id);
        return prompt;
    }
    return "uesim> ";
}

/* String conversion functions */
const char* cli_noun_to_string(cli_noun_t noun) {
    if (noun >= CLI_NOUN_NONE && noun < CLI_NOUN_MAX) {
        return g_noun_strings[noun];
    }
    return "unknown";
}

const char* cli_verb_to_string(cli_verb_t verb) {
    if (verb >= CLI_VERB_NONE && verb < CLI_VERB_MAX) {
        return g_verb_strings[verb];
    }
    return "unknown";
}

cli_noun_t cli_string_to_noun(const char* str) {
    if (!str) return CLI_NOUN_NONE;
    for (int i = 0; i < CLI_NOUN_MAX; i++) {
        if (strcasecmp(str, g_noun_strings[i]) == 0) {
            return (cli_noun_t)i;
        }
    }
    return CLI_NOUN_NONE;
}

cli_verb_t cli_string_to_verb(const char* str) {
    if (!str) return CLI_VERB_NONE;
    for (int i = 0; i < CLI_VERB_MAX; i++) {
        if (strcasecmp(str, g_verb_strings[i]) == 0) {
            return (cli_verb_t)i;
        }
    }
    return CLI_VERB_NONE;
}

/* Error functions */
void cli_error_set(cli_error_ctx_t* err, uesim_error_t code, const char* msg, const char* suggestion) {
    if (err) {
        memset(err, 0, sizeof(cli_error_ctx_t));
        err->error = code;
        if (msg) strncpy(err->message, msg, sizeof(err->message) - 1);
        if (suggestion) strncpy(err->suggestion, suggestion, sizeof(err->suggestion) - 1);
        err->recoverable = true;
    }
}

void cli_error_print(const cli_error_ctx_t* err) {
    if (!err) return;
    
    printf("\nError: %s\n", err->message);
    if (err->suggestion[0]) {
        printf("  Suggestion: %s\n", err->suggestion);
    }
    if (err->available_options[0]) {
        printf("  Available: %s\n", err->available_options);
    }
    printf("\n");
}

/* Parse a single line into command structure */
uesim_error_t cli_parse_line(const char* line, cli_parsed_cmd_t* cmd) {
    if (!line || !cmd) return UESIM_ERROR_INVALID_PARAM;
    
    memset(cmd, 0, sizeof(cli_parsed_cmd_t));
    
    /* Skip leading whitespace */
    while (*line && isspace((unsigned char)*line)) line++;
    
    /* Empty line */
    if (*line == '\0') {
        cmd->valid = false;
        return UESIM_SUCCESS;
    }
    
    /* Store raw line */
    cmd->raw_line = strdup(line);
    if (!cmd->raw_line) return UESIM_ERROR_MEMORY;
    
    /* Check for command chaining (&&) */
    char* chain_pos = strstr(line, "&&");
    if (chain_pos) {
        /* For now, just parse the first command */
        /* Full chaining support would require multiple parsed commands */
        size_t first_len = chain_pos - line;
        char* first_cmd = (char*)uesim_malloc(first_len + 1);
        if (first_cmd) {
            strncpy(first_cmd, line, first_len);
            first_cmd[first_len] = '\0';
            uesim_free(cmd->raw_line);
            cmd->raw_line = first_cmd;
            line = first_cmd;
        }
    }
    
    /* Tokenize */
    char* line_copy = strdup(cmd->raw_line);
    if (!line_copy) {
        uesim_free(cmd->raw_line);
        return UESIM_ERROR_MEMORY;
    }
    
    char* tokens[CLI_MAX_ARGS];
    int token_count = 0;
    
    char* saveptr;
    char* token = strtok_r(line_copy, " \t\n", &saveptr);
    while (token && token_count < CLI_MAX_ARGS) {
        tokens[token_count++] = strdup(token);
        token = strtok_r(NULL, " \t\n", &saveptr);
    }
    
    free(line_copy);
    
    if (token_count == 0) {
        cmd->valid = false;
        return UESIM_SUCCESS;
    }
    
    /* Parse verb and noun */
    int arg_idx = 0;
    
    /* First token could be verb or noun */
    /* Check if it's a verb-noun pattern or just a legacy command */
    cli_verb_t potential_verb = cli_string_to_verb(tokens[0]);
    cli_noun_t potential_noun = cli_string_to_noun(tokens[0]);
    
    if (potential_noun != CLI_NOUN_NONE && token_count > 1) {
        /* Pattern: noun verb [args] */
        cmd->noun = potential_noun;
        arg_idx = 1;
        cmd->verb = cli_string_to_verb(tokens[1]);
        if (cmd->verb != CLI_VERB_NONE) {
            arg_idx = 2;
        }
    } else if (potential_verb != CLI_VERB_NONE) {
        /* Pattern: verb [noun] [args] or legacy command */
        cmd->verb = potential_verb;
        arg_idx = 1;
        
        /* Check if second token is a noun */
        if (token_count > 1) {
            cli_noun_t second_noun = cli_string_to_noun(tokens[1]);
            if (second_noun != CLI_NOUN_NONE) {
                cmd->noun = second_noun;
                arg_idx = 2;
            }
        }
    } else {
        /* Unknown command */
        cmd->valid = false;
        cmd->error_msg = strdup("Unknown command");
        for (int i = 0; i < token_count; i++) free(tokens[i]);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Store remaining arguments */
    cmd->arg_count = token_count - arg_idx;
    if (cmd->arg_count > 0) {
        cmd->args = (char**)uesim_malloc(cmd->arg_count * sizeof(char*));
        if (cmd->args) {
            for (size_t i = 0; i < cmd->arg_count; i++) {
                cmd->args[i] = tokens[arg_idx + i];
            }
        }
    }
    
    /* Free unused tokens */
    for (int i = 0; i < arg_idx && i < token_count; i++) {
        free(tokens[i]);
    }
    
    cmd->valid = true;
    return UESIM_SUCCESS;
}

/* Free parsed command */
void cli_free_parsed_cmd(cli_parsed_cmd_t* cmd) {
    if (!cmd) return;
    
    if (cmd->raw_line) {
        uesim_free(cmd->raw_line);
        cmd->raw_line = NULL;
    }
    if (cmd->args) {
        for (size_t i = 0; i < cmd->arg_count; i++) {
            uesim_free(cmd->args[i]);
        }
        uesim_free(cmd->args);
        cmd->args = NULL;
    }
    if (cmd->error_msg) {
        uesim_free(cmd->error_msg);
        cmd->error_msg = NULL;
    }
    cmd->arg_count = 0;
    cmd->valid = false;
}

/* Validate arguments for command */
bool cli_validate_args(const cli_parsed_cmd_t* cmd, cli_error_ctx_t* err) {
    if (!cmd || !cmd->valid) {
        cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "Invalid command", NULL);
        return false;
    }
    
    /* Validate based on verb and noun */
    switch (cmd->noun) {
        case CLI_NOUN_UE:
            switch (cmd->verb) {
                case CLI_VERB_START:
                    if (cmd->arg_count == 0) return true; /* Default 1 UE */
                    if (cmd->arg_count == 1) {
                        int count = atoi(cmd->args[0]);
                        if (count <= 0 || count > MAX_UE_INSTANCES) {
                            cli_error_set(err, UESIM_ERROR_INVALID_PARAM,
                                "Invalid UE count", "Use 'ue start <1-1024>'");
                            snprintf(err->available_options, sizeof(err->available_options),
                                "Current active UEs: %u, Available slots: %d",
                                g_num_active_ues, MAX_UE_INSTANCES - g_num_active_ues);
                            return false;
                        }
                    }
                    break;
                    
                case CLI_VERB_STOP:
                    if (cmd->arg_count == 0) {
                        cli_error_set(err, UESIM_ERROR_INVALID_PARAM,
                            "UE ID required", "Use 'ue stop <id>' or 'ue stop all'");
                        return false;
                    }
                    if (strcasecmp(cmd->args[0], "all") != 0) {
                        int id = atoi(cmd->args[0]);
                        if (id < 0 || id >= MAX_UE_INSTANCES) {
                            cli_error_set(err, UESIM_ERROR_NOT_FOUND,
                                "Invalid UE ID", "Use 'ue stop <0-1023>' or 'ue stop all'");
                            return false;
                        }
                    }
                    break;
                    
                case CLI_VERB_SELECT:
                    if (cmd->arg_count == 0) {
                        cli_error_set(err, UESIM_ERROR_INVALID_PARAM,
                            "UE ID required", "Use 'ue select <id>'");
                        return false;
                    }
                    break;
                    
                default:
                    break;
            }
            break;
            
        case CLI_NOUN_GNB:
            switch (cmd->verb) {
                case CLI_VERB_ADD:
                    if (cmd->arg_count < 3) {
                        cli_error_set(err, UESIM_ERROR_INVALID_PARAM,
                            "Insufficient arguments", "Use 'gnb add <id> <ip> <port> [type]'");
                        return false;
                    }
                    break;
                    
                case CLI_VERB_CONNECT:
                    if (cmd->arg_count < 2) {
                        cli_error_set(err, UESIM_ERROR_INVALID_PARAM,
                            "Insufficient arguments", "Use 'gnb connect <ue> <gnb>'");
                        return false;
                    }
                    break;
                    
                default:
                    break;
            }
            break;
            
        case CLI_NOUN_SESSION:
            if (cmd->verb == CLI_VERB_CREATE || cmd->verb == CLI_VERB_LIST) {
                if (cmd->arg_count == 0 && g_cli_context.selected_ue_id < 0) {
                    cli_error_set(err, UESIM_ERROR_INVALID_PARAM,
                        "UE ID required", "Use 'session create <ue>' or select a UE first");
                    return false;
                }
            }
            break;
            
        case CLI_NOUN_SCENARIO:
            if (cmd->verb == CLI_VERB_RUN && cmd->arg_count == 0) {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM,
                    "Scenario type required", "Use 'scenario run <type> [ue]'");
                return false;
            }
            break;
            
        case CLI_NOUN_CONFIG:
            if (cmd->verb == CLI_VERB_SET && cmd->arg_count < 2) {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM,
                    "Insufficient arguments", "Use 'config set <section.key> <value>'");
                return false;
            }
            break;
            
        default:
            break;
    }
    
    return true;
}

/* Pre-execution validation */
bool cli_validate_pre_execute(const cli_parsed_cmd_t* cmd, cli_error_ctx_t* err) {
    /* Additional runtime validation can be added here */
    (void)cmd;
    (void)err;
    return true;
}

/* Help functions */
void cli_print_noun_help(cli_noun_t noun) {
    printf("\n");
    switch (noun) {
        case CLI_NOUN_UE:
            printf("UE Management Commands:\n");
            printf("======================\n");
            printf("  ue start <n>           Start n UE instances (default: 1)\n");
            printf("  ue stop <id|all>       Stop specific UE or all UEs\n");
            printf("  ue status [id]         Show UE status\n");
            printf("  ue list                List all UEs\n");
            printf("  ue select <id>         Select UE for context mode\n");
            break;
            
        case CLI_NOUN_GNB:
            printf("gNB Management Commands:\n");
            printf("========================\n");
            printf("  gnb add <id> <ip> <port> [type]  Add gNB\n");
            printf("  gnb remove <id>                 Remove gNB\n");
            printf("  gnb connect <ue> <gnb>           Connect UE to gNB\n");
            printf("  gnb disconnect <ue>              Disconnect UE\n");
            printf("  gnb list                         List all gNBs\n");
            printf("  gnb status [id]                  Show gNB status\n");
            break;
            
        case CLI_NOUN_SESSION:
            printf("PDU Session Commands:\n");
            printf("=====================\n");
            printf("  session create <ue> [type]   Create PDU session\n");
            printf("  session release <ue> <id>    Release session\n");
            printf("  session modify <ue> <id>     Modify session\n");
            printf("  session list <ue>            List sessions\n");
            break;
            
        case CLI_NOUN_QOS:
            printf("QoS Flow Commands:\n");
            printf("==================\n");
            printf("  qos add <ue> <5qi> [gbr]     Add QoS flow\n");
            printf("  qos remove <ue> <qfi>       Remove QoS flow\n");
            printf("  qos list <ue>               List QoS flows\n");
            printf("  qos set-ambr <ue> <ul> <dl>  Set session AMBR\n");
            break;
            
        case CLI_NOUN_SCENARIO:
            printf("Scenario Commands:\n");
            printf("==================\n");
            printf("  scenario run <type> [ue]    Execute scenario\n");
            printf("  scenario list               List available scenarios\n");
            printf("  scenario status             Show scenario status\n");
            printf("\nScenarios: registration, establishment, reestablishment, handover\n");
            break;
            
        case CLI_NOUN_LOADTEST:
            printf("Load Test Commands:\n");
            printf("==================\n");
            printf("  loadtest start <scenario> <ues> <dur>  Start load test\n");
            printf("  loadtest stop                           Stop load test\n");
            printf("  loadtest status                         Show status\n");
            printf("  loadtest report <format>                Generate report\n");
            break;
            
        case CLI_NOUN_CONFIG:
            printf("Configuration Commands:\n");
            printf("======================\n");
            printf("  config show [section]         Show configuration\n");
            printf("  config set <section.key> <v>  Set value\n");
            printf("  config load <file>            Load from file\n");
            printf("  config save <file>            Save to file\n");
            printf("  config validate               Validate configuration\n");
            printf("\nSections: general, network, ue, rrc, pdcp, rlc, mac, nas, performance, security\n");
            break;
            
        case CLI_NOUN_SYSTEM:
            printf("System Commands:\n");
            printf("================\n");
            printf("  system status      Show system status\n");
            printf("  system info        Show system information\n");
            printf("  system benchmark   Run benchmarks\n");
            break;
            
        case CLI_NOUN_SCRIPT:
            printf("Script Commands:\n");
            printf("================\n");
            printf("  script run <file>    Execute script file\n");
            printf("  script history       Show command history\n");
            printf("  script record <file> Record commands to file\n");
            break;
            
        default:
            printf("Unknown noun. Type 'help' for available commands.\n");
            break;
    }
    printf("\n");
}

void cli_print_verb_help(cli_verb_t verb) {
    printf("\nVerb: %s\n", cli_verb_to_string(verb));
    printf("Usage varies by noun. Type 'help <noun>' for details.\n\n");
}

void cli_print_examples(cli_noun_t noun) {
    printf("\nExamples:\n");
    switch (noun) {
        case CLI_NOUN_UE:
            printf("  ue start 5                    # Start 5 UEs\n");
            printf("  ue stop all                   # Stop all UEs\n");
            printf("  ue select 0                   # Select UE 0 for context\n");
            printf("  ue status                     # Show all UE status\n");
            break;
        case CLI_NOUN_GNB:
            printf("  gnb add gnb1 192.168.1.100 38412   # Add gNB\n");
            printf("  gnb connect 0 gnb1                 # Connect UE 0 to gnb1\n");
            break;
        case CLI_NOUN_SCENARIO:
            printf("  scenario run registration 0   # Run registration on UE 0\n");
            printf("  scenario run handover 1       # Run handover on UE 1\n");
            break;
        case CLI_NOUN_CONFIG:
            printf("  config set general.verbose true    # Set verbose\n");
            printf("  config show network                 # Show network config\n");
            break;
        default:
            printf("  No examples available.\n");
            break;
    }
    printf("\n");
}

/* Get global context */
cli_context_t* cli_get_context(void) {
    return &g_cli_context;
}