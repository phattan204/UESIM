/*
 * 5G UE Simulation Application
 * Header file for core structures and definitions
 */

#ifndef UESIM_H
#define UESIM_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

/* Platform-specific includes */
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <process.h>
    
    /* Windows compatibility definitions */
    typedef HANDLE pthread_t;
    typedef HANDLE pthread_mutex_t;
    typedef HANDLE pthread_cond_t;
    typedef LONG atomic_int;
    typedef LONG atomic_uint;
    typedef LONG atomic_bool;
    typedef LONG atomic_size_t;
    
    #define pthread_mutex_lock(mutex) WaitForSingleObject(*(mutex), INFINITE)
    #define pthread_mutex_unlock(mutex) ReleaseMutex(*(mutex))
    #define pthread_cond_wait(cond, mutex) WaitForSingleObject(*(cond), INFINITE)
    #define pthread_cond_signal(cond) SetEvent(*(cond))
    #define pthread_cond_broadcast(cond) SetEvent(*(cond))
    
    /* Windows socket compatibility */
    #define close(sock) closesocket(sock)
    #define sleep(sec) Sleep((sec) * 1000)
    
#else
    #include <unistd.h>
    #include <pthread.h>
    #include <semaphore.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <stdatomic.h>
#endif

/* Memory layout constants */
#define UESIM_STACK_SIZE    (8 * 1024 * 1024)  /* 8MB stack per thread */
#define UESIM_HEAP_SIZE     (64 * 1024 * 1024) /* 64MB heap for main process */
#define UESIM_DATA_SEGMENT  (16 * 1024 * 1024) /* 16MB for data segment */

/* Maximum limits */
#define MAX_UE_INSTANCES    1024
#define MAX_BUFFER_SIZE     65536
#define MAX_RRC_PROCEDURES  32

/* Error codes */
typedef enum {
    UESIM_SUCCESS = 0,
    UESIM_ERROR_INVALID_PARAM = -1,
    UESIM_ERROR_MEMORY = -2,
    UESIM_ERROR_SOCKET = -3,
    UESIM_ERROR_THREAD = -4,
    UESIM_ERROR_TIMEOUT = -5,
    UESIM_ERROR_PROTOCOL = -6
} uesim_error_t;

/* RRC States */
typedef enum {
    RRC_STATE_IDLE = 0,
    RRC_STATE_CONNECTED,
    RRC_STATE_INACTIVE,
    RRC_STATE_MAX
} rrc_state_t;

/* RRC Procedures */
typedef enum {
    RRC_PROC_REGISTRATION = 0,
    RRC_PROC_ESTABLISHMENT,
    RRC_PROC_REESTABLISHMENT,
    RRC_PROC_HANDOVER,
    RRC_PROC_MAX
} rrc_procedure_t;

/* UE Context Structure */
typedef struct {
    uint32_t ue_id;
    rrc_state_t current_state;
    
    /* Platform-specific atomic handling */
#ifdef _WIN32
    volatile LONG active;
#else
    atomic_bool active;
#endif
    
    /* Socket information */
    int ngap_socket;
    int gtpu_socket;
    struct sockaddr_in gnb_addr;
    
    /* Thread management */
    pthread_t thread_id;
    pthread_mutex_t state_mutex;
    pthread_cond_t state_cond;
    
    /* Buffer management */
    void* rx_buffer;
    void* tx_buffer;
    size_t rx_buffer_size;
    size_t tx_buffer_size;
    
    /* Configuration */
    char imsi[16];
    char msisdn[16];
    uint16_t tac;
    uint32_t gnb_ip;
    uint16_t gnb_port;
    
} ue_context_t;

/* Thread Pool Structure */
typedef struct {
    pthread_t* threads;
    uint32_t thread_count;
    struct task_queue* task_queue;
    pthread_mutex_t queue_mutex;
    pthread_cond_t queue_cond;
    
    /* Platform-specific atomic handling */
#ifdef _WIN32
    volatile LONG shutdown;
#else
    atomic_bool shutdown;
#endif
} thread_pool_t;

