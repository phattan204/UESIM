/*
 * 5G UE Simulation Application
 * Main entry point
 */

#include "uesim.h"
#include "config/config.h"
#include "cli/cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>

// Global variables
static thread_pool_t* g_thread_pool = NULL;
static ue_context_t* g_ue_instances[MAX_UE_INSTANCES] = {NULL};
static atomic_int g_active_ue_count = 0;
static bool g_running = false;
static uesim_config_t g_config = {0};

// CLI options
static struct option long_options[] = {
    {"config", required_argument, 0, 'c'},
    {"instances", required_argument, 0, 'i'},
    {"verbose", no_argument, 0, 'v'},
    {"debug", no_argument, 0, 'd'},
    {"help", no_argument, 0, 'h'},
    {"interactive", no_argument, 0, 'I'},
    {0, 0, 0, 0}
};

// Function prototypes
static void print_usage(const char* program_name);
static uesim_error_t parse_arguments(int argc, char* argv[]);
static uesim_error_t initialize_application(void);
static void cleanup_application(void);
static uesim_error_t create_ue_instances(int count);
static void signal_handler(int sig);

int main(int argc, char* argv[]) {
    uesim_error_t result = UESIM_SUCCESS;
    int instance_count = 1;
    bool interactive_mode = false;
    char* config_file = NULL;
    
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize configuration
    result = config_init(&g_config);
    if (result != UESIM_SUCCESS) {
        fprintf(stderr, "Failed to initialize configuration: %d\n", result);
        return EXIT_FAILURE;
    }
    
    // Parse command line arguments
    result = parse_arguments(argc, argv);
    if (result != UESIM_SUCCESS) {
        config_cleanup(&g_config);
        return EXIT_FAILURE;
    }
    
    // Check for interactive mode
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-I") == 0 || strcmp(argv[i], "--interactive") == 0) {
            interactive_mode = true;
            break;
        }
    }
    
    // Load configuration file if specified
    if (config_file) {
        result = config_load(&g_config, config_file);
        if (result != UESIM_SUCCESS) {
            fprintf(stderr, "Failed to load configuration file: %s\n", config_file);
            config_cleanup(&g_config);
            return EXIT_FAILURE;
        }
        instance_count = g_config.general.num_instances;
    } else {
        // Load default configuration
        result = config_load_default(&g_config);
        if (result != UESIM_SUCCESS) {
            fprintf(stderr, "Failed to load default configuration\n");
            config_cleanup(&g_config);
            return EXIT_FAILURE;
        }
    }
    
    // Initialize application
    result = initialize_application();
    if (result != UESIM_SUCCESS) {
        fprintf(stderr, "Failed to initialize application: %d\n", result);
        config_cleanup(&g_config);
        return EXIT_FAILURE;
    }
    
    // Check for interactive mode
    if (interactive_mode) {
        printf("Starting interactive mode...\n");
        result = cli_start_interactive_mode();
        if (result != UESIM_SUCCESS) {
            fprintf(stderr, "Failed to start interactive mode: %d\n", result);
        }
        cleanup_application();
        config_cleanup(&g_config);
        return result == UESIM_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    
    // Create UE instances
    result = create_ue_instances(instance_count);
    if (result != UESIM_SUCCESS) {
        fprintf(stderr, "Failed to create UE instances: %d\n", result);
        cleanup_application();
        config_cleanup(&g_config);
        return EXIT_FAILURE;
    }
    
    printf("5G UE Simulation started with %d instance(s)\n", instance_count);
    printf("Press Ctrl+C to stop...\n");
    
    // Main loop
    g_running = true;
    while (g_running) {
        sleep(1);
        // Check for signals or commands
    }
    
    // Cleanup
    cleanup_application();
    config_cleanup(&g_config);
    printf("5G UE Simulation stopped\n");
    
    return EXIT_SUCCESS;
}

static void print_usage(const char* program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("\nOptions:\n");
    printf("  -c, --config FILE    Configuration file\n");
    printf("  -i, --instances N    Number of UE instances (default: 1)\n");
    printf("  -v, --verbose        Verbose logging\n");
    printf("  -d, --debug          Debug mode\n");
    printf("  -I, --interactive    Interactive mode\n");
    printf("  -h, --help           Show this help message\n");
}

