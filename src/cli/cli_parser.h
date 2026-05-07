/*
 * 5G UE Simulation Application
 * CLI Parser Header - Hierarchical Command Parser
 */

#ifndef CLI_PARSER_H
#define CLI_PARSER_H

#include "../uesim.h"

/* Maximum limits */
#define CLI_MAX_ARGS           32
#define CLI_MAX_ARG_LEN        256
#define CLI_MAX_LINE_LEN       1024
#define CLI_MAX_HISTORY        100
#define CLI_MAX_SCRIPT_DEPTH   10

/* Command nouns (objects) */
typedef enum {
    CLI_NOUN_NONE = 0,
    CLI_NOUN_UE,
    CLI_NOUN_GNB,
    CLI_NOUN_SESSION,
    CLI_NOUN_QOS,
    CLI_NOUN_SCENARIO,
    CLI_NOUN_LOADTEST,
    CLI_NOUN_CONFIG,
    CLI_NOUN_SYSTEM,
    CLI_NOUN_SCRIPT,
    CLI_NOUN_MEAS,
    CLI_NOUN_NAS,
    CLI_NOUN_MAX
} cli_noun_t;

/* Command verbs (actions) */
typedef enum {
    CLI_VERB_NONE = 0,
    CLI_VERB_START,
    CLI_VERB_STOP,
    CLI_VERB_STATUS,
    CLI_VERB_LIST,
    CLI_VERB_ADD,
    CLI_VERB_REMOVE,
    CLI_VERB_CONNECT,
    CLI_VERB_DISCONNECT,
    CLI_VERB_CREATE,
    CLI_VERB_RELEASE,
    CLI_VERB_MODIFY,
    CLI_VERB_RUN,
    CLI_VERB_SHOW,
    CLI_VERB_SET,
    CLI_VERB_LOAD,
    CLI_VERB_SAVE,
    CLI_VERB_VALIDATE,
    CLI_VERB_SELECT,
    CLI_VERB_HISTORY,
    CLI_VERB_RECORD,
    CLI_VERB_INFO,
    CLI_VERB_BENCHMARK,
    CLI_VERB_HELP,
    CLI_VERB_EXIT,
    CLI_VERB_MAX
} cli_verb_t;

/* Parsed command structure */
typedef struct {
    cli_verb_t verb;
    cli_noun_t noun;
    char* raw_line;
    char** args;
    size_t arg_count;
    char* error_msg;
    bool valid;
} cli_parsed_cmd_t;

/* Command context */
typedef struct {
    int selected_ue_id;         /* -1 if no UE selected */
    char selected_gnb_id[64];   /* Empty if no gNB selected */
    bool in_context_mode;
    char script_file[256];
    int script_depth;
    bool recording;
    char record_file[256];
} cli_context_t;

/* Error context structure */
typedef struct {
    uesim_error_t error;
    char message[256];
    char suggestion[256];
    char failed_command[128];
    bool recoverable;
    char available_options[512];
} cli_error_ctx_t;

/* Parser API */
void cli_parser_init(void);
void cli_parser_cleanup(void);

uesim_error_t cli_parse_line(const char* line, cli_parsed_cmd_t* cmd);
void cli_free_parsed_cmd(cli_parsed_cmd_t* cmd);

/* Context API */
void cli_context_init(cli_context_t* ctx);
void cli_context_set_ue(cli_context_t* ctx, int ue_id);
void cli_context_clear(cli_context_t* ctx);
const char* cli_context_prompt(cli_context_t* ctx);

/* Error API */
void cli_error_set(cli_error_ctx_t* err, uesim_error_t code, const char* msg, const char* suggestion);
void cli_error_print(const cli_error_ctx_t* err);

/* Validation API */
bool cli_validate_args(const cli_parsed_cmd_t* cmd, cli_error_ctx_t* err);
bool cli_validate_pre_execute(const cli_parsed_cmd_t* cmd, cli_error_ctx_t* err);

/* Help API */
void cli_print_noun_help(cli_noun_t noun);
void cli_print_verb_help(cli_verb_t verb);
void cli_print_examples(cli_noun_t noun);

/* String conversion */
const char* cli_noun_to_string(cli_noun_t noun);
const char* cli_verb_to_string(cli_verb_t verb);
cli_noun_t cli_string_to_noun(const char* str);
cli_verb_t cli_string_to_verb(const char* str);

/* Context access */
cli_context_t* cli_get_context(void);

#endif /* CLI_PARSER_H */
