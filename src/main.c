/*
 * 5G UE Simulation Application
 * Main entry point
 */

#include "uesim.h"
#include "config/config.h"
#include "cli/cli.h"
#include "core/thread_pool.h"
#include "protocol/rrc.h"
#include "utils/log.h"
#include "mock_integration/mock_test_env.h"
#include "mock_integration/test_flow_controller.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>

// Global variables
static thread_pool_t* g_thread_pool = NULL;
static ue_context_t* g_ue_instances[MAX_UE_INSTANCES] = {NULL};
#ifdef _WIN32
static volatile LONG g_active_ue_count = 0;
#else
static atomic_int g_active_ue_count = 0;
#endif
static bool g_running = false;
uesim_config_t g_config = {0};

// Test mode globals
static mock_test_env_t* g_test_env = NULL;
static test_flow_controller_t* g_test_controller = NULL;
static bool g_test_mode = false;
static bool g_with_mock = false;

// CLI options
static struct option long_options[] = {
    {"config", required_argument, 0, 'c'},
    {"instances", required_argument, 0, 'i'},
    {"verbose", no_argument, 0, 'v'},
    {"debug", no_argument, 0, 'd'},
    {"help", no_argument, 0, 'h'},
    {"interactive", no_argument, 0, 'I'},
    {"test-mode", no_argument, 0, 't'},
    {"test-scenario", required_argument, 0, 's'},
    {"test-ues", required_argument, 0, 'u'},
    {"test-report", required_argument, 0, 'r'},
    {"with-mock", no_argument, 0, 'M'},
    {0, 0, 0, 0}
};

