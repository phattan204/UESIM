/*
 * 5G UE Simulation Application
 * Mock Core Network Server - Implementation
 */

#include "mock_core_server.h"
#include "mock_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define sleep_ms(ms) usleep((ms) * 1000)
#endif

/* ============== Default Configuration ============== */

void mock_core_server_get_default_config(mock_core_server_config_t* config) {
    if (!config) return;
    
    memset(config, 0, sizeof(mock_core_server_config_t));
    
    /* AMF Configuration */
    config->amf.enabled = true;
    strncpy(config->amf.bind_ip, "127.0.0.1", sizeof(config->amf.bind_ip) - 1);
    config->amf.port = 38412;
    config->amf.log_messages = true;
    
    /* SMF Configuration */
    config->smf.enabled = true;
    strncpy(config->smf.bind_ip, "127.0.0.1", sizeof(config->smf.bind_ip) - 1);
    config->smf.port = 8805;
    config->smf.log_messages = true;
    
    /* UPF Configuration */
    config->upf.enabled = true;
    strncpy(config->upf.bind_ip, "127.0.0.1", sizeof(config->upf.bind_ip) - 1);
    config->upf.port = 2152;
    config->upf.log_messages = true;
    
    /* CU-CP Configuration */
    config->cu_cp.enabled = true;
    strncpy(config->cu_cp.bind_ip, "127.0.0.1", sizeof(config->cu_cp.bind_ip) - 1);
    config->cu_cp.port = 38472;
    config->cu_cp.log_messages = true;
    
    /* DU Configuration */
    config->du.enabled = true;
    strncpy(config->du.bind_ip, "127.0.0.1", sizeof(config->du.bind_ip) - 1);
    config->du.port = 38472;
    config->du.log_messages = true;
    
    /* CU-UP Configuration */
    config->cu_up.enabled = true;
    strncpy(config->cu_up.bind_ip, "127.0.0.1", sizeof(config->cu_up.bind_ip) - 1);
    config->cu_up.port = 38470;
    config->cu_up.log_messages = true;
    
    /* XnAP Configuration */
    config->xnap.enabled = true;
    strncpy(config->xnap.bind_ip, "127.0.0.1", sizeof(config->xnap.bind_ip) - 1);
    config->xnap.port = 38422;
    config->xnap.log_messages = true;
    
    /* AMF Settings */
    config->amf_id = 1;
    strncpy(config->amf_name, "Mock-AMF-01", sizeof(config->amf_name) - 1);
    config->tac = 1;
    
    /* Behavior */
    config->auto_respond = true;
    config->response_delay_ms = 10;
    config->verbose = true;
    
    /* Logging */
    config->log_to_console = true;
}

/* ============== Server Management ============== */

mock_core_server_ctx_t* mock_core_server_create(const mock_core_server_config_t* config) {
    mock_core_server_ctx_t* ctx = (mock_core_server_ctx_t*)calloc(1, sizeof(mock_core_server_ctx_t));
    if (!ctx) {
        return NULL;
    }
    
    if (config) {
        memcpy(&ctx->config, config, sizeof(mock_core_server_config_t));
    } else {
        mock_core_server_get_default_config(&ctx->config);
    }
    
    ctx->state = MOCK_CORE_SERVER_STATE_IDLE;
    ctx->last_error[0] = '\0';
    
    return ctx;
}

void mock_core_server_destroy(mock_core_server_ctx_t* ctx) {
    if (!ctx) return;
    
    if (ctx->state != MOCK_CORE_SERVER_STATE_IDLE) {
        mock_core_server_stop(ctx);
    }
    
    free(ctx);
}

