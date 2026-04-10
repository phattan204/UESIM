# 5G UE Simulation Application API Reference

## Overview

This document provides a comprehensive reference for the 5G UE Simulation application's public API. The API is designed to be thread-safe, efficient, and suitable for integration into larger 5G testing and development environments.

## Core API

### Initialization and Cleanup

#### `uesim_init()`
```c
uesim_error_t uesim_init(void);
```
Initialize the UE simulation core system.

**Description:**
Initializes all core subsystems including memory management, socket system, and protocol stack. This function must be called before any other API functions.

**Parameters:**
None

**Returns:**
- `UESIM_SUCCESS` - Initialization successful
- `UESIM_ERROR_MEMORY` - Memory allocation failed
- `UESIM_ERROR_THREAD` - Thread initialization failed

**Thread Safety:**
Thread-safe. Can be called multiple times; subsequent calls are no-ops.

**Example:**
```c
uesim_error_t result = uesim_init();
if (result != UESIM_SUCCESS) {
    fprintf(stderr, "Failed to initialize UE simulation: %d\n", result);
    return -1;
}
```

#### `uesim_cleanup()`
```c
void uesim_cleanup(void);
```
Cleanup and shutdown the UE simulation core system.

**Description:**
Releases all allocated resources, closes sockets, and shuts down subsystems. This function should be called before application exit.

**Parameters:**
None

**Returns:**
None

**Thread Safety:**
Thread-safe. Can be called multiple times.

### UE Instance Management

#### `uesim_create_ue_instance()`
```c
uesim_error_t uesim_create_ue_instance(ue_context_t** ue_ctx);
```
Create a new UE instance context.

**Description:**
Allocates and initializes a new UE context with default configuration. The context includes socket connections, buffer management, and RRC state tracking.

**Parameters:**
- `ue_ctx` - Pointer to store the created UE context

**Returns:**
- `UESIM_SUCCESS` - Instance created successfully
- `UESIM_ERROR_INVALID_PARAM` - Invalid parameter
- `UESIM_ERROR_MEMORY` - Memory allocation failed
- `UESIM_ERROR_THREAD` - Thread synchronization failed

**Thread Safety:**
Thread-safe. Multiple instances can be created concurrently.

**Example:**
```c
ue_context_t* ue_ctx = NULL;
uesim_error_t result = uesim_create_ue_instance(&ue_ctx);
if (result == UESIM_SUCCESS) {
    printf("Created UE instance %u\n", ue_ctx->ue_id);
}
```

#### `uesim_start_ue()`
```c
uesim_error_t uesim_start_ue(ue_context_t* ue_ctx);
```
Start a UE instance and establish connections.

**Description:**
Initializes socket connections to gNB, starts I/O threads, and prepares the UE for RRC procedures.

**Parameters:**
- `ue_ctx` - UE context to start

**Returns:**
- `UESIM_SUCCESS` - UE started successfully
- `UESIM_ERROR_INVALID_PARAM` - Invalid parameter
- `UESIM_ERROR_SOCKET` - Socket initialization failed
- `UESIM_ERROR_THREAD` - Thread creation failed

**Thread Safety:**
Thread-safe when called with different UE contexts.

#### `uesim_stop_ue()`
```c
uesim_error_t uesim_stop_ue(ue_context_t* ue_ctx);
```
Stop a UE instance and close connections.

**Description:**
Closes socket connections, stops I/O threads, and cleans up UE resources.

**Parameters:**
- `ue_ctx` - UE context to stop

**Returns:**
- `UESIM_SUCCESS` - UE stopped successfully
- `UESIM_ERROR_INVALID_PARAM` - Invalid parameter

**Thread Safety:**
Thread-safe when called with different UE contexts.

### RRC State Management

#### `uesim_execute_procedure()`
```c
uesim_error_t uesim_execute_procedure(ue_context_t* ue_ctx, rrc_procedure_t procedure);
```
Execute an RRC procedure on a UE instance.

**Description:**
Initiates the specified RRC procedure and handles the complete procedure flow including message exchange and state transitions.

**Parameters:**
- `ue_ctx` - UE context to execute procedure on
- `procedure` - RRC procedure to execute

**Returns:**
- `UESIM_SUCCESS` - Procedure executed successfully
- `UESIM_ERROR_INVALID_PARAM` - Invalid parameter
- `UESIM_ERROR_PROTOCOL` - Protocol error
- `UESIM_ERROR_TIMEOUT` - Procedure timed out

