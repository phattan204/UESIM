/*
 * 5G UE Simulation Application
 * QoS Flow Management Implementation
 */

#include "qos_flow.h"
#include "../core/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 5QI profile table */
static const struct { uint8_t five_qi; bool gbr; } g_5qi_profiles[] = {
    { 1, true }, { 2, true }, { 3, true }, { 4, true },
    { 5, false }, { 6, false }, { 7, false }, { 8, false }, { 9, false },
    { 65, true }, { 66, true }, { 67, true }, { 68, true },
    { 69, false }, { 70, false }, { 75, true }, { 76, true },
    { 79, true }, { 80, false }, { 84, false }, { 85, false }
};
static const int g_5qi_count = sizeof(g_5qi_profiles) / sizeof(g_5qi_profiles[0]);

static bool g_qos_initialized = false;
static qos_flow_manager_t* g_default_manager = NULL;

const char* qos_flow_state_str(qos_flow_state_t state) {
    static const char* strs[] = { "Inactive", "Active", "Suspended", "Unknown" };
    return (state < QOS_FLOW_STATE_MAX) ? strs[state] : "Unknown";
}

bool qos_flow_is_gbr(uint8_t five_qi) {
    for (int i = 0; i < g_5qi_count; i++)
        if (g_5qi_profiles[i].five_qi == five_qi) return g_5qi_profiles[i].gbr;
    return false;
}

uesim_error_t qos_flow_init(void) {
    if (g_qos_initialized) {
        return UESIM_SUCCESS;
    }
    
    /* Create default QoS flow manager */
    uesim_error_t result = qos_flow_create_manager(&g_default_manager);
    if (result != UESIM_SUCCESS) {
        printf("QoS flow: Failed to create default manager, error=%d\n", result);
        return result;
    }
    
    /* Create default QoS flow (5QI=9 for default bearer) */
    qos_flow_t* flow = NULL;
    arp_t default_arp = { .priority_level = 8, .pre_emption_capability = false, .pre_emption_vulnerability = true };
    result = qos_flow_create(g_default_manager, QOS_DEFAULT_5QI, &default_arp, NULL, NULL, &flow);
    if (result != UESIM_SUCCESS) {
        printf("QoS flow: Failed to create default flow, error=%d\n", result);
        qos_flow_destroy_manager(g_default_manager);
        g_default_manager = NULL;
        return result;
    }
    
    /* Activate default flow */
    qos_flow_activate(g_default_manager, flow->qfi);
    
    g_qos_initialized = true;
    printf("QoS flow module initialized with default manager (QFI=%u, 5QI=%u)\n", 
           flow->qfi, flow->five_qi);
    return UESIM_SUCCESS;
}

void qos_flow_cleanup(void) {
    if (!g_qos_initialized) {
        return;
    }
    
    /* Destroy default QoS flow manager */
    if (g_default_manager) {
        qos_flow_destroy_manager(g_default_manager);
        g_default_manager = NULL;
    }
    
    g_qos_initialized = false;
    printf("QoS flow module cleanup completed\n");
}

uesim_error_t qos_flow_create_manager(qos_flow_manager_t** manager) {
    if (!manager) return UESIM_ERROR_INVALID_PARAM;
    qos_flow_manager_t* mgr = (qos_flow_manager_t*)uesim_calloc(1, sizeof(qos_flow_manager_t));
    if (!mgr) return UESIM_ERROR_MEMORY;
#ifdef _WIN32
    mgr->manager_mutex = CreateMutex(NULL, FALSE, NULL);
    if (!mgr->manager_mutex) { uesim_free(mgr); return UESIM_ERROR_THREAD; }
#else
    if (pthread_mutex_init(&mgr->manager_mutex, NULL)) { uesim_free(mgr); return UESIM_ERROR_THREAD; }
#endif
    mgr->num_flows = 0; mgr->default_qfi = 0; mgr->next_rule_id = 1;
    mgr->session_ambr.uplink = 100000; mgr->session_ambr.downlink = 200000;
    *manager = mgr;
    return UESIM_SUCCESS;
}

