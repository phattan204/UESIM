/*
 * 5G UE Simulation Application
 * Command Line Interface Implementation
 */

#include "cli.h"
#include "cli_parser.h"
#include "cli_script.h"
#include "../protocol/rrc.h"
#include "../protocol/rrc_meas.h"
#include "../transport/socket_mgr.h"
#include "../config/config.h"
#include "../core/thread_pool.h"
#include "../nas/nas.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#ifdef _WIN32
#define strcasecmp _stricmp
#define strdup _strdup
#endif

/* External configuration reference */
extern uesim_config_t g_config;

/* Global variables */
#ifdef _WIN32
static volatile LONG g_cli_running = 0;
#else
static atomic_bool g_cli_running = false;
#endif
static pthread_t g_cli_thread;

/* Global UE context management */
ue_context_t* g_ue_contexts[MAX_UE_INSTANCES];
uint32_t g_num_active_ues = 0;

/* Global gNB registry - stores gNB contexts by name */
#define MAX_GNB_REGISTRY 32
typedef struct {
    char name[64];
    gnb_context_t* gnb_ctx;
    uint32_t gnb_id;
    char ip[46];          /* IPv6 max length */
    uint16_t port;
    gnb_type_t type;
} gnb_registry_entry_t;
static gnb_registry_entry_t g_gnb_registry[MAX_GNB_REGISTRY];
static uint32_t g_num_registered_gnbs = 0;
static uint32_t g_gnb_id_counter = 0;

/* Legacy command string mappings */
static const char* g_command_strings[] = {
    "start", "stop", "status", "config", "scenario",
    "help", "exit", "show", "set", "save", "load",
    "qos", "session", "loadtest", "gnb"
};

/* Forward declarations for new handlers */
static uesim_error_t cli_exec_ue_cmd(const cli_parsed_cmd_t* cmd, cli_error_ctx_t* err);
static uesim_error_t cli_exec_gnb_cmd(const cli_parsed_cmd_t* cmd, cli_error_ctx_t* err);
static uesim_error_t cli_exec_session_cmd(const cli_parsed_cmd_t* cmd, cli_error_ctx_t* err);
static uesim_error_t cli_exec_qos_cmd(const cli_parsed_cmd_t* cmd, cli_error_ctx_t* err);
static uesim_error_t cli_exec_scenario_cmd(const cli_parsed_cmd_t* cmd, cli_error_ctx_t* err);
static uesim_error_t cli_exec_loadtest_cmd(const cli_parsed_cmd_t* cmd, cli_error_ctx_t* err);
static uesim_error_t cli_exec_config_cmd(const cli_parsed_cmd_t* cmd, cli_error_ctx_t* err);
static uesim_error_t cli_exec_system_cmd(const cli_parsed_cmd_t* cmd, cli_error_ctx_t* err);
static uesim_error_t cli_exec_script_cmd(const cli_parsed_cmd_t* cmd, cli_error_ctx_t* err);
static uesim_error_t cli_exec_meas_cmd(const cli_parsed_cmd_t* cmd, cli_error_ctx_t* err);
static uesim_error_t cli_exec_nas_cmd(const cli_parsed_cmd_t* cmd, cli_error_ctx_t* err);

/* Legacy command handlers (for backward compatibility) */
uesim_error_t cli_handle_start(cli_command_t* command);
uesim_error_t cli_handle_stop(cli_command_t* command);
uesim_error_t cli_handle_status(cli_command_t* command);

/* Initialization */
uesim_error_t cli_init(void) {
    memset(g_ue_contexts, 0, sizeof(g_ue_contexts));
    g_num_active_ues = 0;
    cli_parser_init();
    printf("CLI initialized successfully\n");
    return UESIM_SUCCESS;
}

void cli_cleanup(void) {
    cli_stop_interactive_mode();
    cli_parser_cleanup();
    printf("CLI cleanup completed\n");
}

/* Main command processor using new parser */
uesim_error_t cli_process_command(const char* input) {
    if (input == NULL || strlen(input) == 0) {
        return UESIM_SUCCESS;
    }
    
    cli_parsed_cmd_t cmd;
    cli_error_ctx_t err;
    uesim_error_t result = UESIM_SUCCESS;
    
    /* Parse the command */
    result = cli_parse_line(input, &cmd);
    if (result != UESIM_SUCCESS || !cmd.valid) {
        if (cmd.error_msg) {
            printf("Error: %s\n", cmd.error_msg);
        }
        cli_free_parsed_cmd(&cmd);
        return result;
    }
    
    /* Validate arguments */
    if (!cli_validate_args(&cmd, &err)) {
        cli_error_print(&err);
        cli_free_parsed_cmd(&cmd);
        return err.error;
    }
    
    /* Execute based on noun */
    switch (cmd.noun) {
        case CLI_NOUN_UE:
            result = cli_exec_ue_cmd(&cmd, &err);
            break;
        case CLI_NOUN_GNB:
            result = cli_exec_gnb_cmd(&cmd, &err);
            break;
        case CLI_NOUN_SESSION:
            result = cli_exec_session_cmd(&cmd, &err);
            break;
        case CLI_NOUN_QOS:
            result = cli_exec_qos_cmd(&cmd, &err);
            break;
        case CLI_NOUN_SCENARIO:
            result = cli_exec_scenario_cmd(&cmd, &err);
            break;
        case CLI_NOUN_LOADTEST:
            result = cli_exec_loadtest_cmd(&cmd, &err);
            break;
        case CLI_NOUN_CONFIG:
            result = cli_exec_config_cmd(&cmd, &err);
            break;
        case CLI_NOUN_SYSTEM:
            result = cli_exec_system_cmd(&cmd, &err);
            break;
        case CLI_NOUN_SCRIPT:
            result = cli_exec_script_cmd(&cmd, &err);
            break;
        case CLI_NOUN_MEAS:
            result = cli_exec_meas_cmd(&cmd, &err);
            break;
        case CLI_NOUN_NAS:
            result = cli_exec_nas_cmd(&cmd, &err);
            break;
        case CLI_NOUN_NONE:
            /* Handle verb-only commands */
            switch (cmd.verb) {
                case CLI_VERB_HELP:
                    cli_print_main_help();
                    break;
                case CLI_VERB_EXIT:
                    printf("Exiting...\n");
                    atomic_store(&g_cli_running, false);
                    break;
                case CLI_VERB_STATUS:
                    cli_print_system_status();
                    break;
                default:
                    printf("Unknown command. Type 'help' for available commands.\n");
                    break;
            }
            break;
        default:
            printf("Unknown command. Type 'help' for available commands.\n");
            break;
    }
    
    /* Print error if execution failed */
    if (result != UESIM_SUCCESS && err.message[0]) {
        cli_error_print(&err);
    }
    
    cli_free_parsed_cmd(&cmd);
    return result;
}

/* UE command handler */
static uesim_error_t cli_exec_ue_cmd(const cli_parsed_cmd_t* cmd, cli_error_ctx_t* err) {
    cli_context_t* ctx = cli_get_context();
    
    switch (cmd->verb) {
        case CLI_VERB_START: {
            uint32_t count = (cmd->arg_count > 0) ? (uint32_t)atoi(cmd->args[0]) : 1;
            if (count == 0 || count > MAX_UE_INSTANCES - g_num_active_ues) {
                cli_error_set(err, UESIM_ERROR_CAPACITY, "Cannot create UEs",
                    count == 0 ? "Count must be > 0" : "Not enough available slots");
                return UESIM_ERROR_CAPACITY;
            }
            
            printf("Starting %u UE instance(s)...\n", count);
            for (uint32_t i = 0; i < count; i++) {
                int slot = -1;
                for (int j = 0; j < MAX_UE_INSTANCES; j++) {
                    if (g_ue_contexts[j] == NULL) { slot = j; break; }
                }
                if (slot < 0) {
                    cli_error_set(err, UESIM_ERROR_CAPACITY, "No available slots", NULL);
                    return UESIM_ERROR_CAPACITY;
                }
                
                ue_context_t* ue_ctx = NULL;
                uesim_error_t result = uesim_create_ue_instance(&ue_ctx);
                if (result != UESIM_SUCCESS) {
                    cli_error_set(err, result, "Failed to create UE instance", NULL);
                    return result;
                }
                
                ue_ctx->ue_id = (uint32_t)slot;
                result = uesim_start_ue(ue_ctx);
                if (result != UESIM_SUCCESS) {
                    uesim_free(ue_ctx);
                    cli_error_set(err, result, "Failed to start UE", NULL);
                    return result;
                }
                
                g_ue_contexts[slot] = ue_ctx;
                g_num_active_ues++;
            }
            printf("Started %u UE(s). Total active: %u\n", count, g_num_active_ues);
            return UESIM_SUCCESS;
        }
        
        case CLI_VERB_STOP: {
            if (cmd->arg_count == 0) {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "UE ID required", "Use 'ue stop <id>' or 'ue stop all'");
                return UESIM_ERROR_INVALID_PARAM;
            }
            
            if (strcasecmp(cmd->args[0], "all") == 0) {
                for (int i = 0; i < MAX_UE_INSTANCES; i++) {
                    if (g_ue_contexts[i] != NULL) {
                        uesim_stop_ue(g_ue_contexts[i]);
                        uesim_free(g_ue_contexts[i]);
                        g_ue_contexts[i] = NULL;
                    }
                }
                g_num_active_ues = 0;
                cli_context_clear(ctx);
                printf("All UEs stopped.\n");
            } else {
                int id = atoi(cmd->args[0]);
                if (id < 0 || id >= MAX_UE_INSTANCES || g_ue_contexts[id] == NULL) {
                    cli_error_set(err, UESIM_ERROR_NOT_FOUND, "UE not found", NULL);
                    return UESIM_ERROR_NOT_FOUND;
                }
                uesim_stop_ue(g_ue_contexts[id]);
                uesim_free(g_ue_contexts[id]);
                g_ue_contexts[id] = NULL;
                g_num_active_ues--;
                if (ctx->selected_ue_id == id) {
                    cli_context_clear(ctx);
                }
                printf("UE %d stopped.\n", id);
            }
            return UESIM_SUCCESS;
        }
        
        case CLI_VERB_STATUS: {
            if (cmd->arg_count > 0) {
                int id = atoi(cmd->args[0]);
                if (id < 0 || id >= MAX_UE_INSTANCES || g_ue_contexts[id] == NULL) {
                    cli_error_set(err, UESIM_ERROR_NOT_FOUND, "UE not found", NULL);
                    return UESIM_ERROR_NOT_FOUND;
                }
                ue_context_t* ue = g_ue_contexts[id];
                printf("\nUE %d Status:\n", id);
                printf("  RRC State: %s\n", rrc_state_to_string(rrc_get_current_state(ue)));
                printf("  Serving gNB: %u\n", ue->serving_gnb ? ue->serving_gnb->gnb_id : 0xFFFF);
            } else {
                printf("\nUE Status (Active: %u):\n", g_num_active_ues);
                printf("  %-6s %-12s %-10s\n", "ID", "RRC State", "gNB");
                printf("  %-6s %-12s %-10s\n", "------", "----------", "----");
                for (int i = 0; i < MAX_UE_INSTANCES; i++) {
                    if (g_ue_contexts[i] != NULL) {
                        ue_context_t* ue = g_ue_contexts[i];
                        printf("  %-6d %-12s %-10u\n", i,
                               rrc_state_to_string(rrc_get_current_state(ue)),
                               ue->serving_gnb ? ue->serving_gnb->gnb_id : 0xFFFF);
                    }
                }
            }
            printf("\n");
            return UESIM_SUCCESS;
        }
        
        case CLI_VERB_LIST: {
            printf("\nActive UEs (%u):\n", g_num_active_ues);
            for (int i = 0; i < MAX_UE_INSTANCES; i++) {
                if (g_ue_contexts[i] != NULL) {
                    printf("  UE %d: %s\n", i, rrc_state_to_string(rrc_get_current_state(g_ue_contexts[i])));
                }
            }
            printf("\n");
            return UESIM_SUCCESS;
        }
        
        case CLI_VERB_SELECT: {
            if (cmd->arg_count == 0) {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "UE ID required", "Use 'ue select <id>'");
                return UESIM_ERROR_INVALID_PARAM;
            }
            int id = atoi(cmd->args[0]);
            if (id < 0 || id >= MAX_UE_INSTANCES || g_ue_contexts[id] == NULL) {
                cli_error_set(err, UESIM_ERROR_NOT_FOUND, "UE not found", NULL);
                return UESIM_ERROR_NOT_FOUND;
            }
            cli_context_set_ue(ctx, id);
            printf("Selected UE %d. Context mode enabled.\n", id);
            return UESIM_SUCCESS;
        }
        
        default:
            cli_print_noun_help(CLI_NOUN_UE);
            return UESIM_SUCCESS;
    }
}