**Thread Safety:**
Thread-safe when called with different UE contexts.

**Example:**
```c
// Execute RRC registration
uesim_error_t result = uesim_execute_procedure(ue_ctx, RRC_PROC_REGISTRATION);
if (result == UESIM_SUCCESS) {
    printf("Registration completed successfully\n");
}
```

#### `uesim_lock_state()`
```c
uesim_error_t uesim_lock_state(ue_context_t* ue_ctx);
```
Acquire exclusive lock on UE state.

**Description:**
Locks the UE context's state mutex for exclusive access to state variables.

**Parameters:**
- `ue_ctx` - UE context to lock

**Returns:**
- `UESIM_SUCCESS` - Lock acquired successfully
- `UESIM_ERROR_INVALID_PARAM` - Invalid parameter
- `UESIM_ERROR_THREAD` - Lock acquisition failed

**Thread Safety:**
Thread-safe.

#### `uesim_unlock_state()`
```c
uesim_error_t uesim_unlock_state(ue_context_t* ue_ctx);
```
Release exclusive lock on UE state.

**Description:**
Unlocks the UE context's state mutex.

**Parameters:**
- `ue_ctx` - UE context to unlock

**Returns:**
- `UESIM_SUCCESS` - Lock released successfully
- `UESIM_ERROR_INVALID_PARAM` - Invalid parameter
- `UESIM_ERROR_THREAD` - Unlock failed

**Thread Safety:**
Thread-safe.

#### `uesim_wait_for_state_change()`
```c
uesim_error_t uesim_wait_for_state_change(ue_context_t* ue_ctx, rrc_state_t expected_state);
```
Wait for UE to transition to expected state.

**Description:**
Blocks until the UE transitions to the expected RRC state or timeout occurs.

**Parameters:**
- `ue_ctx` - UE context to monitor
- `expected_state` - Target RRC state

**Returns:**
- `UESIM_SUCCESS` - State transition completed
- `UESIM_ERROR_INVALID_PARAM` - Invalid parameter
- `UESIM_ERROR_TIMEOUT` - Timeout waiting for state change
- `UESIM_ERROR_THREAD` - Synchronization error

**Thread Safety:**
Thread-safe.

## Memory Management API

### Custom Memory Allocation

#### `uesim_malloc()`
```c
void* uesim_malloc(size_t size);
```
Allocate memory from custom memory pool.

**Description:**
Allocates memory from the application's custom memory pool. Falls back to system malloc if pool is exhausted.

**Parameters:**
- `size` - Number of bytes to allocate

**Returns:**
- Pointer to allocated memory, or NULL on failure

**Thread Safety:**
Thread-safe.

**Example:**
```c
char* buffer = (char*)uesim_malloc(1024);
if (buffer != NULL) {
    // Use buffer
    uesim_free(buffer);
}
```

#### `uesim_calloc()`
```c
void* uesim_calloc(size_t nmemb, size_t size);
```
Allocate and zero-initialize memory.

**Description:**
Allocates memory for an array of `nmemb` elements of `size` bytes each and initializes to zero.

**Parameters:**
- `nmemb` - Number of elements
- `size` - Size of each element

**Returns:**
- Pointer to allocated and zeroed memory, or NULL on failure

**Thread Safety:**
Thread-safe.

#### `uesim_free()`
```c
void uesim_free(void* ptr);
```
Free allocated memory.

**Description:**
Frees memory allocated by `uesim_malloc()` or `uesim_calloc()`. Memory from the custom pool is returned to the pool; system-allocated memory is freed normally.

**Parameters:**
- `ptr` - Pointer to memory to free

**Returns:**
None

**Thread Safety:**
Thread-safe.

## Socket Transport API

### Socket Management

#### `create_ngap_socket()`
```c
uesim_error_t create_ngap_socket(ue_context_t* ue_ctx);
```
Create NGAP/SCTP socket connection.

**Description:**
Establishes an SCTP connection to the gNB for NGAP signaling. Falls back to TCP if SCTP is unavailable.

**Parameters:**
- `ue_ctx` - UE context to create socket for

**Returns:**
- `UESIM_SUCCESS` - Socket created successfully
- `UESIM_ERROR_INVALID_PARAM` - Invalid parameter
- `UESIM_ERROR_SOCKET` - Socket creation failed

**Thread Safety:**
Thread-safe when called with different UE contexts.

#### `create_gtpu_socket()`
```c
uesim_error_t create_gtpu_socket(ue_context_t* ue_ctx);
```
Create GTP-U/UDP socket connection.

