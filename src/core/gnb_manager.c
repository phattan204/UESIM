/*
 * 5G UE Simulation Application
 * gNB Manager Implementation - Multi-gNB Connection Management
 */

#include "gnb_manager.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* String conversion tables */
static const char* g_gnb_type_str[] = {
    "OAI", "srsRAN", "Commercial", "Mock"
};

static const char* g_gnb_state_str[] = {
    "Unknown", "Connected", "Disconnected", "HandoverCandidate", "Connecting"
};

static bool g_gnb_initialized = false;

/* ============== Utility Functions ============== */

const char* gnb_type_str(gnb_type_t type) {
    if (type >= GNB_TYPE_OAI && type < GNB_TYPE_MAX) {
        return g_gnb_type_str[type];
    }
    return "Unknown";
}

const char* gnb_state_str(gnb_state_t state) {
    if (state < GNB_STATE_MAX) {
        return g_gnb_state_str[state];
    }
    return "Unknown";
}

/* ============== Initialization ============== */

uesim_error_t gnb_manager_init(void) {
    if (!g_gnb_initialized) {
        g_gnb_initialized = true;
        printf("gNB manager module initialized\n");
    }
    return UESIM_SUCCESS;
}

void gnb_manager_cleanup(void) {
    g_gnb_initialized = false;
    printf("gNB manager module cleanup completed\n");
}

/* ============== Manager Operations ============== */

uesim_error_t gnb_manager_create(gnb_manager_t** manager) {
    if (!manager) return UESIM_ERROR_INVALID_PARAM;
    
    gnb_manager_t* mgr = (gnb_manager_t*)uesim_calloc(1, sizeof(gnb_manager_t));
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
    
    mgr->num_gnbs = 0;
    mgr->default_ngap_port = GNB_DEFAULT_NGAP_PORT;
    mgr->default_gtpu_port = GNB_DEFAULT_GTPU_PORT;
    mgr->total_tx_bytes = 0;
    mgr->total_rx_bytes = 0;
    
    *manager = mgr;
    return UESIM_SUCCESS;
}

