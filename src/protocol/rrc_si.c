/*
 * 5G UE Simulation Application
 * RRC System Information Processing Implementation
 */

#include "rrc_si.h"
#include "asn1_per.h"
#include "../core/memory.h"
#include <string.h>
#include <stdio.h>

/* Platform-specific time */
#ifdef _WIN32
#include <windows.h>
static uint32_t get_current_time_ms(void) { return (uint32_t)GetTickCount(); }
#else
#include <sys/time.h>
static uint32_t get_current_time_ms(void) {
    struct timeval tv; gettimeofday(&tv, NULL);
    return (uint32_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}
#endif

/* SI Context Management */
uesim_error_t rrc_si_init(rrc_si_context_t* ctx) {
    if (!ctx) return UESIM_ERROR_INVALID_PARAM;
    memset(ctx, 0, sizeof(rrc_si_context_t));
    ctx->mib_valid = false;
    ctx->sib1_valid = false;
    ctx->sib2_valid = false;
    ctx->sib3_valid = false;
    printf("RRC SI: Context initialized\n");
    return UESIM_SUCCESS;
}

void rrc_si_cleanup(rrc_si_context_t* ctx) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(rrc_si_context_t));
    printf("RRC SI: Context cleaned up\n");
}

uesim_error_t rrc_si_reset(rrc_si_context_t* ctx) {
    if (!ctx) return UESIM_ERROR_INVALID_PARAM;
    ctx->mib_valid = false;
    ctx->sib1_valid = false;
    ctx->sib2_valid = false;
    ctx->sib3_valid = false;
    ctx->si_expiry_time = 0;
    printf("RRC SI: Context reset\n");
    return UESIM_SUCCESS;
}

/* MIB Processing - 3GPP TS 38.331 Section 6.2.2 */
uesim_error_t rrc_decode_mib(const uint8_t* data, size_t len, rrc_mib_t* mib) {
    if (!data || !mib || len < 4) return UESIM_ERROR_INVALID_PARAM;
    
    size_t bit_offset = 0;
    
    /* SFN - 10 bits */
    uint32_t sfn;
    asn1_decode_bits(data, &bit_offset, &sfn, 10);
    mib->sfn = (uint16_t)sfn;
    
    /* Subcarrier spacing for SIB1 - 1 bit */
    uint32_t scs;
    asn1_decode_bits(data, &bit_offset, &scs, 1);
    mib->subcarrier_spacing = (uint8_t)scs;
    
    /* DM-RS Type A position - 1 bit */
    uint32_t dmrs;
    asn1_decode_bits(data, &bit_offset, &dmrs, 1);
    mib->dmrs_type_a_position = (uint8_t)dmrs;
    
    /* PDCCH Config SIB1 - 8 bits */
    uint32_t pdcch;
    asn1_decode_bits(data, &bit_offset, &pdcch, 8);
    mib->pdcch_config_sib1 = (uint8_t)pdcch;
    
    /* Cell Barred - 1 bit */
    uint32_t barred;
    asn1_decode_bits(data, &bit_offset, &barred, 1);
    mib->cell_barred = (uint8_t)barred;
    
    /* Intra-frequency Reselection - 1 bit */
    uint32_t reselection;
    asn1_decode_bits(data, &bit_offset, &reselection, 1);
    mib->intra_freq_reselection = (uint8_t)reselection;
    
    /* Spare - 12 bits */
    uint32_t spare;
    asn1_decode_bits(data, &bit_offset, &spare, 12);
    mib->spare = (uint8_t)(spare >> 4);
    
    return UESIM_SUCCESS;
}

