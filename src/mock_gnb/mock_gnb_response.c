/*
 * 5G UE Simulation Application
 * Mock gNB Response Generator Implementation
 * 
 * Uses 3GPP ASN.1 PER encoding for RRC messages via asn1_per module.
 */

#include "mock_gnb_response.h"
#include "../protocol/asn1_per.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============== Message Format ============== */

/* 
 * Message format for mock testing:
 * [2 bytes: category][2 bytes: type][4 bytes: transaction_id][4 bytes: data_len][data]
 */

typedef struct {
    uint16_t category;
    uint16_t type;
    uint32_t transaction_id;
    uint32_t data_len;
    uint8_t data[];
} mock_response_header_t;

/* ============== Internal Helpers ============== */

static mock_gnb_error_t build_response(mock_msg_category_t category,
                                       uint16_t type,
                                       uint32_t transaction_id,
                                       const void* data,
                                       size_t data_len,
                                       void** response,
                                       size_t* len) {
    size_t total_len = sizeof(mock_response_header_t) + data_len;
    void* buf = malloc(total_len);
    if (buf == NULL) {
        return MOCK_GNB_ERROR_MEMORY;
    }
    
    mock_response_header_t* hdr = (mock_response_header_t*)buf;
    hdr->category = htons(category);
    hdr->type = htons(type);
    hdr->transaction_id = htonl(transaction_id);
    hdr->data_len = htonl((uint32_t)data_len);
    
    if (data_len > 0 && data != NULL) {
        memcpy(hdr->data, data, data_len);
    }
    
    *response = buf;
    *len = total_len;
    return MOCK_GNB_SUCCESS;
}

/* ============== Response Generator API ============== */

mock_gnb_error_t mock_gnb_response_init(void) {
    /* Initialize any global state if needed */
    return MOCK_GNB_SUCCESS;
}

void mock_gnb_response_cleanup(void) {
    /* Cleanup any global state if needed */
}

/* ============== DU Layer Response Generators ============== */

mock_gnb_error_t mock_gnb_generate_mac_rar(const mock_response_context_t* ctx,
                                           const mock_mac_rar_data_t* rar_data,
                                           void** response, size_t* len) {
    /* MAC RAR PDU format:
     * [1 byte: E/T/RAPID][2 bytes: TA][2 bytes: TC-RNTI][4 bytes: UL Grant][1 byte: Backoff]
     */
    uint8_t data[10];
    data[0] = 0x80 | (rar_data->preamble_id & 0x3F);  /* E=1, T=0, RAPID */
    data[1] = (rar_data->timing_advance >> 4) & 0xFF;
    data[2] = ((rar_data->timing_advance & 0x0F) << 4) | ((rar_data->temp_c_rnti >> 8) & 0x0F);
    data[3] = rar_data->temp_c_rnti & 0xFF;
    data[4] = (rar_data->ul_grant >> 24) & 0xFF;
    data[5] = (rar_data->ul_grant >> 16) & 0xFF;
    data[6] = (rar_data->ul_grant >> 8) & 0xFF;
    data[7] = rar_data->ul_grant & 0xFF;
    data[8] = rar_data->backoff_indicator & 0x0F;
    data[9] = 0;  /* Padding */
    
    return build_response(MOCK_MSG_CAT_DU, MOCK_MAC_RAR, ctx->transaction_id,
                          data, sizeof(data), response, len);
}

mock_gnb_error_t mock_gnb_generate_mac_ul_grant(const mock_response_context_t* ctx,
                                                uint16_t tb_size, uint8_t mcs,
                                                void** response, size_t* len) {
    /* UL Grant format: [2 bytes: TB size][1 byte: MCS][1 byte: params] */
    uint8_t data[4];
    data[0] = (tb_size >> 8) & 0xFF;
    data[1] = tb_size & 0xFF;
    data[2] = mcs;
    data[3] = 0;  /* Additional params (NDI, RV, etc.) */
    
    return build_response(MOCK_MSG_CAT_DU, MOCK_MAC_UL_GRANT, ctx->transaction_id,
                          data, sizeof(data), response, len);
}

mock_gnb_error_t mock_gnb_generate_rlc_status(const mock_response_context_t* ctx,
                                              uint16_t ack_sn,
                                              const uint16_t* nack_sn_list,
                                              uint8_t nack_count,
                                              void** response, size_t* len) {
    /* RLC Status PDU format: [2 bytes: ACK_SN][NACK entries...] */
    size_t data_len = 2 + (nack_count * 3);  /* Each NACK: SN + SO */
    uint8_t* data = (uint8_t*)malloc(data_len);
    if (data == NULL) {
        return MOCK_GNB_ERROR_MEMORY;
    }
    
    data[0] = (ack_sn >> 8) & 0xFF;
    data[1] = ack_sn & 0xFF;
    
    for (uint8_t i = 0; i < nack_count; i++) {
        size_t offset = 2 + (i * 3);
        data[offset] = (nack_sn_list[i] >> 8) & 0xFF;
        data[offset + 1] = nack_sn_list[i] & 0xFF;
        data[offset + 2] = 0;  /* SO = 0 */
    }
    
    mock_gnb_error_t err = build_response(MOCK_MSG_CAT_DU, MOCK_RLC_STATUS_PDU,
                                          ctx->transaction_id, data, data_len,
                                          response, len);
    free(data);
    return err;
}

