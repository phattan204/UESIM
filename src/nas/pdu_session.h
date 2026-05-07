/*
 * 5G UE Simulation Application
 * PDU Session Management Header
 */

#ifndef PDU_SESSION_H
#define PDU_SESSION_H

#include "../uesim.h"
#include "qos_flow.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* PDU Session constants */
#define PDU_MAX_SESSIONS_PER_UE      16
#define PDU_MAX_DNS_SERVERS         2
#define PDU_MAX_SNSSAI_LEN           8
#define PDU_MAX_DRBS                 8

/* PDU Session Type */
typedef enum {
    PDU_SESSION_TYPE_IPV4 = 1,
    PDU_SESSION_TYPE_IPV6 = 2,
    PDU_SESSION_TYPE_IPV4V6 = 3,
    PDU_SESSION_TYPE_ETHERNET = 4,
    PDU_SESSION_TYPE_MAX
} pdu_session_type_t;

/* SSC Mode (Service and Session Continuity) */
typedef enum {
    SSC_MODE_1 = 1,    /* Session continuity maintained */
    SSC_MODE_2 = 2,    /* Network can release session */
    SSC_MODE_3 = 3,    /* UE can request new session */
    SSC_MODE_MAX
} ssc_mode_t;

/* PDU Session State */
typedef enum {
    PDU_SESSION_STATE_INACTIVE = 0,
    PDU_SESSION_STATE_CREATING,
    PDU_SESSION_STATE_ACTIVE,
    PDU_SESSION_STATE_MODIFYING,
    PDU_SESSION_STATE_RELEASING,
    PDU_SESSION_STATE_SUSPENDED,
    PDU_SESSION_STATE_MAX
} pdu_session_state_t;

/* Selection Mode */
typedef enum {
    SELECTION_MODE_UE = 0,           /* UE requested */
    SELECTION_MODE_NETWORK = 1,      /* Network selected */
    SELECTION_MODE_UE_V4_V6 = 2      /* UE requested IPv4v6 */
} selection_mode_t;

/* S-NSSAI (Network Slice Selection) */
typedef struct {
    uint8_t sst;                      /* Slice/Service Type */
    uint8_t sd[PDU_MAX_SNSSAI_LEN];  /* Slice Differentiator */
    uint8_t sd_len;
} snssai_t;

/* PDU Address */
typedef struct {
    pdu_session_type_t type;
    uint32_t ipv4_addr;              /* Network byte order */
    uint8_t ipv6_addr[16];           /* IPv6 address */
    uint8_t ipv6_prefix_len;
    uint8_t ethernet_addr[6];        /* MAC address for Ethernet type */
} pdu_address_t;

/* PDU Session Configuration */
typedef struct {
    pdu_session_type_t session_type;
    ssc_mode_t ssc_mode;
    selection_mode_t selection_mode;
    snssai_t snssai;
    pdu_address_t pdu_address;
    
    /* AMBR */
    uint64_t session_ambr_ul;        /* kbps */
    uint64_t session_ambr_dl;        /* kbps */
    
    /* Always-on PDU session */
    bool always_on_requested;
    bool always_on_granted;
    
    /* DNS */
    uint32_t dns_servers[PDU_MAX_DNS_SERVERS];
    uint8_t num_dns_servers;
} pdu_session_config_t;

/* PDU Session Statistics */
typedef struct {
    uint64_t ul_packets;
    uint64_t dl_packets;
    uint64_t ul_bytes;
    uint64_t dl_bytes;
    uint64_t ul_dropped;
    uint64_t dl_dropped;
    time_t last_ul_activity;
    time_t last_dl_activity;
} pdu_session_stats_t;

/* PDU Session */
typedef struct {
    uint8_t session_id;              /* PDU Session ID (1-15) */
    pdu_session_state_t state;
    pdu_session_config_t config;
    
    /* QoS Flows */
    qos_flow_manager_t* qos_manager;
    uint8_t default_qos_flow_qfi;
    
    /* DRB Mapping */
    uint8_t drb_ids[PDU_MAX_DRBS];
    uint8_t num_drbs;
    
    /* Statistics */
    pdu_session_stats_t stats;
    
    /* Timing */
    time_t create_time;
    time_t last_activity;
    time_t modify_time;
    
    /* NAS transaction */
    uint8_t transaction_id;
    uint8_t procedure_transaction_id;
    
    pthread_mutex_t session_mutex;
} pdu_session_t;