mock_core_server_error_t mock_core_server_start(mock_core_server_ctx_t* ctx) {
    if (!ctx) return MOCK_CORE_SERVER_ERROR_INVALID_PARAM;
    
    if (ctx->state != MOCK_CORE_SERVER_STATE_IDLE) {
        strncpy(ctx->last_error, "Server not in IDLE state", sizeof(ctx->last_error) - 1);
        return MOCK_CORE_SERVER_ERROR_ALREADY_RUNNING;
    }
    
    ctx->state = MOCK_CORE_SERVER_STATE_INITIALIZING;
    
    if (ctx->config.verbose) {
        printf("\n========================================\n");
        printf("    Mock Core Network Server\n");
        printf("========================================\n\n");
    }
    
    /* Start AMF */
    if (ctx->config.amf.enabled) {
        amf_config_t amf_cfg;
        amf_get_default_config(&amf_cfg);
        strncpy(amf_cfg.bind_ip, ctx->config.amf.bind_ip, sizeof(amf_cfg.bind_ip) - 1);
        amf_cfg.ngap_port = ctx->config.amf.port;
        amf_cfg.log_messages = ctx->config.amf.log_messages;
        amf_cfg.amf_id = ctx->config.amf_id;
        strncpy(amf_cfg.amf_name, ctx->config.amf_name, sizeof(amf_cfg.amf_name) - 1);
        amf_cfg.auto_respond = ctx->config.auto_respond;
        amf_cfg.response_delay_ms = ctx->config.response_delay_ms;
        
        ctx->amf = amf_create(&amf_cfg);
        if (!ctx->amf) {
            strncpy(ctx->last_error, "Failed to create AMF", sizeof(ctx->last_error) - 1);
            ctx->state = MOCK_CORE_SERVER_STATE_ERROR;
            return MOCK_CORE_SERVER_ERROR_COMPONENT;
        }
        
        mock_core_error_t err = amf_start(ctx->amf);
        if (err != MOCK_CORE_SUCCESS) {
            strncpy(ctx->last_error, "Failed to start AMF", sizeof(ctx->last_error) - 1);
            amf_destroy(ctx->amf);
            ctx->amf = NULL;
            ctx->state = MOCK_CORE_SERVER_STATE_ERROR;
            return MOCK_CORE_SERVER_ERROR_COMPONENT;
        }
        
        if (ctx->config.verbose) {
            printf("[MockCore] AMF started on %s:%u\n", 
                   ctx->config.amf.bind_ip, ctx->config.amf.port);
        }
    }
    
    /* Start SMF */
    if (ctx->config.smf.enabled) {
        smf_config_t smf_cfg;
        smf_get_default_config(&smf_cfg);
        strncpy(smf_cfg.bind_ip, ctx->config.smf.bind_ip, sizeof(smf_cfg.bind_ip) - 1);
        smf_cfg.log_messages = ctx->config.smf.log_messages;
        
        ctx->smf = smf_create(&smf_cfg);
        if (!ctx->smf) {
            strncpy(ctx->last_error, "Failed to create SMF", sizeof(ctx->last_error) - 1);
            mock_core_server_stop(ctx);
            ctx->state = MOCK_CORE_SERVER_STATE_ERROR;
            return MOCK_CORE_SERVER_ERROR_COMPONENT;
        }
        
        if (ctx->config.verbose) {
            printf("[MockCore] SMF started\n");
        }
    }
    
    /* Start UPF */
    if (ctx->config.upf.enabled) {
        upf_config_t upf_cfg;
        upf_get_default_config(&upf_cfg);
        strncpy(upf_cfg.bind_ip, ctx->config.upf.bind_ip, sizeof(upf_cfg.bind_ip) - 1);
        upf_cfg.gtpu_port = ctx->config.upf.port;
        upf_cfg.log_packets = ctx->config.upf.log_messages;
        
        ctx->upf = upf_create(&upf_cfg);
        if (!ctx->upf) {
            strncpy(ctx->last_error, "Failed to create UPF", sizeof(ctx->last_error) - 1);
            mock_core_server_stop(ctx);
            ctx->state = MOCK_CORE_SERVER_STATE_ERROR;
            return MOCK_CORE_SERVER_ERROR_COMPONENT;
        }
        
        mock_core_error_t err = upf_start(ctx->upf);
        if (err != MOCK_CORE_SUCCESS) {
            strncpy(ctx->last_error, "Failed to start UPF", sizeof(ctx->last_error) - 1);
            upf_destroy(ctx->upf);
            ctx->upf = NULL;
            mock_core_server_stop(ctx);
            ctx->state = MOCK_CORE_SERVER_STATE_ERROR;
            return MOCK_CORE_SERVER_ERROR_COMPONENT;
        }
        
        if (ctx->config.verbose) {
            printf("[MockCore] UPF started on %s:%u\n",
                   ctx->config.upf.bind_ip, ctx->config.upf.port);
        }
    }
    
    /* Start CU-CP */
    if (ctx->config.cu_cp.enabled) {
        cu_cp_config_t cu_cp_cfg;
        cu_cp_get_default_config(&cu_cp_cfg);
        strncpy(cu_cp_cfg.bind_ip, ctx->config.cu_cp.bind_ip, sizeof(cu_cp_cfg.bind_ip) - 1);
        cu_cp_cfg.log_messages = ctx->config.cu_cp.log_messages;
        
        ctx->cu_cp = cu_cp_create(&cu_cp_cfg);
        if (!ctx->cu_cp) {
            strncpy(ctx->last_error, "Failed to create CU-CP", sizeof(ctx->last_error) - 1);
            mock_core_server_stop(ctx);
            ctx->state = MOCK_CORE_SERVER_STATE_ERROR;
            return MOCK_CORE_SERVER_ERROR_COMPONENT;
        }
        
        mock_core_error_t err = cu_cp_start(ctx->cu_cp);
        if (err != MOCK_CORE_SUCCESS) {
            strncpy(ctx->last_error, "Failed to start CU-CP", sizeof(ctx->last_error) - 1);
            cu_cp_destroy(ctx->cu_cp);
            ctx->cu_cp = NULL;
            mock_core_server_stop(ctx);
            ctx->state = MOCK_CORE_SERVER_STATE_ERROR;
            return MOCK_CORE_SERVER_ERROR_COMPONENT;
        }
        
        if (ctx->config.verbose) {
            printf("[MockCore] CU-CP started on %s:%u\n",
                   ctx->config.cu_cp.bind_ip, ctx->config.cu_cp.port);
        }
    }
    
    /* Start DU */
    if (ctx->config.du.enabled) {
        du_config_t du_cfg;
        du_get_default_config(&du_cfg);
        strncpy(du_cfg.bind_ip, ctx->config.du.bind_ip, sizeof(du_cfg.bind_ip) - 1);
        du_cfg.log_messages = ctx->config.du.log_messages;
        
        ctx->du = du_create(&du_cfg);
        if (!ctx->du) {
            strncpy(ctx->last_error, "Failed to create DU", sizeof(ctx->last_error) - 1);
            mock_core_server_stop(ctx);
            ctx->state = MOCK_CORE_SERVER_STATE_ERROR;
            return MOCK_CORE_SERVER_ERROR_COMPONENT;
        }
        
        if (ctx->config.verbose) {
            printf("[MockCore] DU created\n");
        }
    }
    
    /* Start CU-UP */
    if (ctx->config.cu_up.enabled) {
        cu_up_config_t cu_up_cfg;
        cu_up_get_default_config(&cu_up_cfg);
        strncpy(cu_up_cfg.bind_ip, ctx->config.cu_up.bind_ip, sizeof(cu_up_cfg.bind_ip) - 1);
        cu_up_cfg.log_messages = ctx->config.cu_up.log_messages;
        
        ctx->cu_up = cu_up_create(&cu_up_cfg);
        if (!ctx->cu_up) {
            strncpy(ctx->last_error, "Failed to create CU-UP", sizeof(ctx->last_error) - 1);
            mock_core_server_stop(ctx);
            ctx->state = MOCK_CORE_SERVER_STATE_ERROR;
            return MOCK_CORE_SERVER_ERROR_COMPONENT;
        }
        
        if (ctx->config.verbose) {
            printf("[MockCore] CU-UP created\n");
        }
    }
    
    /* Start XnAP */
    if (ctx->config.xnap.enabled) {
        xnap_config_t xnap_cfg;
        xnap_get_default_config(&xnap_cfg);
        strncpy(xnap_cfg.bind_ip, ctx->config.xnap.bind_ip, sizeof(xnap_cfg.bind_ip) - 1);
        xnap_cfg.log_messages = ctx->config.xnap.log_messages;
        
        ctx->xnap = xnap_create(&xnap_cfg);
        if (!ctx->xnap) {
            strncpy(ctx->last_error, "Failed to create XnAP", sizeof(ctx->last_error) - 1);
            mock_core_server_stop(ctx);
            ctx->state = MOCK_CORE_SERVER_STATE_ERROR;
            return MOCK_CORE_SERVER_ERROR_COMPONENT;
        }
        
        if (ctx->config.verbose) {
            printf("[MockCore] XnAP created\n");
        }
    }
    
    ctx->state = MOCK_CORE_SERVER_STATE_RUNNING;
    ctx->running = true;
    ctx->stats.start_time = time(NULL);
    
    if (ctx->config.verbose) {
        printf("\n[MockCore] All components started successfully\n");
        printf("[MockCore] Press Ctrl+C to stop\n\n");
    }
    
    return MOCK_CORE_SERVER_SUCCESS;
}