// Function prototypes
static void print_usage(const char* program_name);
static uesim_error_t parse_arguments(int argc, char* argv[]);
static uesim_error_t initialize_application(void);
static void cleanup_application(void);
static uesim_error_t create_ue_instances(int count);
static void signal_handler(int sig);
static uesim_error_t run_test_mode(int num_ues, const char* scenario_file, const char* report_file);
static uesim_error_t start_mock_environment(void);
static void stop_mock_environment(void);

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
    
    // Check for test mode
    if (g_test_mode) {
        int test_num_ues = 1;
        char* test_scenario_file = NULL;
        char* test_report_file = NULL;
        
        // Parse test mode specific arguments
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "--test-ues") == 0) {
                if (i + 1 < argc) {
                    test_num_ues = atoi(argv[++i]);
                    if (test_num_ues <= 0) test_num_ues = 1;
                }
            } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--test-scenario") == 0) {
                if (i + 1 < argc) {
                    test_scenario_file = argv[++i];
                }
            } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--test-report") == 0) {
                if (i + 1 < argc) {
                    test_report_file = argv[++i];
                }
            }
        }
        
        printf("\n========================================\n");
        printf("        UESim Test Mode\n");
        printf("========================================\n");
        printf("Test UEs: %d\n", test_num_ues);
        if (test_scenario_file) {
            printf("Scenario: %s\n", test_scenario_file);
        }
        if (test_report_file) {
            printf("Report: %s\n", test_report_file);
        }
        printf("\n");
        
        result = run_test_mode(test_num_ues, test_scenario_file, test_report_file);
        config_cleanup(&g_config);
        return result == UESIM_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
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
        // Start mock environment if requested
        if (g_with_mock) {
            result = start_mock_environment();
            if (result != UESIM_SUCCESS) {
                fprintf(stderr, "Failed to start mock environment: %d\n", result);
                cleanup_application();
                config_cleanup(&g_config);
                return EXIT_FAILURE;
            }
        }
        
        printf("Starting interactive mode...\n");
        result = cli_start_interactive_mode();
        if (result != UESIM_SUCCESS) {
            fprintf(stderr, "Failed to start interactive mode: %d\n", result);
        }
        
        // Stop mock environment if it was started
        if (g_with_mock) {
            stop_mock_environment();
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
        uesim_sleep(1);
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
    printf("\nTest Mode Options:\n");
    printf("  -t, --test-mode      Enable test mode with mock core/gNB\n");
    printf("  -s, --test-scenario  Test scenario file (JSON)\n");
    printf("  -u, --test-ues N     Number of test UEs (default: 1)\n");
    printf("  -r, --test-report F  Test report output file\n");
    printf("  -M, --with-mock      Start mock components with interactive mode\n");
    printf("\nExamples:\n");
    printf("  %s -i 10                    # Run 10 UE instances\n", program_name);
    printf("  %s -t -u 5                  # Test mode with 5 UEs\n", program_name);
    printf("  %s -t -s scenario.json      # Test mode with scenario file\n", program_name);
    printf("  %s -t -u 3 -r report.txt    # Test mode with report output\n", program_name);
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
            case 't':
                g_test_mode = true;
                printf("Test mode enabled\n");
                break;
            case 's':
                // Test scenario file - handled in test mode
                break;
            case 'u':
                // Number of test UEs - handled in test mode
                break;
            case 'r':
                // Test report file - handled in test mode
                break;
            case 'M':
                g_with_mock = true;
                printf("With-mock mode enabled\n");
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
    
    // Initialize logging
    result = log_init();
    if (result != UESIM_SUCCESS) {
        fprintf(stderr, "Failed to initialize logging: %d\n", result);
        return result;
    }
    LOG_INFO(LOG_CAT_NAME_CORE, "Logging initialized");
    
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
    int thread_pool_size = g_config.performance.thread_pool_size;
    if (thread_pool_size == 0) {
        thread_pool_size = 4; // Default
    }
    
    result = thread_pool_create((uint32_t)thread_pool_size, &g_thread_pool);
    if (result != UESIM_SUCCESS) {
        fprintf(stderr, "Failed to create thread pool: %d\n", result);
        cli_cleanup();
        uesim_cleanup();
        return result;
    }
    
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
    
    // Destroy thread pool
    if (g_thread_pool != NULL) {
        thread_pool_wait(g_thread_pool);
        thread_pool_destroy(g_thread_pool);
        g_thread_pool = NULL;
    }
    
    // Cleanup CLI
    cli_cleanup();
    
    // Cleanup core system
    uesim_cleanup();
    
    // Cleanup logging
    LOG_INFO(LOG_CAT_NAME_CORE, "Application cleanup completed");
    log_cleanup();
    
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
        
        // Apply UE configuration from config
        config_ue_t* ue_config = config_get_ue(&g_config);
        config_network_t* net_config = config_get_network(&g_config);
        
        if (ue_config != NULL) {
            // Generate IMSI from prefix and start number
            snprintf(ue_ctx->imsi, sizeof(ue_ctx->imsi), "%s%07u", 
                     ue_config->imsi_prefix, ue_config->imsi_start + i);
            
            // Generate MSISDN from prefix and start number
            snprintf(ue_ctx->msisdn, sizeof(ue_ctx->msisdn), "%s%07u", 
                     ue_config->msisdn_prefix, ue_config->msisdn_start + i);
            
            // Apply TAC
            ue_ctx->tac = ue_config->tac;
            
            printf("UE %d configured: IMSI=%s, TAC=%u\n", 
                   i, ue_ctx->imsi, ue_ctx->tac);
        }
        
        // Apply network configuration
        if (net_config != NULL) {
            if (strlen(net_config->gnb_ip) > 0) {
                ue_ctx->gnb_ip = inet_addr(net_config->gnb_ip);
            }
            ue_ctx->gnb_port = net_config->gnb_ngap_port;
            
            printf("UE %d gNB: %s:%u\n", 
                   i, net_config->gnb_ip[0] ? net_config->gnb_ip : "default",
                   ue_ctx->gnb_port);
        }
        
        // Start the UE
        result = uesim_start_ue(ue_ctx);
        if (result != UESIM_SUCCESS) {
            fprintf(stderr, "Failed to start UE instance %d: %d\n", i, result);
            uesim_free(ue_ctx);
            return result;
        }
        
        g_ue_instances[i] = ue_ctx;
#ifdef _WIN32
        InterlockedExchangeAdd(&g_active_ue_count, 1);
#else
        atomic_fetch_add(&g_active_ue_count, 1);
#endif
    }
    
    return UESIM_SUCCESS;
}

static void signal_handler(int sig) {
    printf("\nReceived signal %d, shutting down...\n", sig);
    g_running = false;
}

/* UE Registry Accessor Functions for I/O thread */
ue_context_t** uesim_get_ue_instances(void) {
    return g_ue_instances;
}

int uesim_get_ue_instance_count(void) {
    return MAX_UE_INSTANCES;
}

int uesim_get_active_ue_count(void) {
#ifdef _WIN32
    return InterlockedCompareExchange(&g_active_ue_count, 0, 0);
#else
    return atomic_load(&g_active_ue_count);
#endif
}