uesim_error_t rrc_handle_mib(ue_context_t* ue_ctx, const uint8_t* data, size_t len) {
    if (!ue_ctx || !data || len < 4) return UESIM_ERROR_INVALID_PARAM;
    
    rrc_si_context_t* si_ctx = ue_ctx->rrc_si_ctx;
    rrc_mib_t mib;
    
    uesim_error_t ret = rrc_decode_mib(data, len, &mib);
    if (ret != UESIM_SUCCESS) return ret;
    
    si_ctx->mib = mib;
    si_ctx->mib_valid = true;
    si_ctx->last_mib_time = get_current_time_ms();
    si_ctx->sfn = mib.sfn;
    
    printf("RRC SI: MIB received - SFN=%u, SCS=%u, CellBarred=%u\n",
           mib.sfn, mib.subcarrier_spacing, mib.cell_barred);
    
    /* Check if cell is barred */
    if (mib.cell_barred) {
        printf("RRC SI: Cell is barred!\n");
        return UESIM_ERROR_PROTOCOL;
    }
    
    return UESIM_SUCCESS;
}

bool rrc_is_mib_valid(rrc_si_context_t* ctx) {
    if (!ctx) return false;
    
    /* MIB validity period is 160ms */
    uint32_t now = get_current_time_ms();
    return ctx->mib_valid && (now - ctx->last_mib_time < 160);
}

/* SIB1 Processing - 3GPP TS 38.331 Section 6.2.2 */
uesim_error_t rrc_decode_sib1(const uint8_t* data, size_t len, rrc_sib1_t* sib1) {
    if (!data || !sib1 || len < 8) return UESIM_ERROR_INVALID_PARAM;
    
    memset(sib1, 0, sizeof(rrc_sib1_t));
    size_t bit_offset = 0;
    
    /* PLMN ID (simplified) - 24 bits for MCC+MNC */
    uint32_t plmn;
    asn1_decode_bits(data, &bit_offset, &plmn, 24);
    sib1->plmn_id.mcc = (uint16_t)(plmn >> 12);
    sib1->plmn_id.mnc = (uint16_t)(plmn & 0xFFF);
    sib1->plmn_id.mnc_length = 3;
    
    /* TAC - 24 bits */
    uint32_t tac;
    asn1_decode_bits(data, &bit_offset, &tac, 24);
    sib1->tac = (uint16_t)tac;
    
    /* Cell ID - 36 bits */
    uint32_t cell_high, cell_low;
    asn1_decode_bits(data, &bit_offset, &cell_high, 4);
    asn1_decode_bits(data, &bit_offset, &cell_low, 32);
    sib1->cell_id = ((uint64_t)cell_high << 32) | cell_low;
    
    /* Cell Reserved - 1 bit */
    bool reserved;
    asn1_decode_boolean(data, &bit_offset, &reserved);
    sib1->cell_reserved = reserved;
    
    /* SI Periodicity - 3 bits */
    uint32_t periodicity;
    asn1_decode_bits(data, &bit_offset, &periodicity, 3);
    sib1->si_periodicity = 8 << periodicity; /* rf8, rf16, rf32, etc. */
    
    /* SI Window Length - 3 bits */
    uint32_t window;
    asn1_decode_bits(data, &bit_offset, &window, 3);
    sib1->si_window_length = 5 << window; /* ms5, ms10, ms20, etc. */
    
    return UESIM_SUCCESS;
}

uesim_error_t rrc_handle_sib1(ue_context_t* ue_ctx, const uint8_t* data, size_t len) {
    if (!ue_ctx || !data || len < 8) return UESIM_ERROR_INVALID_PARAM;
    
    rrc_si_context_t* si_ctx = ue_ctx->rrc_si_ctx;
    rrc_sib1_t sib1;
    
    uesim_error_t ret = rrc_decode_sib1(data, len, &sib1);
    if (ret != UESIM_SUCCESS) return ret;
    
    si_ctx->sib1 = sib1;
    si_ctx->sib1_valid = true;
    si_ctx->last_sib1_time = get_current_time_ms();
    
    printf("RRC SI: SIB1 received - PLMN=%u-%u, TAC=%u, CellID=%u, Reserved=%d\n",
           sib1.plmn_id.mcc, sib1.plmn_id.mnc, sib1.tac, sib1.cell_id, sib1.cell_reserved);
    
    /* Check if cell is reserved */
    if (sib1.cell_reserved) {
        printf("RRC SI: Cell is reserved for operator use!\n");
    }
    
    return UESIM_SUCCESS;
}

