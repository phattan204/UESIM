/*
 * 5G UE Simulation Application
 * PHY (Physical) Layer Abstraction Implementation
 */

#include "phy.h"
#include "../core/memory.h"
#include "../uesim.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

/* Global PHY instance counter */
static unsigned long g_phy_counter = 0;

/* TBS lookup table (simplified) - based on 3GPP TS 38.214 */
static const uint16_t g_tbs_table[5][5] = {
    /* 1 RB, 10 RBs, 50 RBs, 100 RBs, 273 RBs */
    {24,     216,    1080,   2160,    5896},  /* QPSK, 1/2 */
    {48,     456,    2280,   4584,    12504}, /* 16QAM, 1/2 */
    {96,     936,    4680,   9360,    25536}, /* 64QAM, 1/2 */
    {144,    1416,   7080,   14160,   38688}, /* 64QAM, 3/4 */
    {192,    1872,   9360,   18720,   51072}  /* 256QAM, 3/4 */
};

/* Platform-specific atomic increment */
#ifdef _WIN32
static inline uint32_t atomic_increment(volatile unsigned long* counter) {
    return (uint32_t)InterlockedIncrement((volatile LONG*)counter) - 1;
}
#else
static inline uint32_t atomic_increment(volatile unsigned long* counter) {
    return (uint32_t)__sync_fetch_and_add(counter, 1);
}
#endif

/* Use centralized uesim_get_time_ms() from uesim.h */

const char* phy_modulation_to_string(phy_modulation_t mod) {
    switch (mod) {
        case PHY_MOD_BPSK: return "BPSK";
        case PHY_MOD_QPSK: return "QPSK";
        case PHY_MOD_16QAM: return "16QAM";
        case PHY_MOD_64QAM: return "64QAM";
        case PHY_MOD_256QAM: return "256QAM";
        default: return "UNKNOWN";
    }
}

const char* phy_scs_to_string(phy_scs_t scs) {
    switch (scs) {
        case PHY_SCS_15: return "15 kHz";
        case PHY_SCS_30: return "30 kHz";
        case PHY_SCS_60: return "60 kHz";
        case PHY_SCS_120: return "120 kHz";
        default: return "UNKNOWN";
    }
}

uesim_error_t phy_init(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Create PHY context for UE */
    phy_context_t* phy_ctx = NULL;
    uesim_error_t result = phy_create_context(&phy_ctx);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    /* Store PHY context in UE context */
    result = ue_set_phy_context(ue_ctx, phy_ctx);
    if (result != UESIM_SUCCESS) {
        phy_destroy_context(phy_ctx);
        return result;
    }
    
    printf("PHY: Initialized for UE %u (phy_id=%u)\n", ue_ctx->ue_id, phy_ctx->phy_id);
    return UESIM_SUCCESS;
}

void phy_cleanup(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) return;
    
    phy_context_t* phy_ctx = ue_get_phy_context(ue_ctx);
    if (phy_ctx != NULL) {
        phy_destroy_context(phy_ctx);
        ue_set_phy_context(ue_ctx, NULL);
    }
    
    printf("PHY: Cleanup completed for UE %u\n", ue_ctx->ue_id);
}