**Description:**
Creates a UDP socket for GTP-U user plane data transmission.

**Parameters:**
- `ue_ctx` - UE context to create socket for

**Returns:**
- `UESIM_SUCCESS` - Socket created successfully
- `UESIM_ERROR_INVALID_PARAM` - Invalid parameter
- `UESIM_ERROR_SOCKET` - Socket creation failed

**Thread Safety:**
Thread-safe when called with different UE contexts.

### Message Transmission

#### `send_ngap_message()`
```c
uesim_error_t send_ngap_message(ue_context_t* ue_ctx, const void* data, size_t length);
```
Send NGAP message to gNB.

**Description:**
Transmits an encoded NGAP message over the SCTP/TCP connection to the gNB.

**Parameters:**
- `ue_ctx` - UE context with active NGAP socket
- `data` - Pointer to message data
- `length` - Length of message data

**Returns:**
- `UESIM_SUCCESS` - Message sent successfully
- `UESIM_ERROR_INVALID_PARAM` - Invalid parameter
- `UESIM_ERROR_SOCKET` - Transmission failed

**Thread Safety:**
Thread-safe when called with different UE contexts.

#### `send_gtpu_packet()`
```c
uesim_error_t send_gtpu_packet(ue_context_t* ue_ctx, const void* data, size_t length);
```
Send GTP-U packet to gNB.

**Description:**
Transmits a GTP-U packet over the UDP connection to the gNB.

**Parameters:**
- `ue_ctx` - UE context with active GTP-U socket
- `data` - Pointer to packet data
- `length` - Length of packet data

**Returns:**
- `UESIM_SUCCESS` - Packet sent successfully
- `UESIM_ERROR_INVALID_PARAM` - Invalid parameter
- `UESIM_ERROR_SOCKET` - Transmission failed

**Thread Safety:**
Thread-safe when called with different UE contexts.

## RRC Protocol API

### RRC State Management

#### `rrc_init()`
```c
uesim_error_t rrc_init(ue_context_t* ue_ctx);
```
Initialize RRC subsystem for UE.

**Description:**
Initializes the RRC state machine and prepares the UE for RRC procedures.

**Parameters:**
- `ue_ctx` - UE context to initialize

**Returns:**
- `UESIM_SUCCESS` - RRC initialized successfully
- `UESIM_ERROR_INVALID_PARAM` - Invalid parameter

**Thread Safety:**
Thread-safe when called with different UE contexts.

#### `rrc_cleanup()`
```c
void rrc_cleanup(ue_context_t* ue_ctx);
```
Cleanup RRC subsystem for UE.

**Description:**
Releases RRC resources and cleans up procedure contexts.

**Parameters:**
- `ue_ctx` - UE context to cleanup

**Returns:**
None

**Thread Safety:**
Thread-safe when called with different UE contexts.

### RRC State Transitions

#### `rrc_change_state()`
```c
uesim_error_t rrc_change_state(ue_context_t* ue_ctx, rrc_state_t new_state);
```
Change RRC state for UE.

**Description:**
Transitions the UE to the specified RRC state and updates state tracking information.

**Parameters:**
- `ue_ctx` - UE context to update
- `new_state` - Target RRC state

**Returns:**
- `UESIM_SUCCESS` - State changed successfully
- `UESIM_ERROR_INVALID_PARAM` - Invalid parameter

**Thread Safety:**
Thread-safe when called with different UE contexts.

#### `rrc_get_current_state()`
```c
rrc_state_t rrc_get_current_state(ue_context_t* ue_ctx);
```
Get current RRC state for UE.

**Description:**
Returns the current RRC state of the UE instance.

**Parameters:**
- `ue_ctx` - UE context to query

**Returns:**
- Current RRC state, or `RRC_STATE_MAX` on error

**Thread Safety:**
Thread-safe.

### RRC Message Handling

#### `rrc_send_message()`
```c
uesim_error_t rrc_send_message(ue_context_t* ue_ctx, rrc_message_t* message);
```
Send RRC message to gNB.

**Description:**
Encodes and transmits an RRC message to the gNB via the NGAP socket.

**Parameters:**
- `ue_ctx` - UE context to send message from
- `message` - RRC message to send

**Returns:**
- `UESIM_SUCCESS` - Message sent successfully
- `UESIM_ERROR_INVALID_PARAM` - Invalid parameter
- `UESIM_ERROR_SOCKET` - Transmission failed