/* PDU Session Manager (per UE) */
typedef struct {
    pdu_session_t sessions[PDU_MAX_SESSIONS_PER_UE];
    uint8_t num_sessions;
    uint8_t next_session_id;
    
    /* Default settings */
    pdu_session_type_t default_session_type;
    ssc_mode_t default_ssc_mode;
    snssai_t default_snssai;
    
    pthread_mutex_t manager_mutex;
} pdu_session_manager_t;

/* ============== Initialization ============== */

uesim_error_t pdu_session_init(void);
void pdu_session_cleanup(void);

/* ============== Manager Operations ============== */

uesim_error_t pdu_session_create_manager(pdu_session_manager_t** manager);
uesim_error_t pdu_session_destroy_manager(pdu_session_manager_t* manager);

/* ============== Session Operations ============== */

uesim_error_t pdu_session_create(pdu_session_manager_t* manager,
                                  pdu_session_type_t type,
                                  ssc_mode_t ssc_mode,
                                  const snssai_t* snssai,
                                  pdu_session_t** session);

uesim_error_t pdu_session_release(pdu_session_manager_t* manager, uint8_t session_id);
uesim_error_t pdu_session_modify(pdu_session_manager_t* manager, uint8_t session_id,
                                  const pdu_session_config_t* new_config);

uesim_error_t pdu_session_activate(pdu_session_manager_t* manager, uint8_t session_id);
uesim_error_t pdu_session_suspend(pdu_session_manager_t* manager, uint8_t session_id);
uesim_error_t pdu_session_resume(pdu_session_manager_t* manager, uint8_t session_id);

/* ============== QoS Flow Integration ============== */

uesim_error_t pdu_session_add_qos_flow(pdu_session_t* session,
                                         uint8_t five_qi,
                                         const arp_t* arp,
                                         const bit_rate_t* gbr,
                                         const bit_rate_t* mbr,
                                         uint8_t* qfi);

uesim_error_t pdu_session_remove_qos_flow(pdu_session_t* session, uint8_t qfi);
uesim_error_t pdu_session_set_default_qos_flow(pdu_session_t* session, uint8_t qfi);
qos_flow_t* pdu_session_get_qos_flow(pdu_session_t* session, uint8_t qfi);

/* ============== DRB Binding ============== */

uesim_error_t pdu_session_bind_drb(pdu_session_t* session, uint8_t drb_id);
uesim_error_t pdu_session_unbind_drb(pdu_session_t* session, uint8_t drb_id);
uint8_t pdu_session_get_drb_for_qos(pdu_session_t* session, uint8_t qfi);

/* ============== Address Management ============== */

uesim_error_t pdu_session_set_ipv4_address(pdu_session_t* session, uint32_t addr);
uesim_error_t pdu_session_set_ipv6_address(pdu_session_t* session, const uint8_t* addr, uint8_t prefix_len);
uesim_error_t pdu_session_set_ambr(pdu_session_t* session, uint64_t ul_kbps, uint64_t dl_kbps);

/* ============== Statistics ============== */

uesim_error_t pdu_session_update_stats(pdu_session_t* session,
                                        uint64_t ul_bytes, uint64_t dl_bytes,
                                        uint64_t ul_packets, uint64_t dl_packets);

void pdu_session_reset_stats(pdu_session_t* session);

/* ============== Lookup ============== */

pdu_session_t* pdu_session_find_by_id(pdu_session_manager_t* manager, uint8_t session_id);
pdu_session_t* pdu_session_find_by_drb(pdu_session_manager_t* manager, uint8_t drb_id);
pdu_session_t* pdu_session_find_default(pdu_session_manager_t* manager);

/* ============== Utility ============== */

const char* pdu_session_type_str(pdu_session_type_t type);
const char* pdu_session_state_str(pdu_session_state_t state);
const char* ssc_mode_str(ssc_mode_t mode);

#endif /* PDU_SESSION_H */