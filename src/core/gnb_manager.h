/*
 * 5G UE Simulation Application
 * gNB Manager Header - Multi-gNB Connection Management
 */

#ifndef GNB_MANAGER_H
#define GNB_MANAGER_H

#include "../uesim.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* gNB Constants */
#define GNB_MAX_INSTANCES           32
#define GNB_MAX_CELLS_PER_GNB       5
#define GNB_DEFAULT_NGAP_PORT      38412
#define GNB_DEFAULT_GTPU_PORT      2152

/* gNB Capabilities */
typedef struct {
    bool supports_xn_handover;
    bool supports_n2_handover;
    bool supports_daps;
    bool supports_conditional_handover;
    uint16_t max_ues_supported;
    uint16_t supported_bands[8];
    uint8_t num_bands;
} gnb_capabilities_t;

/* Cell Information */
typedef struct {
    uint16_t pci;            /* Physical Cell ID */
    uint32_t cell_id;        /* Global Cell ID (NCI) */
    uint16_t tac;            /* Tracking Area Code */
    uint32_t arfcn;          /* NR-ARFCN */
    uint8_t scs;             /* Subcarrier spacing */
    uint8_t bandwidth_mhz;
    int32_t rsrp;            /* dBm */
    int32_t rsrq;            /* dB */
    int32_t sinr;            /* dB */
} gnb_cell_info_t;

/* Extended gNB Context (extends gnb_context_t from uesim.h) */
typedef struct gnb_extended_context {
    gnb_context_t base;              /* Base context from uesim.h */
    
    /* Extended fields */
    char gnb_name[64];
    
    /* Cell information */
    gnb_cell_info_t cells[GNB_MAX_CELLS_PER_GNB];
    uint8_t num_cells;
    gnb_cell_info_t* primary_cell;
    
    /* Capabilities */
    gnb_capabilities_t capabilities;
    
    /* Connection stats */
    time_t connect_time;
    time_t last_activity;
    uint64_t tx_bytes;
    uint64_t rx_bytes;
    uint32_t connected_ues;
    
    /* Handover support */
    bool is_handover_candidate;
    int32_t handover_priority;       /* Higher = better candidate */
    
    /* Thread safety */
    pthread_mutex_t gnb_mutex;
} gnb_extended_context_t;

/* gNB Manager */
typedef struct {
    gnb_extended_context_t gnb_list[GNB_MAX_INSTANCES];
    uint8_t num_gnbs;
    
    /* Default settings */
    uint16_t default_ngap_port;
    uint16_t default_gtpu_port;
    
    /* Statistics */
    uint64_t total_tx_bytes;
    uint64_t total_rx_bytes;
    
    /* Thread safety */
    pthread_mutex_t manager_mutex;
} gnb_manager_t;

/* ============== Initialization ============== */

uesim_error_t gnb_manager_init(void);
void gnb_manager_cleanup(void);

/* ============== Manager Operations ============== */

uesim_error_t gnb_manager_create(gnb_manager_t** manager);
uesim_error_t gnb_manager_destroy(gnb_manager_t* manager);

/* ============== gNB Operations ============== */

uesim_error_t gnb_manager_add(gnb_manager_t* manager,
                               uint32_t gnb_id,
                               gnb_type_t type,
                               const char* ip,
                               uint16_t ngap_port,
                               uint16_t gtpu_port,
                               gnb_extended_context_t** gnb_ctx);

uesim_error_t gnb_manager_remove(gnb_manager_t* manager, uint32_t gnb_id);
uesim_error_t gnb_manager_connect(gnb_manager_t* manager, uint32_t gnb_id);
uesim_error_t gnb_manager_disconnect(gnb_manager_t* manager, uint32_t gnb_id);

/* ============== Lookup ============== */

gnb_extended_context_t* gnb_manager_find_by_id(gnb_manager_t* manager, uint32_t gnb_id);
gnb_extended_context_t* gnb_manager_find_by_socket(gnb_manager_t* manager, int socket);
gnb_extended_context_t* gnb_manager_find_best_handover(gnb_manager_t* manager,
                                                        gnb_extended_context_t* exclude_gnb);

/* ============== Cell Operations ============== */

uesim_error_t gnb_add_cell(gnb_extended_context_t* gnb, const gnb_cell_info_t* cell);
uesim_error_t gnb_remove_cell(gnb_extended_context_t* gnb, uint16_t pci);
gnb_cell_info_t* gnb_find_cell_by_pci(gnb_extended_context_t* gnb, uint16_t pci);

/* ============== Measurement Updates ============== */

uesim_error_t gnb_update_measurements(gnb_extended_context_t* gnb,
                                       int32_t rsrp, int32_t rsrq, int32_t sinr);

/* ============== Handover Candidate Management ============== */

uesim_error_t gnb_set_handover_candidate(gnb_extended_context_t* gnb, bool is_candidate);
uesim_error_t gnb_set_handover_priority(gnb_extended_context_t* gnb, int32_t priority);

/* ============== Statistics ============== */

uesim_error_t gnb_update_stats(gnb_extended_context_t* gnb,
                                uint64_t tx_bytes, uint64_t rx_bytes);

void gnb_reset_stats(gnb_extended_context_t* gnb);

/* ============== Utility ============== */

const char* gnb_type_str(gnb_type_t type);
const char* gnb_state_str(gnb_state_t state);

#endif /* GNB_MANAGER_H */