/* Helper: Find gNB in registry by name */
static gnb_registry_entry_t* find_gnb_in_registry(const char* name) {
    for (uint32_t i = 0; i < g_num_registered_gnbs; i++) {
        if (strcasecmp(g_gnb_registry[i].name, name) == 0) {
            return &g_gnb_registry[i];
        }
    }
    return NULL;
}

/* gNB command handler */
static uesim_error_t cli_exec_gnb_cmd(const cli_parsed_cmd_t* cmd, cli_error_ctx_t* err) {
    switch (cmd->verb) {
        case CLI_VERB_ADD: {
            /* gnb add <name> <ip> <port> [type] */
            if (cmd->arg_count < 3) {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "Insufficient arguments",
                    "Use 'gnb add <name> <ip> <port> [type]'");
                return UESIM_ERROR_INVALID_PARAM;
            }
            
            const char* name = cmd->args[0];
            const char* ip = cmd->args[1];
            uint16_t port = (uint16_t)atoi(cmd->args[2]);
            gnb_type_t type = GNB_TYPE_MOCK;
            
            /* Parse optional type */
            if (cmd->arg_count > 3) {
                const char* type_str = cmd->args[3];
                if (strcasecmp(type_str, "oai") == 0) type = GNB_TYPE_OAI;
                else if (strcasecmp(type_str, "srsran") == 0) type = GNB_TYPE_SRSRAN;
                else if (strcasecmp(type_str, "commercial") == 0) type = GNB_TYPE_COMMERCIAL;
                else if (strcasecmp(type_str, "mock") == 0) type = GNB_TYPE_MOCK;
            }
            
            /* Check if name already exists */
            if (find_gnb_in_registry(name) != NULL) {
                cli_error_set(err, UESIM_ERROR_ALREADY_EXISTS, "gNB name already exists", NULL);
                return UESIM_ERROR_ALREADY_EXISTS;
            }
            
            /* Check registry capacity */
            if (g_num_registered_gnbs >= MAX_GNB_REGISTRY) {
                cli_error_set(err, UESIM_ERROR_CAPACITY, "gNB registry full", NULL);
                return UESIM_ERROR_CAPACITY;
            }
            
            /* Store in registry - we'll create the actual gNB context when connecting to a UE */
            gnb_registry_entry_t* entry = &g_gnb_registry[g_num_registered_gnbs++];
            strncpy(entry->name, name, sizeof(entry->name) - 1);
            entry->name[sizeof(entry->name) - 1] = '\0';
            strncpy(entry->ip, ip, sizeof(entry->ip) - 1);
            entry->ip[sizeof(entry->ip) - 1] = '\0';
            entry->port = port;
            entry->type = type;
            entry->gnb_ctx = NULL;  /* Will be created when connected to UE */
            entry->gnb_id = ++g_gnb_id_counter;
            
            printf("Added gNB '%s' (ID: %u) at %s:%u, type: %s\n", 
                   name, entry->gnb_id, ip, port, uesim_gnb_type_str(type));
            return UESIM_SUCCESS;
        }
            
        case CLI_VERB_LIST: {
            printf("\nConfigured gNBs (%u):\n", g_num_registered_gnbs);
            if (g_num_registered_gnbs == 0) {
                printf("  (none)\n");
            } else {
                printf("  %-12s %-6s %-15s %-6s %-10s\n", "Name", "ID", "IP", "Port", "State");
                printf("  %-12s %-6s %-15s %-6s %-10s\n", "----", "--", "--", "----", "-----");
                for (uint32_t i = 0; i < g_num_registered_gnbs; i++) {
                    gnb_registry_entry_t* entry = &g_gnb_registry[i];
                    const char* state = "Defined";
                    if (entry->gnb_ctx != NULL) {
                        state = uesim_gnb_state_str(entry->gnb_ctx->state);
                    }
                    /* Get IP/port from gNB context if available, otherwise show N/A */
                    char ip_port[32];
                    if (entry->gnb_ctx != NULL) {
                        snprintf(ip_port, sizeof(ip_port), "%s:%u",
                                 inet_ntoa(entry->gnb_ctx->addr.sin_addr),
                                 ntohs(entry->gnb_ctx->addr.sin_port));
                    } else {
                        snprintf(ip_port, sizeof(ip_port), "N/A");
                    }
                    printf("  %-12s %-6u %-15s %-6s %-10s\n", 
                           entry->name, entry->gnb_id, ip_port, "-", state);
                }
            }
            printf("\n");
            return UESIM_SUCCESS;
        }
            
        case CLI_VERB_CONNECT: {
            /* gnb connect <ue_id> <gnb_name> */
            if (cmd->arg_count < 2) {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "Insufficient arguments",
                    "Use 'gnb connect <ue> <gnb>'");
                return UESIM_ERROR_INVALID_PARAM;
            }
            
            int ue_id = atoi(cmd->args[0]);
            const char* gnb_name = cmd->args[1];
            
            /* Validate UE */
            if (ue_id < 0 || ue_id >= MAX_UE_INSTANCES || g_ue_contexts[ue_id] == NULL) {
                cli_error_set(err, UESIM_ERROR_NOT_FOUND, "UE not found", NULL);
                return UESIM_ERROR_NOT_FOUND;
            }
            
            /* Find gNB in registry */
            gnb_registry_entry_t* entry = find_gnb_in_registry(gnb_name);
            if (entry == NULL) {
                cli_error_set(err, UESIM_ERROR_NOT_FOUND, "gNB not found", 
                    "Use 'gnb list' to see available gNBs");
                return UESIM_ERROR_NOT_FOUND;
            }
            
            ue_context_t* ue_ctx = g_ue_contexts[ue_id];
            
            /* Create gNB context if not yet created for this UE */
            if (entry->gnb_ctx == NULL) {
                gnb_context_t* gnb_ctx = NULL;
                uesim_error_t result = uesim_add_gnb(ue_ctx, entry->type, 
                                                     entry->ip, entry->port, &gnb_ctx);
                if (result != UESIM_SUCCESS) {
                    cli_error_set(err, result, "Failed to create gNB context", NULL);
                    return result;
                }
                entry->gnb_ctx = gnb_ctx;
            }
            
            /* Connect to gNB */
            uesim_error_t result = uesim_connect_gnb(ue_ctx, entry->gnb_ctx);
            if (result != UESIM_SUCCESS) {
                cli_error_set(err, result, "Failed to connect to gNB", 
                    "Check if gNB is reachable");
                return result;
            }
            
            printf("Connected UE %d to gNB '%s' (ID: %u)\n", ue_id, gnb_name, entry->gnb_id);
            return UESIM_SUCCESS;
        }
        
        case CLI_VERB_DISCONNECT: {
            /* gnb disconnect <ue_id> */
            if (cmd->arg_count < 1) {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "UE ID required",
                    "Use 'gnb disconnect <ue>'");
                return UESIM_ERROR_INVALID_PARAM;
            }
            
            int ue_id = atoi(cmd->args[0]);
            
            if (ue_id < 0 || ue_id >= MAX_UE_INSTANCES || g_ue_contexts[ue_id] == NULL) {
                cli_error_set(err, UESIM_ERROR_NOT_FOUND, "UE not found", NULL);
                return UESIM_ERROR_NOT_FOUND;
            }
            
            ue_context_t* ue_ctx = g_ue_contexts[ue_id];
            if (ue_ctx->serving_gnb == NULL) {
                printf("UE %d has no gNB connection\n", ue_id);
                return UESIM_SUCCESS;
            }
            
            uint32_t gnb_id = ue_ctx->serving_gnb->gnb_id;
            uesim_error_t result = uesim_disconnect_gnb(ue_ctx, ue_ctx->serving_gnb);
            if (result != UESIM_SUCCESS) {
                cli_error_set(err, result, "Failed to disconnect from gNB", NULL);
                return result;
            }
            
            printf("Disconnected UE %d from gNB %u\n", ue_id, gnb_id);
            return UESIM_SUCCESS;
        }
        
        case CLI_VERB_REMOVE: {
            /* gnb remove <name> */
            if (cmd->arg_count < 1) {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "gNB name required",
                    "Use 'gnb remove <name>'");
                return UESIM_ERROR_INVALID_PARAM;
            }
            
            const char* name = cmd->args[0];
            gnb_registry_entry_t* entry = find_gnb_in_registry(name);
            if (entry == NULL) {
                cli_error_set(err, UESIM_ERROR_NOT_FOUND, "gNB not found", NULL);
                return UESIM_ERROR_NOT_FOUND;
            }
            
            /* Disconnect from all UEs using this gNB */
            if (entry->gnb_ctx != NULL) {
                for (int i = 0; i < MAX_UE_INSTANCES; i++) {
                    if (g_ue_contexts[i] != NULL && 
                        g_ue_contexts[i]->serving_gnb == entry->gnb_ctx) {
                        uesim_disconnect_gnb(g_ue_contexts[i], entry->gnb_ctx);
                    }
                }
            }
            
            /* Remove from registry by shifting */
            uint32_t idx = (uint32_t)(entry - g_gnb_registry);
            for (uint32_t i = idx; i < g_num_registered_gnbs - 1; i++) {
                g_gnb_registry[i] = g_gnb_registry[i + 1];
            }
            g_num_registered_gnbs--;
            
            printf("Removed gNB '%s'\n", name);
            return UESIM_SUCCESS;
        }
            
        default:
            cli_print_noun_help(CLI_NOUN_GNB);
            return UESIM_SUCCESS;
    }
}

