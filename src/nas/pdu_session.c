/*
 * 5G UE Simulation Application
 * PDU Session Management Implementation
 */

#include "pdu_session.h"
#include "../core/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* String conversion tables */
static const char* g_session_type_str[] = {
    "Unknown", "IPv4", "IPv6", "IPv4v6", "Ethernet"
};

static const char* g_session_state_str[] = {
    "Inactive", "Creating", "Active", "Modifying", "Releasing", "Suspended"
};

static const char* g_ssc_mode_str[] = {
    "Unknown", "SSC1", "SSC2", "SSC3"
};

static bool g_pdu_initialized = false;

/* ============== Utility Functions ============== */

const char* pdu_session_type_str(pdu_session_type_t type) {
    if (type >= PDU_SESSION_TYPE_IPV4 && type < PDU_SESSION_TYPE_MAX) {
        return g_session_type_str[type];
    }
    return "Unknown";
}

const char* pdu_session_state_str(pdu_session_state_t state) {
    if (state < PDU_SESSION_STATE_MAX) {
        return g_session_state_str[state];
    }
    return "Unknown";
}

const char* ssc_mode_str(ssc_mode_t mode) {
    if (mode >= SSC_MODE_1 && mode < SSC_MODE_MAX) {
        return g_ssc_mode_str[mode];
    }
    return "Unknown";
}

/* ============== Initialization ============== */

uesim_error_t pdu_session_init(void) {
    if (!g_pdu_initialized) {
        g_pdu_initialized = true;
        printf("PDU session module initialized\n");
    }
    return UESIM_SUCCESS;
}

void pdu_session_cleanup(void) {
    g_pdu_initialized = false;
    printf("PDU session module cleanup completed\n");
}

/* ============== Manager Operations ============== */

uesim_error_t pdu_session_create_manager(pdu_session_manager_t** manager) {
    if (!manager) return UESIM_ERROR_INVALID_PARAM;
    
    pdu_session_manager_t* mgr = (pdu_session_manager_t*)uesim_calloc(1, sizeof(pdu_session_manager_t));
    if (!mgr) return UESIM_ERROR_MEMORY;
    
#ifdef _WIN32
    mgr->manager_mutex = CreateMutex(NULL, FALSE, NULL);
    if (!mgr->manager_mutex) {
        uesim_free(mgr);
        return UESIM_ERROR_THREAD;
    }
#else
    if (pthread_mutex_init(&mgr->manager_mutex, NULL) != 0) {
        uesim_free(mgr);
        return UESIM_ERROR_THREAD;
    }
#endif
    
    mgr->num_sessions = 0;
    mgr->next_session_id = 1;
    mgr->default_session_type = PDU_SESSION_TYPE_IPV4;
    mgr->default_ssc_mode = SSC_MODE_1;
    mgr->default_snssai.sst = 1;  /* Default slice */
    
    *manager = mgr;
    return UESIM_SUCCESS;
}

uesim_error_t pdu_session_destroy_manager(pdu_session_manager_t* manager) {
    if (!manager) return UESIM_ERROR_INVALID_PARAM;
    
    /* Release all sessions */
    for (int i = 0; i < manager->num_sessions; i++) {
        pdu_session_t* session = &manager->sessions[i];
        
        /* Destroy QoS manager */
        if (session->qos_manager) {
            qos_flow_destroy_manager(session->qos_manager);
            session->qos_manager = NULL;
        }
        
#ifdef _WIN32
        CloseHandle(session->session_mutex);
#else
        pthread_mutex_destroy(&session->session_mutex);
#endif
    }
    
#ifdef _WIN32
    CloseHandle(manager->manager_mutex);
#else
    pthread_mutex_destroy(&manager->manager_mutex);
#endif
    
    uesim_free(manager);
    return UESIM_SUCCESS;
}

/* ============== Session Operations ============== */