bool rrc_is_sib1_valid(rrc_si_context_t* ctx) {
    if (!ctx) return false;
    
    /* SIB1 validity period is typically 600s (10 minutes) */
    uint32_t now = get_current_time_ms();
    return ctx->sib1_valid && (now - ctx->last_sib1_time < 600000);
}

/* SIB2 Processing */
uesim_error_t rrc_decode_sib2(const uint8_t* data, size_t len, rrc_sib2_t* sib2) {
    if (!data || !sib2 || len < 4) return UESIM_ERROR_INVALID_PARAM;
    
    memset(sib2, 0, sizeof(rrc_sib2_t));
    size_t bit_offset = 0;
    
    /* Intra-frequency reselection priority */
    uint32_t priority;
    asn1_decode_bits(data, &bit_offset, &priority, 3);
    sib2->intra_freq.priority = (uint8_t)priority;
    
    /* Q-RxLevMin - 6 bits (signed, range -70 to -22) */
    uint32_t q_rx_lev;
    asn1_decode_bits(data, &bit_offset, &q_rx_lev, 6);
    sib2->intra_freq.q_rx_lev_min = (int8_t)(q_rx_lev) - 70;
    
    /* Q-QualMin - 5 bits (signed, range -34 to -3) */
    uint32_t q_qual;
    asn1_decode_bits(data, &bit_offset, &q_qual, 5);
    sib2->intra_freq.q_qual_min = (int8_t)(q_qual) - 34;
    
    /* Q-Hyst - 4 bits */
    uint32_t q_hyst;
    asn1_decode_bits(data, &bit_offset, &q_hyst, 4);
    sib2->q_hyst = (uint8_t)q_hyst;
    
    /* Treselection-EUTRA - 3 bits */
    uint32_t t_resel;
    asn1_decode_bits(data, &bit_offset, &t_resel, 3);
    sib2->t_resel_eutra = (uint8_t)t_resel;
    
    return UESIM_SUCCESS;
}

uesim_error_t rrc_handle_sib2(ue_context_t* ue_ctx, const uint8_t* data, size_t len) {
    if (!ue_ctx || !data || len < 4) return UESIM_ERROR_INVALID_PARAM;
    
    rrc_si_context_t* si_ctx = ue_ctx->rrc_si_ctx;
    rrc_sib2_t sib2;
    
    uesim_error_t ret = rrc_decode_sib2(data, len, &sib2);
    if (ret != UESIM_SUCCESS) return ret;
    
    si_ctx->sib2 = sib2;
    si_ctx->sib2_valid = true;
    si_ctx->last_si_time = get_current_time_ms();
    
    printf("RRC SI: SIB2 received - Priority=%u, Q_Hyst=%u\n",
           sib2.intra_freq.priority, sib2.q_hyst);
    return UESIM_SUCCESS;
}