/* Session command handler */
static uesim_error_t cli_exec_session_cmd(const cli_parsed_cmd_t* cmd, cli_error_ctx_t* err) {
    cli_context_t* ctx = cli_get_context();
    int ue_id = ctx->selected_ue_id;
    
    if (cmd->arg_count > 0) {
        ue_id = atoi(cmd->args[0]);
    }
    
    if (ue_id < 0) {
        cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "UE ID required",
            "Use 'session <verb> <ue>' or select a UE first");
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Validate UE exists */
    if (ue_id < 0 || ue_id >= MAX_UE_INSTANCES || g_ue_contexts[ue_id] == NULL) {
        cli_error_set(err, UESIM_ERROR_NOT_FOUND, "UE not found", NULL);
        return UESIM_ERROR_NOT_FOUND;
    }
    
    ue_context_t* ue_ctx = g_ue_contexts[ue_id];
    
    switch (cmd->verb) {
        case CLI_VERB_CREATE: {
            /* session create [ue] [type] */
            nas_ue_context_t* nas_ctx = ue_get_nas_context(ue_ctx);
            if (nas_ctx == NULL) {
                cli_error_set(err, UESIM_ERROR_NOT_INITIALIZED, "NAS context not initialized", 
                    "UE must be registered first");
                return UESIM_ERROR_NOT_INITIALIZED;
            }
            
            /* Find free PDU session ID (1-15) */
            uint8_t session_id = 0;
            for (int i = 1; i < NAS_MAX_PDU_SESSIONS; i++) {
                if (!nas_ctx->pdu_sessions[i].active) {
                    session_id = i;
                    break;
                }
            }
            
            if (session_id == 0) {
                cli_error_set(err, UESIM_ERROR_CAPACITY, "No available PDU session slots", NULL);
                return UESIM_ERROR_CAPACITY;
            }
            
            /* Initiate PDU session establishment */
            uesim_error_t result = nas_initiate_pdu_session_establishment(
                nas_ctx, session_id, NAS_PDU_SESSION_TYPE_IPV4);
            
            if (result != UESIM_SUCCESS) {
                cli_error_set(err, result, "Failed to create PDU session", NULL);
                return result;
            }
            
            printf("Created PDU session %u for UE %d (IPv4)\n", session_id, ue_id);
            return UESIM_SUCCESS;
        }
            
        case CLI_VERB_LIST: {
            nas_ue_context_t* nas_ctx = ue_get_nas_context(ue_ctx);
            if (nas_ctx == NULL) {
                printf("\nPDU Sessions for UE %d: NAS not initialized\n\n", ue_id);
                return UESIM_SUCCESS;
            }
            
            printf("\nPDU Sessions for UE %d:\n", ue_id);
            printf("  %-8s %-8s %-10s %-15s %-8s\n", "ID", "Type", "State", "IP Address", "QoS");
            printf("  %-8s %-8s %-10s %-15s %-8s\n", "----", "----", "-----", "----------", "----");
            
            bool found = false;
            for (int i = 1; i < NAS_MAX_PDU_SESSIONS; i++) {
                nas_pdu_session_t* sess = &nas_ctx->pdu_sessions[i];
                if (sess->active) {
                    found = true;
                    const char* type_str = "IPv4";
                    if (sess->session_type == NAS_PDU_SESSION_TYPE_IPV6) type_str = "IPv6";
                    else if (sess->session_type == NAS_PDU_SESSION_TYPE_IPV4V6) type_str = "IPv4v6";
                    
                    const char* state_str = "Inactive";
                    switch (sess->state) {
                        case NAS_5GSM_PDU_SESSION_ACTIVE: state_str = "Active"; break;
                        case NAS_5GSM_PDU_SESSION_ACTIVE_PENDING: state_str = "Pending"; break;
                        case NAS_5GSM_PDU_SESSION_MODIFICATION_PENDING: state_str = "Modifying"; break;
                        case NAS_5GSM_PDU_SESSION_RELEASED_PENDING: state_str = "Releasing"; break;
                        default: break;
                    }
                    
                    char ip_str[20] = "N/A";
                    if (sess->pdu_address != 0) {
                        struct in_addr addr;
                        addr.s_addr = sess->pdu_address;
                        strncpy(ip_str, inet_ntoa(addr), sizeof(ip_str) - 1);
                    }
                    
                    printf("  %-8u %-8s %-10s %-15s %u flow(s)\n", 
                           sess->pdu_session_id, type_str, state_str, ip_str, sess->num_qos_flows);
                }
            }
            
            if (!found) {
                printf("  No active sessions\n");
            }
            printf("\n");
            return UESIM_SUCCESS;
        }
            
        case CLI_VERB_RELEASE: {
            /* session release <ue> [session_id] */
            uint8_t session_id = 1;  /* Default to first session */
            if (cmd->arg_count > 1) {
                session_id = (uint8_t)atoi(cmd->args[1]);
            }
            
            nas_ue_context_t* nas_ctx = ue_get_nas_context(ue_ctx);
            if (nas_ctx == NULL) {
                cli_error_set(err, UESIM_ERROR_NOT_INITIALIZED, "NAS context not initialized", NULL);
                return UESIM_ERROR_NOT_INITIALIZED;
            }
            
            if (session_id >= NAS_MAX_PDU_SESSIONS || !nas_ctx->pdu_sessions[session_id].active) {
                cli_error_set(err, UESIM_ERROR_NOT_FOUND, "PDU session not found", NULL);
                return UESIM_ERROR_NOT_FOUND;
            }
            
            /* Mark session as releasing */
            nas_ctx->pdu_sessions[session_id].state = NAS_5GSM_PDU_SESSION_RELEASED_PENDING;
            nas_ctx->pdu_sessions[session_id].active = false;
            nas_ctx->num_active_sessions--;
            
            printf("Released PDU session %u for UE %d\n", session_id, ue_id);
            return UESIM_SUCCESS;
        }
            
        default:
            cli_print_noun_help(CLI_NOUN_SESSION);
            return UESIM_SUCCESS;
    }
}