/* ============== CU Layer Response Generators (RRC) ============== */

mock_gnb_error_t mock_gnb_generate_rrc_setup_full(const mock_response_context_t* ctx,
                                                   const mock_rrc_setup_data_t* rrc_data,
                                                   void** response, size_t* len) {
    /* RRC Setup message format:
     * [1 byte: RRC Transaction ID][1 byte: config_len][config][1 byte: cell_group_len][cell_group][nas_pdu]
     */
    size_t data_len = 2 + rrc_data->radio_bearer_config_len + 
                      1 + rrc_data->master_cell_group_len +
                      2 + rrc_data->dedicated_nas_pdu_len;
    uint8_t* data = (uint8_t*)malloc(data_len);
    if (data == NULL) {
        return MOCK_GNB_ERROR_MEMORY;
    }
    
    size_t offset = 0;
    data[offset++] = rrc_data->rrc_transaction_id;
    data[offset++] = (uint8_t)rrc_data->radio_bearer_config_len;
    if (rrc_data->radio_bearer_config_len > 0) {
        memcpy(&data[offset], rrc_data->radio_bearer_config, rrc_data->radio_bearer_config_len);
        offset += rrc_data->radio_bearer_config_len;
    }
    data[offset++] = (uint8_t)rrc_data->master_cell_group_len;
    if (rrc_data->master_cell_group_len > 0) {
        memcpy(&data[offset], rrc_data->master_cell_group, rrc_data->master_cell_group_len);
        offset += rrc_data->master_cell_group_len;
    }
    data[offset++] = (uint8_t)(rrc_data->dedicated_nas_pdu_len >> 8);
    data[offset++] = (uint8_t)rrc_data->dedicated_nas_pdu_len;
    if (rrc_data->dedicated_nas_pdu_len > 0) {
        memcpy(&data[offset], rrc_data->dedicated_nas_pdu, rrc_data->dedicated_nas_pdu_len);
        offset += rrc_data->dedicated_nas_pdu_len;
    }
    
    mock_gnb_error_t err = build_response(MOCK_MSG_CAT_CU, MOCK_RRC_SETUP,
                                          ctx->transaction_id, data, offset,
                                          response, len);
    free(data);
    return err;
}

mock_gnb_error_t mock_gnb_generate_rrc_reject(const mock_response_context_t* ctx,
                                              uint8_t wait_time,
                                              void** response, size_t* len) {
    uint8_t data[1];
    data[0] = wait_time;
    
    return build_response(MOCK_MSG_CAT_CU, MOCK_RRC_REJECT, ctx->transaction_id,
                          data, sizeof(data), response, len);
}

mock_gnb_error_t mock_gnb_generate_rrc_reconfiguration(const mock_response_context_t* ctx,
                                                       const mock_rrc_reconfig_data_t* reconfig_data,
                                                       void** response, size_t* len) {
    /* RRC Reconfiguration format */
    size_t data_len = 1 + 2 + reconfig_data->radio_bearer_config_len +
                      2 + reconfig_data->secondary_cell_group_len +
                      2 + reconfig_data->meas_config_len +
                      2 + reconfig_data->dedicated_nas_pdu_len;
    uint8_t* data = (uint8_t*)malloc(data_len);
    if (data == NULL) {
        return MOCK_GNB_ERROR_MEMORY;
    }
    
    size_t offset = 0;
    data[offset++] = reconfig_data->rrc_transaction_id;
    
    /* Radio Bearer Config */
    data[offset++] = (reconfig_data->radio_bearer_config_len >> 8) & 0xFF;
    data[offset++] = reconfig_data->radio_bearer_config_len & 0xFF;
    if (reconfig_data->radio_bearer_config_len > 0) {
        memcpy(&data[offset], reconfig_data->radio_bearer_config, reconfig_data->radio_bearer_config_len);
        offset += reconfig_data->radio_bearer_config_len;
    }
    
    /* Secondary Cell Group */
    data[offset++] = (reconfig_data->secondary_cell_group_len >> 8) & 0xFF;
    data[offset++] = reconfig_data->secondary_cell_group_len & 0xFF;
    if (reconfig_data->secondary_cell_group_len > 0) {
        memcpy(&data[offset], reconfig_data->secondary_cell_group, reconfig_data->secondary_cell_group_len);
        offset += reconfig_data->secondary_cell_group_len;
    }
    
    /* Measurement Config */
    data[offset++] = (reconfig_data->meas_config_len >> 8) & 0xFF;
    data[offset++] = reconfig_data->meas_config_len & 0xFF;
    if (reconfig_data->meas_config_len > 0) {
        memcpy(&data[offset], reconfig_data->meas_config, reconfig_data->meas_config_len);
        offset += reconfig_data->meas_config_len;
    }
    
    /* Dedicated NAS PDU */
    data[offset++] = (reconfig_data->dedicated_nas_pdu_len >> 8) & 0xFF;
    data[offset++] = reconfig_data->dedicated_nas_pdu_len & 0xFF;
    if (reconfig_data->dedicated_nas_pdu_len > 0) {
        memcpy(&data[offset], reconfig_data->dedicated_nas_pdu, reconfig_data->dedicated_nas_pdu_len);
        offset += reconfig_data->dedicated_nas_pdu_len;
    }
    
    mock_gnb_error_t err = build_response(MOCK_MSG_CAT_CU, MOCK_RRC_RECONFIGURATION,
                                          ctx->transaction_id, data, offset,
                                          response, len);
    free(data);
    return err;
}