void mock_core_server_stop(mock_core_server_ctx_t* ctx) {
    if (!ctx) return;
    
    ctx->state = MOCK_CORE_SERVER_STATE_STOPPING;
    ctx->running = false;
    
    if (ctx->config.verbose) {
        printf("\n[MockCore] Stopping all components...\n");
    }
    
    /* Stop in reverse order */
    
    /* Stop XnAP */
    if (ctx->xnap) {
        xnap_stop(ctx->xnap);
        xnap_destroy(ctx->xnap);
        ctx->xnap = NULL;
        if (ctx->config.verbose) printf("[MockCore] XnAP stopped\n");
    }
    
    /* Stop CU-UP */
    if (ctx->cu_up) {
        cu_up_stop(ctx->cu_up);
        cu_up_destroy(ctx->cu_up);
        ctx->cu_up = NULL;
        if (ctx->config.verbose) printf("[MockCore] CU-UP stopped\n");
    }
    
    /* Stop DU */
    if (ctx->du) {
        du_stop(ctx->du);
        du_destroy(ctx->du);
        ctx->du = NULL;
        if (ctx->config.verbose) printf("[MockCore] DU stopped\n");
    }
    
    /* Stop CU-CP */
    if (ctx->cu_cp) {
        cu_cp_stop(ctx->cu_cp);
        cu_cp_destroy(ctx->cu_cp);
        ctx->cu_cp = NULL;
        if (ctx->config.verbose) printf("[MockCore] CU-CP stopped\n");
    }
    
    /* Stop UPF */
    if (ctx->upf) {
        upf_stop(ctx->upf);
        upf_destroy(ctx->upf);
        ctx->upf = NULL;
        if (ctx->config.verbose) printf("[MockCore] UPF stopped\n");
    }
    
    /* Stop SMF */
    if (ctx->smf) {
        smf_destroy(ctx->smf);
        ctx->smf = NULL;
        if (ctx->config.verbose) printf("[MockCore] SMF stopped\n");
    }
    
    /* Stop AMF */
    if (ctx->amf) {
        amf_stop(ctx->amf);
        amf_destroy(ctx->amf);
        ctx->amf = NULL;
        if (ctx->config.verbose) printf("[MockCore] AMF stopped\n");
    }
    
    ctx->state = MOCK_CORE_SERVER_STATE_IDLE;
    
    if (ctx->config.verbose) {
        printf("[MockCore] All components stopped\n");
    }
}