/* Test Mode Implementation */
static uesim_error_t run_test_mode(int num_ues, const char* scenario_file, const char* report_file) {
    printf("[TestMode] Initializing test environment...\n");
    
    // Create test environment with default configuration
    mock_test_env_config_t env_config;
    mock_test_env_get_default_config(&env_config);
    env_config.verbose = true;
    env_config.max_ues = (uint32_t)num_ues;
    
    g_test_env = mock_test_env_create(&env_config);
    if (!g_test_env) {
        fprintf(stderr, "[TestMode] Failed to create test environment\n");
        return UESIM_ERROR_MEMORY;
    }
    
    // Start test environment (starts all mock components)
    printf("[TestMode] Starting mock components...\n");
    mock_test_error_t err = mock_test_env_start(g_test_env);
    if (err != MOCK_TEST_SUCCESS) {
        fprintf(stderr, "[TestMode] Failed to start test environment: %s\n",
                mock_test_error_to_string(err));
        mock_test_env_destroy(g_test_env);
        g_test_env = NULL;
        return UESIM_ERROR_INIT;
    }
    
    printf("[TestMode] Test environment started successfully\n");
    mock_test_env_print_status(g_test_env);
    
    // Create test flow controller
    g_test_controller = test_flow_controller_create(g_test_env);
    if (!g_test_controller) {
        fprintf(stderr, "[TestMode] Failed to create test flow controller\n");
        mock_test_env_stop(g_test_env);
        mock_test_env_destroy(g_test_env);
        g_test_env = NULL;
        return UESIM_ERROR_MEMORY;
    }
    
    // Load or create test scenario
    test_scenario_t scenario;
    
    if (scenario_file) {
        printf("[TestMode] Loading scenario from: %s\n", scenario_file);
        // For now, create a complete scenario (JSON loading would be implemented here)
        test_flow_controller_create_complete_scenario(&scenario, (uint32_t)num_ues);
    } else {
        printf("[TestMode] Creating default complete test scenario for %d UE(s)\n", num_ues);
        test_flow_error_t scenario_err = test_flow_controller_create_complete_scenario(&scenario, (uint32_t)num_ues);
        if (scenario_err != TEST_FLOW_SUCCESS) {
            fprintf(stderr, "[TestMode] Failed to create test scenario\n");
            test_flow_controller_destroy(g_test_controller);
            mock_test_env_stop(g_test_env);
            mock_test_env_destroy(g_test_env);
            g_test_controller = NULL;
            g_test_env = NULL;
            return UESIM_ERROR_INIT;
        }
    }
    
    // Add scenario to controller
    test_flow_controller_add_scenario(g_test_controller, &scenario);
    
    // Run the test scenario
    printf("\n[TestMode] Running test scenario...\n\n");
    test_flow_error_t run_err = test_flow_controller_run_all(g_test_controller);
    
    // Generate report
    if (report_file) {
        printf("[TestMode] Generating report: %s\n", report_file);
        test_flow_controller_generate_report(g_test_controller, report_file);
    } else {
        test_flow_controller_generate_report(g_test_controller, NULL);
    }
    
    // Print final statistics
    printf("\n[TestMode] Final Statistics:\n");
    mock_test_env_print_stats(g_test_env);
    
    // Cleanup
    test_flow_controller_destroy(g_test_controller);
    mock_test_env_stop(g_test_env);
    mock_test_env_destroy(g_test_env);
    g_test_controller = NULL;
    g_test_env = NULL;
    
    printf("\n[TestMode] Test completed: %s\n", 
           run_err == TEST_FLOW_SUCCESS ? "PASSED" : "FAILED");
    
    return run_err == TEST_FLOW_SUCCESS ? UESIM_SUCCESS : UESIM_ERROR_TEST_FAILED;
}

/* Mock Environment Functions for Interactive Mode */
static uesim_error_t start_mock_environment(void) {
    printf("[MockEnv] Starting mock environment for interactive mode...\n");
    
    // Create test environment with default configuration
    mock_test_env_config_t env_config;
    mock_test_env_get_default_config(&env_config);
    env_config.verbose = true;
    env_config.max_ues = 16;  // Default for interactive mode
    
    g_test_env = mock_test_env_create(&env_config);
    if (!g_test_env) {
        fprintf(stderr, "[MockEnv] Failed to create mock environment\n");
        return UESIM_ERROR_MEMORY;
    }
    
    // Start test environment (starts all mock components)
    printf("[MockEnv] Starting mock components (AMF, SMF, UPF, CU-CP, DU, CU-UP, XnAP)...\n");
    mock_test_error_t err = mock_test_env_start(g_test_env);
    if (err != MOCK_TEST_SUCCESS) {
        fprintf(stderr, "[MockEnv] Failed to start mock environment: %s\n",
                mock_test_error_to_string(err));
        mock_test_env_destroy(g_test_env);
        g_test_env = NULL;
        return UESIM_ERROR_INIT;
    }
    
    printf("[MockEnv] Mock environment started successfully\n");
    mock_test_env_print_status(g_test_env);
    
    return UESIM_SUCCESS;
}

static void stop_mock_environment(void) {
    if (g_test_env) {
        printf("[MockEnv] Stopping mock environment...\n");
        mock_test_env_stop(g_test_env);
        mock_test_env_destroy(g_test_env);
        g_test_env = NULL;
        printf("[MockEnv] Mock environment stopped\n");
    }
}
