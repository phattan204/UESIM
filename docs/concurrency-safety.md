# Concurrency Safety Guidelines

## Lock Ordering (Deadlock Prevention)

To prevent deadlocks, all locks MUST be acquired in the following order:

### Lock Hierarchy (highest to lowest priority)

| Level | Lock Name | Description |
|-------|-----------|-------------|
| 1 | `g_init_mutex` | Global initialization mutex |
| 2 | `manager_mutex` | gNB manager mutex |
| 3 | `gnb_list_mutex` | UE gNB list mutex |
| 4 | `gnb_mutex` | Individual gNB context mutex |
| 5 | `state_mutex` | UE state mutex |
| 6 | `resource_lock` | Resource tracker lock |

### Rules

1. **Always acquire locks in order**: Never acquire a lower-numbered lock while holding a higher-numbered one
2. **Release in reverse order**: Release locks in the opposite order of acquisition
3. **Use try-lock when backtracking**: If you need multiple locks and can't guarantee order, use try-lock and release on failure
4. **Document lock dependencies**: Any function that acquires locks must document which locks it acquires

### Example: Correct Lock Acquisition

```c
/* Correct: Acquire in order */
uesim_lock_state(ue_ctx);           /* Level 5 */
uesim_lock_gnb(ue_ctx->serving_gnb); /* Level 4 - ERROR! Violates ordering */

/* Correct approach: Release higher lock first */
uesim_unlock_state(ue_ctx);          /* Release Level 5 */
uesim_lock_gnb(ue_ctx->serving_gnb); /* Acquire Level 4 */
uesim_lock_state(ue_ctx);           /* Re-acquire Level 5 */
```

### Example: Using try-lock for conditional acquisition

```c
/* Try to acquire second lock without blocking */
if (pthread_mutex_trylock(&gnb->gnb_mutex) != 0) {
    /* Failed to acquire - release first lock and retry in order */
    pthread_mutex_unlock(&ue_ctx->state_mutex);
    pthread_mutex_lock(&gnb->gnb_mutex);
    pthread_mutex_lock(&ue_ctx->state_mutex);
}
```

## State Transition Locking

### UE State Transitions

All UE state transitions MUST hold `state_mutex`:

| From State | To State | Required Locks |
|------------|----------|----------------|
| IDLE | CONNECTING | state_mutex |
| CONNECTING | CONNECTED | state_mutex, gnb_mutex |
| CONNECTED | HANDOVER_PREPARING | state_mutex, gnb_list_mutex |
| HANDOVER_PREPARING | CONNECTED | state_mutex, gnb_mutex |
| * | IDLE | state_mutex |

### gNB State Transitions

All gNB state transitions MUST hold `gnb_mutex`:

| From State | To State | Required Locks |
|------------|----------|----------------|
| DISCONNECTED | CONNECTING | gnb_mutex |
| CONNECTING | CONNECTED | gnb_mutex |
| CONNECTED | DISCONNECTED | gnb_mutex |
| CONNECTED | HANDOVER_CANDIDATE | gnb_mutex |

## Return Value Checking

### Disconnect Operations

All disconnect operations MUST check return values:

```c
uesim_error_t result = uesim_disconnect_gnb(ue_ctx, gnb_ctx);
if (result != UESIM_SUCCESS) {
    /* Log error but continue cleanup */
    fprintf(stderr, "Warning: disconnect failed with error %d\n", result);
    /* Continue with cleanup - don't return early */
}
```

### Socket Close Operations

```c
if (gnb_ctx->ngap_socket >= 0) {
    int result = uesim_sock_close(gnb_ctx->ngap_socket);
    if (result < 0) {
        fprintf(stderr, "Warning: socket close failed: %d\n", 
                #ifdef _WIN32
                WSAGetLastError()
                #else
                errno
                #endif
                );
    }
    gnb_ctx->ngap_socket = -1;
}
```

## Cleanup Patterns

### Standard goto cleanup Pattern

```c
uesim_error_t some_function(ue_context_t* ue_ctx) {
    uesim_error_t result = UESIM_SUCCESS;
    void* resource1 = NULL;
    int socket = -1;
    
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Allocate resources */
    resource1 = uesim_malloc(SIZE);
    if (resource1 == NULL) {
        result = UESIM_ERROR_MEMORY;
        goto cleanup;
    }
    
    socket = socket(AF_INET, SOCK_STREAM, 0);
    if (socket < 0) {
        result = UESIM_ERROR_SOCKET;
        goto cleanup;
    }
    
    /* Do work... */
    
cleanup:
    /* Cleanup in reverse order */
    if (socket >= 0) {
        uesim_sock_close(socket);
    }
    if (resource1 != NULL) {
        uesim_free(resource1);
    }
    
    return result;
}
```

## Thread Safety Annotations

Use these annotations in function declarations:

- `UESIM_THREAD_SAFE`: Function is fully thread-safe
- `UESIM_REQUIRES_LOCK(lock)`: Function requires specified lock to be held
- `UESIM_ACQUIRES_LOCK(lock)`: Function acquires specified lock
- `UESIM_RELEASES_LOCK(lock)`: Function releases specified lock

### Example

```c
/* Thread-safe function - no locks required */
UESIM_THREAD_SAFE
uint32_t uesim_get_ue_id(const ue_context_t* ctx);

/* Requires state_mutex to be held by caller */
UESIM_REQUIRES_LOCK(state_mutex)
uesim_error_t uesim_set_state(ue_context_t* ctx, rrc_state_t state);

/* Acquires and releases state_mutex internally */
UESIM_ACQUIRES_LOCK(state_mutex)
UESIM_RELEASES_LOCK(state_mutex)
uesim_error_t uesim_transition_state(ue_context_t* ctx, rrc_state_t new_state);
```

## Common Pitfalls

1. **Double-locking**: Never call a function that acquires a lock you already hold
2. **Lock leakage**: Always ensure locks are released on all error paths
3. **Callback locks**: Be careful with callbacks - they may acquire locks
4. **Signal handlers**: Never acquire locks in signal handlers

## Testing

Use the following to verify lock correctness:

1. Enable thread sanitizer: `CFLAGS=-fsanitize=thread`
2. Use lock debugging: `#define UESIM_DEBUG_LOCKS 1`
3. Run with helgrind: `valgrind --tool=helgrind ./uesim`