static uesim_error_t parse_arguments(int argc, char* argv[]) {
    int opt;
    int option_index = 0;
    char* config_file = NULL;
    
    while ((opt = getopt_long(argc, argv, "c:i:vdhI", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                config_file = optarg;
                printf("Configuration file: %s\n", config_file);
                break;
            case 'i':
                {
                    int count = atoi(optarg);
                    if (count <= 0 || count > CONFIG_MAX_UE_INSTANCES) {
                        fprintf(stderr, "Invalid instance count: %s\n", optarg);
                        return UESIM_ERROR_INVALID_PARAM;
                    }
                    // Store in config if loaded
                    if (config_is_loaded(&g_config)) {
                        config_set_int(&g_config, CONFIG_SECTION_GENERAL, "num_instances", count);
                    }
                }
                break;
            case 'v':
                printf("Verbose mode enabled\n");
                if (config_is_loaded(&g_config)) {
                    config_set_bool(&g_config, CONFIG_SECTION_GENERAL, "verbose", true);
                }
                break;
            case 'd':
                printf("Debug mode enabled\n");
                if (config_is_loaded(&g_config)) {
                    config_set_bool(&g_config, CONFIG_SECTION_GENERAL, "debug", true);
                }
                break;
            case 'I':
                // Interactive mode flag - handled in main
                break;
            case 'h':
                print_usage(argv[0]);
                exit(EXIT_SUCCESS);
            default:
                print_usage(argv[0]);
                return UESIM_ERROR_INVALID_PARAM;
        }
    }
    
    return UESIM_SUCCESS;
}

static uesim_error_t initialize_application(void) {
    uesim_error_t result = UESIM_SUCCESS;
    
    // Initialize core system
    result = uesim_init();
    if (result != UESIM_SUCCESS) {
        fprintf(stderr, "Failed to initialize UE simulation core: %d\n", result);
        return result;
    }
    
    // Initialize CLI
    result = cli_init();
    if (result != UESIM_SUCCESS) {
        fprintf(stderr, "Failed to initialize CLI: %d\n", result);
        uesim_cleanup();
        return result;
    }
    
    // Initialize thread pool
    // TODO: Create thread pool based on config
    int thread_pool_size = g_config.performance.thread_pool_size;
    if (thread_pool_size == 0) {
        thread_pool_size = 4; // Default
    }
    printf("Thread pool size: %d\n", thread_pool_size);
    
    printf("Application initialized successfully\n");
    return UESIM_SUCCESS;
}

static void cleanup_application(void) {
    // Stop all UE instances
    for (int i = 0; i < MAX_UE_INSTANCES; i++) {
        if (g_ue_instances[i] != NULL) {
            uesim_stop_ue(g_ue_instances[i]);
            uesim_free(g_ue_instances[i]);
            g_ue_instances[i] = NULL;
        }
    }
    
    // Cleanup CLI
    cli_cleanup();
    
    // Cleanup core system
    uesim_cleanup();
    
    printf("Application cleanup completed\n");
}

static uesim_error_t create_ue_instances(int count) {
    uesim_error_t result = UESIM_SUCCESS;
    
    for (int i = 0; i < count; i++) {
        ue_context_t* ue_ctx = NULL;
        
        result = uesim_create_ue_instance(&ue_ctx);
        if (result != UESIM_SUCCESS) {
            fprintf(stderr, "Failed to create UE instance %d: %d\n", i, result);
            return result;
        }
        
        // Configure UE based on settings
        // TODO: Apply UE configuration from config
        
        // Start the UE
        result = uesim_start_ue(ue_ctx);
        if (result != UESIM_SUCCESS) {
            fprintf(stderr, "Failed to start UE instance %d: %d\n", i, result);
            uesim_free(ue_ctx);
            return result;
        }
        
        g_ue_instances[i] = ue_ctx;
        atomic_fetch_add(&g_active_ue_count, 1);
    }
    
    return UESIM_SUCCESS;
}

static void signal_handler(int sig) {
    printf("\nReceived signal %d, shutting down...\n", sig);
    g_running = false;
}
