/*
 * 5G UE Simulation Application
 * Mock Core Network Server - Main Entry Point
 * 
 * Usage: mock_core_server [options]
 * 
 * Options:
 *   --amf-port <port>     AMF NGAP port (default: 38412)
 *   --smf-port <port>     SMF PFCP port (default: 8805)
 *   --upf-port <port>     UPF GTP-U port (default: 2152)
 *   --cu-cp-port <port>   CU-CP F1AP port (default: 38472)
 *   --cu-up-port <port>   CU-UP E1AP port (default: 38470)
 *   --xnap-port <port>    XnAP port (default: 38422)
 *   --bind <ip>           Bind IP address (default: 127.0.0.1)
 *   --amf-id <id>         AMF ID (default: 1)
 *   --tac <tac>           TAC value (default: 1)
 *   --no-auto             Disable auto-response mode
 *   --delay <ms>          Response delay in ms (default: 10)
 *   --quiet               Suppress message logging
 *   --help                Show this help
 */

#include "mock_core_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#ifdef _WIN32
#include <windows.h>
#define sleep_sec(s) Sleep((s) * 1000)
#else
#include <unistd.h>
#define sleep_sec(s) sleep(s)
#endif

/* Global server context for signal handler */
static mock_core_server_ctx_t* g_server_ctx = NULL;

/* Signal handler for graceful shutdown */
static void signal_handler(int sig) {
    (void)sig;
    printf("\n\nShutting down mock core server...\n");
    if (g_server_ctx) {
        mock_core_server_stop(g_server_ctx);
    }
}

static void print_usage(const char* prog_name) {
    printf("Mock Core Network Server for 5G UE Simulation\n");
    printf("\nUsage: %s [options]\n\n", prog_name);
    printf("Options:\n");
    printf("  --amf-port <port>     AMF NGAP port (default: %d)\n", 38412);
    printf("  --smf-port <port>     SMF PFCP port (default: %d)\n", 8805);
    printf("  --upf-port <port>     UPF GTP-U port (default: %d)\n", 2152);
    printf("  --cu-cp-port <port>   CU-CP F1AP port (default: %d)\n", 38472);
    printf("  --cu-up-port <port>   CU-UP E1AP port (default: %d)\n", 38470);
    printf("  --xnap-port <port>    XnAP port (default: %d)\n", 38422);
    printf("  --bind <ip>           Bind IP address (default: 127.0.0.1)\n");
    printf("  --amf-id <id>         AMF ID (default: 1)\n");
    printf("  --tac <tac>           TAC value (default: 1)\n");
    printf("  --no-auto             Disable auto-response mode\n");
    printf("  --delay <ms>          Response delay in ms (default: 10)\n");
    printf("  --quiet               Suppress message logging\n");
    printf("  --help                Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s                              # Start with defaults\n", prog_name);
    printf("  %s --bind 0.0.0.0              # Listen on all interfaces\n", prog_name);
    printf("  %s --amf-port 38412 --delay 50\n", prog_name);
    printf("\nComponents Started:\n");
    printf("  AMF   - Access and Mobility Management Function (NGAP)\n");
    printf("  SMF   - Session Management Function (PFCP)\n");
    printf("  UPF   - User Plane Function (GTP-U)\n");
    printf("  CU-CP - Central Unit Control Plane (F1AP)\n");
    printf("  DU    - Distributed Unit (F1AP client)\n");
    printf("  CU-UP - Central Unit User Plane (E1AP)\n");
    printf("  XnAP  - gNB-to-gNB Interface\n");
}