**Thread Safety:**
Thread-safe when called with different UE contexts.

#### `rrc_receive_message()`
```c
uesim_error_t rrc_receive_message(ue_context_t* ue_ctx, rrc_message_t* message);
```
Receive RRC message from gNB.

**Description:**
Receives and decodes an RRC message from the gNB.

**Parameters:**
- `ue_ctx` - UE context to receive message for
- `message` - Structure to store received message

**Returns:**
- `UESIM_SUCCESS` - Message received successfully
- `UESIM_ERROR_INVALID_PARAM` - Invalid parameter
- `UESIM_ERROR_SOCKET` - Reception failed

**Thread Safety:**
Thread-safe when called with different UE contexts.

## CLI Interface API

### Command Processing

#### `cli_init()`
```c
uesim_error_t cli_init(void);
```
Initialize CLI subsystem.

**Description:**
Initializes the command-line interface subsystem and prepares for command processing.

**Parameters:**
None

**Returns:**
- `UESIM_SUCCESS` - CLI initialized successfully

**Thread Safety:**
Thread-safe.

#### `cli_cleanup()`
```c
void cli_cleanup(void);
```
Cleanup CLI subsystem.

**Description:**
Releases CLI resources and stops interactive mode if active.

**Parameters:**
None

**Returns:**
None

**Thread Safety:**
Thread-safe.

#### `cli_process_command()`
```c
uesim_error_t cli_process_command(const char* input);
```
Process CLI command string.

**Description:**
Parses and executes a CLI command string.

**Parameters:**
- `input` - Command string to process

**Returns:**
- `UESIM_SUCCESS` - Command processed successfully
- `UESIM_ERROR_INVALID_PARAM` - Invalid command

**Thread Safety:**
Thread-safe.

#### `cli_start_interactive_mode()`
```c
uesim_error_t cli_start_interactive_mode(void);
```
Start interactive CLI mode.

**Description:**
Enters interactive command-line mode, reading commands from stdin until exit.

**Parameters:**
None

**Returns:**
- `UESIM_SUCCESS` - Interactive mode completed
- `UESIM_ERROR_THREAD` - Thread creation failed

**Thread Safety:**
Not thread-safe. Only one interactive session allowed.

## Utility API

### Ring Buffer Operations

#### `ring_buffer_init()`
```c
uesim_error_t ring_buffer_init(ring_buffer_t* rb, size_t size);
```
Initialize ring buffer.

**Description:**
Creates and initializes a thread-safe ring buffer for IPC.

**Parameters:**
- `rb` - Ring buffer structure to initialize
- `size` - Size of buffer in bytes

**Returns:**
- `UESIM_SUCCESS` - Buffer initialized successfully
- `UESIM_ERROR_INVALID_PARAM` - Invalid parameter
- `UESIM_ERROR_MEMORY` - Memory allocation failed
- `UESIM_ERROR_THREAD` - Thread synchronization failed

**Thread Safety:**
Thread-safe.

#### `ring_buffer_destroy()`
```c
void ring_buffer_destroy(ring_buffer_t* rb);
```
Destroy ring buffer.

**Description:**
Releases ring buffer resources.

**Parameters:**
- `rb` - Ring buffer to destroy

**Returns:**
None

**Thread Safety:**
Thread-safe.

#### `ring_buffer_write()`
```c
uesim_error_t ring_buffer_write(ring_buffer_t* rb, const void* data, size_t length);
```
Write data to ring buffer.

**Description:**
Writes data to the ring buffer, blocking if buffer is full.

**Parameters:**
- `rb` - Ring buffer to write to
- `data` - Data to write
- `length` - Length of data

**Returns:**
- `UESIM_SUCCESS` - Data written successfully
- `UESIM_ERROR_INVALID_PARAM` - Invalid parameter
- `UESIM_ERROR_THREAD` - Synchronization error

**Thread Safety:**
Thread-safe. Multiple writers supported.

#### `ring_buffer_read()`
```c
uesim_error_t ring_buffer_read(ring_buffer_t* rb, void* data, size_t length);
```
Read data from ring buffer.

**Description:**
Reads data from the ring buffer, blocking if buffer is empty.

**Parameters:**
- `rb` - Ring buffer to read from
- `data` - Buffer to store read data
- `length` - Length of data to read

**Returns:**
- `UESIM_SUCCESS` - Data read successfully
- `UESIM_ERROR_INVALID_PARAM` - Invalid parameter
- `UESIM_ERROR_THREAD` - Synchronization error