/* QoS command handler */
static uesim_error_t cli_exec_qos_cmd(const cli_parsed_cmd_t* cmd, cli_error_ctx_t* err) {
    cli_context_t* ctx = cli_get_context();
    int ue_id = ctx->selected_ue_id;
    
    if (cmd->arg_count > 0 && cmd->verb != CLI_VERB_SET) {
        ue_id = atoi(cmd->args[0]);
    }
    
    if (ue_id < 0) {
        cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "UE ID required",
            "Use 'qos <verb> <ue>' or select a UE first");
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Validate UE exists */
    if (ue_id < 0 || ue_id >= MAX_UE_INSTANCES || g_ue_contexts[ue_id] == NULL) {
        cli_error_set(err, UESIM_ERROR_NOT_FOUND, "UE not found", NULL);
        return UESIM_ERROR_NOT_FOUND;
    }
    
    ue_context_t* ue_ctx = g_ue_contexts[ue_id];
    
    switch (cmd->verb) {
        case CLI_VERB_ADD: {
            /* qos add <ue> <5qi> [session_id] */
            if (cmd->arg_count < 2) {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "5QI required", 
                    "Use 'qos add <ue> <5qi> [session_id]'");
                return UESIM_ERROR_INVALID_PARAM;
            }
            
            uint8_t five_qi = (uint8_t)atoi(cmd->args[1]);
            uint8_t session_id = 1;  /* Default to first session */
            if (cmd->arg_count > 2) {
                session_id = (uint8_t)atoi(cmd->args[2]);
            }
            
            nas_ue_context_t* nas_ctx = ue_get_nas_context(ue_ctx);
            if (nas_ctx == NULL) {
                cli_error_set(err, UESIM_ERROR_NOT_INITIALIZED, "NAS context not initialized", 
                    "UE must have active PDU session");
                return UESIM_ERROR_NOT_INITIALIZED;
            }
            
            if (session_id >= NAS_MAX_PDU_SESSIONS || !nas_ctx->pdu_sessions[session_id].active) {
                cli_error_set(err, UESIM_ERROR_NOT_FOUND, "PDU session not found or inactive", NULL);
                return UESIM_ERROR_NOT_FOUND;
            }
            
            nas_pdu_session_t* session = &nas_ctx->pdu_sessions[session_id];
            
            /* Check if QoS flow already exists for this 5QI */
            for (int i = 0; i < session->num_qos_flows; i++) {
                if (session->qos_flows[i].qci == five_qi) {
                    cli_error_set(err, UESIM_ERROR_ALREADY_EXISTS, "QoS flow with this 5QI already exists", NULL);
                    return UESIM_ERROR_ALREADY_EXISTS;
                }
            }
            
            /* Check capacity */
            if (session->num_qos_flows >= 8) {
                cli_error_set(err, UESIM_ERROR_CAPACITY, "Maximum QoS flows reached for session", NULL);
                return UESIM_ERROR_CAPACITY;
            }
            
            /* Create QoS flow */
            uint8_t qfi = (uint8_t)(session->num_qos_flows + 1);
            nas_qos_flow_t* flow = &session->qos_flows[session->num_qos_flows];
            flow->qfi = qfi;
            flow->qci = five_qi;
            flow->arp = 1;  /* Default priority */
            flow->gbr_ul = 0;
            flow->gbr_dl = 0;
            flow->mbr_ul = 0;
            flow->mbr_dl = 0;
            flow->active = true;
            session->num_qos_flows++;
            
            printf("Added QoS flow QFI=%u, 5QI=%u for UE %d session %u\n", 
                   qfi, five_qi, ue_id, session_id);
            return UESIM_SUCCESS;
        }
            
        case CLI_VERB_LIST: {
            /* qos list <ue> [session_id] */
            uint8_t session_id = 1;
            if (cmd->arg_count > 1) {
                session_id = (uint8_t)atoi(cmd->args[1]);
            }
            
            nas_ue_context_t* nas_ctx = ue_get_nas_context(ue_ctx);
            if (nas_ctx == NULL) {
                printf("\nQoS Flows for UE %d: NAS not initialized\n\n", ue_id);
                return UESIM_SUCCESS;
            }
            
            printf("\nQoS Flows for UE %d", ue_id);
            if (nas_ctx->pdu_sessions[session_id].active) {
                printf(" Session %u:\n", session_id);
                nas_pdu_session_t* session = &nas_ctx->pdu_sessions[session_id];
                
                printf("  %-4s %-6s %-5s %-8s %-8s %-8s\n", 
                       "QFI", "5QI", "ARP", "GBR_UL", "GBR_DL", "State");
                printf("  %-4s %-6s %-5s %-8s %-8s %-8s\n", 
                       "---", "----", "---", "------", "------", "-----");
                
                for (int i = 0; i < session->num_qos_flows; i++) {
                    nas_qos_flow_t* flow = &session->qos_flows[i];
                    printf("  %-4u %-6u %-5u %-8u %-8u %s\n",
                           flow->qfi, flow->qci, flow->arp,
                           flow->gbr_ul, flow->gbr_dl,
                           flow->active ? "ACTIVE" : "INACTIVE");
                }
                
                if (session->num_qos_flows == 0) {
                    printf("  No QoS flows configured\n");
                }
            } else {
                printf(": Session %u not active\n\n", session_id);
            }
            printf("\n");
            return UESIM_SUCCESS;
        }
        
        case CLI_VERB_REMOVE: {
            /* qos remove <ue> <qfi> [session_id] */
            if (cmd->arg_count < 2) {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "QFI required",
                    "Use 'qos remove <ue> <qfi> [session_id]'");
                return UESIM_ERROR_INVALID_PARAM;
            }
            
            uint8_t qfi = (uint8_t)atoi(cmd->args[1]);
            uint8_t session_id = 1;
            if (cmd->arg_count > 2) {
                session_id = (uint8_t)atoi(cmd->args[2]);
            }
            
            nas_ue_context_t* nas_ctx = ue_get_nas_context(ue_ctx);
            if (nas_ctx == NULL || !nas_ctx->pdu_sessions[session_id].active) {
                cli_error_set(err, UESIM_ERROR_NOT_FOUND, "Session not found", NULL);
                return UESIM_ERROR_NOT_FOUND;
            }
            
            nas_pdu_session_t* session = &nas_ctx->pdu_sessions[session_id];
            bool found = false;
            for (int i = 0; i < session->num_qos_flows; i++) {
                if (session->qos_flows[i].qfi == qfi) {
                    /* Shift remaining flows */
                    for (int j = i; j < session->num_qos_flows - 1; j++) {
                        session->qos_flows[j] = session->qos_flows[j + 1];
                    }
                    session->num_qos_flows--;
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                cli_error_set(err, UESIM_ERROR_NOT_FOUND, "QoS flow not found", NULL);
                return UESIM_ERROR_NOT_FOUND;
            }
            
            printf("Removed QoS flow QFI=%u from UE %d\n", qfi, ue_id);
            return UESIM_SUCCESS;
        }
        
        case CLI_VERB_SET: {
            /* qos set <ue> <qfi> <param> <value> [session_id] */
            if (cmd->arg_count < 4) {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "Insufficient arguments",
                    "Use 'qos set <ue> <qfi> <param> <value> [session_id]'");
                return UESIM_ERROR_INVALID_PARAM;
            }
            
            ue_id = atoi(cmd->args[0]);
            if (ue_id < 0 || ue_id >= MAX_UE_INSTANCES || g_ue_contexts[ue_id] == NULL) {
                cli_error_set(err, UESIM_ERROR_NOT_FOUND, "UE not found", NULL);
                return UESIM_ERROR_NOT_FOUND;
            }
            
            ue_ctx = g_ue_contexts[ue_id];
            uint8_t qfi = (uint8_t)atoi(cmd->args[1]);
            const char* param = cmd->args[2];
            uint32_t value = (uint32_t)atoi(cmd->args[3]);
            uint8_t session_id = 1;
            if (cmd->arg_count > 4) {
                session_id = (uint8_t)atoi(cmd->args[4]);
            }
            
            nas_ue_context_t* nas_ctx = ue_get_nas_context(ue_ctx);
            if (nas_ctx == NULL || !nas_ctx->pdu_sessions[session_id].active) {
                cli_error_set(err, UESIM_ERROR_NOT_FOUND, "Session not found", NULL);
                return UESIM_ERROR_NOT_FOUND;
            }
            
            nas_qos_flow_t* flow = NULL;
            nas_pdu_session_t* session = &nas_ctx->pdu_sessions[session_id];
            for (int i = 0; i < session->num_qos_flows; i++) {
                if (session->qos_flows[i].qfi == qfi) {
                    flow = &session->qos_flows[i];
                    break;
                }
            }
            
            if (flow == NULL) {
                cli_error_set(err, UESIM_ERROR_NOT_FOUND, "QoS flow not found", NULL);
                return UESIM_ERROR_NOT_FOUND;
            }
            
            if (strcasecmp(param, "arp") == 0) {
                flow->arp = (uint8_t)value;
            } else if (strcasecmp(param, "gbr_ul") == 0) {
                flow->gbr_ul = (uint16_t)value;
            } else if (strcasecmp(param, "gbr_dl") == 0) {
                flow->gbr_dl = (uint16_t)value;
            } else if (strcasecmp(param, "mbr_ul") == 0) {
                flow->mbr_ul = (uint16_t)value;
            } else if (strcasecmp(param, "mbr_dl") == 0) {
                flow->mbr_dl = (uint16_t)value;
            } else {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "Unknown parameter",
                    "Available: arp, gbr_ul, gbr_dl, mbr_ul, mbr_dl");
                return UESIM_ERROR_INVALID_PARAM;
            }
            
            printf("Set QFI %u %s = %u\n", qfi, param, value);
            return UESIM_SUCCESS;
        }
            
        default:
            cli_print_noun_help(CLI_NOUN_QOS);
            return UESIM_SUCCESS;
    }
}

/* Scenario command handler */
static uesim_error_t cli_exec_scenario_cmd(const cli_parsed_cmd_t* cmd, cli_error_ctx_t* err) {
    cli_context_t* ctx = cli_get_context();
    
    switch (cmd->verb) {
        case CLI_VERB_RUN: {
            if (cmd->arg_count == 0) {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "Scenario type required",
                    "Use 'scenario run <type> [ue]'");
                return UESIM_ERROR_INVALID_PARAM;
            }
            
            int ue_id = ctx->selected_ue_id;
            if (cmd->arg_count > 1) {
                ue_id = atoi(cmd->args[1]);
            }
            
            if (ue_id < 0 || ue_id >= MAX_UE_INSTANCES || g_ue_contexts[ue_id] == NULL) {
                cli_error_set(err, UESIM_ERROR_NOT_FOUND, "UE not found", NULL);
                return UESIM_ERROR_NOT_FOUND;
            }
            
            const char* scenario = cmd->args[0];
            uesim_error_t result = UESIM_SUCCESS;
            
            printf("Running scenario '%s' on UE %d...\n", scenario, ue_id);
            
            if (strcasecmp(scenario, "registration") == 0) {
                result = rrc_execute_registration(g_ue_contexts[ue_id]);
            } else if (strcasecmp(scenario, "establishment") == 0) {
                result = rrc_execute_establishment(g_ue_contexts[ue_id]);
            } else if (strcasecmp(scenario, "reestablishment") == 0) {
                result = rrc_execute_reestablishment(g_ue_contexts[ue_id]);
            } else if (strcasecmp(scenario, "handover") == 0) {
                result = rrc_execute_handover(g_ue_contexts[ue_id]);
            } else {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "Unknown scenario type",
                    "Available: registration, establishment, reestablishment, handover");
                return UESIM_ERROR_INVALID_PARAM;
            }
            
            if (result == UESIM_SUCCESS) {
                printf("Scenario '%s' completed successfully. RRC State: %s\n",
                       scenario, rrc_state_to_string(rrc_get_current_state(g_ue_contexts[ue_id])));
            } else {
                cli_error_set(err, result, "Scenario execution failed", NULL);
            }
            return result;
        }
        
        case CLI_VERB_LIST:
            printf("\nAvailable Scenarios:\n");
            printf("  registration    - RRC registration\n");
            printf("  establishment   - RRC establishment\n");
            printf("  reestablishment - RRC re-establishment\n");
            printf("  handover        - RRC handover\n\n");
            return UESIM_SUCCESS;
            
        default:
            cli_print_noun_help(CLI_NOUN_SCENARIO);
            return UESIM_SUCCESS;
    }
}

/* LoadTest state tracking */
typedef struct {
    bool running;
    char scenario[64];
    uint32_t num_ues;
    uint32_t duration_sec;
    time_t start_time;
    uint32_t ues_completed;
    uint32_t ues_failed;
} loadtest_state_t;
static loadtest_state_t g_loadtest_state = {0};

