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
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdatomic.h>

// Memory layout constants
#define UESIM_STACK_SIZE    (8 * 1024 * 1024)  // 8MB stack per thread
#define UESIM_HEAP_SIZE     (64 * 1024 * 1024) // 64MB heap for main process
#define UESIM_DATA_SEGMENT  (16 * 1024 * 1024) // 16MB for data segment

// Maximum limits
#define MAX_UE_INSTANCES    1024
#define MAX_BUFFER_SIZE     65536
#define MAX_RRC_PROCEDURES  32

// Error codes
typedef enum {
    UESIM_SUCCESS = 0,
    UESIM_ERROR_INVALID_PARAM = -1,
    UESIM_ERROR_MEMORY = -2,
    UESIM_ERROR_SOCKET = -3,
    UESIM_ERROR_THREAD = -4,
    UESIM_ERROR_TIMEOUT = -5,
    UESIM_ERROR_PROTOCOL = -6
} uesim_error_t;

// RRC States
typedef enum {
    RRC_STATE_IDLE = 0,
    RRC_STATE_CONNECTED,
    RRC_STATE_INACTIVE,
    RRC_STATE_MAX
} rrc_state_t;

// RRC Procedures
typedef enum {
    RRC_PROC_REGISTRATION = 0,
    RRC_PROC_ESTABLISHMENT,
    RRC_PROC_REESTABLISHMENT,
    RRC_PROC_HANDOVER,
    RRC_PROC_MAX
} rrc_procedure_t;

// UE Context Structure
typedef struct {
    uint32_t ue_id;
    rrc_state_t current_state;
    atomic_bool active;
    
    // Socket information
    int ngap_socket;
    int gtpu_socket;
    struct sockaddr_in gnb_addr;
    
    // Thread management
    pthread_t thread_id;
    pthread_mutex_t state_mutex;
    pthread_cond_t state_cond;
    
    // Buffer management
    void* rx_buffer;
    void* tx_buffer;
    size_t rx_buffer_size;
    size_t tx_buffer_size;
    
    // Configuration
    char imsi[16];
    char msisdn[16];
    uint16_t tac;
    uint32_t gnb_ip;
    uint16_t gnb_port;
    
} ue_context_t;

// Thread Pool Structure
typedef struct {
    pthread_t* threads;
    uint32_t thread_count;
    struct task_queue* task_queue;
    pthread_mutex_t queue_mutex;
    pthread_cond_t queue_cond;
    atomic_bool shutdown;
} thread_pool_t;

// Ring Buffer for IPC
typedef struct {
    uint8_t* buffer;
    size_t size;
    atomic_size_t head;
    atomic_size_t tail;
    pthread_mutex_t head_lock;
    pthread_mutex_t tail_lock;
} ring_buffer_t;

// Function prototypes
uesim_error_t uesim_init(void);
uesim_error_t uesim_create_ue_instance(ue_context_t** ue_ctx);
uesim_error_t uesim_start_ue(ue_context_t* ue_ctx);
uesim_error_t uesim_stop_ue(ue_context_t* ue_ctx);
uesim_error_t uesim_execute_procedure(ue_context_t* ue_ctx, rrc_procedure_t procedure);
void uesim_cleanup(void);

// Memory management functions
void* uesim_malloc(size_t size);
void uesim_free(void* ptr);
void* uesim_calloc(size_t nmemb, size_t size);

// Thread-safe functions
uesim_error_t uesim_lock_state(ue_context_t* ue_ctx);
uesim_error_t uesim_unlock_state(ue_context_t* ue_ctx);
uesim_error_t uesim_wait_for_state_change(ue_context_t* ue_ctx, rrc_state_t expected_state);

#endif // UESIM_H