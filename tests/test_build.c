/*
 * 5G UE Simulation Application
 * Build test to verify compilation
 */

#include "../src/uesim.h"
#include "../src/core/memory.h"
#include "../src/transport/socket_mgr.h"
#include "../src/protocol/rrc.h"
#include "../src/cli/cli.h"
#include "../src/utils/ring_buffer.h"

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("5G UE Simulation Build Test\n");
    printf("===========================\n");
    
    // Test basic initialization
    uesim_error_t result = uesim_init();
    if (result != UESIM_SUCCESS) {
        fprintf(stderr, "Failed to initialize UE simulation core: %d\n", result);
        return EXIT_FAILURE;
    }
    
    printf("✓ Core initialization successful\n");
    
    // Test memory management
    void* test_ptr = uesim_malloc(1024);
    if (test_ptr == NULL) {
        fprintf(stderr, "Failed to allocate memory\n");
        uesim_cleanup();
        return EXIT_FAILURE;
    }
    
    uesim_free(test_ptr);
    printf("✓ Memory management functions working\n");
    
    // Test ring buffer
    ring_buffer_t rb;
    result = ring_buffer_init(&rb, 4096);
    if (result != UESIM_SUCCESS) {
        fprintf(stderr, "Failed to initialize ring buffer: %d\n", result);
        uesim_cleanup();
        return EXIT_FAILURE;
    }
    
    ring_buffer_destroy(&rb);
    printf("✓ Ring buffer functions working\n");
    
    // Test CLI initialization
    result = cli_init();
    if (result != UESIM_SUCCESS) {
        fprintf(stderr, "Failed to initialize CLI: %d\n", result);
        uesim_cleanup();
        return EXIT_FAILURE;
    }
    
    cli_cleanup();
    printf("✓ CLI functions working\n");
    
    // Cleanup
    uesim_cleanup();
    printf("✓ Cleanup successful\n");
    
    printf("\nAll build tests passed!\n");
    printf("5G UE Simulation application is ready for development.\n");
    
    return EXIT_SUCCESS;
}