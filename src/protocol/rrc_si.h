/*
 * 5G UE Simulation Application
 * RRC System Information Processing Header
 */

#ifndef RRC_SI_H
#define RRC_SI_H

#include "../uesim.h"
#include <stdint.h>
#include <stdbool.h>

/* MIB (Master Information Block) - 3GPP TS 38.331 */
typedef struct {
    uint16_t sfn;                   /* System Frame Number (0-1023) */
    uint8_t subcarrier_spacing;     /* Subcarrier spacing for SIB1 */
    uint8_t dmrs_type_a_position;   /* DM-RS position for PDSCH */
    uint8_t pdcch_config_sib1;      /* PDCCH configuration for SIB1 */
    uint8_t cell_barred;            /* Cell barred status */
    uint8_t intra_freq_reselection; /* Intra-frequency reselection allowed */
    uint8_t spare;                  /* Spare bits */
} rrc_mib_t;

/* PLMN Identity */
typedef struct {
    uint16_t mcc;                   /* Mobile Country Code */
    uint16_t mnc;                   /* Mobile Network Code */
    uint8_t mnc_length;             /* MNC length (2 or 3 digits) */
} rrc_plmn_id_t;

/* SIB1 Content - 3GPP TS 38.331 */
typedef struct {
    rrc_plmn_id_t plmn_id;          /* PLMN identity */
    uint16_t tac;                   /* Tracking Area Code */
    uint32_t cell_id;               /* Cell Identity */
    bool cell_reserved;             /* Cell reserved for operator use */
    uint8_t access_barring;         /* Access barring factor */
    uint16_t si_periodicity;        /* SI message periodicity (ms) */
    uint16_t si_window_length;      /* SI window length (ms) */
    uint8_t num_si_messages;        /* Number of SI messages */
    uint8_t si_scheduler_info[8];   /* SI scheduling information */
} rrc_sib1_t;

/* Cell Reselection Priority */
typedef struct {
    uint8_t priority;               /* Cell reselection priority (0-7) */
    int8_t q_rx_lev_min;            /* Minimum received power */
    int8_t q_qual_min;              /* Minimum quality */
    uint8_t thresh_x_high;          /* Threshold for high priority */
    uint8_t thresh_x_low;           /* Threshold for low priority */
} rrc_cell_resel_priority_t;

/* SIB2 Content - Cell Reselection Info */
typedef struct {
    rrc_cell_resel_priority_t intra_freq;
    uint8_t q_hyst;                 /* Hysteresis for ranking */
    uint8_t t_resel_eutra;          /* Treselection for E-UTRA */
    uint8_t s_non_intra_search;     /* Threshold for non-intra freq */
    uint8_t thresh_serving_low;     /* Serving cell threshold */
} rrc_sib2_t;

/* Neighbor Cell Info */
typedef struct {
    uint16_t pci;                   /* Physical Cell ID */
    uint32_t earfcn;                /* E-UTRA Absolute Radio Frequency */
    int8_t q_offset;                /* Q-Offset for cell */
} rrc_neighbor_cell_t;

/* SIB3 Content - Neighbor Cells */
typedef struct {
    rrc_neighbor_cell_t neighbors[32];
    uint8_t num_neighbors;
    uint32_t intra_freq_earfcn;
    uint8_t q_offset_range;         /* Q-Offset range */
} rrc_sib3_t;

/* SI Context */
typedef struct {
    rrc_mib_t mib;
    rrc_sib1_t sib1;
    rrc_sib2_t sib2;
    rrc_sib3_t sib3;
    
    bool mib_valid;
    bool sib1_valid;
    bool sib2_valid;
    bool sib3_valid;
    
    uint32_t si_expiry_time;
    uint32_t last_mib_time;
    uint32_t last_sib1_time;
    uint32_t last_si_time;
    
    uint16_t sfn;
    uint8_t subframe;
} rrc_si_context_t;

/* SI Message Types */
typedef enum {
    RRC_SI_TYPE_MIB = 0,
    RRC_SI_TYPE_SIB1 = 1,
    RRC_SI_TYPE_SIB2 = 2,
    RRC_SI_TYPE_SIB3 = 3,
    RRC_SI_TYPE_SIB4 = 4,
    RRC_SI_TYPE_SIB5 = 5,
    RRC_SI_TYPE_MAX
} rrc_si_type_t;

/* Function Prototypes */

/* SI Context Management */
uesim_error_t rrc_si_init(rrc_si_context_t* ctx);
void rrc_si_cleanup(rrc_si_context_t* ctx);
uesim_error_t rrc_si_reset(rrc_si_context_t* ctx);

/* MIB Processing */
uesim_error_t rrc_decode_mib(const uint8_t* data, size_t len, rrc_mib_t* mib);
uesim_error_t rrc_handle_mib(ue_context_t* ue_ctx, const uint8_t* data, size_t len);
bool rrc_is_mib_valid(rrc_si_context_t* ctx);

/* SIB1 Processing */
uesim_error_t rrc_decode_sib1(const uint8_t* data, size_t len, rrc_sib1_t* sib1);
uesim_error_t rrc_handle_sib1(ue_context_t* ue_ctx, const uint8_t* data, size_t len);
bool rrc_is_sib1_valid(rrc_si_context_t* ctx);

/* SIB2 Processing */
uesim_error_t rrc_decode_sib2(const uint8_t* data, size_t len, rrc_sib2_t* sib2);
uesim_error_t rrc_handle_sib2(ue_context_t* ue_ctx, const uint8_t* data, size_t len);

/* SIB3 Processing */
uesim_error_t rrc_decode_sib3(const uint8_t* data, size_t len, rrc_sib3_t* sib3);
uesim_error_t rrc_handle_sib3(ue_context_t* ue_ctx, const uint8_t* data, size_t len);

/* SI Message Dispatcher */
uesim_error_t rrc_process_si_message(ue_context_t* ue_ctx, rrc_si_type_t type,
                                    const uint8_t* data, size_t len);

/* SI Timer Management */
uesim_error_t rrc_update_si_timers(rrc_si_context_t* ctx, uint32_t current_time_ms);
bool rrc_is_si_valid(rrc_si_context_t* ctx);

/* SI Utility Functions */
uint16_t rrc_get_sfn_from_mib(const uint8_t* mib_data);
uint8_t rrc_get_sfn_increment(uint16_t* sfn, uint8_t* subframe);

#endif /* RRC_SI_H */