/*
 * 5G UE Simulation Application
 * Memory management header
 */

#ifndef MEMORY_H
#define MEMORY_H

#include "../uesim.h"

// Memory management functions
uesim_error_t memory_init(size_t heap_size);
void memory_cleanup(void);

// Memory layout information
void print_memory_layout(void);

#endif // MEMORY_H