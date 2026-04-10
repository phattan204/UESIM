# 5G UE Simulation Application Development Guide

## Overview

This guide provides detailed information for developers working on the 5G UE Simulation application. It covers coding standards, development practices, and contribution guidelines.

## Coding Standards

### C Language Standards

The application follows C11 standard with the following compiler flags:
```makefile
CFLAGS = -std=c11 -Wall -Wextra -Werror -D_GNU_SOURCE
```

### Code Structure

#### Header Files
- Include guards using `#ifndef`/`#define`/`#endif`
- Function prototypes with clear parameter names
- Type definitions and constants
- Documentation comments for public APIs

#### Source Files
- Clear function separation and modularity
- Proper error handling and return codes
- Memory management (allocation/deallocation)
- Thread safety considerations

### Naming Conventions

#### Functions
- `snake_case` for function names
- Prefix with module name: `uesim_init()`, `rrc_execute_procedure()`
- Verb-noun pattern: `create_ue_instance()`, `send_ngap_message()`

#### Variables
- `snake_case` for variable names
- Descriptive names: `ue_context`, `socket_manager`
- Constants in `UPPER_SNAKE_CASE`: `MAX_UE_INSTANCES`, `UESIM_HEAP_SIZE`

#### Types
- `snake_case` with `_t` suffix: `ue_context_t`, `rrc_state_t`
- Enum types in `snake_case` with `_t` suffix: `rrc_procedure_t`

### Memory Management

#### Allocation Functions
```c
// Use custom allocation functions
void* ptr = uesim_malloc(size);
void* ptr = uesim_calloc(nmemb, size);

// Always check for NULL
if (ptr == NULL) {
    return UESIM_ERROR_MEMORY;
}

// Free with custom function
uesim_free(ptr);
```

#### Memory Layout Awareness
```c
// Stack allocation for small, temporary data
char buffer[256];

// Heap allocation for large or persistent data
large_data_t* data = uesim_malloc(sizeof(large_data_t));

// Data segment for global constants
static const config_t default_config = { ... };
```

### Error Handling

#### Error Codes
```c
typedef enum {
    UESIM_SUCCESS = 0,
    UESIM_ERROR_INVALID_PARAM = -1,
    UESIM_ERROR_MEMORY = -2,
    UESIM_ERROR_SOCKET = -3,
    UESIM_ERROR_THREAD = -4,
    UESIM_ERROR_TIMEOUT = -5
} uesim_error_t;
```

#### Error Propagation
```c
uesim_error_t function_name(parameters) {
    uesim_error_t result = UESIM_SUCCESS;
    
    // Validate parameters
    if (param == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Perform operations
    result = sub_function(param);
    if (result != UESIM_SUCCESS) {
        // Cleanup resources if needed
        cleanup_resources();
        return result;
    }
    
    return UESIM_SUCCESS;
}
```

### Thread Safety

#### Mutex Usage
```c
// Always check return values
if (pthread_mutex_lock(&mutex) != 0) {
    return UESIM_ERROR_THREAD;
}

// Critical section
// ...

if (pthread_mutex_unlock(&mutex) != 0) {
    return UESIM_ERROR_THREAD;
}
```

#### Atomic Operations
```c
#include <stdatomic.h>

static atomic_int g_counter = 0;
static atomic_bool g_initialized = false;

// Atomic read/write
int count = atomic_load(&g_counter);
atomic_store(&g_initialized, true);

// Atomic increment
int new_count = atomic_fetch_add(&g_counter, 1);
```

#### Condition Variables
```c
// Wait with timeout
struct timespec timeout;
clock_gettime(CLOCK_REALTIME, &timeout);
timeout.tv_sec += 30;

int result = pthread_cond_timedwait(&cond, &mutex, &timeout);
if (result == ETIMEDOUT) {
    // Handle timeout
}
```

## Advanced C Features

### Pointer Manipulation

#### Function Pointers
```c
typedef uesim_error_t (*rrc_handler_t)(ue_context_t*, rrc_message_t*);

static rrc_handler_t g_handlers[RRC_PROC_MAX] = {
    [RRC_PROC_REGISTRATION] = handle_registration,
    [RRC_PROC_ESTABLISHMENT] = handle_establishment,
    // ...
};

// Usage
uesim_error_t result = g_handlers[procedure_type](ue_ctx, message);
```