bool mock_core_server_is_running(const mock_core_server_ctx_t* ctx) {
    return ctx ? ctx->running : false;
}

mock_core_server_state_t mock_core_server_get_state(const mock_core_server_ctx_t* ctx) {
    return ctx ? ctx->state : MOCK_CORE_SERVER_STATE_IDLE;
}

mock_core_server_error_t mock_core_server_get_stats(mock_core_server_ctx_t* ctx,
                                                     mock_core_server_stats_t* stats) {
    if (!ctx || !stats) return MOCK_CORE_SERVER_ERROR_INVALID_PARAM;
    
    if (ctx->stats.start_time > 0) {
        ctx->stats.uptime_seconds = (uint64_t)(time(NULL) - ctx->stats.start_time);
    }
    
    memcpy(stats, &ctx->stats, sizeof(mock_core_server_stats_t));
    
    return MOCK_CORE_SERVER_SUCCESS;
}

const char* mock_core_server_get_last_error(const mock_core_server_ctx_t* ctx) {
    return ctx ? ctx->last_error : "NULL context";
}

void mock_core_server_print_status(const mock_core_server_ctx_t* ctx) {
    if (!ctx) {
        printf("[MockCore] NULL context\n");
        return;
    }
    
    printf("\n=== Mock Core Server Status ===\n");
    printf("State: %s\n", mock_core_server_state_to_string(ctx->state));
    printf("Running: %s\n", ctx->running ? "Yes" : "No");
    
    printf("\nComponents:\n");
    printf("  AMF:   %s\n", ctx->amf ? "Running" : "Stopped");
    printf("  SMF:   %s\n", ctx->smf ? "Running" : "Stopped");
    printf("  UPF:   %s\n", ctx->upf ? "Running" : "Stopped");
    printf("  CU-CP: %s\n", ctx->cu_cp ? "Running" : "Stopped");
    printf("  DU:    %s\n", ctx->du ? "Running" : "Stopped");
    printf("  CU-UP: %s\n", ctx->cu_up ? "Running" : "Stopped");
    printf("  XnAP:  %s\n", ctx->xnap ? "Running" : "Stopped");
    
    if (ctx->last_error[0]) {
        printf("\nLast Error: %s\n", ctx->last_error);
    }
    
    printf("==============================\n\n");
}

