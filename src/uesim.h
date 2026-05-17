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
#define MAX_GNB_CANDIDATES  8

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
    UESIM_ERROR_NOT_INITIALIZED = -8,
    UESIM_ERROR_NOT_FOUND = -9,
    UESIM_ERROR_ALREADY_EXISTS = -10,
    UESIM_ERROR_CAPACITY = -11,
    UESIM_ERROR_KEY_REFRESH_REQUIRED = -12,
    UESIM_ERROR_MAX_RETRIES = -13,
    UESIM_ERROR_RETRY = -14,
    UESIM_ERROR_INIT = -15,
    UESIM_ERROR_TEST_FAILED = -16
} uesim_error_t;

/* RRC States */
typedef enum {
    RRC_STATE_IDLE = 0,
    RRC_STATE_CONNECTED,
    RRC_STATE_INACTIVE,
    RRC_STATE_CONNECTING,
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

/* Forward declarations for layer contexts */
struct nas_ue_context_t;
struct rrc_state_context_t;
struct rrc_meas_context_t;
struct rrc_si_context_t;
struct mac_entity_t;
struct rlc_entity_t;
struct pdcp_entity_t;

/* Radio Bearer Constants */
#define UESIM_MAX_SRB           4
#define UESIM_MAX_DRB           32
#define UESIM_MAX_RLC_ENTITIES  36
#define UESIM_MAX_PDCP_ENTITIES 36
#define UESIM_MAX_UE_BANDS      16

/* gNB Types */
typedef enum {
    GNB_TYPE_OAI = 0,
    GNB_TYPE_SRSRAN,
    GNB_TYPE_COMMERCIAL,
    GNB_TYPE_MOCK,
    GNB_TYPE_MAX
} gnb_type_t;

/* gNB Connection States */
typedef enum {
    GNB_STATE_UNKNOWN = 0,
    GNB_STATE_CONNECTED,
    GNB_STATE_DISCONNECTED,
    GNB_STATE_HANDOVER_CANDIDATE,
    GNB_STATE_CONNECTING,
    GNB_STATE_MAX
} gnb_state_t;

/* gNB Context Structure */
typedef struct {
    uint32_t gnb_id;               /* gNB identifier */
    gnb_type_t type;               /* gNB type (OAI, srsRAN, etc.) */
    gnb_state_t state;             /* Connection state */
    struct sockaddr_in addr;       /* gNB address */
    int ngap_socket;               /* NGAP/SCTP socket */
    int gtpu_socket;               /* GTP-U/UDP socket */
    uint16_t cell_id;              /* Physical cell ID */
    uint16_t tac;                  /* Tracking area code */
    int32_t rsrp;                  /* Reference signal received power (dBm) */
    int32_t rsrq;                  /* Reference signal received quality (dB) */
    time_t connect_time;           /* Connection timestamp */
    time_t last_activity;          /* Last activity timestamp */
    bool is_serving;               /* True if this is the serving gNB */
    pthread_mutex_t gnb_mutex;     /* Per-gNB protection */
} gnb_context_t;

/* UE Capabilities Structure */
typedef struct {
    uint8_t ue_category;                              /* UE category for NR */
    uint8_t supported_bands[UESIM_MAX_UE_BANDS];      /* Supported frequency bands */
    uint8_t num_bands;                                /* Number of supported bands */
    bool nr_capability;                               /* NR capability flag */
    bool lte_capability;                              /* LTE capability flag */
    uint8_t pdcp_sn_lengths;                          /* Bitmap: bit0=5bit, bit1=12bit, bit2=18bit */
    bool supported_modulation_ul;                     /* 0=64QAM, 1=256QAM */
    bool supported_modulation_dl;                     /* 0=256QAM, 1=1024QAM */
} ue_capabilities_t;

/* Radio Bearer Configuration */
typedef struct {
    bool active;                /* Bearer is active */
    uint8_t bearer_id;          /* Bearer identity */
    uint8_t lcid;               /* Logical Channel ID */
    uint8_t priority;           /* Logical channel priority (1-16) */
    uint16_t prioritized_bit_rate; /* PBR in kbps */
    uint16_t bucket_size_duration; /* BSD in ms */
} ue_bearer_config_t;

/* DRX (Discontinuous Reception) Configuration */
typedef struct {
    bool enabled;                       /* DRX enabled flag */
    uint32_t on_duration_timer;         /* On duration timer in ms */
    uint32_t drx_inactivity_timer;      /* DRX inactivity timer in ms */
    uint32_t drx_retransmission_timer;  /* DRX retransmission timer in ms */
    uint32_t long_drx_cycle;            /* Long DRX cycle in ms */
    uint32_t short_drx_cycle;           /* Short DRX cycle in ms */
    uint16_t drx_short_cycle_count;     /* Number of short cycles before long */
    uint32_t drx_start_offset;          /* DRX start offset */
    bool use_short_drx;                 /* Use short DRX cycle */
} ue_drx_config_t;

/* RRC Timer States */
typedef struct {
    uint32_t t300_start_time;       /* T300 timer start time (ms) */
    uint32_t t301_start_time;       /* T301 timer start time (ms) */
    uint32_t t302_start_time;       /* T302 timer start time (ms) */
    uint32_t t304_start_time;       /* T304 timer start time (ms) */
    uint32_t t310_start_time;       /* T310 timer start time (ms) */
    uint32_t t311_start_time;       /* T311 timer start time (ms) */
    bool t300_running;              /* T300 running flag */
    bool t301_running;              /* T301 running flag */
    bool t302_running;              /* T302 running flag */
    bool t304_running;              /* T304 running flag */
    bool t310_running;              /* T310 running flag */
    bool t311_running;              /* T311 running flag */
} ue_rrc_timers_t;

/* UE Statistics */
typedef struct {
    uint64_t tx_bytes;              /* Total transmitted bytes */
    uint64_t rx_bytes;              /* Total received bytes */
    uint64_t tx_packets;            /* Total transmitted packets */
    uint64_t rx_packets;            /* Total received packets */
    uint64_t rrc_procedures_success;/* Successful RRC procedures */
    uint64_t rrc_procedures_failed; /* Failed RRC procedures */
    uint64_t handovers_success;     /* Successful handovers */
    uint64_t handovers_failed;      /* Failed handovers */
    uint64_t rach_attempts;         /* RACH attempts */
    uint64_t rach_success;          /* Successful RACH */
    time_t connection_start_time;   /* Connection start timestamp */
    time_t last_activity_time;      /* Last activity timestamp */
    uint32_t connection_duration_s; /* Total connection duration in seconds */
} ue_stats_t;

/* UE Context Structure */
typedef struct {
    /* ========== Core Identity & State ========== */
    uint32_t ue_id;
    rrc_state_t current_state;
    
    /* Platform-specific atomic handling */
#ifdef _WIN32
    volatile LONG active;
#else
    atomic_bool active;
#endif
    
    /* ========== gNB Connection Management ========== */
    gnb_context_t* serving_gnb;                          /* Current serving gNB */
    gnb_context_t* candidate_gnbs[MAX_GNB_CANDIDATES];   /* Handover candidates */
    uint8_t num_candidate_gnbs;                          /* Number of candidate gNBs */
    pthread_mutex_t gnb_list_mutex;                      /* gNB list protection */
    
    /* Legacy single-gNB fields (backward compat, derived from serving_gnb) */
    int ngap_socket;
    int gtpu_socket;
    struct sockaddr_in gnb_addr;
    
    /* ========== Thread Management ========== */
    pthread_t thread_id;
    pthread_mutex_t state_mutex;
    pthread_cond_t state_cond;
    
    /* ========== Buffer Management ========== */
    void* rx_buffer;
    void* tx_buffer;
    size_t rx_buffer_size;
    size_t tx_buffer_size;
    
    /* ========== UE Identity Configuration ========== */
    char imsi[16];
    char msisdn[16];
    uint16_t tac;
    uint32_t gnb_ip;
    uint16_t gnb_port;
    
    /* ========== State Tracking ========== */
    time_t state_change_time;
    
    /* ========== Layer Contexts ========== */
    struct nas_ue_context_t* nas_ctx;                    /* NAS layer context */
    struct rrc_state_context_t* rrc_state_ctx;           /* RRC state management context */
    struct rrc_meas_context_t* rrc_meas_ctx;             /* RRC measurement context */
    struct rrc_si_context_t* rrc_si_ctx;                 /* RRC SI context */
    struct phy_context_t* phy_ctx;                       /* PHY layer context */
    
    /* ========== Protocol Layer Entities ========== */
    struct mac_entity_t* mac_entity;                     /* Primary MAC entity */
    struct rlc_entity_t* rlc_entities[UESIM_MAX_RLC_ENTITIES];   /* RLC entities per bearer */
    struct pdcp_entity_t* pdcp_entities[UESIM_MAX_PDCP_ENTITIES]; /* PDCP entities per bearer */
    uint8_t num_active_rlc_entities;                     /* Count of active RLC entities */
    uint8_t num_active_pdcp_entities;                    /* Count of active PDCP entities */
    
    /* ========== Radio Bearer Configuration ========== */
    ue_bearer_config_t srb_config[UESIM_MAX_SRB];        /* SRB configurations (SRB0-3) */
    ue_bearer_config_t drb_config[UESIM_MAX_DRB];        /* DRB configurations (DRB1-32) */
    uint8_t num_active_srbs;                             /* Number of active SRBs */
    uint8_t num_active_drbs;                             /* Number of active DRBs */
    
    /* ========== UE Capabilities ========== */
    ue_capabilities_t capabilities;                       /* UE capability information */
    
    /* ========== DRX Configuration ========== */
    ue_drx_config_t drx_config;                          /* DRX settings */
    
    /* ========== RRC Timer States ========== */
    ue_rrc_timers_t rrc_timers;                          /* RRC procedure timers */
    
    /* ========== Statistics ========== */
    ue_stats_t stats;                                    /* UE performance statistics */
    
} ue_context_t;

/* Forward declaration for thread pool (defined in thread_pool.h) */
typedef struct thread_pool thread_pool_t;

/* Function prototypes */
uesim_error_t uesim_init(void);
uesim_error_t uesim_create_ue_instance(ue_context_t** ue_ctx);
uesim_error_t uesim_start_ue(ue_context_t* ue_ctx);
uesim_error_t uesim_stop_ue(ue_context_t* ue_ctx);
uesim_error_t uesim_execute_procedure(ue_context_t* ue_ctx, rrc_procedure_t procedure);
void uesim_cleanup(void);

/* Multi-gNB management functions */
uesim_error_t uesim_add_gnb(ue_context_t* ue_ctx, gnb_type_t type,
                            const char* ip, uint16_t port, gnb_context_t** gnb_ctx);
uesim_error_t uesim_remove_gnb(ue_context_t* ue_ctx, gnb_context_t* gnb_ctx);
uesim_error_t uesim_switch_serving_gnb(ue_context_t* ue_ctx, gnb_context_t* new_gnb);
uesim_error_t uesim_connect_gnb(ue_context_t* ue_ctx, gnb_context_t* gnb_ctx);
uesim_error_t uesim_disconnect_gnb(ue_context_t* ue_ctx, gnb_context_t* gnb_ctx);
gnb_context_t* uesim_find_gnb_by_id(ue_context_t* ue_ctx, uint32_t gnb_id);
gnb_context_t* uesim_get_serving_gnb(ue_context_t* ue_ctx);
uint8_t uesim_get_candidate_gnb_count(ue_context_t* ue_ctx);
const char* uesim_gnb_type_str(gnb_type_t type);
const char* uesim_gnb_state_str(gnb_state_t state);

/* Memory management functions */
void* uesim_malloc(size_t size);
void uesim_free(void* ptr);
void* uesim_calloc(size_t nmemb, size_t size);
void* uesim_realloc(void* ptr, size_t size);

/* Thread-safe functions */
uesim_error_t uesim_lock_state(ue_context_t* ue_ctx);
uesim_error_t uesim_unlock_state(ue_context_t* ue_ctx);
uesim_error_t uesim_wait_for_state_change(ue_context_t* ue_ctx, rrc_state_t expected_state);

/* ========== Layer Context Accessor Functions ========== */

/* NAS context access */
struct nas_ue_context_t* ue_get_nas_context(ue_context_t* ue_ctx);
uesim_error_t ue_set_nas_context(ue_context_t* ue_ctx, struct nas_ue_context_t* nas_ctx);

/* RRC state context access */
struct rrc_state_context_t* ue_get_rrc_state_context(ue_context_t* ue_ctx);
uesim_error_t ue_set_rrc_state_context(ue_context_t* ue_ctx, struct rrc_state_context_t* rrc_ctx);

/* RRC measurement context access */
struct rrc_meas_context_t* ue_get_rrc_meas_context(ue_context_t* ue_ctx);
uesim_error_t ue_set_rrc_meas_context(ue_context_t* ue_ctx, struct rrc_meas_context_t* meas_ctx);

/* PHY context access */
struct phy_context_t* ue_get_phy_context(ue_context_t* ue_ctx);
uesim_error_t ue_set_phy_context(ue_context_t* ue_ctx, struct phy_context_t* phy_ctx);

/* MAC entity access */
struct mac_entity_t* ue_get_mac_entity(ue_context_t* ue_ctx);
uesim_error_t ue_set_mac_entity(ue_context_t* ue_ctx, struct mac_entity_t* mac_entity);

/* RLC entity access by bearer type */
struct rlc_entity_t* ue_get_rlc_entity(ue_context_t* ue_ctx, uint8_t bearer_id);
uesim_error_t ue_set_rlc_entity(ue_context_t* ue_ctx, uint8_t bearer_id, struct rlc_entity_t* rlc_entity);
uesim_error_t ue_remove_rlc_entity(ue_context_t* ue_ctx, uint8_t bearer_id);

/* PDCP entity access by bearer type */
struct pdcp_entity_t* ue_get_pdcp_entity(ue_context_t* ue_ctx, uint8_t bearer_id);
uesim_error_t ue_set_pdcp_entity(ue_context_t* ue_ctx, uint8_t bearer_id, struct pdcp_entity_t* pdcp_entity);
uesim_error_t ue_remove_pdcp_entity(ue_context_t* ue_ctx, uint8_t bearer_id);

/* Radio Bearer configuration */
uesim_error_t ue_configure_srb(ue_context_t* ue_ctx, uint8_t srb_id, uint8_t lcid, uint8_t priority);
uesim_error_t ue_configure_drb(ue_context_t* ue_ctx, uint8_t drb_id, uint8_t lcid, uint8_t priority,
                               uint16_t pbr, uint16_t bsd);
uesim_error_t ue_remove_bearer(ue_context_t* ue_ctx, uint8_t bearer_id);
ue_bearer_config_t* ue_get_bearer_config(ue_context_t* ue_ctx, uint8_t bearer_id);

/* DRX configuration */
uesim_error_t ue_configure_drx(ue_context_t* ue_ctx, const ue_drx_config_t* drx_config);
uesim_error_t ue_disable_drx(ue_context_t* ue_ctx);

/* UE Capabilities */
uesim_error_t ue_set_capabilities(ue_context_t* ue_ctx, const ue_capabilities_t* caps);
const ue_capabilities_t* ue_get_capabilities(ue_context_t* ue_ctx);

/* Statistics */
uesim_error_t ue_update_stats(ue_context_t* ue_ctx);
uesim_error_t ue_reset_stats(ue_context_t* ue_ctx);
const ue_stats_t* ue_get_stats(ue_context_t* ue_ctx);

/* UE Registry Accessors (for I/O thread) */
ue_context_t** uesim_get_ue_instances(void);
int uesim_get_ue_instance_count(void);
int uesim_get_active_ue_count(void);

/* Platform-specific initialization */
#ifdef _WIN32
int windows_init_sockets(void);
void windows_cleanup_sockets(void);
#endif

#endif /* UESIM_H */