mock_gnb_error_t mock_gnb_generate_rrc_security_mode(const mock_response_context_t* ctx,
                                                     uint8_t ciphering_alg,
                                                     uint8_t integrity_alg,
                                                     void** response, size_t* len) {
    uint8_t data[2];
    data[0] = ciphering_alg;
    data[1] = integrity_alg;
    
    return build_response(MOCK_MSG_CAT_CU, MOCK_RRC_SECURITY_MODE_COMMAND,
                          ctx->transaction_id, data, sizeof(data), response, len);
}

mock_gnb_error_t mock_gnb_generate_rrc_handover(const mock_response_context_t* ctx,
                                                 const mock_rrc_handover_data_t* ho_data,
                                                 void** response, size_t* len) {
    size_t data_len = 1 + 2 + 4 + 2 + 1 + 2 + ho_data->radio_bearer_config_len +
                      1 + ho_data->rach_config_len;
    uint8_t* data = (uint8_t*)malloc(data_len);
    if (data == NULL) {
        return MOCK_GNB_ERROR_MEMORY;
    }
    
    size_t offset = 0;
    data[offset++] = ho_data->rrc_transaction_id;
    data[offset++] = (ho_data->target_pci >> 8) & 0xFF;
    data[offset++] = ho_data->target_pci & 0xFF;
    data[offset++] = (ho_data->target_cell_id >> 24) & 0xFF;
    data[offset++] = (ho_data->target_cell_id >> 16) & 0xFF;
    data[offset++] = (ho_data->target_cell_id >> 8) & 0xFF;
    data[offset++] = ho_data->target_cell_id & 0xFF;
    data[offset++] = (ho_data->new_c_rnti >> 8) & 0xFF;
    data[offset++] = ho_data->new_c_rnti & 0xFF;
    data[offset++] = (uint8_t)ho_data->radio_bearer_config_len;
    if (ho_data->radio_bearer_config_len > 0) {
        memcpy(&data[offset], ho_data->radio_bearer_config, ho_data->radio_bearer_config_len);
        offset += ho_data->radio_bearer_config_len;
    }
    data[offset++] = (uint8_t)ho_data->rach_config_len;
    if (ho_data->rach_config_len > 0) {
        memcpy(&data[offset], ho_data->rach_config, ho_data->rach_config_len);
        offset += ho_data->rach_config_len;
    }
    
    mock_gnb_error_t err = build_response(MOCK_MSG_CAT_CU, MOCK_RRC_HANDOVER_COMMAND,
                                          ctx->transaction_id, data, offset,
                                          response, len);
    free(data);
    return err;
}

mock_gnb_error_t mock_gnb_generate_rrc_release(const mock_response_context_t* ctx,
                                               uint8_t release_cause,
                                               void** response, size_t* len) {
    uint8_t data[1];
    data[0] = release_cause;
    
    return build_response(MOCK_MSG_CAT_CU, MOCK_RRC_RELEASE, ctx->transaction_id,
                          data, sizeof(data), response, len);
}

mock_gnb_error_t mock_gnb_generate_rrc_meas_config(const mock_response_context_t* ctx,
                                                   const uint8_t* meas_config,
                                                   size_t meas_config_len,
                                                   void** response, size_t* len) {
    return build_response(MOCK_MSG_CAT_CU, MOCK_RRC_MEAS_CONFIG, ctx->transaction_id,
                          meas_config, meas_config_len, response, len);
}

/* ============== Core Layer Response Generators (NAS) ============== */