uesim_error_t pdu_session_create(pdu_session_manager_t* manager,
                                  pdu_session_type_t type,
                                  ssc_mode_t ssc_mode,
                                  const snssai_t* snssai,
                                  pdu_session_t** session) {
    if (!manager || !session) return UESIM_ERROR_INVALID_PARAM;
    if (manager->num_sessions >= PDU_MAX_SESSIONS_PER_UE) return UESIM_ERROR_CAPACITY;
    
    pdu_session_t* sess = &manager->sessions[manager->num_sessions];
    memset(sess, 0, sizeof(pdu_session_t));
    
    sess->session_id = manager->next_session_id++;
    if (sess->session_id > 15) sess->session_id = 1;  /* PDU Session ID: 1-15 */
    
    sess->state = PDU_SESSION_STATE_CREATING;
    sess->config.session_type = type;
    sess->config.ssc_mode = ssc_mode;
    
    if (snssai) {
        memcpy(&sess->config.snssai, snssai, sizeof(snssai_t));
    } else {
        memcpy(&sess->config.snssai, &manager->default_snssai, sizeof(snssai_t));
    }
    
    /* Default AMBR */
    sess->config.session_ambr_ul = 100000;  /* 100 Mbps */
    sess->config.session_ambr_dl = 200000;  /* 200 Mbps */
    
    /* Create QoS flow manager */
    uesim_error_t result = qos_flow_create_manager(&sess->qos_manager);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    /* Create default QoS flow (5QI=9 for internet) */
    qos_flow_t* default_flow = NULL;
    arp_t default_arp = { .priority_level = 8, .pre_emption_capability = false, .pre_emption_vulnerability = false };
    result = qos_flow_create(sess->qos_manager, 9, &default_arp, NULL, NULL, &default_flow);
    if (result == UESIM_SUCCESS) {
        sess->default_qos_flow_qfi = default_flow->qfi;
        qos_flow_activate(sess->qos_manager, default_flow->qfi);
    }
    
    sess->create_time = time(NULL);
    
#ifdef _WIN32
    sess->session_mutex = CreateMutex(NULL, FALSE, NULL);
    if (!sess->session_mutex) {
        qos_flow_destroy_manager(sess->qos_manager);
        return UESIM_ERROR_THREAD;
    }
#else
    if (pthread_mutex_init(&sess->session_mutex, NULL) != 0) {
        qos_flow_destroy_manager(sess->qos_manager);
        return UESIM_ERROR_THREAD;
    }
#endif
    
    manager->num_sessions++;
    *session = sess;
    
    printf("PDU session created: ID=%u, Type=%s, SSC=%s\n",
           sess->session_id, pdu_session_type_str(type), ssc_mode_str(ssc_mode));
    
    return UESIM_SUCCESS;
}

uesim_error_t pdu_session_release(pdu_session_manager_t* manager, uint8_t session_id) {
    if (!manager) return UESIM_ERROR_INVALID_PARAM;
    
    for (int i = 0; i < manager->num_sessions; i++) {
        if (manager->sessions[i].session_id == session_id) {
            pdu_session_t* sess = &manager->sessions[i];
            
            sess->state = PDU_SESSION_STATE_RELEASING;
            
            /* Destroy QoS manager */
            if (sess->qos_manager) {
                qos_flow_destroy_manager(sess->qos_manager);
                sess->qos_manager = NULL;
            }
            
#ifdef _WIN32
            CloseHandle(sess->session_mutex);
#else
            pthread_mutex_destroy(&sess->session_mutex);
#endif
            
            /* Shift remaining sessions */
            for (int j = i; j < manager->num_sessions - 1; j++) {
                manager->sessions[j] = manager->sessions[j + 1];
            }
            manager->num_sessions--;
            
            printf("PDU session released: ID=%u\n", session_id);
            return UESIM_SUCCESS;
        }
    }
    
    return UESIM_ERROR_NOT_FOUND;
}

uesim_error_t pdu_session_modify(pdu_session_manager_t* manager, uint8_t session_id,
                                  const pdu_session_config_t* new_config) {
    if (!manager || !new_config) return UESIM_ERROR_INVALID_PARAM;
    
    pdu_session_t* sess = pdu_session_find_by_id(manager, session_id);
    if (!sess) return UESIM_ERROR_NOT_FOUND;
    
    sess->state = PDU_SESSION_STATE_MODIFYING;
    
    /* Update configuration */
    if (new_config->session_ambr_ul > 0) {
        sess->config.session_ambr_ul = new_config->session_ambr_ul;
    }
    if (new_config->session_ambr_dl > 0) {
        sess->config.session_ambr_dl = new_config->session_ambr_dl;
    }
    
    sess->modify_time = time(NULL);
    sess->state = PDU_SESSION_STATE_ACTIVE;
    
    printf("PDU session modified: ID=%u\n", session_id);
    return UESIM_SUCCESS;
}