**Thread Safety:**
Thread-safe. Multiple readers supported.

## Data Structures

### Core Structures

#### `ue_context_t`
```c
typedef struct {
    uint32_t ue_id;                    // Unique UE identifier
    rrc_state_t current_state;         // Current RRC state
    atomic_bool active;                // UE active status
    
    // Socket information
    int ngap_socket;                   // NGAP/SCTP socket
    int gtpu_socket;                   // GTP-U/UDP socket
    struct sockaddr_in gnb_addr;       // gNB address
    
    // Thread management
    pthread_t thread_id;               // UE thread
    pthread_mutex_t state_mutex;       // State protection
    pthread_cond_t state_cond;         // State signaling
    
    // Buffer management
    void* rx_buffer;                   // Receive buffer
    void* tx_buffer;                   // Transmit buffer
    size_t rx_buffer_size;             // RX buffer size
    size_t tx_buffer_size;             // TX buffer size
    
    // Configuration
    char imsi[16];                     // IMSI string
    char msisdn[16];                   // MSISDN string
    uint16_t tac;                      // Tracking Area Code
    uint32_t gnb_ip;                   // gNB IP address
    uint16_t gnb_port;                 // gNB port
} ue_context_t;
```

#### `rrc_message_t`
```c
typedef struct {
    rrc_message_type_t message_type;   // RRC message type
    uint32_t message_id;               // Unique message ID
    uint32_t transaction_id;           // Transaction identifier
    size_t data_length;                // Length of message data
    void* data;                        // Message data
} rrc_message_t;
```

### Enumerations

#### RRC States
```c
typedef enum {
    RRC_STATE_IDLE = 0,                // RRC idle state
    RRC_STATE_CONNECTED,               // RRC connected state
    RRC_STATE_INACTIVE,                // RRC inactive state
    RRC_STATE_MAX                      // Maximum state value
} rrc_state_t;
```

#### RRC Procedures
```c
typedef enum {
    RRC_PROC_REGISTRATION = 0,         // Registration procedure
    RRC_PROC_ESTABLISHMENT,            // Establishment procedure
    RRC_PROC_REESTABLISHMENT,          // Re-establishment procedure
    RRC_PROC_HANDOVER,                 // Handover procedure
    RRC_PROC_MAX                       // Maximum procedure value
} rrc_procedure_t;
```

#### Error Codes
```c
typedef enum {
    UESIM_SUCCESS = 0,                 // Success
    UESIM_ERROR_INVALID_PARAM = -1,    // Invalid parameter
    UESIM_ERROR_MEMORY = -2,           // Memory allocation error
    UESIM_ERROR_SOCKET = -3,           // Socket error
    UESIM_ERROR_THREAD = -4,           // Thread synchronization error
    UESIM_ERROR_TIMEOUT = -5,          // Timeout error
    UESIM_ERROR_PROTOCOL = -6          // Protocol error
} uesim_error_t;
```

## Constants

### Memory Layout Constants
```c
#define UESIM_STACK_SIZE    (8 * 1024 * 1024)    // 8MB per thread
#define UESIM_HEAP_SIZE     (64 * 1024 * 1024)   // 64MB heap
#define UESIM_DATA_SEGMENT  (16 * 1024 * 1024)   // 16MB data segment
```

### Limits
```c
#define MAX_UE_INSTANCES    1024                 // Maximum UE instances
#define MAX_BUFFER_SIZE     65536                // Maximum buffer size
#define MAX_RRC_PROCEDURES  32                   // Maximum procedures
```

## Thread Safety

All API functions are designed to be thread-safe when called with different UE contexts. Functions that modify shared global state use appropriate synchronization mechanisms including:

- Mutexes for resource protection
- Condition variables for signaling
- Atomic operations for lock-free programming
- Read-write locks for shared data access

## Error Handling

The API uses a consistent error handling approach with negative error codes. Functions return `UESIM_SUCCESS` (0) on success and negative values on error. Callers should always check return values and handle errors appropriately.

## Memory Management

The API provides custom memory allocation functions that use a memory pool for improved performance. Applications should use `uesim_malloc()`, `uesim_calloc()`, and `uesim_free()` instead of standard library functions for consistency and performance.

## Performance Considerations

- Memory pool allocation reduces malloc overhead
- Lock-free data structures where possible
- Efficient I/O multiplexing with epoll
- Thread pool for multi-UE handling
- Cache-friendly data layout

This API reference provides comprehensive documentation for integrating and extending the 5G UE Simulation application.