/* Ring Buffer for IPC */
typedef struct {
    uint8_t* buffer;
    size_t size;
    
    /* Platform-specific atomic handling */
#ifdef _WIN32
    volatile LONG head;
    volatile LONG tail;
#else
    atomic_size_t head;
    atomic_size_t tail;
#endif
    
    pthread_mutex_t head_lock;
    pthread_mutex_t tail_lock;
} ring_buffer_t;

/* Function prototypes */
uesim_error_t uesim_init(void);
uesim_error_t uesim_create_ue_instance(ue_context_t** ue_ctx);
uesim_error_t uesim_start_ue(ue_context_t* ue_ctx);
uesim_error_t uesim_stop_ue(ue_context_t* ue_ctx);
uesim_error_t uesim_execute_procedure(ue_context_t* ue_ctx, rrc_procedure_t procedure);
void uesim_cleanup(void);

/* Memory management functions */
void* uesim_malloc(size_t size);
void uesim_free(void* ptr);
void* uesim_calloc(size_t nmemb, size_t size);

/* Thread-safe functions */
uesim_error_t uesim_lock_state(ue_context_t* ue_ctx);
uesim_error_t uesim_unlock_state(ue_context_t* ue_ctx);
uesim_error_t uesim_wait_for_state_change(ue_context_t* ue_ctx, rrc_state_t expected_state);

/* Platform-specific initialization */
#ifdef _WIN32
int windows_init_sockets(void);
void windows_cleanup_sockets(void);
#endif

#endif /* UESIM_H */

</final_file_content>

IMPORTANT: For any future changes to this file, use the final_file_content shown above as your reference. This content reflects the current state of the file, including any auto-formatting (e.g., if you used single quotes but the formatter converted them to double quotes). Always base your SEARCH/REPLACE operations on this final version to ensure accuracy.

 

				# TODO LIST UPDATE REQUIRED - You MUST include the task_progress parameter in your NEXT tool call.

				**Current Progress: 12/13 items completed (92%)**

				- [x] Plan gNB integration testing framework
- [x] Plan O-RAN architecture documentation
- [x] Plan Wireshark capture and analysis guide
- [x] Plan enhanced RRC procedure testing
- [x] Plan cross-component test framework
- [x] Plan test execution and validation
- [x] Implement gNB integration testing solution
- [x] Create documentation for gNB integration testing
- [x] Create OAI-O-RAN test implementation
- [x] Create Commercial O-RAN test implementation
- [x] Update Makefile with new test targets
- [x] Create test runner script
- [ ] Verify all components work together

				

				
1. To create or update a todo list, include the task_progress parameter in the next tool call
2. Review each item and update its status:
   - Mark completed items with: - [x]
   - Keep incomplete items as: - [ ]
   - Add new items if you discover additional steps
3. Modify the list as needed:
		- Add any new steps you've discovered
		- Reorder if the sequence has changed
4. Ensure the list accurately reflects the current state

**Remember:** Keeping the task_progress list updated helps track progress and ensures nothing is missed.

				

**Note:** 92% of items are complete! Focus on finishing the remaining items.

				 <environment_details>
# Visual Studio Code Visible Files
src/uesim.h

# Visual Studio Code Open Tabs
compress.mk
config.mk
src/uesim.h
rules.mk
src/core/memory.c
src/core/uesim_core.c
src/core/memory.h
src/transport/socket_mgr.c
src/transport/socket_mgr.h
src/protocol/rrc.h
src/protocol/rrc.c
src/utils/ring_buffer.h
src/utils/ring_buffer.c
README.md
etc/uesim.conf
LICENSE
docs/architecture.md
docs/api.md
CONTRIBUTING.md
MAINTAINERS.md
SECURITY.md
tests/test_build.c
src/config/config.h
src/config/config.c
tests/test_config.c
src/main.c
src/cli/cli.h
tests/test_cli.c
src/benchmark/benchmark.h
src/benchmark/benchmark.c
tests/test_benchmark.c
tests/test_runner.c
src/nas/nas.h
src/nas/nas.c
tests/test_nas_pdu.c
CHANGELOG.md
src/cli/cli.c
src/protocol/mac.h
src/protocol/mac.c
tests/test_mac.c
docs/testing_mechanism.md
docs/gnb_integration_testing.md
tests/test_gnb_oai.c
tests/test_gnb_oran.c
Makefile
run_gnb_integration_tests.sh
tests/test_nas.c