uesim_error_t pdu_session_activate(pdu_session_manager_t* manager, uint8_t session_id) {
    if (!manager) return UESIM_ERROR_INVALID_PARAM;
    
    pdu_session_t* sess = pdu_session_find_by_id(manager, session_id);
    if (!sess) return UESIM_ERROR_NOT_FOUND;
    
    sess->state = PDU_SESSION_STATE_ACTIVE;
    printf("PDU session activated: ID=%u\n", session_id);
    return UESIM_SUCCESS;
}

uesim_error_t pdu_session_suspend(pdu_session_manager_t* manager, uint8_t session_id) {
    if (!manager) return UESIM_ERROR_INVALID_PARAM;
    
    pdu_session_t* sess = pdu_session_find_by_id(manager, session_id);
    if (!sess) return UESIM_ERROR_NOT_FOUND;
    
    sess->state = PDU_SESSION_STATE_SUSPENDED;
    printf("PDU session suspended: ID=%u\n", session_id);
    return UESIM_SUCCESS;
}

uesim_error_t pdu_session_resume(pdu_session_manager_t* manager, uint8_t session_id) {
    if (!manager) return UESIM_ERROR_INVALID_PARAM;
    
    pdu_session_t* sess = pdu_session_find_by_id(manager, session_id);
    if (!sess) return UESIM_ERROR_NOT_FOUND;
    
    sess->state = PDU_SESSION_STATE_ACTIVE;
    printf("PDU session resumed: ID=%u\n", session_id);
    return UESIM_SUCCESS;
}

/* ============== QoS Flow Integration ============== */

uesim_error_t pdu_session_add_qos_flow(pdu_session_t* session,
                                         uint8_t five_qi,
                                         const arp_t* arp,
                                         const bit_rate_t* gbr,
                                         const bit_rate_t* mbr,
                                         uint8_t* qfi) {
    if (!session || !session->qos_manager || !qfi) return UESIM_ERROR_INVALID_PARAM;
    
    qos_flow_t* flow = NULL;
    uesim_error_t result = qos_flow_create(session->qos_manager, five_qi, arp, gbr, mbr, &flow);
    if (result == UESIM_SUCCESS) {
        *qfi = flow->qfi;
        qos_flow_activate(session->qos_manager, flow->qfi);
    }
    return result;
}

uesim_error_t pdu_session_remove_qos_flow(pdu_session_t* session, uint8_t qfi) {
    if (!session || !session->qos_manager) return UESIM_ERROR_INVALID_PARAM;
    return qos_flow_release(session->qos_manager, qfi);
}

uesim_error_t pdu_session_set_default_qos_flow(pdu_session_t* session, uint8_t qfi) {
    if (!session) return UESIM_ERROR_INVALID_PARAM;
    
    qos_flow_t* flow = qos_flow_find_by_qfi(session->qos_manager, qfi);
    if (!flow) return UESIM_ERROR_NOT_FOUND;
    
    session->default_qos_flow_qfi = qfi;
    return UESIM_SUCCESS;
}

qos_flow_t* pdu_session_get_qos_flow(pdu_session_t* session, uint8_t qfi) {
    if (!session || !session->qos_manager) return NULL;
    return qos_flow_find_by_qfi(session->qos_manager, qfi);
}

/* ============== DRB Binding ============== */

uesim_error_t pdu_session_bind_drb(pdu_session_t* session, uint8_t drb_id) {
    if (!session) return UESIM_ERROR_INVALID_PARAM;
    if (session->num_drbs >= PDU_MAX_DRBS) return UESIM_ERROR_CAPACITY;
    
    /* Check if already bound */
    for (int i = 0; i < session->num_drbs; i++) {
        if (session->drb_ids[i] == drb_id) {
            return UESIM_ERROR_ALREADY_EXISTS;
        }
    }
    
    session->drb_ids[session->num_drbs++] = drb_id;
    printf("DRB %u bound to PDU session %u\n", drb_id, session->session_id);
    return UESIM_SUCCESS;
}

uesim_error_t pdu_session_unbind_drb(pdu_session_t* session, uint8_t drb_id) {
    if (!session) return UESIM_ERROR_INVALID_PARAM;
    
    for (int i = 0; i < session->num_drbs; i++) {
        if (session->drb_ids[i] == drb_id) {
            for (int j = i; j < session->num_drbs - 1; j++) {
                session->drb_ids[j] = session->drb_ids[j + 1];
            }
            session->num_drbs--;
            printf("DRB %u unbound from PDU session %u\n", drb_id, session->session_id);
            return UESIM_SUCCESS;
        }
    }
    return UESIM_ERROR_NOT_FOUND;
}