mock_gnb_error_t mock_gnb_generate_nas_reg_accept(const mock_response_context_t* ctx,
                                                   const mock_nas_reg_accept_data_t* reg_data,
                                                   void** response, size_t* len) {
    /* NAS Registration Accept format */
    size_t data_len = 1 + 24 + 2 + 4 + 4 + 4 + 2 + reg_data->allowed_nssai_len +
                      2 + reg_data->network_slicing_len;
    uint8_t* data = (uint8_t*)malloc(data_len);
    if (data == NULL) {
        return MOCK_GNB_ERROR_MEMORY;
    }
    
    size_t offset = 0;
    data[offset++] = reg_data->registration_result;
    
    /* GUTI */
    memcpy(&data[offset], reg_data->guti, 24);
    offset += 24;
    
    /* TAC */
    data[offset++] = (reg_data->tac >> 8) & 0xFF;
    data[offset++] = reg_data->tac & 0xFF;
    
    /* PLMN ID */
    data[offset++] = (reg_data->plmn_id >> 24) & 0xFF;
    data[offset++] = (reg_data->plmn_id >> 16) & 0xFF;
    data[offset++] = (reg_data->plmn_id >> 8) & 0xFF;
    data[offset++] = reg_data->plmn_id & 0xFF;
    
    /* T3412 timer */
    data[offset++] = (reg_data->t3412_value >> 24) & 0xFF;
    data[offset++] = (reg_data->t3412_value >> 16) & 0xFF;
    data[offset++] = (reg_data->t3412_value >> 8) & 0xFF;
    data[offset++] = reg_data->t3412_value & 0xFF;
    
    /* T3402 timer */
    data[offset++] = (reg_data->t3402_value >> 24) & 0xFF;
    data[offset++] = (reg_data->t3402_value >> 16) & 0xFF;
    data[offset++] = (reg_data->t3402_value >> 8) & 0xFF;
    data[offset++] = reg_data->t3402_value & 0xFF;
    
    /* Allowed NSSAI */
    data[offset++] = (reg_data->allowed_nssai_len >> 8) & 0xFF;
    data[offset++] = reg_data->allowed_nssai_len & 0xFF;
    if (reg_data->allowed_nssai_len > 0) {
        memcpy(&data[offset], reg_data->allowed_nssai, reg_data->allowed_nssai_len);
        offset += reg_data->allowed_nssai_len;
    }
    
    /* Network Slicing */
    data[offset++] = (reg_data->network_slicing_len >> 8) & 0xFF;
    data[offset++] = reg_data->network_slicing_len & 0xFF;
    if (reg_data->network_slicing_len > 0) {
        memcpy(&data[offset], reg_data->network_slicing, reg_data->network_slicing_len);
        offset += reg_data->network_slicing_len;
    }
    
    mock_gnb_error_t err = build_response(MOCK_MSG_CAT_CORE, MOCK_NAS_RESP_REGISTRATION_ACCEPT,
                                          ctx->transaction_id, data, offset,
                                          response, len);
    free(data);
    return err;
}

mock_gnb_error_t mock_gnb_generate_nas_reg_reject(const mock_response_context_t* ctx,
                                                   uint8_t reject_cause,
                                                   void** response, size_t* len) {
    uint8_t data[1];
    data[0] = reject_cause;
    
    return build_response(MOCK_MSG_CAT_CORE, MOCK_NAS_RESP_REGISTRATION_REJECT,
                          ctx->transaction_id, data, sizeof(data), response, len);
}

mock_gnb_error_t mock_gnb_generate_nas_auth_request(const mock_response_context_t* ctx,
                                                     const mock_nas_auth_request_data_t* auth_data,
                                                     void** response, size_t* len) {
    /* NAS Authentication Request format: [1 byte: ngksi][16 bytes: RAND][16 bytes: AUTN][1 byte: auth_type] */
    uint8_t data[34];
    data[0] = auth_data->ngksi;
    memcpy(&data[1], auth_data->rand, 16);
    memcpy(&data[17], auth_data->autn, 16);
    data[33] = auth_data->auth_type;
    
    return build_response(MOCK_MSG_CAT_CORE, MOCK_NAS_RESP_AUTHENTICATION_REQUEST,
                          ctx->transaction_id, data, sizeof(data), response, len);
}

mock_gnb_error_t mock_gnb_generate_nas_security_mode(const mock_response_context_t* ctx,
                                                      const mock_nas_security_mode_data_t* security_data,
                                                      void** response, size_t* len) {
    size_t data_len = 1 + 1 + 1 + 2 + security_data->ue_security_capability_len;
    uint8_t* data = (uint8_t*)malloc(data_len);
    if (data == NULL) {
        return MOCK_GNB_ERROR_MEMORY;
    }
    
    size_t offset = 0;
    data[offset++] = security_data->ngksi;
    data[offset++] = security_data->ciphering_alg;
    data[offset++] = security_data->integrity_alg;
    data[offset++] = (security_data->ue_security_capability_len >> 8) & 0xFF;
    data[offset++] = security_data->ue_security_capability_len & 0xFF;
    if (security_data->ue_security_capability_len > 0) {
        memcpy(&data[offset], security_data->ue_security_capability, security_data->ue_security_capability_len);
        offset += security_data->ue_security_capability_len;
    }
    
    mock_gnb_error_t err = build_response(MOCK_MSG_CAT_CORE, MOCK_NAS_RESP_SECURITY_MODE_COMMAND,
                                          ctx->transaction_id, data, offset,
                                          response, len);
    free(data);
    return err;
}