int main(int argc, char* argv[]) {
    mock_core_server_config_t config;
    mock_core_server_get_default_config(&config);
    
    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "--amf-port") == 0 && i + 1 < argc) {
            config.amf.port = (uint16_t)atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--smf-port") == 0 && i + 1 < argc) {
            config.smf.port = (uint16_t)atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--upf-port") == 0 && i + 1 < argc) {
            config.upf.port = (uint16_t)atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--cu-cp-port") == 0 && i + 1 < argc) {
            config.cu_cp.port = (uint16_t)atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--cu-up-port") == 0 && i + 1 < argc) {
            config.cu_up.port = (uint16_t)atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--xnap-port") == 0 && i + 1 < argc) {
            config.xnap.port = (uint16_t)atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--bind") == 0 && i + 1 < argc) {
            strncpy(config.amf.bind_ip, argv[++i], sizeof(config.amf.bind_ip) - 1);
            strncpy(config.smf.bind_ip, argv[i], sizeof(config.smf.bind_ip) - 1);
            strncpy(config.upf.bind_ip, argv[i], sizeof(config.upf.bind_ip) - 1);
            strncpy(config.cu_cp.bind_ip, argv[i], sizeof(config.cu_cp.bind_ip) - 1);
            strncpy(config.du.bind_ip, argv[i], sizeof(config.du.bind_ip) - 1);
            strncpy(config.cu_up.bind_ip, argv[i], sizeof(config.cu_up.bind_ip) - 1);
            strncpy(config.xnap.bind_ip, argv[i], sizeof(config.xnap.bind_ip) - 1);
        }
        else if (strcmp(argv[i], "--amf-id") == 0 && i + 1 < argc) {
            config.amf_id = (uint32_t)atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--tac") == 0 && i + 1 < argc) {
            config.tac = (uint32_t)atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--no-auto") == 0) {
            config.auto_respond = false;
        }
        else if (strcmp(argv[i], "--delay") == 0 && i + 1 < argc) {
            config.response_delay_ms = (uint32_t)atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--quiet") == 0) {
            config.verbose = false;
            config.amf.log_messages = false;
            config.smf.log_messages = false;
            config.upf.log_messages = false;
            config.cu_cp.log_messages = false;
            config.du.log_messages = false;
            config.cu_up.log_messages = false;
            config.xnap.log_messages = false;
        }
        else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }
    
    /* Setup signal handlers */
#ifdef _WIN32
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
#else
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
#endif
    
    /* Print banner */
    printf("\n");
    printf("========================================\n");
    printf("    Mock Core Network Server\n");
    printf("========================================\n\n");
    printf("Configuration:\n");
    printf("  AMF NGAP Port:  %u\n", config.amf.port);
    printf("  SMF PFCP Port:  %u\n", config.smf.port);
    printf("  UPF GTP-U Port: %u\n", config.upf.port);
    printf("  CU-CP F1AP Port: %u\n", config.cu_cp.port);
    printf("  CU-UP E1AP Port: %u\n", config.cu_up.port);
    printf("  XnAP Port:      %u\n", config.xnap.port);
    printf("  Bind Address:   %s\n", config.amf.bind_ip);
    printf("  AMF ID:         %u\n", config.amf_id);
    printf("  TAC:            %u\n", config.tac);
    printf("  Auto-respond:   %s\n", config.auto_respond ? "enabled" : "disabled");
    printf("  Response delay: %u ms\n", config.response_delay_ms);
    printf("\n");
    
    /* Create server context */
    g_server_ctx = mock_core_server_create(&config);
    if (!g_server_ctx) {
        fprintf(stderr, "Failed to create mock core server context\n");
        return 1;
    }
    
    /* Start server */
    mock_core_server_error_t result = mock_core_server_start(g_server_ctx);
    if (result != MOCK_CORE_SERVER_SUCCESS) {
        fprintf(stderr, "Failed to start mock core server: %s\n",
                mock_core_server_error_to_string(result));
        mock_core_server_destroy(g_server_ctx);
        return 1;
    }
    
    printf("Mock Core Network Server is running. Press Ctrl+C to stop.\n\n");
    
    /* Main loop - wait for signal */
    while (mock_core_server_is_running(g_server_ctx)) {
        sleep_sec(1);
        
        /* Print stats periodically */
        static int counter = 0;
        if (++counter >= 60) {  /* Every 60 seconds */
            counter = 0;
            if (config.verbose) {
                mock_core_server_print_stats(g_server_ctx);
            }
        }
    }
    
    /* Cleanup */
    mock_core_server_stop(g_server_ctx);
    mock_core_server_destroy(g_server_ctx);
    g_server_ctx = NULL;
    
    printf("Mock Core Network Server stopped.\n");
    return 0;
}