/* LoadTest command handler */
static uesim_error_t cli_exec_loadtest_cmd(const cli_parsed_cmd_t* cmd, cli_error_ctx_t* err) {
    switch (cmd->verb) {
        case CLI_VERB_START: {
            /* loadtest start <scenario> <ues> <duration> */
            if (cmd->arg_count < 3) {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "Insufficient arguments",
                    "Usage: loadtest start <scenario> <ues> <duration>");
                return UESIM_ERROR_INVALID_PARAM;
            }
            
            if (g_loadtest_state.running) {
                cli_error_set(err, UESIM_ERROR_ALREADY_EXISTS, "Load test already running",
                    "Use 'loadtest stop' to stop current test");
                return UESIM_ERROR_ALREADY_EXISTS;
            }
            
            const char* scenario = cmd->args[0];
            uint32_t num_ues = (uint32_t)atoi(cmd->args[1]);
            uint32_t duration = (uint32_t)atoi(cmd->args[2]);
            
            /* Validate parameters */
            if (num_ues == 0 || num_ues > MAX_UE_INSTANCES) {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "Invalid UE count",
                    "Must be 1-1024");
                return UESIM_ERROR_INVALID_PARAM;
            }
            
            if (duration == 0) {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "Invalid duration",
                    "Must be > 0 seconds");
                return UESIM_ERROR_INVALID_PARAM;
            }
            
            /* Validate scenario */
            if (strcasecmp(scenario, "registration") != 0 && 
                strcasecmp(scenario, "establishment") != 0 &&
                strcasecmp(scenario, "handover") != 0) {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "Unknown scenario",
                    "Available: registration, establishment, handover");
                return UESIM_ERROR_INVALID_PARAM;
            }
            
            /* Initialize load test state */
            g_loadtest_state.running = true;
            strncpy(g_loadtest_state.scenario, scenario, sizeof(g_loadtest_state.scenario) - 1);
            g_loadtest_state.num_ues = num_ues;
            g_loadtest_state.duration_sec = duration;
            g_loadtest_state.start_time = time(NULL);
            g_loadtest_state.ues_completed = 0;
            g_loadtest_state.ues_failed = 0;
            
            /* Create UEs for load test */
            printf("Starting load test: scenario=%s, UEs=%u, duration=%us\n",
                   scenario, num_ues, duration);
            
            uint32_t ues_created = 0;
            for (uint32_t i = 0; i < num_ues && g_num_active_ues < MAX_UE_INSTANCES; i++) {
                int slot = -1;
                for (int j = 0; j < MAX_UE_INSTANCES; j++) {
                    if (g_ue_contexts[j] == NULL) { slot = j; break; }
                }
                if (slot < 0) break;
                
                ue_context_t* ue_ctx = NULL;
                uesim_error_t result = uesim_create_ue_instance(&ue_ctx);
                if (result == UESIM_SUCCESS) {
                    ue_ctx->ue_id = (uint32_t)slot;
                    uesim_start_ue(ue_ctx);
                    g_ue_contexts[slot] = ue_ctx;
                    g_num_active_ues++;
                    ues_created++;
                }
            }
            
            printf("Created %u UEs for load test\n", ues_created);
            printf("Load test running. Use 'loadtest status' to monitor.\n");
            return UESIM_SUCCESS;
        }
        
        case CLI_VERB_STOP: {
            if (!g_loadtest_state.running) {
                printf("No load test currently running.\n");
                return UESIM_SUCCESS;
            }
            
            printf("Stopping load test...\n");
            printf("  Completed: %u UEs\n", g_loadtest_state.ues_completed);
            printf("  Failed: %u UEs\n", g_loadtest_state.ues_failed);
            
            g_loadtest_state.running = false;
            printf("Load test stopped.\n");
            return UESIM_SUCCESS;
        }
            
        case CLI_VERB_STATUS: {
            printf("\nLoad Test Status:\n");
            printf("=================\n");
            
            if (!g_loadtest_state.running) {
                printf("  State: IDLE\n\n");
            } else {
                time_t elapsed = time(NULL) - g_loadtest_state.start_time;
                time_t remaining = (g_loadtest_state.duration_sec > elapsed) ? 
                                   (g_loadtest_state.duration_sec - elapsed) : 0;
                
                printf("  State: RUNNING\n");
                printf("  Scenario: %s\n", g_loadtest_state.scenario);
                printf("  UEs: %u (target) / %u (active)\n", 
                       g_loadtest_state.num_ues, g_num_active_ues);
                printf("  Duration: %lus / %lus\n", elapsed, g_loadtest_state.duration_sec);
                printf("  Remaining: %lus\n", remaining);
                printf("  Completed: %u\n", g_loadtest_state.ues_completed);
                printf("  Failed: %u\n", g_loadtest_state.ues_failed);
                
                if (g_loadtest_state.ues_completed + g_loadtest_state.ues_failed > 0) {
                    float success_rate = (float)g_loadtest_state.ues_completed / 
                        (float)(g_loadtest_state.ues_completed + g_loadtest_state.ues_failed) * 100.0f;
                    printf("  Success Rate: %.1f%%\n", success_rate);
                }
                printf("\n");
            }
            return UESIM_SUCCESS;
        }
        
        case CLI_VERB_LIST: {
            printf("\nAvailable Load Test Scenarios:\n");
            printf("  registration  - RRC registration stress test\n");
            printf("  establishment - RRC establishment stress test\n");
            printf("  handover      - Handover stress test\n\n");
            printf("Usage: loadtest start <scenario> <ues> <duration>\n\n");
            return UESIM_SUCCESS;
        }
            
        default:
            cli_print_noun_help(CLI_NOUN_LOADTEST);
            return UESIM_SUCCESS;
    }
}

/* Config command handler */
static uesim_error_t cli_exec_config_cmd(const cli_parsed_cmd_t* cmd, cli_error_ctx_t* err) {
    switch (cmd->verb) {
        case CLI_VERB_SHOW: {
            /* config show [section] */
            if (cmd->arg_count > 0) {
                const char* section = cmd->args[0];
                
                printf("\nConfiguration: %s\n", section);
                printf("====================\n");
                
                if (strcasecmp(section, "general") == 0) {
                    config_general_t* g = config_get_general(&g_config);
                    printf("  num_instances: %d\n", g->num_instances);
                    printf("  log_level: %d\n", g->log_level);
                    printf("  verbose: %s\n", g->verbose ? "true" : "false");
                    printf("  debug: %s\n", g->debug ? "true" : "false");
                    printf("  log_file: %s\n", g->log_file);
                } else if (strcasecmp(section, "network") == 0) {
                    config_network_t* n = config_get_network(&g_config);
                    printf("  gnb_ip: %s\n", n->gnb_ip);
                    printf("  gnb_ngap_port: %d\n", n->gnb_ngap_port);
                    printf("  gnb_gtpu_port: %d\n", n->gnb_gtpu_port);
                    printf("  local_ip: %s\n", n->local_ip);
                    printf("  connection_timeout: %d\n", n->connection_timeout);
                } else if (strcasecmp(section, "ue") == 0) {
                    config_ue_t* u = config_get_ue(&g_config);
                    printf("  imsi_prefix: %s\n", u->imsi_prefix);
                    printf("  imsi_start: %u\n", u->imsi_start);
                    printf("  msisdn_prefix: %s\n", u->msisdn_prefix);
                    printf("  tac: %u\n", u->tac);
                    printf("  mcc: %s\n", u->mcc);
                    printf("  mnc: %s\n", u->mnc);
                    printf("  apn: %s\n", u->apn);
                } else if (strcasecmp(section, "rrc") == 0) {
                    config_rrc_t* r = config_get_rrc(&g_config);
                    printf("  registration_timeout: %d\n", r->registration_timeout);
                    printf("  establishment_timeout: %d\n", r->establishment_timeout);
                    printf("  handover_timeout: %d\n", r->handover_timeout);
                    printf("  max_retransmissions: %d\n", r->max_retransmissions);
                    printf("  t300_value: %d ms\n", r->t300_value);
                    printf("  t301_value: %d ms\n", r->t301_value);
                } else if (strcasecmp(section, "nas") == 0) {
                    config_nas_t* n = config_get_nas(&g_config);
                    printf("  registration_timer: %d\n", n->registration_timer);
                    printf("  max_pdu_sessions: %d\n", n->max_pdu_sessions);
                    printf("  enable_5g_features: %s\n", n->enable_5g_features ? "true" : "false");
                } else if (strcasecmp(section, "performance") == 0) {
                    config_performance_t* p = config_get_performance(&g_config);
                    printf("  thread_pool_size: %d\n", p->thread_pool_size);
                    printf("  rx_buffer_size: %d\n", p->rx_buffer_size);
                    printf("  tx_buffer_size: %d\n", p->tx_buffer_size);
                    printf("  worker_threads: %d\n", p->worker_threads);
                } else if (strcasecmp(section, "security") == 0) {
                    config_security_t* s = config_get_security(&g_config);
                    printf("  enable_encryption: %s\n", s->enable_encryption ? "true" : "false");
                    printf("  enable_integrity_protection: %s\n", s->enable_integrity_protection ? "true" : "false");
                    printf("  enable_tls: %s\n", s->enable_tls ? "true" : "false");
                } else if (strcasecmp(section, "pdcp") == 0) {
                    config_pdcp_t* p = config_get_pdcp(&g_config);
                    printf("  max_pdu_size: %d\n", p->max_pdu_size);
                    printf("  enable_ciphering: %s\n", p->enable_ciphering ? "true" : "false");
                    printf("  ciphering_algorithm: %d\n", p->ciphering_algorithm);
                } else if (strcasecmp(section, "rlc") == 0) {
                    config_rlc_t* r = config_get_rlc(&g_config);
                    printf("  am_window_size: %d\n", r->am_window_size);
                    printf("  buffer_size: %d\n", r->buffer_size);
                    printf("  enable_arq: %s\n", r->enable_arq ? "true" : "false");
                } else if (strcasecmp(section, "mac") == 0) {
                    config_mac_t* m = config_get_mac(&g_config);
                    printf("  harq_processes: %d\n", m->harq_processes);
                    printf("  max_harq_retransmissions: %d\n", m->max_harq_retransmissions);
                    printf("  enable_harq: %s\n", m->enable_harq ? "true" : "false");
                } else {
                    printf("  Unknown section. Available: general, network, ue, rrc, nas,\n");
                    printf("  pdcp, rlc, mac, performance, security\n");
                }
                printf("\n");
            } else {
                printf("\nConfiguration Sections:\n");
                printf("=======================\n");
                printf("  general     - General application settings\n");
                printf("  network     - Network and connection settings\n");
                printf("  ue          - UE identity and parameters\n");
                printf("  rrc         - RRC protocol settings\n");
                printf("  nas         - NAS protocol settings\n");
                printf("  pdcp        - PDCP layer settings\n");
                printf("  rlc         - RLC layer settings\n");
                printf("  mac         - MAC layer settings\n");
                printf("  performance - Performance tuning\n");
                printf("  security    - Security settings\n\n");
                printf("Use 'config show <section>' to view specific section\n\n");
            }
            return UESIM_SUCCESS;
        }
            
        case CLI_VERB_SET: {
            /* config set <section.key> <value> */
            if (cmd->arg_count < 2) {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "Insufficient arguments",
                    "Use 'config set <section.key> <value>'");
                return UESIM_ERROR_INVALID_PARAM;
            }
            
            /* Parse section.key format */
            char key_str[128];
            strncpy(key_str, cmd->args[0], sizeof(key_str) - 1);
            key_str[sizeof(key_str) - 1] = '\0';
            
            char* dot = strchr(key_str, '.');
            if (dot == NULL) {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "Invalid key format",
                    "Use 'config set <section.key> <value>'");
                return UESIM_ERROR_INVALID_PARAM;
            }
            
            *dot = '\0';
            const char* section = key_str;
            const char* key = dot + 1;
            const char* value = cmd->args[1];
            
            /* Apply setting to appropriate section */
            config_section_t sec_id = CONFIG_SECTION_MAX;
            if (strcasecmp(section, "general") == 0) sec_id = CONFIG_SECTION_GENERAL;
            else if (strcasecmp(section, "network") == 0) sec_id = CONFIG_SECTION_NETWORK;
            else if (strcasecmp(section, "ue") == 0) sec_id = CONFIG_SECTION_UE;
            else if (strcasecmp(section, "rrc") == 0) sec_id = CONFIG_SECTION_RRC;
            else if (strcasecmp(section, "nas") == 0) sec_id = CONFIG_SECTION_NAS;
            else if (strcasecmp(section, "pdcp") == 0) sec_id = CONFIG_SECTION_PDCP;
            else if (strcasecmp(section, "rlc") == 0) sec_id = CONFIG_SECTION_RLC;
            else if (strcasecmp(section, "mac") == 0) sec_id = CONFIG_SECTION_MAC;
            else if (strcasecmp(section, "performance") == 0) sec_id = CONFIG_SECTION_PERFORMANCE;
            else if (strcasecmp(section, "security") == 0) sec_id = CONFIG_SECTION_SECURITY;
            else {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "Unknown section",
                    "Available: general, network, ue, rrc, nas, pdcp, rlc, mac, performance, security");
                return UESIM_ERROR_INVALID_PARAM;
            }
            
            /* Try to set as integer first, then as string */
            int int_val = atoi(value);
            uesim_error_t result = config_set_int(&g_config, sec_id, key, int_val);
            if (result != UESIM_SUCCESS) {
                result = config_set_string(&g_config, sec_id, key, value);
            }
            
            if (result == UESIM_SUCCESS) {
                printf("Set %s.%s = %s\n", section, key, value);
            } else {
                cli_error_set(err, result, "Failed to set configuration", NULL);
            }
            return result;
        }
            
        case CLI_VERB_LOAD:
            if (cmd->arg_count == 0) {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "File path required",
                    "Use 'config load <file>'");
                return UESIM_ERROR_INVALID_PARAM;
            }
            printf("Loading configuration from %s...\n", cmd->args[0]);
            return config_load(&g_config, cmd->args[0]);
            
        case CLI_VERB_SAVE:
            if (cmd->arg_count == 0) {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "File path required",
                    "Use 'config save <file>'");
                return UESIM_ERROR_INVALID_PARAM;
            }
            printf("Saving configuration to %s...\n", cmd->args[0]);
            return config_save(&g_config, cmd->args[0]);
            
        default:
            cli_print_noun_help(CLI_NOUN_CONFIG);
            return UESIM_SUCCESS;
    }
}