mock_gnb_error_t mock_gnb_generate_nas_pdu_session_accept(const mock_response_context_t* ctx,
                                                           const mock_nas_pdu_session_accept_data_t* pdu_data,
                                                           void** response, size_t* len) {
    size_t data_len = 1 + 1 + 4 + 1 + 1 + 2 + 2 + 2 + pdu_data->s_nssai_len;
    uint8_t* data = (uint8_t*)malloc(data_len);
    if (data == NULL) {
        return MOCK_GNB_ERROR_MEMORY;
    }
    
    size_t offset = 0;
    data[offset++] = pdu_data->pdu_session_id;
    data[offset++] = pdu_data->pdu_session_type;
    data[offset++] = (pdu_data->ue_ip_address >> 24) & 0xFF;
    data[offset++] = (pdu_data->ue_ip_address >> 16) & 0xFF;
    data[offset++] = (pdu_data->ue_ip_address >> 8) & 0xFF;
    data[offset++] = pdu_data->ue_ip_address & 0xFF;
    data[offset++] = pdu_data->default_qos_flow_id;
    data[offset++] = pdu_data->five_qi;
    data[offset++] = (pdu_data->session_ambr_ul >> 8) & 0xFF;
    data[offset++] = pdu_data->session_ambr_ul & 0xFF;
    data[offset++] = (pdu_data->session_ambr_dl >> 8) & 0xFF;
    data[offset++] = pdu_data->session_ambr_dl & 0xFF;
    data[offset++] = (pdu_data->s_nssai_len >> 8) & 0xFF;
    data[offset++] = pdu_data->s_nssai_len & 0xFF;
    if (pdu_data->s_nssai_len > 0) {
        memcpy(&data[offset], pdu_data->s_nssai, pdu_data->s_nssai_len);
        offset += pdu_data->s_nssai_len;
    }
    
    mock_gnb_error_t err = build_response(MOCK_MSG_CAT_CORE, MOCK_NAS_RESP_PDU_SESSION_EST_ACCEPT,
                                          ctx->transaction_id, data, offset,
                                          response, len);
    free(data);
    return err;
}

mock_gnb_error_t mock_gnb_generate_nas_pdu_session_release(const mock_response_context_t* ctx,
                                                            uint8_t pdu_session_id,
                                                            uint8_t release_cause,
                                                            void** response, size_t* len) {
    uint8_t data[2];
    data[0] = pdu_session_id;
    data[1] = release_cause;
    
    return build_response(MOCK_MSG_CAT_CORE, MOCK_NAS_RESP_PDU_SESSION_RELEASE_COMMAND,
                          ctx->transaction_id, data, sizeof(data), response, len);
}

mock_gnb_error_t mock_gnb_generate_nas_dl_transport(const mock_response_context_t* ctx,
                                                     const uint8_t* nas_pdu,
                                                     size_t nas_pdu_len,
                                                     void** response, size_t* len) {
    return build_response(MOCK_MSG_CAT_CORE, MOCK_NAS_RESP_DL_NAS_TRANSPORT,
                          ctx->transaction_id, nas_pdu, nas_pdu_len,
                          response, len);
}

/* ============== Complete Procedure Response Chains ============== */

mock_gnb_error_t mock_gnb_generate_registration_procedure(mock_gnb_ue_context_t* ue_ctx,
                                                          void** responses,
                                                          size_t* lengths,
                                                          uint8_t* count) {
    mock_response_context_t ctx;
    mock_gnb_build_response_context(ue_ctx, &ctx);
    
    uint8_t resp_idx = 0;
    
    /* 1. Generate Authentication Request */
    mock_nas_auth_request_data_t auth_data = {
        .ngksi = 0,
        .auth_type = 0  /* 5G-AKA */
    };
    /* Generate random RAND and AUTN for testing */
    for (int i = 0; i < 16; i++) {
        auth_data.rand[i] = (uint8_t)(rand() & 0xFF);
        auth_data.autn[i] = (uint8_t)(rand() & 0xFF);
    }
    
    if (mock_gnb_generate_nas_auth_request(&ctx, &auth_data,
                                            &responses[resp_idx], &lengths[resp_idx]) == MOCK_GNB_SUCCESS) {
        resp_idx++;
    }
    
    /* 2. Generate Security Mode Command */
    mock_nas_security_mode_data_t sec_data = {
        .ngksi = 0,
        .ciphering_alg = 2,  /* NEA2 */
        .integrity_alg = 2,  /* NIA2 */
        .ue_security_capability_len = 4
    };
    sec_data.ue_security_capability[0] = 0xE0;  /* NEA0, NEA1, NEA2 supported */
    sec_data.ue_security_capability[1] = 0xE0;  /* NIA0, NIA1, NIA2 supported */
    sec_data.ue_security_capability[2] = 0;
    sec_data.ue_security_capability[3] = 0;
    
    if (mock_gnb_generate_nas_security_mode(&ctx, &sec_data,
                                             &responses[resp_idx], &lengths[resp_idx]) == MOCK_GNB_SUCCESS) {
        resp_idx++;
    }
    
    /* 3. Generate Registration Accept */
    mock_nas_reg_accept_data_t reg_data;
    mock_gnb_get_default_reg_accept(&reg_data);
    snprintf(reg_data.guti, sizeof(reg_data.guti), "5G:GUTI-%08X", (uint32_t)ue_ctx->ran_ue_ngap_id);
    reg_data.tac = ctx.tac;
    reg_data.plmn_id = ctx.plmn_id;
    
    if (mock_gnb_generate_nas_reg_accept(&ctx, &reg_data,
                                          &responses[resp_idx], &lengths[resp_idx]) == MOCK_GNB_SUCCESS) {
        resp_idx++;
    }
    
    *count = resp_idx;
    return MOCK_GNB_SUCCESS;
}