void mock_core_server_print_stats(const mock_core_server_ctx_t* ctx) {
    if (!ctx) {
        printf("[MockCore] NULL context\n");
        return;
    }
    
    printf("\n=== Mock Core Server Statistics ===\n");
    printf("Uptime: %llu seconds\n", (unsigned long long)ctx->stats.uptime_seconds);
    printf("Total Registrations: %u\n", ctx->stats.total_registrations);
    printf("Successful Registrations: %u\n", ctx->stats.successful_registrations);
    printf("Total PDU Sessions: %u\n", ctx->stats.total_pdu_sessions);
    printf("Active PDU Sessions: %u\n", ctx->stats.active_pdu_sessions);
    printf("Total UE Contexts: %u\n", ctx->stats.total_ue_contexts);
    printf("==================================\n\n");
}

/* ============== Utility Functions ============== */

const char* mock_core_server_error_to_string(mock_core_server_error_t error) {
    switch (error) {
        case MOCK_CORE_SERVER_SUCCESS: return "Success";
        case MOCK_CORE_SERVER_ERROR_INVALID_PARAM: return "Invalid parameter";
        case MOCK_CORE_SERVER_ERROR_MEMORY: return "Memory allocation failed";
        case MOCK_CORE_SERVER_ERROR_SOCKET: return "Socket error";
        case MOCK_CORE_SERVER_ERROR_THREAD: return "Thread error";
        case MOCK_CORE_SERVER_ERROR_TIMEOUT: return "Timeout";
        case MOCK_CORE_SERVER_ERROR_NOT_INITIALIZED: return "Not initialized";
        case MOCK_CORE_SERVER_ERROR_ALREADY_RUNNING: return "Already running";
        case MOCK_CORE_SERVER_ERROR_NOT_RUNNING: return "Not running";
        case MOCK_CORE_SERVER_ERROR_COMPONENT: return "Component error";
        default: return "Unknown error";
    }
}

const char* mock_core_server_state_to_string(mock_core_server_state_t state) {
    switch (state) {
        case MOCK_CORE_SERVER_STATE_IDLE: return "IDLE";
        case MOCK_CORE_SERVER_STATE_INITIALIZING: return "INITIALIZING";
        case MOCK_CORE_SERVER_STATE_STARTING: return "STARTING";
        case MOCK_CORE_SERVER_STATE_RUNNING: return "RUNNING";
        case MOCK_CORE_SERVER_STATE_STOPPING: return "STOPPING";
        case MOCK_CORE_SERVER_STATE_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}