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
#include <time.h>

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
    
    /* pthread mutex wrappers */
    static int pthread_mutex_init(pthread_mutex_t* mutex, void* attr) {
        (void)attr;
        *mutex = CreateMutex(NULL, FALSE, NULL);
        return (*mutex == NULL) ? -1 : 0;
    }
    static int pthread_mutex_destroy(pthread_mutex_t* mutex) {
        return CloseHandle(*mutex) ? 0 : -1;
    }
    static int pthread_mutex_lock(pthread_mutex_t* mutex) {
        return (WaitForSingleObject(*mutex, INFINITE) == WAIT_OBJECT_0) ? 0 : -1;
    }
    static int pthread_mutex_unlock(pthread_mutex_t* mutex) {
        return ReleaseMutex(*mutex) ? 0 : -1;
    }
    
    /* pthread cond wrappers */
    static int pthread_cond_init(pthread_cond_t* cond, void* attr) {
        (void)attr;
        *cond = CreateEvent(NULL, FALSE, FALSE, NULL);
        return (*cond == NULL) ? -1 : 0;
    }
    static int pthread_cond_destroy(pthread_cond_t* cond) {
        return CloseHandle(*cond) ? 0 : -1;
    }
    static int pthread_cond_signal(pthread_cond_t* cond) {
        return SetEvent(*cond) ? 0 : -1;
    }
    static int pthread_cond_broadcast(pthread_cond_t* cond) {
        return SetEvent(*cond) ? 0 : -1;
    }
    static int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex) {
        ReleaseMutex(*mutex);
        WaitForSingleObject(*cond, INFINITE);
        WaitForSingleObject(*mutex, INFINITE);
        return 0;
    }
    
    /* pthread thread wrappers */
    static int pthread_create(pthread_t* thread, void* attr, void* (*start_routine)(void*), void* arg) {
        (void)attr;
        *thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)start_routine, arg, 0, NULL);
        return (*thread == NULL) ? -1 : 0;
    }
    static int pthread_join(pthread_t thread, void** retval) {
        (void)retval;
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
        return 0;
    }
    
    /* Windows socket/compat wrappers */
    static int uesim_sock_close(int sock) { return closesocket(sock); }
    static void uesim_sleep(unsigned int sec) { Sleep(sec * 1000); }
    
    /* Atomic operations for Windows */
    static int atomic_fetch_add(volatile LONG* obj, int arg) {
        return InterlockedExchangeAdd(obj, arg);
    }
    static void atomic_init(volatile LONG* obj, int val) {
        InterlockedExchange(obj, val);
    }
    static int atomic_load(volatile LONG* obj) {
        return InterlockedCompareExchange(obj, 0, 0);
    }
    static void atomic_store(volatile LONG* obj, int val) {
        InterlockedExchange(obj, val);
    }
    static int atomic_fetch_sub(volatile LONG* obj, int arg) {
        return InterlockedExchangeAdd(obj, -arg);
    }
    
#else
    #include <unistd.h>
    #include <pthread.h>
    #include <semaphore.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <stdatomic.h>
    #include <strings.h>
    
    /* Unix wrappers matching Windows API */
    static inline int uesim_sock_close(int sock) { return close(sock); }
    static inline void uesim_sleep(unsigned int sec) { sleep(sec); }
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
    UESIM_ERROR_PROTOCOL = -6,
    UESIM_ERROR_FILE = -7,
    UESIM_ERROR_NOT_INITIALIZED = -8
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
    
    /* State tracking */
    time_t state_change_time;
    
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
void* uesim_realloc(void* ptr, size_t size);

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