/* System command handler */
static uesim_error_t cli_exec_system_cmd(const cli_parsed_cmd_t* cmd, cli_error_ctx_t* err) {
    (void)err;
    
    switch (cmd->verb) {
        case CLI_VERB_STATUS:
            cli_print_system_status();
            return UESIM_SUCCESS;
            
        case CLI_VERB_INFO:
            printf("\nSystem Information:\n");
            printf("  Version: 1.1.0\n");
            printf("  Max UEs: %d\n", MAX_UE_INSTANCES);
            printf("  Thread Pool: Active\n");
            printf("\n");
            return UESIM_SUCCESS;
            
        default:
            cli_print_noun_help(CLI_NOUN_SYSTEM);
            return UESIM_SUCCESS;
    }
}

/* Script command handler */
static uesim_error_t cli_exec_script_cmd(const cli_parsed_cmd_t* cmd, cli_error_ctx_t* err) {
    switch (cmd->verb) {
        case CLI_VERB_RUN:
            if (cmd->arg_count == 0) {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "File path required",
                    "Use 'script run <file>'");
                return UESIM_ERROR_INVALID_PARAM;
            }
            return cli_script_execute(cmd->args[0]);
            
        case CLI_VERB_HISTORY:
            cli_history_print();
            return UESIM_SUCCESS;
            
        case CLI_VERB_RECORD:
            if (cmd->arg_count == 0) {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "File path required",
                    "Use 'script record <file>' or 'script record stop'");
                return UESIM_ERROR_INVALID_PARAM;
            }
            if (strcasecmp(cmd->args[0], "stop") == 0) {
                return cli_record_stop();
            }
            return cli_record_start(cmd->args[0]);
            
        default:
            cli_print_noun_help(CLI_NOUN_SCRIPT);
            return UESIM_SUCCESS;
    }
}

/* Print main help */
void cli_print_main_help(void) {
    printf("\n5G UE Simulation CLI - Hierarchical Commands\n");
    printf("=============================================\n");
    printf("\nCommand Format: <verb> <noun> [arguments]\n");
    printf("            or: <noun> <verb> [arguments]\n\n");
    
    printf("Objects (nouns):\n");
    printf("  ue        - UE instance management\n");
    printf("  gnb       - gNB management\n");
    printf("  session   - PDU session management\n");
    printf("  qos       - QoS flow management\n");
    printf("  scenario  - RRC scenario execution\n");
    printf("  loadtest  - Load testing framework\n");
    printf("  config    - Configuration management\n");
    printf("  system    - System operations\n");
    printf("  script    - Script/batch operations\n\n");
    
    printf("Actions (verbs):\n");
    printf("  start, stop, status, list, add, remove, connect, create, release, run, show, set, load, save\n\n");
    
    printf("Quick Reference:\n");
    printf("  ue start 5              Start 5 UEs\n");
    printf("  ue stop all             Stop all UEs\n");
    printf("  ue select 0             Select UE 0 for context mode\n");
    printf("  scenario run registration 0   Run registration on UE 0\n");
    printf("  config show network     Show network configuration\n");
    printf("  help <noun>             Show help for specific object\n");
    printf("  exit                    Exit CLI\n\n");
}

/* Print system status */
void cli_print_system_status(void) {
    printf("\nSystem Status:\n");
    printf("==============\n");
    printf("  Active UEs: %u\n", g_num_active_ues);
    printf("  Config Loaded: %s\n", config_is_loaded(&g_config) ? "Yes" : "No");
    printf("  Thread Pool: Active\n");
    
    /* Get RRC state from first active UE */
    const char* rrc_state = "Not Connected";
    for (int i = 0; i < MAX_UE_INSTANCES; i++) {
        if (g_ue_contexts[i] != NULL) {
            rrc_state = rrc_state_to_string(rrc_get_current_state(g_ue_contexts[i]));
            break;
        }
    }
    printf("  RRC State: %s\n", rrc_state);
    printf("\n");
}

/* Legacy command execution (for backward compatibility) */
uesim_error_t cli_execute_command(cli_command_t* command) {
    if (!command) return UESIM_ERROR_INVALID_PARAM;
    
    /* Convert to new format and process */
    char line[CLI_MAX_LINE_LEN] = {0};
    snprintf(line, sizeof(line), "%s", g_command_strings[command->command_type]);
    for (size_t i = 0; i < command->arg_count; i++) {
        strncat(line, " ", sizeof(line) - strlen(line) - 1);
        strncat(line, command->arg_values[i], sizeof(line) - strlen(line) - 1);
    }
    
    return cli_process_command(line);
}