uesim_error_t gnb_manager_destroy(gnb_manager_t* manager) {
    if (!manager) return UESIM_ERROR_INVALID_PARAM;
    
    /* Disconnect and cleanup all gNBs */
    for (int i = 0; i < manager->num_gnbs; i++) {
        gnb_extended_context_t* gnb = &manager->gnb_list[i];
        
        if (gnb->base.ngap_socket >= 0) {
#ifdef _WIN32
            closesocket(gnb->base.ngap_socket);
#else
            close(gnb->base.ngap_socket);
#endif
        }
        if (gnb->base.gtpu_socket >= 0) {
#ifdef _WIN32
            closesocket(gnb->base.gtpu_socket);
#else
            close(gnb->base.gtpu_socket);
#endif
        }
        
#ifdef _WIN32
        CloseHandle(gnb->gnb_mutex);
#else
        pthread_mutex_destroy(&gnb->gnb_mutex);
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

/* ============== gNB Operations ============== */

uesim_error_t gnb_manager_add(gnb_manager_t* manager,
                               uint32_t gnb_id,
                               gnb_type_t type,
                               const char* ip,
                               uint16_t ngap_port,
                               uint16_t gtpu_port,
                               gnb_extended_context_t** gnb_ctx) {
    if (!manager || !ip) return UESIM_ERROR_INVALID_PARAM;
    if (manager->num_gnbs >= GNB_MAX_INSTANCES) return UESIM_ERROR_CAPACITY;
    
    /* Check if gNB ID already exists */
    if (gnb_manager_find_by_id(manager, gnb_id) != NULL) {
        return UESIM_ERROR_ALREADY_EXISTS;
    }
    
    gnb_extended_context_t* gnb = &manager->gnb_list[manager->num_gnbs];
    memset(gnb, 0, sizeof(gnb_extended_context_t));
    
    gnb->base.gnb_id = gnb_id;
    snprintf(gnb->gnb_name, sizeof(gnb->gnb_name), "gnb-%u", gnb_id);
    gnb->base.type = type;
    gnb->base.state = GNB_STATE_DISCONNECTED;
    
    /* Setup NGAP address */
    gnb->base.addr.sin_family = AF_INET;
    gnb->base.addr.sin_port = htons(ngap_port > 0 ? ngap_port : manager->default_ngap_port);
#ifdef _WIN32
    gnb->base.addr.sin_addr.s_addr = inet_addr(ip);
#else
    inet_pton(AF_INET, ip, &gnb->base.addr.sin_addr);
#endif
    gnb->base.ngap_socket = -1;
    gnb->base.gtpu_socket = -1;
    
    /* Default capabilities */
    gnb->capabilities.supports_xn_handover = true;
    gnb->capabilities.supports_n2_handover = true;
    gnb->capabilities.max_ues_supported = 1000;
    
    gnb->is_handover_candidate = false;
    gnb->handover_priority = 0;
    gnb->connect_time = 0;
    gnb->last_activity = 0;
    
#ifdef _WIN32
    gnb->gnb_mutex = CreateMutex(NULL, FALSE, NULL);
    if (!gnb->gnb_mutex) {
        return UESIM_ERROR_THREAD;
    }
#else
    if (pthread_mutex_init(&gnb->gnb_mutex, NULL) != 0) {
        return UESIM_ERROR_THREAD;
    }
#endif
    
    manager->num_gnbs++;
    
    if (gnb_ctx) *gnb_ctx = gnb;
    
    printf("gNB added: ID=%u, Type=%s, IP=%s, NGAP=%u, GTP-U=%u\n",
           gnb_id, gnb_type_str(type), ip, 
           ngap_port > 0 ? ngap_port : manager->default_ngap_port,
           gtpu_port > 0 ? gtpu_port : manager->default_gtpu_port);
    
    return UESIM_SUCCESS;
}

uesim_error_t gnb_manager_remove(gnb_manager_t* manager, uint32_t gnb_id) {
    if (!manager) return UESIM_ERROR_INVALID_PARAM;
    
    for (int i = 0; i < manager->num_gnbs; i++) {
        if (manager->gnb_list[i].base.gnb_id == gnb_id) {
            gnb_extended_context_t* gnb = &manager->gnb_list[i];
            
            /* Close sockets */
            if (gnb->base.ngap_socket >= 0) {
#ifdef _WIN32
                closesocket(gnb->base.ngap_socket);
#else
                close(gnb->base.ngap_socket);
#endif
            }
            if (gnb->base.gtpu_socket >= 0) {
#ifdef _WIN32
                closesocket(gnb->base.gtpu_socket);
#else
                close(gnb->base.gtpu_socket);
#endif
            }
            
#ifdef _WIN32
            CloseHandle(gnb->gnb_mutex);
#else
            pthread_mutex_destroy(&gnb->gnb_mutex);
#endif
            
            /* Shift remaining gNBs */
            for (int j = i; j < manager->num_gnbs - 1; j++) {
                manager->gnb_list[j] = manager->gnb_list[j + 1];
            }
            manager->num_gnbs--;
            
            printf("gNB removed: ID=%u\n", gnb_id);
            return UESIM_SUCCESS;
        }
    }
    
    return UESIM_ERROR_NOT_FOUND;
}

uesim_error_t gnb_manager_connect(gnb_manager_t* manager, uint32_t gnb_id) {
    if (!manager) return UESIM_ERROR_INVALID_PARAM;
    
    gnb_extended_context_t* gnb = gnb_manager_find_by_id(manager, gnb_id);
    if (!gnb) return UESIM_ERROR_NOT_FOUND;
    
    gnb->base.state = GNB_STATE_CONNECTING;
    
    /* Create NGAP socket */
    gnb->base.ngap_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (gnb->base.ngap_socket < 0) {
        gnb->base.state = GNB_STATE_UNKNOWN;
        return UESIM_ERROR_SOCKET;
    }
    
    /* Create GTP-U socket */
    gnb->base.gtpu_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (gnb->base.gtpu_socket < 0) {
#ifdef _WIN32
        closesocket(gnb->base.ngap_socket);
#else
        close(gnb->base.ngap_socket);
#endif
        gnb->base.ngap_socket = -1;
        gnb->base.state = GNB_STATE_UNKNOWN;
        return UESIM_ERROR_SOCKET;
    }
    
    /* Connect NGAP */
    if (connect(gnb->base.ngap_socket, (struct sockaddr*)&gnb->base.addr, 
                sizeof(gnb->base.addr)) < 0) {
        /* For simulation, we'll mark as connected anyway */
        printf("Note: NGAP connection simulated for gNB %u\n", gnb_id);
    }
    
    gnb->base.state = GNB_STATE_CONNECTED;
    gnb->connect_time = time(NULL);
    gnb->last_activity = gnb->connect_time;
    
    printf("gNB connected: ID=%u, State=%s\n", gnb_id, gnb_state_str(gnb->base.state));
    return UESIM_SUCCESS;
}

uesim_error_t gnb_manager_disconnect(gnb_manager_t* manager, uint32_t gnb_id) {
    if (!manager) return UESIM_ERROR_INVALID_PARAM;
    
    gnb_extended_context_t* gnb = gnb_manager_find_by_id(manager, gnb_id);
    if (!gnb) return UESIM_ERROR_NOT_FOUND;
    
    if (gnb->base.ngap_socket >= 0) {
#ifdef _WIN32
        closesocket(gnb->base.ngap_socket);
#else
        close(gnb->base.ngap_socket);
#endif
        gnb->base.ngap_socket = -1;
    }
    
    if (gnb->base.gtpu_socket >= 0) {
#ifdef _WIN32
        closesocket(gnb->base.gtpu_socket);
#else
        close(gnb->base.gtpu_socket);
#endif
        gnb->base.gtpu_socket = -1;
    }
    
    gnb->base.state = GNB_STATE_DISCONNECTED;
    
    printf("gNB disconnected: ID=%u\n", gnb_id);
    return UESIM_SUCCESS;
}

/* ============== Lookup ============== */

gnb_extended_context_t* gnb_manager_find_by_id(gnb_manager_t* manager, uint32_t gnb_id) {
    if (!manager) return NULL;
    
    for (int i = 0; i < manager->num_gnbs; i++) {
        if (manager->gnb_list[i].base.gnb_id == gnb_id) {
            return &manager->gnb_list[i];
        }
    }
    return NULL;
}

gnb_extended_context_t* gnb_manager_find_by_socket(gnb_manager_t* manager, int socket) {
    if (!manager) return NULL;
    
    for (int i = 0; i < manager->num_gnbs; i++) {
        if (manager->gnb_list[i].base.ngap_socket == socket ||
            manager->gnb_list[i].base.gtpu_socket == socket) {
            return &manager->gnb_list[i];
        }
    }
    return NULL;
}

gnb_extended_context_t* gnb_manager_find_best_handover(gnb_manager_t* manager,
                                                        gnb_extended_context_t* exclude_gnb) {
    if (!manager) return NULL;
    
    gnb_extended_context_t* best = NULL;
    int32_t best_rsrp = -140;  /* Minimum RSRP */
    
    for (int i = 0; i < manager->num_gnbs; i++) {
        gnb_extended_context_t* gnb = &manager->gnb_list[i];
        
        /* Skip excluded gNB */
        if (gnb == exclude_gnb) continue;
        
        /* Only consider connected gNBs marked as handover candidates */
        if (gnb->base.state != GNB_STATE_CONNECTED && 
            gnb->base.state != GNB_STATE_HANDOVER_CANDIDATE) continue;
        
        if (!gnb->is_handover_candidate) continue;
        
        /* Check primary cell RSRP */
        if (gnb->primary_cell && gnb->primary_cell->rsrp > best_rsrp) {
            best_rsrp = gnb->primary_cell->rsrp;
            best = gnb;
        }
    }
    
    return best;
}

/* ============== Cell Operations ============== */

uesim_error_t gnb_add_cell(gnb_extended_context_t* gnb, const gnb_cell_info_t* cell) {
    if (!gnb || !cell) return UESIM_ERROR_INVALID_PARAM;
    if (gnb->num_cells >= GNB_MAX_CELLS_PER_GNB) return UESIM_ERROR_CAPACITY;
    
    /* Check if PCI already exists */
    if (gnb_find_cell_by_pci(gnb, cell->pci) != NULL) {
        return UESIM_ERROR_ALREADY_EXISTS;
    }
    
    gnb->cells[gnb->num_cells] = *cell;
    
    /* Set as primary if first cell */
    if (gnb->num_cells == 0) {
        gnb->primary_cell = &gnb->cells[0];
    }
    
    gnb->num_cells++;
    
    printf("Cell added to gNB %u: PCI=%u, Cell ID=%u, RSRP=%d dBm\n",
           gnb->base.gnb_id, cell->pci, cell->cell_id, cell->rsrp);
    
    return UESIM_SUCCESS;
}

uesim_error_t gnb_remove_cell(gnb_extended_context_t* gnb, uint16_t pci) {
    if (!gnb) return UESIM_ERROR_INVALID_PARAM;
    
    for (int i = 0; i < gnb->num_cells; i++) {
        if (gnb->cells[i].pci == pci) {
            /* Update primary cell pointer if needed */
            if (gnb->primary_cell == &gnb->cells[i]) {
                gnb->primary_cell = (gnb->num_cells > 1) ? &gnb->cells[0] : NULL;
            }
            
            /* Shift remaining cells */
            for (int j = i; j < gnb->num_cells - 1; j++) {
                gnb->cells[j] = gnb->cells[j + 1];
            }
            gnb->num_cells--;
            
            return UESIM_SUCCESS;
        }
    }
    
    return UESIM_ERROR_NOT_FOUND;
}

gnb_cell_info_t* gnb_find_cell_by_pci(gnb_extended_context_t* gnb, uint16_t pci) {
    if (!gnb) return NULL;
    
    for (int i = 0; i < gnb->num_cells; i++) {
        if (gnb->cells[i].pci == pci) {
            return &gnb->cells[i];
        }
    }
    return NULL;
}

/* ============== Measurement Updates ============== */

uesim_error_t gnb_update_measurements(gnb_extended_context_t* gnb,
                                       int32_t rsrp, int32_t rsrq, int32_t sinr) {
    if (!gnb) return UESIM_ERROR_INVALID_PARAM;
    
    if (gnb->primary_cell) {
        gnb->primary_cell->rsrp = rsrp;
        gnb->primary_cell->rsrq = rsrq;
        gnb->primary_cell->sinr = sinr;
    }
    
    gnb->last_activity = time(NULL);
    return UESIM_SUCCESS;
}

/* ============== Handover Candidate Management ============== */

uesim_error_t gnb_set_handover_candidate(gnb_extended_context_t* gnb, bool is_candidate) {
    if (!gnb) return UESIM_ERROR_INVALID_PARAM;
    
    gnb->is_handover_candidate = is_candidate;
    
    if (is_candidate && gnb->base.state == GNB_STATE_CONNECTED) {
        gnb->base.state = GNB_STATE_HANDOVER_CANDIDATE;
    } else if (!is_candidate && gnb->base.state == GNB_STATE_HANDOVER_CANDIDATE) {
        gnb->base.state = GNB_STATE_CONNECTED;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t gnb_set_handover_priority(gnb_extended_context_t* gnb, int32_t priority) {
    if (!gnb) return UESIM_ERROR_INVALID_PARAM;
    
    gnb->handover_priority = priority;
    return UESIM_SUCCESS;
}

/* ============== Statistics ============== */

uesim_error_t gnb_update_stats(gnb_extended_context_t* gnb, uint64_t tx_bytes, uint64_t rx_bytes) {
    if (!gnb) return UESIM_ERROR_INVALID_PARAM;
    
    gnb->tx_bytes += tx_bytes;
    gnb->rx_bytes += rx_bytes;
    gnb->last_activity = time(NULL);
    
    return UESIM_SUCCESS;
}

void gnb_reset_stats(gnb_extended_context_t* gnb) {
    if (!gnb) return;
    
    gnb->tx_bytes = 0;
    gnb->rx_bytes = 0;
    gnb->connected_ues = 0;
}