mock_gnb_error_t mock_gnb_generate_pdu_session_procedure(mock_gnb_ue_context_t* ue_ctx,
                                                         uint8_t session_id,
                                                         void** response, size_t* len) {
    mock_response_context_t ctx;
    mock_gnb_build_response_context(ue_ctx, &ctx);
    
    mock_nas_pdu_session_accept_data_t pdu_data;
    mock_gnb_get_default_pdu_session_accept(&pdu_data);
    pdu_data.pdu_session_id = session_id;
    pdu_data.ue_ip_address = 0x0A000001 + ue_ctx->ran_ue_ngap_id;
    
    return mock_gnb_generate_nas_pdu_session_accept(&ctx, &pdu_data, response, len);
}

mock_gnb_error_t mock_gnb_generate_handover_procedure(mock_gnb_ue_context_t* ue_ctx,
                                                      uint16_t target_pci,
                                                      void** response, size_t* len) {
    mock_response_context_t ctx;
    mock_gnb_build_response_context(ue_ctx, &ctx);
    
    mock_rrc_handover_data_t ho_data = {
        .rrc_transaction_id = (uint8_t)(ue_ctx->ran_ue_ngap_id & 0xFF),
        .target_pci = target_pci,
        .target_cell_id = target_pci * 256,
        .new_c_rnti = (uint16_t)(rand() & 0xFFFF),
        .radio_bearer_config_len = 0,
        .rach_config_len = 0
    };
    
    return mock_gnb_generate_rrc_handover(&ctx, &ho_data, response, len);
}

/* ============== Utility Functions ============== */

void mock_gnb_build_response_context(const mock_gnb_ue_context_t* ue_ctx,
                                     mock_response_context_t* response_ctx) {
    if (ue_ctx == NULL || response_ctx == NULL) return;
    
    memset(response_ctx, 0, sizeof(mock_response_context_t));
    response_ctx->ran_ue_ngap_id = ue_ctx->ran_ue_ngap_id;
    response_ctx->amf_ue_ngap_id = ue_ctx->amf_ue_ngap_id;
    response_ctx->c_rnti = ue_ctx->rnti;
    strncpy(response_ctx->guti, ue_ctx->guti, sizeof(response_ctx->guti) - 1);
    strncpy(response_ctx->imsi, ue_ctx->imsi, sizeof(response_ctx->imsi) - 1);
    response_ctx->security_enabled = ue_ctx->security_context_valid;
    response_ctx->ciphering_alg = ue_ctx->ciphering_alg;
    response_ctx->integrity_alg = ue_ctx->integrity_alg;
    response_ctx->transaction_id = ue_ctx->ran_ue_ngap_id;
}

void mock_gnb_get_default_rrc_setup(mock_rrc_setup_data_t* rrc_data) {
    if (rrc_data == NULL) return;
    
    memset(rrc_data, 0, sizeof(mock_rrc_setup_data_t));
    rrc_data->rrc_transaction_id = 1;
    
    /* Minimal radio bearer config for SRB1 */
    rrc_data->radio_bearer_config[0] = 0x01;  /* SRB1 */
    rrc_data->radio_bearer_config_len = 1;
    
    /* Minimal cell group config */
    rrc_data->master_cell_group[0] = 0x01;  /* SpCell config */
    rrc_data->master_cell_group_len = 1;
}

void mock_gnb_get_default_reg_accept(mock_nas_reg_accept_data_t* reg_data) {
    if (reg_data == NULL) return;
    
    memset(reg_data, 0, sizeof(mock_nas_reg_accept_data_t));
    reg_data->registration_result = 1;  /* 3GPP access */
    reg_data->t3412_value = 3240;  /* 54 minutes */
    reg_data->t3402_value = 720;   /* 12 minutes */
}

void mock_gnb_get_default_pdu_session_accept(mock_nas_pdu_session_accept_data_t* pdu_data) {
    if (pdu_data == NULL) return;
    
    memset(pdu_data, 0, sizeof(mock_nas_pdu_session_accept_data_t));
    pdu_data->pdu_session_type = 1;  /* IPv4 */
    pdu_data->default_qos_flow_id = 1;
    pdu_data->five_qi = 9;  /* Default */
    pdu_data->session_ambr_ul = 100000;  /* 100 Mbps */
    pdu_data->session_ambr_dl = 100000;
    pdu_data->s_nssai[0] = 1;  /* SST */
    pdu_data->s_nssai_len = 1;
}

/* ============== ASN.1 PER Encoded RRC Generators ============== */