# Current Time
4/14/2026, 9:46:43 AM (Asia/Bangkok, UTC+7:00)

# Context Window Usage
180,723 / 256K tokens used (71%)

# Current Mode
ACT MODE

</final_file_content>

IMPORTANT: For any future changes to this file, use the final_file_content shown above as your reference. This content reflects the current state of the file, including any auto-formatting (e.g., if you used single quotes but the formatter converted them to double quotes). Always base your SEARCH/REPLACE operations on this final version to ensure accuracy.

 

				# TODO LIST UPDATE REQUIRED - You MUST include the task_progress parameter in your NEXT tool call.

				**Current Progress: 12/13 items completed (92%)**

				- [x] Plan gNB integration testing framework
- [x] Plan O-RAN architecture documentation
- [x] Plan Wireshark capture and analysis guide
- [x] Plan enhanced RRC procedure testing
- [x] Plan cross-component test framework
- [x] Plan test execution and validation
- [x] Implement gNB integration testing solution
- [x] Create documentation for gNB integration testing
- [x] Create OAI-O-RAN test implementation
- [x] Create Commercial O-RAN test implementation
- [x] Update Makefile with new test targets
- [x] Create test runner script
- [ ] Verify all components work together

				

				
1. To create or update a todo list, include the task_progress parameter in the next tool call
2. Review each item and update its status:
   - Mark completed items with: - [x]
   - Keep incomplete items as: - [ ]
   - Add new items if you discover additional steps
3. Modify the list as needed:
		- Add any new steps you've discovered
		- Reorder if the sequence has changed
4. Ensure the list accurately reflects the current state

**Remember:** Keeping the task_progress list updated helps track progress and ensures nothing is missed.

				

**Note:** 92% of items are complete! Focus on finishing the remaining items.

				 <environment_details>
# Visual Studio Code Visible Files
src/uesim.h

# Visual Studio Code Open Tabs
compress.mk
config.mk
rules.mk
src/uesim.h
src/core/memory.c
src/core/uesim_core.c
src/core/memory.h
src/transport/socket_mgr.c
src/transport/socket_mgr.h
src/protocol/rrc.h
src/protocol/rrc.c
src/utils/ring_buffer.h
src/utils/ring_buffer.c
README.md
etc/uesim.conf
LICENSE
docs/architecture.md
docs/api.md
CONTRIBUTING.md
MAINTAINERS.md
SECURITY.md
tests/test_build.c
src/config/config.h
src/config/config.c
tests/test_config.c
src/main.c
src/cli/cli.h
tests/test_cli.c
src/benchmark/benchmark.h
src/benchmark/benchmark.c
tests/test_benchmark.c
tests/test_runner.c
src/nas/nas.h
src/nas/nas.c
tests/test_nas_pdu.c
CHANGELOG.md
src/cli/cli.c
src/protocol/mac.h
src/protocol/mac.c
tests/test_mac.c
docs/testing_mechanism.md
docs/gnb_integration_testing.md
tests/test_gnb_oai.c
tests/test_gnb_oran.c
Makefile
run_gnb_integration_tests.sh
tests/test_nas.c

# Current Time
4/14/2026, 9:45:23 AM (Asia/Bangkok, UTC+7:00)

# Context Window Usage
176,556 / 256K tokens used (69%)

# Current Mode
ACT MODE