#### Pointer Arithmetic
```c
// Safe buffer manipulation
uint8_t* ptr = buffer;
size_t remaining = buffer_size;

while (data_to_process > 0 && remaining > 0) {
    size_t chunk_size = (data_to_process < remaining) ? data_to_process : remaining;
    memcpy(ptr, source_data, chunk_size);
    
    ptr += chunk_size;
    remaining -= chunk_size;
    data_to_process -= chunk_size;
}
```

#### Void Pointers
```c
// Generic data handling
typedef struct {
    void* data;
    size_t length;
    void (*cleanup)(void*);
} generic_data_t;

// Cleanup function
static void cleanup_string(void* data) {
    if (data != NULL) {
        uesim_free(data);
    }
}
```

### Bitwise Operations

#### Bit Manipulation Macros
```c
#define SET_BIT(field, bit)     ((field) |= (1U << (bit)))
#define CLEAR_BIT(field, bit)   ((field) &= ~(1U << (bit)))
#define CHECK_BIT(field, bit)   ((field) & (1U << (bit)))

#define BIT_MASK(start, length) (((1U << (length)) - 1) << (start))

// Usage
uint32_t flags = 0;
SET_BIT(flags, 3);
if (CHECK_BIT(flags, 3)) {
    // Bit 3 is set
}
```

#### Bit Fields
```c
typedef struct {
    uint8_t version : 4;
    uint8_t type : 4;
    uint16_t length : 12;
    uint16_t reserved : 4;
} protocol_header_t;
```

### Macro Usage

#### Conditional Compilation
```c
#ifdef DEBUG_MODE
    #define DEBUG_LOG(fmt, ...) \
        fprintf(stderr, "[DEBUG][%s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
    #define DEBUG_LOG(fmt, ...) do {} while(0)
#endif

// Static assertions
#define STATIC_ASSERT(condition, message) \
    typedef char static_assertion_##message[(condition) ? 1 : -1]

STATIC_ASSERT(sizeof(ue_context_t) <= 1024, ue_context_too_large);
```

#### Utility Macros
```c
// Container of pattern
#define container_of(ptr, type, member) \
    ((type*)((char*)(ptr) - offsetof(type, member)))

// Array size
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// Min/Max
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
```

## Data Structures

### Memory-Efficient Structures

#### Flexible Array Members
```c
typedef struct {
    size_t count;
    size_t capacity;
    ue_context_t* items[];  // Flexible array member
} ue_array_t;

// Allocation
size_t size = sizeof(ue_array_t) + (capacity * sizeof(ue_context_t));
ue_array_t* array = uesim_malloc(size);
```

#### Union Types
```c
typedef enum {
    DATA_TYPE_STRING,
    DATA_TYPE_BINARY,
    DATA_TYPE_INTEGER
} data_type_t;

typedef struct {
    data_type_t type;
    union {
        char* string_value;
        struct {
            void* data;
            size_t length;
        } binary_value;
        int64_t int_value;
    } value;
} generic_data_t;
```

### Thread-Safe Collections

#### Lock-Free Queue
```c
typedef struct {
    atomic_ptr_t head;
    atomic_ptr_t tail;
    pthread_mutex_t head_lock;
    pthread_mutex_t tail_lock;
} lock_free_queue_t;
```

## IPC Mechanisms

### Shared Memory

#### Memory Mapping
```c
#include <sys/mman.h>
#include <sys/stat.h>

static int create_shared_memory(const char* name, size_t size) {
    int fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        return -1;
    }
    
    if (ftruncate(fd, size) == -1) {
        close(fd);
        return -1;
    }
    
    return fd;
}
```

### Message Queues

#### POSIX Message Queues
```c
#include <mqueue.h>

static mqd_t create_message_queue(const char* name) {
    struct mq_attr attr = {
        .mq_flags = 0,
        .mq_maxmsg = 10,
        .mq_msgsize = 8192,
        .mq_curmsgs = 0
    };
    
    mqd_t mq = mq_open(name, O_CREAT | O_RDWR, 0666, &attr);
    return mq;
}
```

### Pipes