mock_gnb_error_t mock_gnb_generate_rrc_setup_per(const mock_response_context_t* ctx,
                                                  uint8_t rrc_transaction_id,
                                                  const uint8_t* radio_bearer_config,
                                                  size_t config_len,
                                                  void** response, size_t* len) {
    asn1_buffer_t buf;
    rrc_setup_t msg;
    
    memset(&msg, 0, sizeof(msg));
    msg.rrc_transaction_id = rrc_transaction_id;
    if (radio_bearer_config && config_len > 0) {
        size_t copy_len = config_len < 256 ? config_len : 256;
        memcpy(msg.radio_bearer_config, radio_bearer_config, copy_len);
        msg.config_len = copy_len;
    }
    
    if (asn1_buffer_alloc(&buf, 512) != UESIM_SUCCESS) {
        return MOCK_GNB_ERROR_MEMORY;
    }
    
    if (rrc_encode_setup(&buf, &msg) != UESIM_SUCCESS) {
        asn1_buffer_free(&buf);
        return MOCK_GNB_ERROR_ENCODING;
    }
    
    mock_gnb_error_t err = build_response(MOCK_MSG_CAT_CU, MOCK_RRC_SETUP,
                                          ctx->transaction_id, buf.data,
                                          asn1_buffer_length(&buf), response, len);
    asn1_buffer_free(&buf);
    return err;
}

mock_gnb_error_t mock_gnb_generate_rrc_reconfig_per(const mock_response_context_t* ctx,
                                                    uint8_t rrc_transaction_id,
                                                    const uint8_t* radio_bearer_config,
                                                    size_t config_len,
                                                    const uint8_t* meas_config,
                                                    size_t meas_config_len,
                                                    void** response, size_t* len) {
    asn1_buffer_t buf;
    rrc_reconfiguration_t msg;
    
    memset(&msg, 0, sizeof(msg));
    msg.rrc_transaction_id = rrc_transaction_id;
    
    if (radio_bearer_config && config_len > 0) {
        size_t copy_len = config_len < 512 ? config_len : 512;
        memcpy(msg.radio_bearer_config, radio_bearer_config, copy_len);
        msg.config_len = copy_len;
    }
    
    if (meas_config && meas_config_len > 0) {
        size_t copy_len = meas_config_len < 256 ? meas_config_len : 256;
        memcpy(msg.meas_config, meas_config, copy_len);
        msg.meas_config_len = copy_len;
    }
    
    if (asn1_buffer_alloc(&buf, 1024) != UESIM_SUCCESS) {
        return MOCK_GNB_ERROR_MEMORY;
    }
    
    if (rrc_encode_reconfiguration(&buf, &msg) != UESIM_SUCCESS) {
        asn1_buffer_free(&buf);
        return MOCK_GNB_ERROR_ENCODING;
    }
    
    mock_gnb_error_t err = build_response(MOCK_MSG_CAT_CU, MOCK_RRC_RECONFIGURATION,
                                          ctx->transaction_id, buf.data,
                                          asn1_buffer_length(&buf), response, len);
    asn1_buffer_free(&buf);
    return err;
}

mock_gnb_error_t mock_gnb_generate_rrc_handover_per(const mock_response_context_t* ctx,
                                                    uint8_t rrc_transaction_id,
                                                    uint16_t target_pci,
                                                    uint32_t target_cell_id,
                                                    uint16_t new_c_rnti,
                                                    const uint8_t* radio_bearer_config,
                                                    size_t config_len,
                                                    void** response, size_t* len) {
    asn1_buffer_t buf;
    rrc_handover_command_t msg;
    
    memset(&msg, 0, sizeof(msg));
    msg.rrc_transaction_id = rrc_transaction_id;
    msg.target_pci = target_pci;
    msg.target_cell_id = target_cell_id;
    msg.new_c_rnti = (uint8_t)(new_c_rnti & 0xFF);
    
    if (radio_bearer_config && config_len > 0) {
        size_t copy_len = config_len < 512 ? config_len : 512;
        memcpy(msg.radio_bearer_config, radio_bearer_config, copy_len);
        msg.config_len = copy_len;
    }
    
    if (asn1_buffer_alloc(&buf, 1024) != UESIM_SUCCESS) {
        return MOCK_GNB_ERROR_MEMORY;
    }
    
    if (rrc_encode_handover_command(&buf, &msg) != UESIM_SUCCESS) {
        asn1_buffer_free(&buf);
        return MOCK_GNB_ERROR_ENCODING;
    }
    
    mock_gnb_error_t err = build_response(MOCK_MSG_CAT_CU, MOCK_RRC_HANDOVER_COMMAND,
                                          ctx->transaction_id, buf.data,
                                          asn1_buffer_length(&buf), response, len);
    asn1_buffer_free(&buf);
    return err;
}

