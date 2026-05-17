/*
 * 5G UE Simulation Application
 * Mock gNB Server - Main Entry Point
 * 
 * Usage: mock_gnb_server [options]
 * 
 * Options:
 *   --port <port>      NGAP port (default: 38412)
 *   --gtpu <port>      GTP-U port (default: 2152)
 *   --bind <ip>        Bind IP address (default: 0.0.0.0)
 *   --pcap <file>      PCAP output file
 *   --delay <ms>       Response delay in ms (default: 10)
 *   --no-auto          Disable auto-response
 *   --quiet            Suppress message logging
 *   --help             Show this help
 */

#include "mock_gnb_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <unistd.h>
#include <signal.h>
#endif

/* Signal handler for graceful shutdown */
static void signal_handler(int sig) {
    (void)sig;
    printf("\nShutting down mock gNB server...\n");
    mock_gnb_server_stop();
    exit(0);
}

static void print_usage(const char* prog_name) {
    printf("Mock gNB Server for 5G UE Simulation\n");
    printf("\nUsage: %s [options]\n\n", prog_name);
    printf("Options:\n");
    printf("  --port <port>      NGAP port (default: %d)\n", MOCK_GNB_DEFAULT_NGAP_PORT);
    printf("  --gtpu <port>      GTP-U port (default: %d)\n", MOCK_GNB_DEFAULT_GTPU_PORT);
    printf("  --bind <ip>        Bind IP address (default: 0.0.0.0)\n");
    printf("  --pcap <file>      PCAP output file for traffic capture\n");
    printf("  --delay <ms>       Response delay in milliseconds (default: 10)\n");
    printf("  --no-auto          Disable auto-response mode\n");
    printf("  --quiet            Suppress message logging\n");
    printf("  --gnb-id <id>      gNB ID (default: 1)\n");
    printf("  --tac <tac>        TAC (default: 1)\n");
    printf("  --pci <pci>        Physical Cell ID (default: 1)\n");
    printf("  --help             Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s --port 38412 --pcap test.pcap\n", prog_name);
    printf("  %s --bind 127.0.0.1 --delay 50\n", prog_name);
    printf("\nTraffic Capture:\n");
    printf("  Run tcpdump in parallel to capture all traffic:\n");
    printf("  tcpdump -i lo -w capture.pcap port 38412 or port 2152\n");
}

int main(int argc, char* argv[]) {
    mock_gnb_config_t config;
    mock_gnb_get_default_config(&config);
    
    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            config.ngap_port = (uint16_t)atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--gtpu") == 0 && i + 1 < argc) {
            config.gtpu_port = (uint16_t)atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--bind") == 0 && i + 1 < argc) {
            strncpy(config.bind_ip, argv[++i], sizeof(config.bind_ip) - 1);
        }
        else if (strcmp(argv[i], "--pcap") == 0 && i + 1 < argc) {
            strncpy(config.pcap_file, argv[++i], sizeof(config.pcap_file) - 1);
        }
        else if (strcmp(argv[i], "--delay") == 0 && i + 1 < argc) {
            config.response_delay_ms = (uint32_t)atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--no-auto") == 0) {
            config.auto_respond = false;
        }
        else if (strcmp(argv[i], "--quiet") == 0) {
            config.log_messages = false;
            config.log_to_console = false;
        }
        else if (strcmp(argv[i], "--gnb-id") == 0 && i + 1 < argc) {
            config.cell_config.gnb_id = (uint32_t)atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--tac") == 0 && i + 1 < argc) {
            config.cell_config.tac = (uint16_t)atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--pci") == 0 && i + 1 < argc) {
            config.cell_config.pci = (uint16_t)atoi(argv[++i]);
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
    printf("    Mock gNB Server for 5G UE Simulation\n");
    printf("========================================\n\n");
    printf("Configuration:\n");
    printf("  NGAP Port:     %u\n", config.ngap_port);
    printf("  GTP-U Port:    %u\n", config.gtpu_port);
    printf("  Bind Address:  %s\n", config.bind_ip);
    printf("  gNB ID:        %u\n", config.cell_config.gnb_id);
    printf("  TAC:           %u\n", config.cell_config.tac);
    printf("  PCI:           %u\n", config.cell_config.pci);
    printf("  Auto-respond:  %s\n", config.auto_respond ? "enabled" : "disabled");
    printf("  Response delay: %u ms\n", config.response_delay_ms);
    if (config.pcap_file[0] != '\0') {
        printf("  PCAP file:    %s\n", config.pcap_file);
    }
    printf("\n");
    
    /* Initialize server */
    mock_gnb_error_t result = mock_gnb_server_init(&config);
    if (result != MOCK_GNB_SUCCESS) {
        fprintf(stderr, "Failed to initialize mock gNB server: %d\n", result);
        return 1;
    }
    
    /* Start server */
    result = mock_gnb_server_start();
    if (result != MOCK_GNB_SUCCESS) {
        fprintf(stderr, "Failed to start mock gNB server: %d\n", result);
        return 1;
    }
    
    printf("Mock gNB server is running. Press Ctrl+C to stop.\n\n");
    
    /* Main loop - wait for signal */
    while (mock_gnb_server_is_running()) {
#ifdef _WIN32
        Sleep(1000);
#else
        sleep(1);
#endif
        
        /* Print stats periodically */
        static int counter = 0;
        if (++counter >= 30) {  /* Every 30 seconds */
            counter = 0;
            mock_gnb_stats_t stats;
            if (mock_gnb_server_get_stats(&stats) == MOCK_GNB_SUCCESS) {
                printf("[Stats] Connections: %u active / %u total, "
                       "Registrations: %u successful, "
                       "Messages: %u TX / %u RX\n",
                       stats.active_connections, stats.total_connections,
                       stats.successful_registrations,
                       stats.ngap_messages_sent, stats.ngap_messages_received);
            }
        }
    }
    
    printf("Mock gNB server stopped.\n");
    return 0;
}