#### Named Pipes
```c
#include <sys/stat.h>

static int create_named_pipe(const char* path) {
    if (mkfifo(path, 0666) == -1) {
        if (errno != EEXIST) {
            return -1;
        }
    }
    
    return open(path, O_RDWR);
}
```

## Socket Programming

### Non-Blocking I/O

#### Socket Configuration
```c
static uesim_error_t set_nonblocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) {
        return UESIM_ERROR_SOCKET;
    }
    
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        return UESIM_ERROR_SOCKET;
    }
    
    return UESIM_SUCCESS;
}
```

#### Epoll Integration
```c
#include <sys/epoll.h>

static int setup_epoll(void) {
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd == -1) {
        return -1;
    }
    
    return epfd;
}
```

## Build System

### Makefile Best Practices

#### Dependency Tracking
```makefile
# Automatic dependency generation
DEPDIR = .deps
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td

COMPILE.c = $(CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) -c
POSTCOMPILE = @mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d && touch $@

%.o : %.c
%.o : %.c $(DEPDIR)/%.d
	@$(MKDIR_P) $(@D) $(DEPDIR)
	$(COMPILE.c) $(OUTPUT_OPTION) $<
	$(POSTCOMPILE)
```

#### Cross-Compilation Support
```makefile
# Cross-compilation variables
ifeq ($(CROSS_COMPILE),yes)
    CC = $(CROSS_COMPILER)-gcc
    STRIP_TOOL = $(CROSS_COMPILER)-strip
endif
```

#### Optimization Flags
```makefile
# Size optimization
SIZE_OPT_FLAGS = -ffunction-sections -fdata-sections
LINKER_GC_FLAGS = -Wl,--gc-sections -Wl,--print-gc-sections

# Security hardening
SECURITY_FLAGS = -fstack-protector-strong -D_FORTIFY_SOURCE=2
RELRO_FLAGS = -Wl,-z,relro,-z,now
PIE_FLAGS = -fPIE -pie
```

## Testing

### Unit Testing Framework

#### Test Structure
```c
// test_rrc.c
#include "rrc.h"
#include <assert.h>

static void test_rrc_state_transitions(void) {
    ue_context_t* ue_ctx = NULL;
    uesim_error_t result;
    
    // Setup
    result = uesim_create_ue_instance(&ue_ctx);
    assert(result == UESIM_SUCCESS);
    
    // Test
    rrc_state_t state = rrc_get_current_state(ue_ctx);
    assert(state == RRC_STATE_IDLE);
    
    result = rrc_change_state(ue_ctx, RRC_STATE_CONNECTED);
    assert(result == UESIM_SUCCESS);
    
    state = rrc_get_current_state(ue_ctx);
    assert(state == RRC_STATE_CONNECTED);
    
    // Cleanup
    uesim_free(ue_ctx);
}

int main(void) {
    test_rrc_state_transitions();
    printf("All tests passed!\n");
    return 0;
}
```

### Integration Testing

#### Mock Services
```c
// mock_gnb.c
typedef struct {
    int socket;
    bool running;
    pthread_t thread;
} mock_gnb_t;

static void* mock_gnb_thread(void* arg) {
    mock_gnb_t* gnb = (mock_gnb_t*)arg;
    
    while (gnb->running) {
        // Simulate gNB behavior
        // Send mock responses
        // Handle timeouts
    }
    
    return NULL;
}
```

## Performance Optimization

### Memory Pool Implementation

#### Custom Allocator
```c
typedef struct {
    void* base_address;
    size_t total_size;
    size_t used_size;
    pthread_mutex_t lock;
} memory_pool_t;

static memory_pool_t g_memory_pool = {0};

void* uesim_malloc(size_t size) {
    void* ptr = NULL;
    
    if (pthread_mutex_lock(&g_memory_pool.lock) != 0) {
        return malloc(size);  // Fallback
    }
    
    if (g_memory_pool.used_size + size <= g_memory_pool.total_size) {
        ptr = (char*)g_memory_pool.base_address + g_memory_pool.used_size;
        g_memory_pool.used_size += size;
    }
    
    pthread_mutex_unlock(&g_memory_pool.lock);
    
    if (ptr == NULL) {
        ptr = malloc(size);  // Fallback to system malloc
    }
    
    return ptr;
}
```

### Lock-Free Programming