/* Interactive mode */
uesim_error_t cli_start_interactive_mode(void) {
    if (atomic_load(&g_cli_running)) {
        return UESIM_SUCCESS;
    }
    
    atomic_store(&g_cli_running, true);
    cli_context_t* ctx = cli_get_context();
    
    printf("\n5G UE Simulation CLI v1.1.0\n");
    printf("Type 'help' for commands, 'exit' to quit.\n\n");
    
    char input[CLI_MAX_LINE_LEN];
    
    while (atomic_load(&g_cli_running)) {
        printf("%s", cli_context_prompt(ctx));
        fflush(stdout);
        
        if (fgets(input, sizeof(input), stdin) == NULL) {
            if (feof(stdin)) {
                printf("\n");
                break;
            }
            continue;
        }
        
        /* Remove trailing newline */
        input[strcspn(input, "\n")] = 0;
        
        /* Skip empty lines */
        if (strlen(input) == 0) continue;
        
        /* Process command */
        cli_process_command(input);
        
        /* Add to history */
        cli_history_add(input);
        
        /* Record if recording */
        if (cli_is_recording()) {
            cli_record_command(input);
        }
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

/* Measurement command handler */
static uesim_error_t cli_exec_meas_cmd(const cli_parsed_cmd_t* cmd, cli_error_ctx_t* err) {
    cli_context_t* ctx = cli_get_context();
    int ue_id = ctx->selected_ue_id;
    
    if (cmd->arg_count > 0 && cmd->verb != CLI_VERB_SHOW && cmd->verb != CLI_VERB_SET) {
        ue_id = atoi(cmd->args[0]);
    }
    
    switch (cmd->verb) {
        case CLI_VERB_SHOW: {
            /* meas show [ue] - Show measurement results */
            if (cmd->arg_count > 0) {
                ue_id = atoi(cmd->args[0]);
            }
            if (ue_id < 0 || ue_id >= MAX_UE_INSTANCES || g_ue_contexts[ue_id] == NULL) {
                cli_error_set(err, UESIM_ERROR_NOT_FOUND, "UE not found", NULL);
                return UESIM_ERROR_NOT_FOUND;
            }
            
            rrc_meas_context_t* meas_ctx = rrc_meas_get_context(g_ue_contexts[ue_id]);
            if (!meas_ctx) {
                printf("\nNo measurement data for UE %d\n\n", ue_id);
                return UESIM_SUCCESS;
            }
            
            printf("\nMeasurement Results for UE %d:\n", ue_id);
            printf("=================================\n");
            printf("Serving Cell:\n");
            printf("  PCI: %u, Cell ID: %u\n", meas_ctx->serving_meas.pci, meas_ctx->serving_meas.cell_id);
            printf("  RSRP: %d dBm, RSRQ: %d dB, SINR: %d dB\n\n",
                   meas_ctx->serving_meas.rsrp, meas_ctx->serving_meas.rsrq, meas_ctx->serving_meas.sinr);
            
            printf("Neighbor Cells (%u):\n", meas_ctx->num_neighbor_meas);
            for (int i = 0; i < meas_ctx->num_neighbor_meas; i++) {
                rrc_meas_result_t* nr = &meas_ctx->neighbor_meas[i];
                printf("  [%d] PCI: %u, RSRP: %d dBm, RSRQ: %d dB\n",
                       i, nr->pci, nr->rsrp, nr->rsrq);
            }
            printf("\n");
            return UESIM_SUCCESS;
        }
        
        case CLI_VERB_SET: {
            /* meas set <event> <param> <value> [ue] */
            if (cmd->arg_count < 3) {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "Insufficient arguments",
                    "Use 'meas set <event> <param> <value> [ue]'");
                return UESIM_ERROR_INVALID_PARAM;
            }
            
            if (ue_id < 0 || ue_id >= MAX_UE_INSTANCES || g_ue_contexts[ue_id] == NULL) {
                cli_error_set(err, UESIM_ERROR_NOT_FOUND, "UE not found", NULL);
                return UESIM_ERROR_NOT_FOUND;
            }
            
            const char* event_str = cmd->args[0];
            const char* param = cmd->args[1];
            const char* value = cmd->args[2];
            
            rrc_meas_event_t event = RRC_MEAS_EVENT_MAX;
            if (strcasecmp(event_str, "A3") == 0) event = RRC_MEAS_EVENT_A3;
            else if (strcasecmp(event_str, "A4") == 0) event = RRC_MEAS_EVENT_A4;
            else if (strcasecmp(event_str, "A5") == 0) event = RRC_MEAS_EVENT_A5;
            else {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "Unknown event",
                    "Available: A3, A4, A5");
                return UESIM_ERROR_INVALID_PARAM;
            }
            
            rrc_meas_context_t* meas_ctx = rrc_meas_get_context(g_ue_contexts[ue_id]);
            if (!meas_ctx) {
                cli_error_set(err, UESIM_ERROR_NOT_INITIALIZED, "Measurement not initialized", NULL);
                return UESIM_ERROR_NOT_INITIALIZED;
            }
            
            rrc_meas_config_t config = meas_ctx->events[event].config;
            if (config.event_id == RRC_MEAS_EVENT_MAX) {
                /* Use default config */
                switch (event) {
                    case RRC_MEAS_EVENT_A3: config = rrc_meas_get_default_a3_config(); break;
                    case RRC_MEAS_EVENT_A4: config = rrc_meas_get_default_a4_config(); break;
                    case RRC_MEAS_EVENT_A5: config = rrc_meas_get_default_a5_config(); break;
                    default: break;
                }
            }
            
            if (strcasecmp(param, "enable") == 0) {
                config.enabled = (strcasecmp(value, "on") == 0 || strcasecmp(value, "true") == 0);
            } else if (strcasecmp(param, "hysteresis") == 0) {
                config.hysteresis_db = (uint8_t)(atof(value) * 2);
            } else if (strcasecmp(param, "ttt") == 0 || strcasecmp(param, "time-to-trigger") == 0) {
                config.time_to_trigger = (uint32_t)atoi(value);
            } else if (strcasecmp(param, "threshold") == 0 || strcasecmp(param, "threshold1") == 0) {
                config.threshold1 = atoi(value);
            } else if (strcasecmp(param, "threshold2") == 0) {
                config.threshold2 = atoi(value);
            } else if (strcasecmp(param, "offset") == 0) {
                config.threshold1 = atoi(value);
            } else {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "Unknown parameter",
                    "Available: enable, hysteresis, ttt, threshold, threshold2, offset");
                return UESIM_ERROR_INVALID_PARAM;
            }
            
            uesim_error_t result = rrc_meas_configure_event(g_ue_contexts[ue_id], &config);
            if (result == UESIM_SUCCESS) {
                printf("Configured %s: %s = %s\n", event_str, param, value);
            }
            return result;
        }
        
        case CLI_VERB_ADD: {
            /* meas add <event> [ue] - Add/configure measurement event */
            if (cmd->arg_count < 1) {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "Event type required",
                    "Use 'meas add <A3|A4|A5> [ue]'");
                return UESIM_ERROR_INVALID_PARAM;
            }
            
            if (ue_id < 0 || ue_id >= MAX_UE_INSTANCES || g_ue_contexts[ue_id] == NULL) {
                cli_error_set(err, UESIM_ERROR_NOT_FOUND, "UE not found", NULL);
                return UESIM_ERROR_NOT_FOUND;
            }
            
            rrc_meas_config_t config;
            const char* event_str = cmd->args[0];
            
            if (strcasecmp(event_str, "A3") == 0) {
                config = rrc_meas_get_default_a3_config();
            } else if (strcasecmp(event_str, "A4") == 0) {
                config = rrc_meas_get_default_a4_config();
            } else if (strcasecmp(event_str, "A5") == 0) {
                config = rrc_meas_get_default_a5_config();
            } else {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "Unknown event type",
                    "Available: A3, A4, A5");
                return UESIM_ERROR_INVALID_PARAM;
            }
            
            uesim_error_t result = rrc_meas_configure_event(g_ue_contexts[ue_id], &config);
            if (result == UESIM_SUCCESS) {
                printf("Added measurement event %s for UE %d\n", event_str, ue_id);
            }
            return result;
        }
        
        case CLI_VERB_REMOVE: {
            /* meas remove <event> [ue] */
            if (cmd->arg_count < 1) {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "Event type required",
                    "Use 'meas remove <A3|A4|A5> [ue]'");
                return UESIM_ERROR_INVALID_PARAM;
            }
            
            if (ue_id < 0 || ue_id >= MAX_UE_INSTANCES || g_ue_contexts[ue_id] == NULL) {
                cli_error_set(err, UESIM_ERROR_NOT_FOUND, "UE not found", NULL);
                return UESIM_ERROR_NOT_FOUND;
            }
            
            const char* event_str = cmd->args[0];
            rrc_meas_event_t event = RRC_MEAS_EVENT_MAX;
            
            if (strcasecmp(event_str, "A3") == 0) event = RRC_MEAS_EVENT_A3;
            else if (strcasecmp(event_str, "A4") == 0) event = RRC_MEAS_EVENT_A4;
            else if (strcasecmp(event_str, "A5") == 0) event = RRC_MEAS_EVENT_A5;
            else {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "Unknown event type", NULL);
                return UESIM_ERROR_INVALID_PARAM;
            }
            
            uesim_error_t result = rrc_meas_remove_event(g_ue_contexts[ue_id], event);
            if (result == UESIM_SUCCESS) {
                printf("Removed measurement event %s\n", event_str);
            }
            return result;
        }
        
        case CLI_VERB_LIST: {
            /* meas list [ue] - List configured events */
            if (cmd->arg_count > 0) {
                ue_id = atoi(cmd->args[0]);
            }
            if (ue_id < 0 || ue_id >= MAX_UE_INSTANCES || g_ue_contexts[ue_id] == NULL) {
                cli_error_set(err, UESIM_ERROR_NOT_FOUND, "UE not found", NULL);
                return UESIM_ERROR_NOT_FOUND;
            }
            
            rrc_meas_context_t* meas_ctx = rrc_meas_get_context(g_ue_contexts[ue_id]);
            if (!meas_ctx) {
                printf("\nNo measurement events configured for UE %d\n\n", ue_id);
                return UESIM_SUCCESS;
            }
            
            printf("\nMeasurement Events for UE %d:\n", ue_id);
            printf("============================\n");
            
            const char* event_names[] = {"A3", "A4", "A5"};
            for (int e = RRC_MEAS_EVENT_A3; e <= RRC_MEAS_EVENT_A5; e++) {
                rrc_meas_event_state_t* state = &meas_ctx->events[e];
                if (state->config.enabled) {
                    printf("  Event %s: ENABLED\n", event_names[e - RRC_MEAS_EVENT_A3]);
                    printf("    Hysteresis: %.1f dB, TTT: %u ms\n",
                           state->config.hysteresis_db * 0.5, state->config.time_to_trigger);
                    if (e == RRC_MEAS_EVENT_A3) {
                        printf("    Offset: %d dB\n", state->config.threshold1);
                    } else if (e == RRC_MEAS_EVENT_A4) {
                        printf("    Threshold: %d dBm\n", state->config.threshold1);
                    } else if (e == RRC_MEAS_EVENT_A5) {
                        printf("    Threshold1: %d dBm, Threshold2: %d dBm\n",
                               state->config.threshold1, state->config.threshold2);
                    }
                    printf("    State: %s\n", state->event_triggered ? "TRIGGERED" :
                           (state->event_entered ? "ENTERED" : "IDLE"));
                } else {
                    printf("  Event %s: DISABLED\n", event_names[e - RRC_MEAS_EVENT_A3]);
                }
            }
            printf("\n");
            return UESIM_SUCCESS;
        }
        
        case CLI_VERB_RUN: {
            /* meas run [ue] - Perform measurement and evaluate events */
            if (cmd->arg_count > 0) {
                ue_id = atoi(cmd->args[0]);
            }
            if (ue_id < 0 || ue_id >= MAX_UE_INSTANCES || g_ue_contexts[ue_id] == NULL) {
                cli_error_set(err, UESIM_ERROR_NOT_FOUND, "UE not found", NULL);
                return UESIM_ERROR_NOT_FOUND;
            }
            
            printf("Performing measurement for UE %d...\n", ue_id);
            
            rrc_meas_event_t triggered_event;
            uint8_t neighbor_idx;
            uesim_error_t result = rrc_meas_evaluate_events(g_ue_contexts[ue_id],
                                                            &triggered_event, &neighbor_idx);
            
            if (result == UESIM_SUCCESS) {
                if (triggered_event != RRC_MEAS_EVENT_MAX) {
                    printf("Event %s TRIGGERED! Neighbor index: %u\n",
                           rrc_meas_event_to_string(triggered_event), neighbor_idx);
                } else {
                    printf("No events triggered.\n");
                }
            }
            return result;
        }
        
        default:
            printf("\nMeasurement Commands:\n");
            printf("=====================\n");
            printf("  meas add <A3|A4|A5> [ue]     Add measurement event\n");
            printf("  meas remove <event> [ue]    Remove measurement event\n");
            printf("  meas list [ue]              List configured events\n");
            printf("  meas show [ue]              Show measurement results\n");
            printf("  meas run [ue]               Perform measurement\n");
            printf("  meas set <event> <param> <value> [ue]\n");
            printf("                              Set event parameter\n");
            printf("\nParameters: enable, hysteresis, ttt, threshold, threshold2, offset\n\n");
            return UESIM_SUCCESS;
    }
}