uesim_error_t qos_flow_destroy_manager(qos_flow_manager_t* manager) {
    if (!manager) return UESIM_ERROR_INVALID_PARAM;
    for (int i = 0; i < manager->num_flows; i++) {
#ifdef _WIN32
        CloseHandle(manager->flows[i].flow_mutex);
#else
        pthread_mutex_destroy(&manager->flows[i].flow_mutex);
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

uesim_error_t qos_flow_create(qos_flow_manager_t* manager, uint8_t five_qi,
                              const arp_t* arp, const bit_rate_t* gbr,
                              const bit_rate_t* mbr, qos_flow_t** flow) {
    if (!manager || !flow) return UESIM_ERROR_INVALID_PARAM;
    if (manager->num_flows >= QOS_MAX_FLOWS_PER_SESSION) return UESIM_ERROR_CAPACITY;
    
    qos_flow_t* f = &manager->flows[manager->num_flows];
    memset(f, 0, sizeof(qos_flow_t));
    f->qfi = manager->num_flows + 1;
    f->five_qi = five_qi;
    f->state = QOS_FLOW_STATE_INACTIVE;
    if (arp) memcpy(&f->arp, arp, sizeof(arp_t));
    else { f->arp.priority_level = 8; }
    if (gbr) memcpy(&f->gbr, gbr, sizeof(bit_rate_t));
    if (mbr) memcpy(&f->mbr, mbr, sizeof(bit_rate_t));
    f->create_time = time(NULL);
    
#ifdef _WIN32
    f->flow_mutex = CreateMutex(NULL, FALSE, NULL);
    if (!f->flow_mutex) return UESIM_ERROR_THREAD;
#else
    if (pthread_mutex_init(&f->flow_mutex, NULL)) return UESIM_ERROR_THREAD;
#endif
    
    if (manager->default_qfi == 0) manager->default_qfi = f->qfi;
    manager->num_flows++;
    *flow = f;
    printf("QoS flow created: QFI=%u, 5QI=%u\n", f->qfi, five_qi);
    return UESIM_SUCCESS;
}

uesim_error_t qos_flow_release(qos_flow_manager_t* manager, uint8_t qfi) {
    if (!manager) return UESIM_ERROR_INVALID_PARAM;
    for (int i = 0; i < manager->num_flows; i++) {
        if (manager->flows[i].qfi == qfi) {
#ifdef _WIN32
            CloseHandle(manager->flows[i].flow_mutex);
#else
            pthread_mutex_destroy(&manager->flows[i].flow_mutex);
#endif
            for (int j = i; j < manager->num_flows - 1; j++)
                manager->flows[j] = manager->flows[j + 1];
            manager->num_flows--;
            return UESIM_SUCCESS;
        }
    }
    return UESIM_ERROR_NOT_FOUND;
}

uesim_error_t qos_flow_activate(qos_flow_manager_t* manager, uint8_t qfi) {
    if (!manager) return UESIM_ERROR_INVALID_PARAM;
    qos_flow_t* f = qos_flow_find_by_qfi(manager, qfi);
    if (!f) return UESIM_ERROR_NOT_FOUND;
    f->state = QOS_FLOW_STATE_ACTIVE;
    return UESIM_SUCCESS;
}

qos_flow_t* qos_flow_find_by_qfi(qos_flow_manager_t* manager, uint8_t qfi) {
    if (!manager) return NULL;
    for (int i = 0; i < manager->num_flows; i++)
        if (manager->flows[i].qfi == qfi) return &manager->flows[i];
    return NULL;
}

qos_flow_t* qos_flow_find_default(qos_flow_manager_t* manager) {
    return manager ? qos_flow_find_by_qfi(manager, manager->default_qfi) : NULL;
}

uesim_error_t qos_flow_set_session_ambr(qos_flow_manager_t* manager, uint64_t ul, uint64_t dl) {
    if (!manager) return UESIM_ERROR_INVALID_PARAM;
    manager->session_ambr.uplink = ul;
    manager->session_ambr.downlink = dl;
    return UESIM_SUCCESS;
}

uesim_error_t qos_flow_update_session_stats(qos_flow_manager_t* manager, uint64_t sent, uint64_t recv) {
    if (!manager) return UESIM_ERROR_INVALID_PARAM;
    manager->session_bytes_sent += sent;
    manager->session_bytes_received += recv;
    return UESIM_SUCCESS;
}

uesim_error_t qos_flow_bind_to_drb(qos_flow_t* flow, uint8_t drb_id) {
    if (!flow) return UESIM_ERROR_INVALID_PARAM;
    flow->drb_id = drb_id;
    return UESIM_SUCCESS;
}

uint8_t qos_flow_get_drb_id(qos_flow_t* flow) { return flow ? flow->drb_id : 0; }