/* SIB3 Processing */
uesim_error_t rrc_decode_sib3(const uint8_t* data, size_t len, rrc_sib3_t* sib3) {
    if (!data || !sib3 || len < 4) return UESIM_ERROR_INVALID_PARAM;
    
    memset(sib3, 0, sizeof(rrc_sib3_t));
    size_t bit_offset = 0;
    
    /* Intra-frequency EARFCN - 18 bits */
    uint32_t earfcn;
    asn1_decode_bits(data, &bit_offset, &earfcn, 18);
    sib3->intra_freq_earfcn = earfcn;
    
    /* Q-Offset Range - 5 bits */
    uint32_t q_offset_range;
    asn1_decode_bits(data, &bit_offset, &q_offset_range, 5);
    sib3->q_offset_range = (uint8_t)q_offset_range;
    
    /* Number of neighbor cells - 4 bits */
    uint32_t num_neighbors;
    asn1_decode_bits(data, &bit_offset, &num_neighbors, 4);
    sib3->num_neighbors = (num_neighbors > 32) ? 32 : (uint8_t)num_neighbors;
    
    /* Parse neighbor cells */
    for (uint8_t i = 0; i < sib3->num_neighbors; i++) {
        uint32_t pci;
        asn1_decode_bits(data, &bit_offset, &pci, 16);
        sib3->neighbors[i].pci = (uint16_t)pci;
        
        uint32_t neighbor_earfcn;
        asn1_decode_bits(data, &bit_offset, &neighbor_earfcn, 18);
        sib3->neighbors[i].earfcn = neighbor_earfcn;
        
        uint32_t q_offset;
        asn1_decode_bits(data, &bit_offset, &q_offset, 5);
        sib3->neighbors[i].q_offset = (int8_t)q_offset - 16;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t rrc_handle_sib3(ue_context_t* ue_ctx, const uint8_t* data, size_t len) {
    if (!ue_ctx || !data || len < 4) return UESIM_ERROR_INVALID_PARAM;
    
    rrc_si_context_t* si_ctx = ue_ctx->rrc_si_ctx;
    rrc_sib3_t sib3;
    
    uesim_error_t ret = rrc_decode_sib3(data, len, &sib3);
    if (ret != UESIM_SUCCESS) return ret;
    
    si_ctx->sib3 = sib3;
    si_ctx->sib3_valid = true;
    si_ctx->last_si_time = get_current_time_ms();
    
    printf("RRC SI: SIB3 received - EARFCN=%u, Neighbors=%u\n",
           sib3.intra_freq_earfcn, sib3.num_neighbors);
    return UESIM_SUCCESS;
}

/* SI Message Dispatcher */
uesim_error_t rrc_process_si_message(ue_context_t* ue_ctx, rrc_si_type_t type,
                                    const uint8_t* data, size_t len) {
    if (!ue_ctx || !data || len == 0) return UESIM_ERROR_INVALID_PARAM;
    
    switch (type) {
        case RRC_SI_TYPE_MIB:
            return rrc_handle_mib(ue_ctx, data, len);
        case RRC_SI_TYPE_SIB1:
            return rrc_handle_sib1(ue_ctx, data, len);
        case RRC_SI_TYPE_SIB2:
            return rrc_handle_sib2(ue_ctx, data, len);
        case RRC_SI_TYPE_SIB3:
            return rrc_handle_sib3(ue_ctx, data, len);
        default:
            printf("RRC SI: Unknown SI type %d\n", type);
            return UESIM_ERROR_INVALID_PARAM;
    }
}

/* SI Timer Management */
uesim_error_t rrc_update_si_timers(rrc_si_context_t* ctx, uint32_t current_time_ms) {
    if (!ctx) return UESIM_ERROR_INVALID_PARAM;
    
    /* Check MIB validity */
    if (ctx->mib_valid && (current_time_ms - ctx->last_mib_time > 160)) {
        printf("RRC SI: MIB expired\n");
        ctx->mib_valid = false;
    }
    
    /* Check SIB1 validity (600 seconds) */
    if (ctx->sib1_valid && (current_time_ms - ctx->last_sib1_time > 600000)) {
        printf("RRC SI: SIB1 expired\n");
        ctx->sib1_valid = false;
    }
    
    /* Check SI validity (1800 seconds) */
    if (ctx->sib2_valid && (current_time_ms - ctx->last_si_time > 1800000)) {
        printf("RRC SI: SIB2/SIB3 expired\n");
        ctx->sib2_valid = false;
        ctx->sib3_valid = false;
    }
    
    return UESIM_SUCCESS;
}

bool rrc_is_si_valid(rrc_si_context_t* ctx) {
    if (!ctx) return false;
    return ctx->mib_valid && ctx->sib1_valid;
}

/* SI Utility Functions */
uint16_t rrc_get_sfn_from_mib(const uint8_t* mib_data) {
    if (!mib_data) return 0;
    
    /* SFN is first 10 bits of MIB */
    size_t bit_offset = 0;
    uint32_t sfn;
    asn1_decode_bits(mib_data, &bit_offset, &sfn, 10);
    return (uint16_t)sfn;
}

uint8_t rrc_get_sfn_increment(uint16_t* sfn, uint8_t* subframe) {
    if (!sfn || !subframe) return 0;
    
    (*subframe)++;
    if (*subframe >= 10) {
        *subframe = 0;
        (*sfn)++;
        if (*sfn >= 1024) {
            *sfn = 0;
        }
        return 1; /* SFN wrapped */
    }
    return 0;
}