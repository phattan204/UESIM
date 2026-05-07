/*
 * 5G UE Simulation Application
 * PHY (Physical) Layer Abstraction Header
 * 
 * Provides abstracted interface for physical layer operations
 */

#ifndef PHY_H
#define PHY_H

#include "../uesim.h"

/* PHY Constants */
#define PHY_MAX_RB              273     /* Maximum Resource Blocks (100MHz) */
#define PHY_MAX_CARRIERS        16      /* Maximum component carriers */
#define PHY_MAX_LAYERS          8       /* Maximum MIMO layers */
#define PHY_MAX_HARQ_PROC       16      /* Maximum HARQ processes */

/* Subcarrier Spacing (kHz) */
typedef enum {
    PHY_SCS_15 = 15,
    PHY_SCS_30 = 30,
    PHY_SCS_60 = 60,
    PHY_SCS_120 = 120
} phy_scs_t;

/* Duplex Mode */
typedef enum {
    PHY_DUPLEX_FDD = 0,
    PHY_DUPLEX_TDD = 1
} phy_duplex_t;

/* Modulation Scheme */
typedef enum {
    PHY_MOD_BPSK = 0,
    PHY_MOD_QPSK = 1,
    PHY_MOD_16QAM = 2,
    PHY_MOD_64QAM = 3,
    PHY_MOD_256QAM = 4
} phy_modulation_t;

/* Channel State */
typedef struct {
    int16_t rsrp;               /* Reference Signal Received Power (dBm) */
    int16_t rsrq;               /* Reference Signal Received Quality (dB) */
    int16_t sinr;               /* Signal to Interference + Noise Ratio (dB) */
    int16_t cqi;                /* Channel Quality Indicator (0-15) */
    uint8_t ri;                 /* Rank Indicator (1-8) */
    uint8_t pmi;                /* Precoding Matrix Indicator */
} phy_channel_state_t;

/* Resource Block Allocation */
typedef struct {
    uint16_t start_rb;          /* Starting RB index */
    uint16_t num_rb;            /* Number of RBs allocated */
    uint8_t start_symbol;       /* Starting OFDM symbol */
    uint8_t num_symbols;        /* Number of symbols */
    phy_modulation_t modulation;/* Modulation scheme */
    uint8_t tbs;                /* Transport Block Size index */
} phy_rb_allocation_t;

/* HARQ Process */
typedef struct {
    uint8_t process_id;         /* HARQ process ID */
    bool active;                /* Process is active */
    bool is_downlink;           /* DL or UL HARQ */
    uint8_t ndi;                /* New Data Indicator */
    uint8_t rv;                 /* Redundancy Version */
    uint16_t harq_feedback;     /* ACK/NACK feedback */
    uint32_t tb_size;           /* Transport Block size */
    uint8_t* tb_data;           /* Transport Block data */
    uint8_t retx_count;         /* Retransmission count */
    uint8_t max_retx;           /* Maximum retransmissions */
} phy_harq_process_t;

/* Timing Advance */
typedef struct {
    uint16_t n_ta;              /* Timing Advance value (Ts units) */
    uint16_t n_ta_common;       /* Common TA for initial access */
    bool ta_valid;              /* TA value is valid */
    uint32_t ta_update_time;    /* Last TA update timestamp */
} phy_timing_advance_t;

/* Power Control */
typedef struct {
    int16_t p_max;              /* Maximum TX power (dBm) */
    int16_t p_pusch;            /* PUSCH power (dBm) */
    int16_t p_pucch;            /* PUCCH power (dBm) */
    int16_t p_srs;              /* SRS power (dBm) */
    int16_t path_loss;          /* Estimated path loss (dB) */
    int16_t p0_pusch;           /* P0 for PUSCH */
    int16_t p0_pucch;           /* P0 for PUCCH */
    uint8_t alpha;              /* Fractional power control alpha */
} phy_power_control_t;

/* PHY Cell Configuration */
typedef struct {
    uint32_t cell_id;           /* Physical Cell ID (0-1007) */
    uint32_t arfcn;             /* Absolute Radio Frequency Channel Number */
    uint16_t band;              /* Operating band */
    phy_scs_t scs;              /* Subcarrier spacing */
    phy_duplex_t duplex;        /* Duplex mode */
    uint16_t bandwidth_rb;      /* Bandwidth in RBs */
    uint32_t sfn;               /* System Frame Number */
    uint8_t subframe;           /* Subframe number */
    uint8_t slot;               /* Slot number */
    uint8_t symbol;             /* Symbol number */
} phy_cell_config_t;

/* PHY Statistics */
typedef struct {
    uint64_t tx_bytes;          /* Transmitted bytes */
    uint64_t rx_bytes;          /* Received bytes */
    uint64_t tx_packets;        /* Transmitted packets */
    uint64_t rx_packets;        /* Received packets */
    uint64_t crc_failures;      /* CRC failures */
    uint64_t harq_retx;         /* HARQ retransmissions */
    uint64_t harq_failures;     /* HARQ failures after max retx */
    uint64_t ta_updates;        /* TA update count */
    double avg_sinr;            /* Average SINR */
    double avg_rsrp;            /* Average RSRP */
    double avg_rsrq;            /* Average RSRQ */
} phy_stats_t;