/* NAS command handler */
static uesim_error_t cli_exec_nas_cmd(const cli_parsed_cmd_t* cmd, cli_error_ctx_t* err) {
    cli_context_t* ctx = cli_get_context();
    int ue_id = ctx->selected_ue_id;
    
    if (cmd->arg_count > 0 && cmd->verb != CLI_VERB_SET) {
        ue_id = atoi(cmd->args[0]);
    }
    
    switch (cmd->verb) {
        case CLI_VERB_STATUS: {
            /* nas status [ue] - Show NAS state */
            if (cmd->arg_count > 0) {
                ue_id = atoi(cmd->args[0]);
            }
            if (ue_id < 0 || ue_id >= MAX_UE_INSTANCES || g_ue_contexts[ue_id] == NULL) {
                cli_error_set(err, UESIM_ERROR_NOT_FOUND, "UE not found", NULL);
                return UESIM_ERROR_NOT_FOUND;
            }
            
            nas_ue_context_t* nas_ctx = ue_get_nas_context(g_ue_contexts[ue_id]);
            if (nas_ctx == NULL) {
                printf("\nNAS not initialized for UE %d\n\n", ue_id);
                return UESIM_SUCCESS;
            }
            
            printf("\nNAS Status for UE %d:\n", ue_id);
            printf("=====================\n");
            
            const char* mm_state_str = "NULL";
            switch (nas_ctx->mm_state) {
                case NAS_5GMM_NULL: mm_state_str = "NULL"; break;
                case NAS_5GMM_DEREGISTERED: mm_state_str = "DEREGISTERED"; break;
                case NAS_5GMM_REGISTERED_INITIATED: mm_state_str = "REGISTERED_INITIATED"; break;
                case NAS_5GMM_REGISTERED: mm_state_str = "REGISTERED"; break;
                case NAS_5GMM_SERVICE_REQUEST_INITIATED: mm_state_str = "SERVICE_REQUEST_INITIATED"; break;
                case NAS_5GMM_DEREGISTERED_INITIATED: mm_state_str = "DEREGISTERED_INITIATED"; break;
            }
            printf("  5GMM State: %s\n", mm_state_str);
            printf("  Authenticated: %s\n", nas_ctx->auth_context.authenticated ? "Yes" : "No");
            printf("  Security Context: %s\n", 
                   nas_ctx->security_context.security_context_valid ? "Valid" : "Not Valid");
            
            if (nas_ctx->security_context.security_context_valid) {
                printf("  Ciphering: NEA%d\n", nas_ctx->security_context.ciphering_alg);
                printf("  Integrity: NIA%d\n", nas_ctx->security_context.integrity_alg);
                printf("  Uplink COUNT: %u\n", nas_ctx->security_context.uplink_count);
                printf("  Downlink COUNT: %u\n", nas_ctx->security_context.downlink_count);
            }
            
            printf("  Active PDU Sessions: %u\n", nas_ctx->num_active_sessions);
            printf("  SUCI: %s\n", nas_ctx->identity.suci[0] ? nas_ctx->identity.suci : "N/A");
            printf("  GUTI: %s\n", nas_ctx->identity.guti[0] ? nas_ctx->identity.guti : "N/A");
            printf("\n");
            return UESIM_SUCCESS;
        }
        
        case CLI_VERB_RUN: {
            /* nas run <procedure> [ue] */
            if (cmd->arg_count < 1) {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "Procedure required",
                    "Use 'nas run <procedure> [ue]'");
                return UESIM_ERROR_INVALID_PARAM;
            }
            
            if (ue_id < 0 || ue_id >= MAX_UE_INSTANCES || g_ue_contexts[ue_id] == NULL) {
                cli_error_set(err, UESIM_ERROR_NOT_FOUND, "UE not found", NULL);
                return UESIM_ERROR_NOT_FOUND;
            }
            
            const char* procedure = cmd->args[0];
            nas_ue_context_t* nas_ctx = ue_get_nas_context(g_ue_contexts[ue_id]);
            if (nas_ctx == NULL) {
                cli_error_set(err, UESIM_ERROR_NOT_INITIALIZED, "NAS not initialized", NULL);
                return UESIM_ERROR_NOT_INITIALIZED;
            }
            
            uesim_error_t result = UESIM_SUCCESS;
            
            if (strcasecmp(procedure, "registration") == 0) {
                printf("Initiating NAS registration for UE %d...\n", ue_id);
                result = nas_initiate_registration(nas_ctx, NAS_REGISTRATION_TYPE_INITIAL);
            } else if (strcasecmp(procedure, "authentication") == 0) {
                printf("Initiating NAS authentication for UE %d...\n", ue_id);
                result = nas_initiate_authentication(nas_ctx);
            } else if (strcasecmp(procedure, "security") == 0) {
                printf("Initiating NAS security mode control for UE %d...\n", ue_id);
                result = nas_initiate_security_mode_control(nas_ctx);
            } else {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "Unknown procedure",
                    "Available: registration, authentication, security");
                return UESIM_ERROR_INVALID_PARAM;
            }
            
            if (result == UESIM_SUCCESS) {
                printf("NAS procedure '%s' initiated successfully\n", procedure);
            } else {
                cli_error_set(err, result, "Failed to initiate NAS procedure", NULL);
            }
            return result;
        }
        
        case CLI_VERB_SET: {
            /* nas set <ue> <param> <value> */
            if (cmd->arg_count < 3) {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "Insufficient arguments",
                    "Use 'nas set <ue> <param> <value>'");
                return UESIM_ERROR_INVALID_PARAM;
            }
            
            ue_id = atoi(cmd->args[0]);
            if (ue_id < 0 || ue_id >= MAX_UE_INSTANCES || g_ue_contexts[ue_id] == NULL) {
                cli_error_set(err, UESIM_ERROR_NOT_FOUND, "UE not found", NULL);
                return UESIM_ERROR_NOT_FOUND;
            }
            
            nas_ue_context_t* nas_ctx = ue_get_nas_context(g_ue_contexts[ue_id]);
            if (nas_ctx == NULL) {
                cli_error_set(err, UESIM_ERROR_NOT_INITIALIZED, "NAS not initialized", NULL);
                return UESIM_ERROR_NOT_INITIALIZED;
            }
            
            const char* param = cmd->args[1];
            const char* value = cmd->args[2];
            
            if (strcasecmp(param, "suci") == 0) {
                strncpy(nas_ctx->identity.suci, value, sizeof(nas_ctx->identity.suci) - 1);
            } else if (strcasecmp(param, "guti") == 0) {
                strncpy(nas_ctx->identity.guti, value, sizeof(nas_ctx->identity.guti) - 1);
            } else if (strcasecmp(param, "imsi") == 0) {
                strncpy(nas_ctx->identity.imsi, value, sizeof(nas_ctx->identity.imsi) - 1);
            } else if (strcasecmp(param, "ciphering") == 0) {
                nas_ctx->security_context.ciphering_alg = (nas_ciphering_algorithm_t)atoi(value);
            } else if (strcasecmp(param, "integrity") == 0) {
                nas_ctx->security_context.integrity_alg = (nas_integrity_algorithm_t)atoi(value);
            } else {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "Unknown parameter",
                    "Available: suci, guti, imsi, ciphering, integrity");
                return UESIM_ERROR_INVALID_PARAM;
            }
            
            printf("Set NAS %s = %s\n", param, value);
            return UESIM_SUCCESS;
        }
        
        case CLI_VERB_SHOW: {
            /* nas show <section> [ue] */
            if (cmd->arg_count < 1) {
                cli_error_set(err, UESIM_ERROR_INVALID_PARAM, "Section required",
                    "Use 'nas show <section> [ue]'");
                return UESIM_ERROR_INVALID_PARAM;
            }
            
            if (cmd->arg_count > 1) {
                ue_id = atoi(cmd->args[1]);
            }
            if (ue_id < 0 || ue_id >= MAX_UE_INSTANCES || g_ue_contexts[ue_id] == NULL) {
                cli_error_set(err, UESIM_ERROR_NOT_FOUND, "UE not found", NULL);
                return UESIM_ERROR_NOT_FOUND;
            }
            
            nas_ue_context_t* nas_ctx = ue_get_nas_context(g_ue_contexts[ue_id]);
            if (nas_ctx == NULL) {
                printf("\nNAS not initialized for UE %d\n\n", ue_id);
                return UESIM_SUCCESS;
            }
            
            const char* section = cmd->args[0];
            printf("\nNAS %s for UE %d:\n", section, ue_id);
            printf("=====================\n");
            
            if (strcasecmp(section, "identity") == 0) {
                printf("  SUCI: %s\n", nas_ctx->identity.suci[0] ? nas_ctx->identity.suci : "N/A");
                printf("  GUTI: %s\n", nas_ctx->identity.guti[0] ? nas_ctx->identity.guti : "N/A");
                printf("  IMSI: %s\n", nas_ctx->identity.imsi[0] ? nas_ctx->identity.imsi : "N/A");
                printf("  IMEI: %s\n", nas_ctx->identity.imei[0] ? nas_ctx->identity.imei : "N/A");
            } else if (strcasecmp(section, "security") == 0) {
                printf("  Context Valid: %s\n", 
                       nas_ctx->security_context.security_context_valid ? "Yes" : "No");
                printf("  Ciphering Algorithm: NEA%d\n", nas_ctx->security_context.ciphering_alg);
                printf("  Integrity Algorithm: NIA%d\n", nas_ctx->security_context.integrity_alg);
                printf("  KSI: %u\n", nas_ctx->security_context.ksi);
                printf("  Uplink COUNT: %u\n", nas_ctx->security_context.uplink_count);
                printf("  Downlink COUNT: %u\n", nas_ctx->security_context.downlink_count);
            } else if (strcasecmp(section, "stats") == 0) {
                printf("  Registration Requests: %llu\n", nas_ctx->stats.registration_requests);
                printf("  Registration Accepts: %llu\n", nas_ctx->stats.registration_accepts);
                printf("  Authentication Requests: %llu\n", nas_ctx->stats.authentication_requests);
                printf("  Authentication Responses: %llu\n", nas_ctx->stats.authentication_responses);
                printf("  Security Mode Commands: %llu\n", nas_ctx->stats.security_mode_commands);
                printf("  Messages Sent: %llu\n", nas_ctx->stats.messages_sent);
                printf("  Messages Received: %llu\n", nas_ctx->stats.messages_received);
                printf("  Authentication Failures: %llu\n", nas_ctx->stats.authentication_failures);
            } else if (strcasecmp(section, "timers") == 0) {
                printf("  T3412: %u seconds (%s)\n", nas_ctx->t3412_timer,
                       nas_ctx->t3412_running ? "running" : "stopped");
                printf("  T3422: %u seconds (%s)\n", nas_ctx->t3422_timer,
                       nas_ctx->t3422_running ? "running" : "stopped");
                printf("  T3450: %u seconds (%s)\n", nas_ctx->t3450_timer,
                       nas_ctx->t3450_running ? "running" : "stopped");
            } else {
                printf("  Unknown section. Available: identity, security, stats, timers\n");
            }
            printf("\n");
            return UESIM_SUCCESS;
        }
        
        default:
            printf("\nNAS Commands:\n");
            printf("=============\n");
            printf("  nas status [ue]                  Show NAS state\n");
            printf("  nas run <procedure> [ue]         Run NAS procedure\n");
            printf("  nas set <ue> <param> <value>     Set NAS parameter\n");
            printf("  nas show <section> [ue]          Show NAS details\n");
            printf("\nProcedures: registration, authentication, security\n");
            printf("Sections: identity, security, stats, timers\n");
            printf("Parameters: suci, guti, imsi, ciphering, integrity\n\n");
            return UESIM_SUCCESS;
    }
}