uesim_error_t phy_create_context(phy_context_t** ctx) {
    if (ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    phy_context_t* phy_ctx = (phy_context_t*)uesim_calloc(1, sizeof(phy_context_t));
    if (phy_ctx == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    phy_ctx->phy_id = atomic_increment(&g_phy_counter);
    phy_ctx->sync = false;
    phy_ctx->active = false;
    
    /* Default channel state */
    phy_ctx->channel.rsrp = -85;  /* dBm */
    phy_ctx->channel.rsrq = -10;  /* dB */
    phy_ctx->channel.sinr = 15;   /* dB */
    phy_ctx->channel.cqi = 10;
    phy_ctx->channel.ri = 2;
    phy_ctx->channel.pmi = 0;
    
    /* Default power control */
    phy_ctx->power.p_max = 23;    /* dBm */
    phy_ctx->power.p_pusch = 0;
    phy_ctx->power.p_pucch = 0;
    phy_ctx->power.path_loss = 100;
    phy_ctx->power.p0_pusch = -76;
    phy_ctx->power.alpha = 90;    /* 0.9 */
    
    /* Initialize mutex */
    if (pthread_mutex_init(&phy_ctx->phy_mutex, NULL) != 0) {
        uesim_free(phy_ctx);
        return UESIM_ERROR_THREAD;
    }
    
    *ctx = phy_ctx;
    printf("PHY: Context created, id=%u\n", phy_ctx->phy_id);
    return UESIM_SUCCESS;
}

void phy_destroy_context(phy_context_t* ctx) {
    if (ctx == NULL) return;
    
    /* Free HARQ buffers */
    for (int i = 0; i < PHY_MAX_HARQ_PROC; i++) {
        if (ctx->harq_dl[i].tb_data != NULL) {
            uesim_free(ctx->harq_dl[i].tb_data);
        }
        if (ctx->harq_ul[i].tb_data != NULL) {
            uesim_free(ctx->harq_ul[i].tb_data);
        }
    }
    
    pthread_mutex_destroy(&ctx->phy_mutex);
    uesim_free(ctx);
    printf("PHY: Context destroyed\n");
}

uesim_error_t phy_configure_cell(phy_context_t* ctx, const phy_cell_config_t* config) {
    if (ctx == NULL || config == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&ctx->phy_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    ctx->cell_config = *config;
    
    pthread_mutex_unlock(&ctx->phy_mutex);
    
    printf("PHY: Cell configured, PCI=%u, ARFCN=%u, Band=%u, BW=%u RB\n",
           config->cell_id, config->arfcn, config->band, config->bandwidth_rb);
    return UESIM_SUCCESS;
}

uesim_error_t phy_set_scs(phy_context_t* ctx, phy_scs_t scs) {
    if (ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    ctx->cell_config.scs = scs;
    printf("PHY: SCS set to %s\n", phy_scs_to_string(scs));
    return UESIM_SUCCESS;
}

uesim_error_t phy_set_bandwidth(phy_context_t* ctx, uint16_t bandwidth_rb) {
    if (ctx == NULL || bandwidth_rb > PHY_MAX_RB) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    ctx->cell_config.bandwidth_rb = bandwidth_rb;
    printf("PHY: Bandwidth set to %u RBs\n", bandwidth_rb);
    return UESIM_SUCCESS;
}

uesim_error_t phy_sync(phy_context_t* ctx) {
    if (ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("PHY: Synchronizing to cell %u...\n", ctx->cell_config.cell_id);
    
    /* Simulate sync time */
    ctx->sync = true;
    ctx->active = true;
    
    printf("PHY: Synchronized! SFN=%u, Slot=%u\n", ctx->cell_config.sfn, ctx->cell_config.slot);
    return UESIM_SUCCESS;
}

uesim_error_t phy_sync_cell(phy_context_t* ctx, uint32_t cell_id) {
    if (ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    ctx->cell_config.cell_id = cell_id;
    return phy_sync(ctx);
}

bool phy_is_synchronized(phy_context_t* ctx) {
    if (ctx == NULL) return false;
    return ctx->sync;
}

uesim_error_t phy_get_timing(phy_context_t* ctx, uint32_t* sfn, uint8_t* slot, uint8_t* symbol) {
    if (ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (sfn) *sfn = ctx->cell_config.sfn;
    if (slot) *slot = ctx->cell_config.slot;
    if (symbol) *symbol = ctx->cell_config.symbol;
    
    return UESIM_SUCCESS;
}

uesim_error_t phy_measure_channel(phy_context_t* ctx) {
    if (ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Simulate channel measurements with some variation */
    int variation = (rand() % 10) - 5;
    
    ctx->channel.rsrp = -85 + variation;
    ctx->channel.rsrq = -10 + (variation / 2);
    ctx->channel.sinr = 15 + variation;
    
    /* Map SINR to CQI (simplified) */
    if (ctx->channel.sinr > 20) ctx->channel.cqi = 15;
    else if (ctx->channel.sinr > 15) ctx->channel.cqi = 12;
    else if (ctx->channel.sinr > 10) ctx->channel.cqi = 9;
    else if (ctx->channel.sinr > 5) ctx->channel.cqi = 6;
    else ctx->channel.cqi = 3;
    
    /* Update statistics */
    ctx->stats.avg_rsrp = (ctx->stats.avg_rsrp * 7 + ctx->channel.rsrp) / 8;
    ctx->stats.avg_rsrq = (ctx->stats.avg_rsrq * 7 + ctx->channel.rsrq) / 8;
    ctx->stats.avg_sinr = (ctx->stats.avg_sinr * 7 + ctx->channel.sinr) / 8;
    
    return UESIM_SUCCESS;
}

uesim_error_t phy_get_channel_state(phy_context_t* ctx, phy_channel_state_t* state) {
    if (ctx == NULL || state == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    *state = ctx->channel;
    return UESIM_SUCCESS;
}

uesim_error_t phy_report_csi(phy_context_t* ctx, uint8_t cqi, uint8_t ri, uint8_t pmi) {
    if (ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&ctx->phy_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    /* Update channel state with reported CSI values */
    ctx->channel.cqi = cqi;
    ctx->channel.ri = (ri > 0) ? ri : 1;  /* RI must be at least 1 */
    ctx->channel.pmi = pmi;
    
    /* Derive SINR from CQI (inverse mapping) */
    /* CQI 1-3: SINR < 5 dB, CQI 4-6: 5-10 dB, CQI 7-9: 10-15 dB, etc. */
    ctx->channel.sinr = (int16_t)((cqi - 1) * 2);  /* Approximate SINR in dB */
    
    pthread_mutex_unlock(&ctx->phy_mutex);
    
    printf("PHY: CSI report processed - CQI=%u, RI=%u, PMI=%u, derived SINR=%d dB\n", 
           cqi, ri, pmi, ctx->channel.sinr);
    return UESIM_SUCCESS;
}

uesim_error_t phy_alloc_rb(phy_context_t* ctx, const phy_rb_allocation_t* alloc, uint32_t* tbs) {
    if (ctx == NULL || alloc == NULL || tbs == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    *tbs = phy_calc_tbs(alloc->modulation, alloc->num_rb, alloc->num_symbols, ctx->channel.ri);
    
    printf("PHY: Allocated %u RBs, TBS=%u bytes\n", alloc->num_rb, *tbs);
    return UESIM_SUCCESS;
}

uint32_t phy_calc_tbs(phy_modulation_t mod, uint16_t num_rb, uint8_t num_symbols, uint8_t num_layers) {
    /* Simplified TBS calculation */
    uint32_t base_tbs;
    uint8_t rb_idx;
    
    /* Map num_rb to table index */
    if (num_rb <= 1) rb_idx = 0;
    else if (num_rb <= 10) rb_idx = 1;
    else if (num_rb <= 50) rb_idx = 2;
    else if (num_rb <= 100) rb_idx = 3;
    else rb_idx = 4;
    
    /* Get base TBS */
    uint8_t mod_idx = (mod < 5) ? mod : 0;
    base_tbs = g_tbs_table[mod_idx][rb_idx];
    
    /* Scale by number of symbols (relative to 14 symbols per slot) */
    base_tbs = (base_tbs * num_symbols) / 14;
    
    /* Scale by number of layers */
    base_tbs *= (num_layers > 0) ? num_layers : 1;
    
    return base_tbs;
}

uesim_error_t phy_init_harq(phy_context_t* ctx, uint8_t num_dl, uint8_t num_ul) {
    if (ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (num_dl > PHY_MAX_HARQ_PROC) num_dl = PHY_MAX_HARQ_PROC;
    if (num_ul > PHY_MAX_HARQ_PROC) num_ul = PHY_MAX_HARQ_PROC;
    
    for (int i = 0; i < num_dl; i++) {
        ctx->harq_dl[i].process_id = i;
        ctx->harq_dl[i].is_downlink = true;
        ctx->harq_dl[i].active = false;
        ctx->harq_dl[i].retx_count = 0;
        ctx->harq_dl[i].max_retx = 4;
        ctx->harq_dl[i].tb_data = NULL;
    }
    
    for (int i = 0; i < num_ul; i++) {
        ctx->harq_ul[i].process_id = i;
        ctx->harq_ul[i].is_downlink = false;
        ctx->harq_ul[i].active = false;
        ctx->harq_ul[i].retx_count = 0;
        ctx->harq_ul[i].max_retx = 4;
        ctx->harq_ul[i].tb_data = NULL;
    }
    
    ctx->num_dl_harq = num_dl;
    ctx->num_ul_harq = num_ul;
    
    printf("PHY: Initialized %u DL + %u UL HARQ processes\n", num_dl, num_ul);
    return UESIM_SUCCESS;
}

uesim_error_t phy_harq_tx(phy_context_t* ctx, uint8_t harq_id, const uint8_t* data,
                         size_t length, bool is_downlink) {
    if (ctx == NULL || data == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    phy_harq_process_t* proc = is_downlink ? 
        &ctx->harq_dl[harq_id % ctx->num_dl_harq] :
        &ctx->harq_ul[harq_id % ctx->num_ul_harq];
    
    if (proc->tb_data != NULL) {
        uesim_free(proc->tb_data);
    }
    
    proc->tb_data = (uint8_t*)uesim_malloc(length);
    if (proc->tb_data == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    memcpy(proc->tb_data, data, length);
    proc->tb_size = (uint32_t)length;
    proc->active = true;
    proc->is_downlink = is_downlink;
    proc->ndi = (proc->retx_count == 0) ? 1 : 0;
    
    ctx->stats.tx_packets++;
    ctx->stats.tx_bytes += length;
    
    printf("PHY: HARQ %s TX, id=%u, len=%zu, NDI=%u\n",
           is_downlink ? "DL" : "UL", harq_id, length, proc->ndi);
    return UESIM_SUCCESS;
}

uesim_error_t phy_harq_rx(phy_context_t* ctx, uint8_t harq_id, phy_pdu_t** pdu) {
    if (ctx == NULL || pdu == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    phy_harq_process_t* proc = &ctx->harq_dl[harq_id % ctx->num_dl_harq];
    
    if (!proc->active || proc->tb_data == NULL) {
        return UESIM_ERROR_NOT_FOUND;
    }
    
    phy_pdu_t* new_pdu = (phy_pdu_t*)uesim_calloc(1, sizeof(phy_pdu_t));
    if (new_pdu == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    new_pdu->data = (uint8_t*)uesim_malloc(proc->tb_size);
    if (new_pdu->data == NULL) {
        uesim_free(new_pdu);
        return UESIM_ERROR_MEMORY;
    }
    
    memcpy(new_pdu->data, proc->tb_data, proc->tb_size);
    new_pdu->length = proc->tb_size;
    new_pdu->harq_id = harq_id;
    new_pdu->is_downlink = true;
    
    *pdu = new_pdu;
    
    ctx->stats.rx_packets++;
    ctx->stats.rx_bytes += proc->tb_size;
    
    return UESIM_SUCCESS;
}

uesim_error_t phy_harq_feedback(phy_context_t* ctx, uint8_t harq_id, bool ack) {
    if (ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    phy_harq_process_t* proc = &ctx->harq_dl[harq_id % ctx->num_dl_harq];
    
    if (ack) {
        printf("PHY: HARQ DL %u ACK - transmission successful\n", harq_id);
        proc->active = false;
        proc->retx_count = 0;
        if (proc->tb_data != NULL) {
            uesim_free(proc->tb_data);
            proc->tb_data = NULL;
        }
    } else {
        printf("PHY: HARQ DL %u NACK - requesting retransmission\n", harq_id);
        proc->retx_count++;
        ctx->stats.harq_retx++;
        
        if (proc->retx_count >= proc->max_retx) {
            printf("PHY: HARQ DL %u - max retransmissions reached\n", harq_id);
            ctx->stats.harq_failures++;
            proc->active = false;
            if (proc->tb_data != NULL) {
                uesim_free(proc->tb_data);
                proc->tb_data = NULL;
            }
        }
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t phy_harq_retx(phy_context_t* ctx, uint8_t harq_id) {
    if (ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    phy_harq_process_t* proc = &ctx->harq_ul[harq_id % ctx->num_ul_harq];
    
    if (!proc->active || proc->tb_data == NULL) {
        return UESIM_ERROR_NOT_FOUND;
    }
    
    proc->retx_count++;
    proc->rv = (proc->retx_count < 4) ? proc->retx_count : 3;
    
    ctx->stats.harq_retx++;
    
    printf("PHY: HARQ UL %u retransmission %u/%u, RV=%u\n",
           harq_id, proc->retx_count, proc->max_retx, proc->rv);
    
    return UESIM_SUCCESS;
}

uint8_t phy_get_available_harq(phy_context_t* ctx, bool is_downlink) {
    if (ctx == NULL) return 0xFF;
    
    uint8_t num = is_downlink ? ctx->num_dl_harq : ctx->num_ul_harq;
    phy_harq_process_t* procs = is_downlink ? ctx->harq_dl : ctx->harq_ul;
    
    for (int i = 0; i < num; i++) {
        if (!procs[i].active) {
            return i;
        }
    }
    
    return 0xFF; /* No available process */
}

uesim_error_t phy_update_ta(phy_context_t* ctx, int16_t ta_offset) {
    if (ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    ctx->timing_advance.n_ta += ta_offset;
    ctx->timing_advance.ta_valid = true;
    ctx->timing_advance.ta_update_time = uesim_get_time_ms();
    ctx->stats.ta_updates++;
    
    printf("PHY: TA updated, N_TA=%u (offset=%d)\n", ctx->timing_advance.n_ta, ta_offset);
    return UESIM_SUCCESS;
}

uesim_error_t phy_apply_ta(phy_context_t* ctx) {
    if (ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (!ctx->timing_advance.ta_valid) {
        printf("PHY: TA not valid, cannot apply\n");
        return UESIM_ERROR_NOT_INITIALIZED;
    }
    
    if (pthread_mutex_lock(&ctx->phy_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    /* Calculate timing adjustment in Ts units (basic time unit) */
    /* N_TA is in units of 16*Tc where Tc = 1/(480kHz * 4096) */
    /* Convert to actual time offset: N_TA * 16 * Tc = N_TA * 16 / (480000 * 4096) seconds */
    uint32_t n_ta = ctx->timing_advance.n_ta;
    
    /* Apply TA to UL timing - adjust transmission timing */
    /* TA is now applied, update the timestamp */
    ctx->timing_advance.ta_update_time = uesim_get_time_ms();
    
    /* Update statistics - count this as a TA update */
    ctx->stats.ta_updates++;
    
    /* Check if TA is within valid range (3GPP TS 38.213) */
    /* Max N_TA = 3846 * 16 = 61536 for normal CP */
    bool ta_in_range = (n_ta <= 61536);
    
    pthread_mutex_unlock(&ctx->phy_mutex);
    
    printf("PHY: TA applied successfully, N_TA=%u, in_range=%s\n", 
           n_ta, ta_in_range ? "yes" : "no");
    
    if (!ta_in_range) {
        printf("PHY: WARNING - TA exceeds maximum, may cause UL timing issues\n");
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t phy_update_power(phy_context_t* ctx) {
    if (ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Simplified power control */
    ctx->power.path_loss = -ctx->channel.rsrp - ctx->power.p0_pusch;
    ctx->power.p_pusch = ctx->power.p0_pusch + 
                         (ctx->power.alpha * ctx->power.path_loss / 100);
    
    /* Clamp to max power */
    if (ctx->power.p_pusch > ctx->power.p_max) {
        ctx->power.p_pusch = ctx->power.p_max;
    }
    
    printf("PHY: Power updated, P_PUSCH=%d dBm, PL=%d dB\n",
           ctx->power.p_pusch, ctx->power.path_loss);
    return UESIM_SUCCESS;
}

uesim_error_t phy_set_tx_power(phy_context_t* ctx, int16_t power_dbm) {
    if (ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (power_dbm > ctx->power.p_max) {
        power_dbm = ctx->power.p_max;
    }
    
    ctx->power.p_pusch = power_dbm;
    printf("PHY: TX power set to %d dBm\n", power_dbm);
    return UESIM_SUCCESS;
}

int16_t phy_get_current_power(phy_context_t* ctx) {
    if (ctx == NULL) return 0;
    return ctx->power.p_pusch;
}

uesim_error_t phy_tx_pdu(phy_context_t* ctx, const uint8_t* data, size_t length,
                        phy_pdu_t** pdu) {
    if (ctx == NULL || data == NULL || pdu == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    phy_pdu_t* new_pdu = (phy_pdu_t*)uesim_calloc(1, sizeof(phy_pdu_t));
    if (new_pdu == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    new_pdu->data = (uint8_t*)uesim_malloc(length);
    if (new_pdu->data == NULL) {
        uesim_free(new_pdu);
        return UESIM_ERROR_MEMORY;
    }
    
    memcpy(new_pdu->data, data, length);
    new_pdu->length = length;
    new_pdu->is_downlink = false;
    
    *pdu = new_pdu;
    
    ctx->stats.tx_packets++;
    ctx->stats.tx_bytes += length;
    
    return UESIM_SUCCESS;
}

uesim_error_t phy_rx_pdu(phy_context_t* ctx, phy_pdu_t* pdu, uint8_t** data,
                        size_t* length) {
    if (ctx == NULL || pdu == NULL || data == NULL || length == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    *data = (uint8_t*)uesim_malloc(pdu->length);
    if (*data == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    memcpy(*data, pdu->data, pdu->length);
    *length = pdu->length;
    
    ctx->stats.rx_packets++;
    ctx->stats.rx_bytes += pdu->length;
    
    return UESIM_SUCCESS;
}

uesim_error_t phy_get_sync_status(phy_context_t* ctx, bool* in_sync) {
    if (ctx == NULL || in_sync == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* In-sync if RSRP > -120 dBm and SINR > 0 */
    *in_sync = (ctx->channel.rsrp > -120) && (ctx->channel.sinr > 0);
    return UESIM_SUCCESS;
}

int16_t phy_get_rsrp(phy_context_t* ctx) {
    if (ctx == NULL) return -140;
    return ctx->channel.rsrp;
}

int16_t phy_get_rsrq(phy_context_t* ctx) {
    if (ctx == NULL) return -40;
    return ctx->channel.rsrq;
}

int16_t phy_get_sinr(phy_context_t* ctx) {
    if (ctx == NULL) return -20;
    return ctx->channel.sinr;
}

uesim_error_t phy_get_stats(phy_context_t* ctx, phy_stats_t* stats) {
    if (ctx == NULL || stats == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    *stats = ctx->stats;
    return UESIM_SUCCESS;
}

uesim_error_t phy_reset_stats(phy_context_t* ctx) {
    if (ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    memset(&ctx->stats, 0, sizeof(phy_stats_t));
    printf("PHY: Statistics reset\n");
    return UESIM_SUCCESS;
}

/* ============== L3 Filtering (TS 38.331) ============== */

/*
 * L3 Filtering formula per TS 38.331:
 * F_n = (1 - a) * F_{n-1} + a * M_n
 * where a = 2^(k/4), k is filterCoefficient
 * Default k=4 gives a=0.5
 */

uesim_error_t phy_configure_l3_filter(phy_context_t* ctx, uint8_t filter_coefficient) {
    if (ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (filter_coefficient > 9) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Calculate filter coefficient: a = 2^(k/4) */
    /* k=0 -> a=0.5, k=1 -> a=0.59, k=2 -> a=0.71, k=3 -> a=0.84, k=4 -> a=1.0 */
    static const double alpha_table[10] = {
        0.5,    /* k=0 */
        0.59,   /* k=1 */
        0.71,   /* k=2 */
        0.84,   /* k=3 */
        1.0,    /* k=4 */
        1.19,   /* k=5 */
        1.41,   /* k=6 */
        1.68,   /* k=7 */
        2.0,    /* k=8 */
        2.38    /* k=9 */
    };
    
    ctx->l3_filter.filter_coefficient = filter_coefficient;
    ctx->l3_filter.filter_configured = true;
    
    /* Initialize filtered values with current measurements */
    ctx->l3_filter.filtered_rsrp = ctx->channel.rsrp;
    ctx->l3_filter.filtered_rsrq = ctx->channel.rsrq;
    ctx->l3_filter.filtered_sinr = ctx->channel.sinr;
    
    printf("PHY: L3 filter configured, k=%u, alpha=%.2f\n", 
           filter_coefficient, alpha_table[filter_coefficient]);
    return UESIM_SUCCESS;
}

uesim_error_t phy_apply_l3_filter(phy_context_t* ctx, int16_t* filtered_rsrp,
                                   int16_t* filtered_rsrq, int16_t* filtered_sinr) {
    if (ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (!ctx->l3_filter.filter_configured) {
        /* Filter not configured, return raw values */
        if (filtered_rsrp) *filtered_rsrp = ctx->channel.rsrp;
        if (filtered_rsrq) *filtered_rsrq = ctx->channel.rsrq;
        if (filtered_sinr) *filtered_sinr = ctx->channel.sinr;
        return UESIM_SUCCESS;
    }
    
    /* Apply L3 filtering: F_n = (1 - a) * F_{n-1} + a * M_n */
    /* Using k=4 (alpha=1.0) as default - simple exponential smoothing */
    double alpha = 0.5;  /* Default smoothing factor */
    
    ctx->l3_filter.filtered_rsrp = (1.0 - alpha) * ctx->l3_filter.filtered_rsrp + 
                                    alpha * ctx->channel.rsrp;
    ctx->l3_filter.filtered_rsrq = (1.0 - alpha) * ctx->l3_filter.filtered_rsrq + 
                                    alpha * ctx->channel.rsrq;
    ctx->l3_filter.filtered_sinr = (1.0 - alpha) * ctx->l3_filter.filtered_sinr + 
                                    alpha * ctx->channel.sinr;
    
    ctx->stats.l3_filter_updates++;
    
    if (filtered_rsrp) *filtered_rsrp = (int16_t)ctx->l3_filter.filtered_rsrp;
    if (filtered_rsrq) *filtered_rsrq = (int16_t)ctx->l3_filter.filtered_rsrq;
    if (filtered_sinr) *filtered_sinr = (int16_t)ctx->l3_filter.filtered_sinr;
    
    return UESIM_SUCCESS;
}

/* ============== Radio Link Monitoring (TS 38.213) ============== */

/*
 * RLM per TS 38.213:
 * - Q_out: Out-of-sync threshold (typically -8 dB SINR)
 * - Q_in: In-sync threshold (typically -6 dB SINR)
 * - N310: Counter for out-of-sync (default 1)
 * - N311: Counter for in-sync (default 1)
 * - T310: Timer (not implemented in simulation)
 */

uesim_error_t phy_configure_rlm(phy_context_t* ctx, int16_t q_out_threshold,
                                 int16_t q_in_threshold) {
    if (ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    ctx->rlm.q_out_threshold = q_out_threshold;
    ctx->rlm.q_in_threshold = q_in_threshold;
    ctx->rlm.n310 = 0;
    ctx->rlm.n311 = 0;
    ctx->rlm.rlm_configured = true;
    
    printf("PHY: RLM configured, Q_out=%d dB, Q_in=%d dB\n", 
           q_out_threshold, q_in_threshold);
    return UESIM_SUCCESS;
}

uesim_error_t phy_evaluate_rlm(phy_context_t* ctx, bool* in_sync, bool* out_of_sync) {
    if (ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (!ctx->rlm.rlm_configured) {
        if (in_sync) *in_sync = true;
        if (out_of_sync) *out_of_sync = false;
        return UESIM_SUCCESS;
    }
    
    bool current_in_sync = false;
    bool current_out_of_sync = false;
    
    /* Evaluate against thresholds */
    if (ctx->channel.sinr < ctx->rlm.q_out_threshold) {
        /* Out-of-sync condition */
        current_out_of_sync = true;
        ctx->rlm.n310++;
        ctx->rlm.n311 = 0;  /* Reset N311 */
        ctx->stats.rlm_out_of_sync++;
        
        printf("PHY: RLM out-of-sync (SINR=%d < Q_out=%d), N310=%u\n",
               ctx->channel.sinr, ctx->rlm.q_out_threshold, ctx->rlm.n310);
        
        /* Check for Radio Link Failure (N310 reaches max) */
        if (ctx->rlm.n310 >= 10) {  /* Max N310 = 10 per spec */
            ctx->stats.rlf_detected++;
            printf("PHY: RLF detected after %u out-of-sync indications\n", ctx->rlm.n310);
            ctx->rlm.n310 = 0;  /* Reset after RLF */
        }
    } else if (ctx->channel.sinr > ctx->rlm.q_in_threshold) {
        /* In-sync condition */
        current_in_sync = true;
        ctx->rlm.n311++;
        ctx->rlm.n310 = 0;  /* Reset N310 */
        ctx->stats.rlm_in_sync++;
        
        printf("PHY: RLM in-sync (SINR=%d > Q_in=%d), N311=%u\n",
               ctx->channel.sinr, ctx->rlm.q_in_threshold, ctx->rlm.n311);
    }
    
    if (in_sync) *in_sync = current_in_sync;
    if (out_of_sync) *out_of_sync = current_out_of_sync;
    
    return UESIM_SUCCESS;
}

uesim_error_t phy_get_rlm_counters(phy_context_t* ctx, uint8_t* n310, uint8_t* n311) {
    if (ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (n310) *n310 = ctx->rlm.n310;
    if (n311) *n311 = ctx->rlm.n311;
    
    return UESIM_SUCCESS;
}

/* ============== PUCCH Power Control (TS 38.213) ============== */

/*
 * PUCCH power control per TS 38.213:
 * P_PUCCH = min{P_CMAX, P0_PUCCH + PL + h(nCQI, nHARQ) + delta_F_PUCCH(F) + g(i)}
 * where:
 * - P0_PUCCH: nominal power (dBm)
 * - PL: path loss (dB)
 * - h(nCQI, nHARQ): power offset based on UCI bits
 * - delta_F_PUCCH(F): format-specific offset
 * - g(i): accumulated TPC commands
 */

uesim_error_t phy_calc_pucch_power(phy_context_t* ctx, uint8_t pucch_format,
                                    uint16_t num_prbs, int16_t* power_dbm) {
    if (ctx == NULL || power_dbm == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* P0_PUCCH nominal power (default -90 dBm) */
    int16_t p0_pucch = ctx->power.p0_pucch;
    if (p0_pucch == 0) p0_pucch = -90;
    
    /* Path loss */
    int16_t pl = ctx->power.path_loss;
    if (pl == 0) pl = 100;  /* Default 100 dB */
    
    /* Format-specific offset (simplified) */
    int16_t delta_format = 0;
    switch (pucch_format) {
        case 0: delta_format = 0; break;   /* Short, 1-2 bits */
        case 1: delta_format = 2; break;   /* Long, 1-2 bits */
        case 2: delta_format = 4; break;   /* Short, >2 bits */
        case 3: delta_format = 6; break;   /* Long, >2 bits */
        case 4: delta_format = 8; break;   /* Long, multi-slot */
        default: delta_format = 0; break;
    }
    
    /* Power offset for number of PRBs (simplified) */
    int16_t delta_prb = (num_prbs > 0) ? (10 * (int16_t)log10(num_prbs)) : 0;
    
    /* Calculate PUCCH power */
    int16_t p_pucch = p0_pucch + pl + delta_format + delta_prb;
    
    /* Clamp to maximum power */
    if (p_pucch > ctx->power.p_max) {
        p_pucch = ctx->power.p_max;
    }
    
    ctx->power.p_pucch = p_pucch;
    *power_dbm = p_pucch;
    
    printf("PHY: PUCCH power calculated, format=%u, PRBs=%u, P=%d dBm\n",
           pucch_format, num_prbs, p_pucch);
    return UESIM_SUCCESS;
}

/* ============== SRS Power Control (TS 38.213) ============== */

/*
 * SRS power control per TS 38.213:
 * P_SRS = min{P_CMAX, P0_SRS + PL + alpha * PL + 10*log10(M_SRS) + f_SRS(i)}
 * where:
 * - P0_SRS: nominal power (dBm)
 * - PL: path loss (dB)
 * - alpha: fractional path loss compensation
 * - M_SRS: SRS bandwidth in PRBs
 * - f_SRS(i): accumulated TPC commands
 */

uesim_error_t phy_calc_srs_power(phy_context_t* ctx, uint16_t num_prbs,
                                  int16_t* power_dbm) {
    if (ctx == NULL || power_dbm == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* P0_SRS nominal power (default -84 dBm) */
    int16_t p0_srs = ctx->power.p0_pusch;  /* Use P0_PUSCH as reference */
    if (p0_srs == 0) p0_srs = -84;
    
    /* Path loss */
    int16_t pl = ctx->power.path_loss;
    if (pl == 0) pl = 100;  /* Default 100 dB */
    
    /* Fractional path loss compensation (alpha) */
    uint8_t alpha = ctx->power.alpha;
    double alpha_factor = alpha / 100.0;  /* alpha is stored as percentage */
    
    /* Power offset for SRS bandwidth */
    int16_t delta_m = (num_prbs > 0) ? (10 * (int16_t)log10(num_prbs)) : 0;
    
    /* Calculate SRS power */
    int16_t p_srs = p0_srs + pl + (int16_t)(alpha_factor * pl) + delta_m;
    
    /* Clamp to maximum power */
    if (p_srs > ctx->power.p_max) {
        p_srs = ctx->power.p_max;
    }
    
    ctx->power.p_srs = p_srs;
    *power_dbm = p_srs;
    
    printf("PHY: SRS power calculated, PRBs=%u, P=%d dBm\n", num_prbs, p_srs);
    return UESIM_SUCCESS;
}