/* PHY Context */
typedef struct {
    uint32_t phy_id;            /* PHY instance ID */
    
    /* Cell Configuration */
    phy_cell_config_t cell_config;
    
    /* Channel State */
    phy_channel_state_t channel;
    
    /* HARQ Processes */
    phy_harq_process_t harq_dl[PHY_MAX_HARQ_PROC];
    phy_harq_process_t harq_ul[PHY_MAX_HARQ_PROC];
    uint8_t num_dl_harq;
    uint8_t num_ul_harq;
    
    /* Timing Advance */
    phy_timing_advance_t timing_advance;
    
    /* Power Control */
    phy_power_control_t power;
    
    /* Statistics */
    phy_stats_t stats;
    
    /* State */
    bool sync;                  /* PHY synchronized */
    bool active;
    pthread_mutex_t phy_mutex;
    
} phy_context_t;

/* PHY PDU Structure */
typedef struct phy_pdu {
    uint8_t* data;
    size_t length;
    uint8_t harq_id;
    bool is_downlink;
    struct phy_pdu* next;
} phy_pdu_t;

/* Function Prototypes */

/* Initialization and Cleanup */
uesim_error_t phy_init(ue_context_t* ue_ctx);
void phy_cleanup(ue_context_t* ue_ctx);
uesim_error_t phy_create_context(phy_context_t** ctx);
void phy_destroy_context(phy_context_t* ctx);

/* Configuration */
uesim_error_t phy_configure_cell(phy_context_t* ctx, const phy_cell_config_t* config);
uesim_error_t phy_set_scs(phy_context_t* ctx, phy_scs_t scs);
uesim_error_t phy_set_bandwidth(phy_context_t* ctx, uint16_t bandwidth_rb);

/* Synchronization */
uesim_error_t phy_sync(phy_context_t* ctx);
uesim_error_t phy_sync_cell(phy_context_t* ctx, uint32_t cell_id);
bool phy_is_synchronized(phy_context_t* ctx);
uesim_error_t phy_get_timing(phy_context_t* ctx, uint32_t* sfn, uint8_t* slot, uint8_t* symbol);

/* Channel State */
uesim_error_t phy_measure_channel(phy_context_t* ctx);
uesim_error_t phy_get_channel_state(phy_context_t* ctx, phy_channel_state_t* state);
uesim_error_t phy_report_csi(phy_context_t* ctx, uint8_t cqi, uint8_t ri, uint8_t pmi);

/* Resource Allocation */
uesim_error_t phy_alloc_rb(phy_context_t* ctx, const phy_rb_allocation_t* alloc,
                          uint32_t* tbs);
uint32_t phy_calc_tbs(phy_modulation_t mod, uint16_t num_rb, uint8_t num_symbols,
                      uint8_t num_layers);

/* HARQ Management */
uesim_error_t phy_init_harq(phy_context_t* ctx, uint8_t num_dl, uint8_t num_ul);
uesim_error_t phy_harq_tx(phy_context_t* ctx, uint8_t harq_id, const uint8_t* data,
                         size_t length, bool is_downlink);
uesim_error_t phy_harq_rx(phy_context_t* ctx, uint8_t harq_id, phy_pdu_t** pdu);
uesim_error_t phy_harq_feedback(phy_context_t* ctx, uint8_t harq_id, bool ack);
uesim_error_t phy_harq_retx(phy_context_t* ctx, uint8_t harq_id);
uint8_t phy_get_available_harq(phy_context_t* ctx, bool is_downlink);

/* Timing Advance */
uesim_error_t phy_update_ta(phy_context_t* ctx, int16_t ta_offset);
uesim_error_t phy_apply_ta(phy_context_t* ctx);

/* Power Control */
uesim_error_t phy_update_power(phy_context_t* ctx);
uesim_error_t phy_set_tx_power(phy_context_t* ctx, int16_t power_dbm);
int16_t phy_get_current_power(phy_context_t* ctx);

/* Data Transfer */
uesim_error_t phy_tx_pdu(phy_context_t* ctx, const uint8_t* data, size_t length,
                        phy_pdu_t** pdu);
uesim_error_t phy_rx_pdu(phy_context_t* ctx, phy_pdu_t* pdu, uint8_t** data,
                        size_t* length);

/* Signal Quality Indication (for RLF) */
uesim_error_t phy_get_sync_status(phy_context_t* ctx, bool* in_sync);
int16_t phy_get_rsrp(phy_context_t* ctx);
int16_t phy_get_rsrq(phy_context_t* ctx);
int16_t phy_get_sinr(phy_context_t* ctx);

/* Statistics */
uesim_error_t phy_get_stats(phy_context_t* ctx, phy_stats_t* stats);
uesim_error_t phy_reset_stats(phy_context_t* ctx);

/* Utility Functions */
const char* phy_modulation_to_string(phy_modulation_t mod);
const char* phy_scs_to_string(phy_scs_t scs);

#endif /* PHY_H */