uint8_t pdu_session_get_drb_for_qos(pdu_session_t* session, uint8_t qfi) {
    if (!session) return 0;
    
    qos_flow_t* flow = qos_flow_find_by_qfi(session->qos_manager, qfi);
    if (flow && flow->drb_id > 0) {
        return flow->drb_id;
    }
    
    /* Return first DRB if no specific binding */
    return (session->num_drbs > 0) ? session->drb_ids[0] : 0;
}

/* ============== Address Management ============== */

uesim_error_t pdu_session_set_ipv4_address(pdu_session_t* session, uint32_t addr) {
    if (!session) return UESIM_ERROR_INVALID_PARAM;
    
    session->config.pdu_address.ipv4_addr = addr;
    session->config.pdu_address.type = PDU_SESSION_TYPE_IPV4;
    
    printf("PDU session %u: IPv4 address assigned\n", session->session_id);
    return UESIM_SUCCESS;
}

uesim_error_t pdu_session_set_ipv6_address(pdu_session_t* session, const uint8_t* addr, uint8_t prefix_len) {
    if (!session || !addr) return UESIM_ERROR_INVALID_PARAM;
    
    memcpy(session->config.pdu_address.ipv6_addr, addr, 16);
    session->config.pdu_address.ipv6_prefix_len = prefix_len;
    session->config.pdu_address.type = PDU_SESSION_TYPE_IPV6;
    
    printf("PDU session %u: IPv6 address assigned (prefix %u)\n", session->session_id, prefix_len);
    return UESIM_SUCCESS;
}

uesim_error_t pdu_session_set_ambr(pdu_session_t* session, uint64_t ul_kbps, uint64_t dl_kbps) {
    if (!session) return UESIM_ERROR_INVALID_PARAM;
    
    session->config.session_ambr_ul = ul_kbps;
    session->config.session_ambr_dl = dl_kbps;
    
    if (session->qos_manager) {
        qos_flow_set_session_ambr(session->qos_manager, ul_kbps, dl_kbps);
    }
    
    printf("PDU session %u: AMBR set to %lu/%lu kbps\n", session->session_id, ul_kbps, dl_kbps);
    return UESIM_SUCCESS;
}

/* ============== Statistics ============== */

uesim_error_t pdu_session_update_stats(pdu_session_t* session,
                                        uint64_t ul_bytes, uint64_t dl_bytes,
                                        uint64_t ul_packets, uint64_t dl_packets) {
    if (!session) return UESIM_ERROR_INVALID_PARAM;
    
    session->stats.ul_bytes += ul_bytes;
    session->stats.dl_bytes += dl_bytes;
    session->stats.ul_packets += ul_packets;
    session->stats.dl_packets += dl_packets;
    session->last_activity = time(NULL);
    
    if (ul_bytes > 0) session->stats.last_ul_activity = session->last_activity;
    if (dl_bytes > 0) session->stats.last_dl_activity = session->last_activity;
    
    return UESIM_SUCCESS;
}

void pdu_session_reset_stats(pdu_session_t* session) {
    if (!session) return;
    memset(&session->stats, 0, sizeof(pdu_session_stats_t));
}

/* ============== Lookup ============== */

pdu_session_t* pdu_session_find_by_id(pdu_session_manager_t* manager, uint8_t session_id) {
    if (!manager) return NULL;
    
    for (int i = 0; i < manager->num_sessions; i++) {
        if (manager->sessions[i].session_id == session_id) {
            return &manager->sessions[i];
        }
    }
    return NULL;
}

pdu_session_t* pdu_session_find_by_drb(pdu_session_manager_t* manager, uint8_t drb_id) {
    if (!manager) return NULL;
    
    for (int i = 0; i < manager->num_sessions; i++) {
        pdu_session_t* sess = &manager->sessions[i];
        for (int j = 0; j < sess->num_drbs; j++) {
            if (sess->drb_ids[j] == drb_id) {
                return sess;
            }
        }
    }
    return NULL;
}

pdu_session_t* pdu_session_find_default(pdu_session_manager_t* manager) {
    if (!manager || manager->num_sessions == 0) return NULL;
    return &manager->sessions[0];  /* First session is default */
}