#### Atomic Operations
```c
// Lock-free counter
static atomic_uint g_packet_counter = 0;

static inline void increment_packet_count(void) {
    atomic_fetch_add(&g_packet_counter, 1);
}

static inline uint32_t get_packet_count(void) {
    return atomic_load(&g_packet_counter);
}
```

## Debugging and Profiling

### Debugging Macros

#### Trace Points
```c
#ifdef ENABLE_TRACING
    #define TRACE_ENTER() fprintf(stderr, "ENTER: %s:%d\n", __func__, __LINE__)
    #define TRACE_EXIT() fprintf(stderr, "EXIT: %s:%d\n", __func__, __LINE__)
    #define TRACE_POINT(msg) fprintf(stderr, "TRACE: %s:%d - %s\n", __func__, __LINE__, msg)
#else
    #define TRACE_ENTER() do {} while(0)
    #define TRACE_EXIT() do {} while(0)
    #define TRACE_POINT(msg) do {} while(0)
#endif
```

### Profiling Integration

#### Performance Counters
```c
#include <time.h>

typedef struct {
    struct timespec start_time;
    struct timespec end_time;
    uint64_t call_count;
} performance_counter_t;

static inline void start_timer(performance_counter_t* counter) {
    clock_gettime(CLOCK_MONOTONIC, &counter->start_time);
}

static inline void stop_timer(performance_counter_t* counter) {
    clock_gettime(CLOCK_MONOTONIC, &counter->end_time);
    counter->call_count++;
}
```

## Security Considerations

### Input Validation

#### Buffer Bounds Checking
```c
uesim_error_t safe_string_copy(char* dest, size_t dest_size, const char* src) {
    if (dest == NULL || src == NULL || dest_size == 0) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    size_t src_len = strlen(src);
    if (src_len >= dest_size) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    strcpy(dest, src);
    return UESIM_SUCCESS;
}
```

### Secure Compilation

#### Hardening Flags
```makefile
# Security compilation flags
SECURITY_CFLAGS = -fstack-protector-strong \
                  -D_FORTIFY_SOURCE=2 \
                  -fPIE -pie \
                  -Wl,-z,relro,-z,now \
                  -fno-strict-overflow \
                  -fno-strict-aliasing

# Address Sanitizer (debug builds)
ASAN_FLAGS = -fsanitize=address -fsanitize=undefined
```

## Documentation

### Code Comments

#### Function Documentation
```c
/**
 * Initialize UE context for RRC operations
 * 
 * @param ue_ctx Pointer to UE context structure
 * @return UESIM_SUCCESS on success, error code on failure
 * 
 * This function initializes the RRC state machine for a UE instance,
 * sets up necessary resources, and prepares the UE for RRC procedures.
 * 
 * Thread Safety: This function is thread-safe when called with different
 * UE contexts. Calling with the same context from multiple threads
 * requires external synchronization.
 */
uesim_error_t rrc_init(ue_context_t* ue_ctx);
```

#### Complex Logic Comments
```c
// Handle RRC reconfiguration with handover
// This is a complex procedure that involves:
// 1. Preparing measurement configuration
// 2. Sending measurement reports
// 3. Processing handover command
// 4. Executing handover procedure
// 5. Updating RRC state
```

## Contribution Guidelines

### Git Workflow

#### Branch Naming
- `feature/` - New features
- `bugfix/` - Bug fixes
- `hotfix/` - Critical production fixes
- `release/` - Release preparation

#### Commit Messages
```
feat(rrc): implement handover procedure

- Add RRC handover preparation message handling
- Implement handover command processing
- Add handover confirmation procedure
- Update state machine for handover scenarios

Resolves #123
```

### Code Review Process

#### Review Checklist
- [ ] Code follows coding standards
- [ ] Memory management is correct
- [ ] Error handling is comprehensive
- [ ] Thread safety is maintained
- [ ] Performance considerations are addressed
- [ ] Security implications are considered
- [ ] Documentation is updated
- [ ] Tests are included/updated

### Continuous Integration

#### Build Matrix
- RHEL 8.5 with GCC 8.5
- Ubuntu 20.04 with GCC 9.4
- Cross-compilation for ARM64
- Static analysis with Clang Static Analyzer
- Dynamic analysis with Valgrind

This development guide ensures consistent, high-quality code development while leveraging advanced C programming techniques and best practices for systems programming.