mock_gnb_error_t mock_gnb_generate_rrc_release_per(const mock_response_context_t* ctx,
                                                   uint8_t release_cause,
                                                   bool redirect_carrier,
                                                   uint16_t redirect_earfcn,
                                                   void** response, size_t* len) {
    asn1_buffer_t buf;
    rrc_connection_release_t msg;
    
    memset(&msg, 0, sizeof(msg));
    msg.release_cause = release_cause;
    msg.redirect_carrier = redirect_carrier;
    msg.redirect_earfcn = redirect_earfcn;
    
    if (asn1_buffer_alloc(&buf, 64) != UESIM_SUCCESS) {
        return MOCK_GNB_ERROR_MEMORY;
    }
    
    if (rrc_encode_connection_release(&buf, &msg) != UESIM_SUCCESS) {
        asn1_buffer_free(&buf);
        return MOCK_GNB_ERROR_ENCODING;
    }
    
    mock_gnb_error_t err = build_response(MOCK_MSG_CAT_CU, MOCK_RRC_RELEASE,
                                          ctx->transaction_id, buf.data,
                                          asn1_buffer_length(&buf), response, len);
    asn1_buffer_free(&buf);
    return err;
}

mock_gnb_error_t mock_gnb_generate_rrc_meas_report_per(const mock_response_context_t* ctx,
                                                       uint8_t meas_id,
                                                       int32_t rsrp,
                                                       int32_t rsrq,
                                                       uint16_t pci,
                                                       uint32_t cell_id,
                                                       void** response, size_t* len) {
    asn1_buffer_t buf;
    rrc_measurement_report_t msg;
    
    memset(&msg, 0, sizeof(msg));
    msg.meas_id = meas_id;
    msg.rsrp = rsrp;
    msg.rsrq = rsrq;
    msg.pci = pci;
    msg.cell_id = cell_id;
    
    if (asn1_buffer_alloc(&buf, 64) != UESIM_SUCCESS) {
        return MOCK_GNB_ERROR_MEMORY;
    }
    
    if (rrc_encode_measurement_report(&buf, &msg) != UESIM_SUCCESS) {
        asn1_buffer_free(&buf);
        return MOCK_GNB_ERROR_ENCODING;
    }
    
    mock_gnb_error_t err = build_response(MOCK_MSG_CAT_CU, MOCK_RRC_MEAS_CONFIG,
                                          ctx->transaction_id, buf.data,
                                          asn1_buffer_length(&buf), response, len);
    asn1_buffer_free(&buf);
    return err;
}

mock_gnb_error_t mock_gnb_generate_rrc_security_mode_per(const mock_response_context_t* ctx,
                                                         uint8_t rrc_transaction_id,
                                                         uint8_t ciphering_alg,
                                                         uint8_t integrity_alg,
                                                         const uint8_t* security_capabilities,
                                                         size_t capabilities_len,
                                                         void** response, size_t* len) {
    asn1_buffer_t buf;
    rrc_security_mode_cmd_t msg;
    
    memset(&msg, 0, sizeof(msg));
    msg.rrc_transaction_id = rrc_transaction_id;
    msg.ciphering_alg = ciphering_alg;
    msg.integrity_alg = integrity_alg;
    
    if (security_capabilities && capabilities_len > 0) {
        size_t copy_len = capabilities_len < 4 ? capabilities_len : 4;
        memcpy(msg.security_capabilities, security_capabilities, copy_len);
        msg.capabilities_len = copy_len;
    }
    
    if (asn1_buffer_alloc(&buf, 64) != UESIM_SUCCESS) {
        return MOCK_GNB_ERROR_MEMORY;
    }
    
    if (rrc_encode_security_mode_cmd(&buf, &msg) != UESIM_SUCCESS) {
        asn1_buffer_free(&buf);
        return MOCK_GNB_ERROR_ENCODING;
    }
    
    mock_gnb_error_t err = build_response(MOCK_MSG_CAT_CU, MOCK_RRC_SECURITY_MODE_COMMAND,
                                          ctx->transaction_id, buf.data,
                                          asn1_buffer_length(&buf), response, len);
    asn1_buffer_free(&buf);
    return err;
}

mock_gnb_error_t mock_gnb_generate_rrc_ue_cap_enquiry_per(const mock_response_context_t* ctx,
                                                          uint8_t enquiry_id,
                                                          const uint8_t* rat_types,
                                                          size_t num_rat_types,
                                                          void** response, size_t* len) {
    asn1_buffer_t buf;
    rrc_ue_cap_enquiry_t msg;
    
    memset(&msg, 0, sizeof(msg));
    msg.enquiry_id = enquiry_id;
    
    if (rat_types && num_rat_types > 0) {
        size_t copy_len = num_rat_types < 8 ? num_rat_types : 8;
        memcpy(msg.rat_types, rat_types, copy_len);
        msg.num_rat_types = copy_len;
    }
    
    if (asn1_buffer_alloc(&buf, 64) != UESIM_SUCCESS) {
        return MOCK_GNB_ERROR_MEMORY;
    }
    
    if (rrc_encode_ue_cap_enquiry(&buf, &msg) != UESIM_SUCCESS) {
        asn1_buffer_free(&buf);
        return MOCK_GNB_ERROR_ENCODING;
    }
    
    mock_gnb_error_t err = build_response(MOCK_MSG_CAT_CU, MOCK_RRC_UE_CAP_ENQUIRY,
                                          ctx->transaction_id, buf.data,
                                          asn1_buffer_length(&buf), response, len);
    asn1_buffer_free(&buf);
    return err;
}
