/*
 * 5G UE Simulation Application
 * ASN.1 PER Encoding/Decoding Implementation
 */

#include "asn1_per.h"
#include "../core/memory.h"
#include <string.h>

uesim_error_t asn1_buffer_init(asn1_buffer_t* buf, uint8_t* data, size_t capacity) {
    if (!buf || !data || capacity == 0) return UESIM_ERROR_INVALID_PARAM;
    buf->data = data; buf->size = 0; buf->capacity = capacity;
    buf->bit_offset = 0; buf->own_data = false;
    return UESIM_SUCCESS;
}

uesim_error_t asn1_buffer_alloc(asn1_buffer_t* buf, size_t initial_capacity) {
    if (!buf || initial_capacity == 0) return UESIM_ERROR_INVALID_PARAM;
    buf->data = (uint8_t*)uesim_malloc(initial_capacity);
    if (!buf->data) return UESIM_ERROR_MEMORY;
    buf->size = 0; buf->capacity = initial_capacity;
    buf->bit_offset = 0; buf->own_data = true;
    return UESIM_SUCCESS;
}

void asn1_buffer_free(asn1_buffer_t* buf) {
    if (buf && buf->own_data && buf->data) { uesim_free(buf->data); buf->data = NULL; }
}

size_t asn1_buffer_length(const asn1_buffer_t* buf) { return buf ? (buf->bit_offset + 7) / 8 : 0; }

uesim_error_t asn1_encode_bits(asn1_buffer_t* buf, uint32_t value, uint8_t num_bits) {
    if (!buf || num_bits > 32) return UESIM_ERROR_INVALID_PARAM;
    for (int i = num_bits - 1; i >= 0; i--) {
        size_t byte_idx = buf->bit_offset / 8;
        size_t bit_idx = 7 - (buf->bit_offset % 8);
        if (byte_idx >= buf->capacity) return UESIM_ERROR_CAPACITY;
        if ((value >> i) & 1) buf->data[byte_idx] |= (1 << bit_idx);
        buf->bit_offset++;
    }
    buf->size = (buf->bit_offset + 7) / 8;
    return UESIM_SUCCESS;
}

uesim_error_t asn1_decode_bits(const uint8_t* data, size_t* bit_offset, uint32_t* value, uint8_t num_bits) {
    if (!data || !bit_offset || !value || num_bits > 32) return UESIM_ERROR_INVALID_PARAM;
    *value = 0;
    for (int i = num_bits - 1; i >= 0; i--) {
        size_t byte_idx = *bit_offset / 8;
        size_t bit_idx = 7 - (*bit_offset % 8);
        if (data[byte_idx] & (1 << bit_idx)) *value |= (1U << i);
        (*bit_offset)++;
    }
    return UESIM_SUCCESS;
}

uesim_error_t asn1_encode_boolean(asn1_buffer_t* buf, bool value) { return asn1_encode_bits(buf, value ? 1 : 0, 1); }

uesim_error_t asn1_decode_boolean(const uint8_t* data, size_t* bit_offset, bool* value) {
    uint32_t v; uesim_error_t ret = asn1_decode_bits(data, bit_offset, &v, 1);
    if (ret == UESIM_SUCCESS) *value = (v != 0);
    return ret;
}

uesim_error_t asn1_encode_integer(asn1_buffer_t* buf, int64_t value, uint8_t num_bits) { return asn1_encode_bits(buf, (uint32_t)value, num_bits); }

uesim_error_t asn1_decode_integer(const uint8_t* data, size_t* bit_offset, int64_t* value, uint8_t num_bits) {
    uint32_t v; uesim_error_t ret = asn1_decode_bits(data, bit_offset, &v, num_bits);
    if (ret == UESIM_SUCCESS) *value = (int64_t)v;
    return ret;
}

uesim_error_t asn1_encode_enumerated(asn1_buffer_t* buf, uint32_t value, uint32_t num_values) {
    if (num_values <= 1) return UESIM_SUCCESS;
    uint8_t bits = 0; uint32_t tmp = num_values - 1;
    while (tmp) { bits++; tmp >>= 1; }
    return asn1_encode_bits(buf, value, bits);
}

uesim_error_t asn1_decode_enumerated(const uint8_t* data, size_t* bit_offset, uint32_t* value, uint32_t num_values) {
    if (num_values <= 1) { *value = 0; return UESIM_SUCCESS; }
    uint8_t bits = 0; uint32_t tmp = num_values - 1;
    while (tmp) { bits++; tmp >>= 1; }
    return asn1_decode_bits(data, bit_offset, value, bits);
}

uesim_error_t asn1_encode_length(asn1_buffer_t* buf, size_t len) {
    if (len < 128) return asn1_encode_bits(buf, (uint32_t)len, 8);
    if (len < 256) { asn1_encode_bits(buf, 0x81, 8); return asn1_encode_bits(buf, (uint32_t)len, 8); }
    asn1_encode_bits(buf, 0x82, 8); return asn1_encode_bits(buf, (uint32_t)len, 16);
}

uesim_error_t asn1_encode_octet_string(asn1_buffer_t* buf, const uint8_t* data, size_t len) {
    if (!buf) return UESIM_ERROR_INVALID_PARAM;
    uesim_error_t ret = asn1_encode_length(buf, len);
    if (ret != UESIM_SUCCESS) return ret;
    if (buf->size + len > buf->capacity) return UESIM_ERROR_CAPACITY;
    if (data && len > 0) memcpy(buf->data + buf->size, data, len);
    buf->bit_offset += len * 8; buf->size += len;
    return UESIM_SUCCESS;
}

uint8_t asn1_count_bits(uint64_t value) { uint8_t bits = 0; while (value) { bits++; value >>= 1; } return bits ? bits : 1; }

uesim_error_t rrc_encode_setup_request(asn1_buffer_t* buf, const rrc_setup_request_t* msg) {
    if (!buf || !msg) return UESIM_ERROR_INVALID_PARAM;
    memset(buf->data, 0, buf->capacity); buf->size = 0; buf->bit_offset = 0;
    asn1_encode_enumerated(buf, msg->establishment_cause, 4);
    asn1_encode_bits(buf, 0, 1);
    asn1_encode_bits(buf, msg->ue_identity.type, 1);
    asn1_encode_bits(buf, (uint32_t)(msg->ue_identity.random_value >> 8), 31);
    asn1_encode_bits(buf, (uint32_t)(msg->ue_identity.random_value & 0xFF), 8);
    return UESIM_SUCCESS;
}

uesim_error_t rrc_decode_setup_request(const uint8_t* data, size_t len, rrc_setup_request_t* msg) {
    if (!data || !msg || len < 6) return UESIM_ERROR_INVALID_PARAM;
    size_t bit_offset = 0;
    uint32_t cause; asn1_decode_enumerated(data, &bit_offset, &cause, 4);
    msg->establishment_cause = (rrc_establishment_cause_t)cause;
    uint32_t spare; asn1_decode_bits(data, &bit_offset, &spare, 1);
    uint32_t type; asn1_decode_bits(data, &bit_offset, &type, 1);
    msg->ue_identity.type = (uint8_t)type;
    uint32_t high, low; asn1_decode_bits(data, &bit_offset, &high, 31);
    asn1_decode_bits(data, &bit_offset, &low, 8);
    msg->ue_identity.random_value = ((uint64_t)high << 8) | low;
    return UESIM_SUCCESS;
}

uesim_error_t rrc_encode_setup(asn1_buffer_t* buf, const rrc_setup_t* msg) {
    if (!buf || !msg) return UESIM_ERROR_INVALID_PARAM;
    memset(buf->data, 0, buf->capacity); buf->size = 0; buf->bit_offset = 0;
    asn1_encode_bits(buf, msg->rrc_transaction_id, 2);
    asn1_encode_bits(buf, 0, 6);
    asn1_encode_octet_string(buf, msg->radio_bearer_config, msg->config_len);
    return UESIM_SUCCESS;
}

uesim_error_t rrc_encode_setup_complete(asn1_buffer_t* buf, const rrc_setup_complete_t* msg) {
    if (!buf || !msg) return UESIM_ERROR_INVALID_PARAM;
    memset(buf->data, 0, buf->capacity); buf->size = 0; buf->bit_offset = 0;
    asn1_encode_bits(buf, msg->rrc_transaction_id, 2);
    asn1_encode_bits(buf, msg->selected_plmn, 4);
    asn1_encode_bits(buf, 0, 2);
    asn1_encode_octet_string(buf, msg->nas_pdu, msg->nas_pdu_len);
    return UESIM_SUCCESS;
}

uesim_error_t rrc_decode_setup(const uint8_t* data, size_t len, rrc_setup_t* msg) {
    if (!data || !msg || len < 4) return UESIM_ERROR_INVALID_PARAM;
    size_t bit_offset = 0;
    uint32_t tid; asn1_decode_bits(data, &bit_offset, &tid, 2);
    msg->rrc_transaction_id = (uint8_t)tid;
    uint32_t spare; asn1_decode_bits(data, &bit_offset, &spare, 6);
    /* Decode radio bearer config - simplified */
    size_t data_offset = (bit_offset + 7) / 8;
    if (data_offset < len) {
        msg->config_len = (len - data_offset < 256) ? len - data_offset : 256;
        memcpy(msg->radio_bearer_config, data + data_offset, msg->config_len);
    } else {
        msg->config_len = 0;
    }
    return UESIM_SUCCESS;
}

/* ============== RRC Reestablishment Messages ============== */

uesim_error_t rrc_encode_reest_request(asn1_buffer_t* buf, const rrc_reest_request_t* msg) {
    if (!buf || !msg) return UESIM_ERROR_INVALID_PARAM;
    memset(buf->data, 0, buf->capacity); buf->size = 0; buf->bit_offset = 0;
    asn1_encode_enumerated(buf, msg->reestablishment_cause, 3);
    asn1_encode_bits(buf, msg->pci, 16);
    asn1_encode_bits(buf, msg->c_rnti, 16);
    asn1_encode_bits(buf, msg->short_mac_i[0], 8);
    asn1_encode_bits(buf, msg->short_mac_i[1], 8);
    asn1_encode_bits(buf, msg->ue_identity.type, 1);
    asn1_encode_bits(buf, (uint32_t)(msg->ue_identity.random_value >> 8), 31);
    asn1_encode_bits(buf, (uint32_t)(msg->ue_identity.random_value & 0xFF), 8);
    return UESIM_SUCCESS;
}

uesim_error_t rrc_decode_reest_request(const uint8_t* data, size_t len, rrc_reest_request_t* msg) {
    if (!data || !msg || len < 12) return UESIM_ERROR_INVALID_PARAM;
    size_t bit_offset = 0;
    uint32_t cause; asn1_decode_enumerated(data, &bit_offset, &cause, 3);
    msg->reestablishment_cause = (rrc_reestablishment_cause_t)cause;
    uint32_t pci, crnti;
    asn1_decode_bits(data, &bit_offset, &pci, 16);
    asn1_decode_bits(data, &bit_offset, &crnti, 16);
    msg->pci = (uint16_t)pci;
    msg->c_rnti = (uint32_t)crnti;
    uint32_t mac0, mac1;
    asn1_decode_bits(data, &bit_offset, &mac0, 8);
    asn1_decode_bits(data, &bit_offset, &mac1, 8);
    msg->short_mac_i[0] = (uint8_t)mac0;
    msg->short_mac_i[1] = (uint8_t)mac1;
    uint32_t type; asn1_decode_bits(data, &bit_offset, &type, 1);
    msg->ue_identity.type = (uint8_t)type;
    uint32_t high, low;
    asn1_decode_bits(data, &bit_offset, &high, 31);
    asn1_decode_bits(data, &bit_offset, &low, 8);
    msg->ue_identity.random_value = ((uint64_t)high << 8) | low;
    return UESIM_SUCCESS;
}

uesim_error_t rrc_encode_reestablishment(asn1_buffer_t* buf, const rrc_reestablishment_t* msg) {
    if (!buf || !msg) return UESIM_ERROR_INVALID_PARAM;
    memset(buf->data, 0, buf->capacity); buf->size = 0; buf->bit_offset = 0;
    asn1_encode_bits(buf, msg->rrc_transaction_id, 2);
    asn1_encode_bits(buf, 0, 6);
    asn1_encode_octet_string(buf, msg->radio_bearer_config, msg->config_len);
    return UESIM_SUCCESS;
}

uesim_error_t rrc_decode_reestablishment(const uint8_t* data, size_t len, rrc_reestablishment_t* msg) {
    if (!data || !msg || len < 4) return UESIM_ERROR_INVALID_PARAM;
    size_t bit_offset = 0;
    uint32_t tid; asn1_decode_bits(data, &bit_offset, &tid, 2);
    msg->rrc_transaction_id = (uint8_t)tid;
    uint32_t spare; asn1_decode_bits(data, &bit_offset, &spare, 6);
    size_t data_offset = (bit_offset + 7) / 8;
    if (data_offset < len) {
        msg->config_len = (len - data_offset < 256) ? len - data_offset : 256;
        memcpy(msg->radio_bearer_config, data + data_offset, msg->config_len);
    } else {
        msg->config_len = 0;
    }
    return UESIM_SUCCESS;
}

uesim_error_t rrc_encode_reest_complete(asn1_buffer_t* buf, const rrc_reest_complete_t* msg) {
    if (!buf || !msg) return UESIM_ERROR_INVALID_PARAM;
    memset(buf->data, 0, buf->capacity); buf->size = 0; buf->bit_offset = 0;
    asn1_encode_bits(buf, msg->rrc_transaction_id, 2);
    asn1_encode_bits(buf, 0, 6);
    return UESIM_SUCCESS;
}

/* ============== RRC Reconfiguration Messages ============== */

uesim_error_t rrc_encode_reconfiguration(asn1_buffer_t* buf, const rrc_reconfiguration_t* msg) {
    if (!buf || !msg) return UESIM_ERROR_INVALID_PARAM;
    memset(buf->data, 0, buf->capacity); buf->size = 0; buf->bit_offset = 0;
    asn1_encode_bits(buf, msg->rrc_transaction_id, 2);
    asn1_encode_bits(buf, 0, 6);
    asn1_encode_octet_string(buf, msg->radio_bearer_config, msg->config_len);
    if (msg->meas_config_len > 0) {
        asn1_encode_boolean(buf, true);
        asn1_encode_octet_string(buf, msg->meas_config, msg->meas_config_len);
    } else {
        asn1_encode_boolean(buf, false);
    }
    if (msg->has_mobility_config && msg->mobility_config_len > 0) {
        asn1_encode_boolean(buf, true);
        asn1_encode_octet_string(buf, msg->mobility_config, msg->mobility_config_len);
    } else {
        asn1_encode_boolean(buf, false);
    }
    return UESIM_SUCCESS;
}

uesim_error_t rrc_decode_reconfiguration(const uint8_t* data, size_t len, rrc_reconfiguration_t* msg) {
    if (!data || !msg || len < 4) return UESIM_ERROR_INVALID_PARAM;
    size_t bit_offset = 0;
    uint32_t tid; asn1_decode_bits(data, &bit_offset, &tid, 2);
    msg->rrc_transaction_id = (uint8_t)tid;
    uint32_t spare; asn1_decode_bits(data, &bit_offset, &spare, 6);
    size_t data_offset = (bit_offset + 7) / 8;
    if (data_offset < len) {
        msg->config_len = (len - data_offset < 512) ? len - data_offset : 512;
        memcpy(msg->radio_bearer_config, data + data_offset, msg->config_len);
    } else {
        msg->config_len = 0;
    }
    msg->meas_config_len = 0;
    msg->has_mobility_config = false;
    msg->mobility_config_len = 0;
    return UESIM_SUCCESS;
}

uesim_error_t rrc_encode_reconfig_complete(asn1_buffer_t* buf, const rrc_reconfig_complete_t* msg) {
    if (!buf || !msg) return UESIM_ERROR_INVALID_PARAM;
    memset(buf->data, 0, buf->capacity); buf->size = 0; buf->bit_offset = 0;
    asn1_encode_bits(buf, msg->rrc_transaction_id, 2);
    asn1_encode_bits(buf, 0, 6);
    return UESIM_SUCCESS;
}

/* ============== RRC Measurement Messages ============== */

uesim_error_t rrc_encode_measurement_report(asn1_buffer_t* buf, const rrc_measurement_report_t* msg) {
    if (!buf || !msg) return UESIM_ERROR_INVALID_PARAM;
    memset(buf->data, 0, buf->capacity); buf->size = 0; buf->bit_offset = 0;
    asn1_encode_bits(buf, msg->meas_id, 6);
    asn1_encode_bits(buf, 0, 2);
    /* RSRP encoded as signed offset from -140 dBm */
    uint32_t rsrp_encoded = (uint32_t)(msg->rsrp + 140);
    asn1_encode_bits(buf, rsrp_encoded, 8);
    /* RSRQ encoded as signed offset from -20 dB */
    uint32_t rsrq_encoded = (uint32_t)(msg->rsrq + 20);
    asn1_encode_bits(buf, rsrq_encoded, 7);
    asn1_encode_bits(buf, msg->pci, 16);
    asn1_encode_bits(buf, msg->cell_id, 28);
    return UESIM_SUCCESS;
}

uesim_error_t rrc_decode_measurement_report(const uint8_t* data, size_t len, rrc_measurement_report_t* msg) {
    if (!data || !msg || len < 10) return UESIM_ERROR_INVALID_PARAM;
    size_t bit_offset = 0;
    uint32_t meas_id; asn1_decode_bits(data, &bit_offset, &meas_id, 6);
    msg->meas_id = (uint8_t)meas_id;
    uint32_t spare; asn1_decode_bits(data, &bit_offset, &spare, 2);
    uint32_t rsrp, rsrq, pci, cell_id;
    asn1_decode_bits(data, &bit_offset, &rsrp, 8);
    msg->rsrp = (int32_t)rsrp - 140;
    asn1_decode_bits(data, &bit_offset, &rsrq, 7);
    msg->rsrq = (int32_t)rsrq - 20;
    asn1_decode_bits(data, &bit_offset, &pci, 16);
    msg->pci = (uint16_t)pci;
    asn1_decode_bits(data, &bit_offset, &cell_id, 28);
    msg->cell_id = cell_id;
    return UESIM_SUCCESS;
}

/* ============== RRC Handover Messages ============== */

uesim_error_t rrc_encode_handover_command(asn1_buffer_t* buf, const rrc_handover_command_t* msg) {
    if (!buf || !msg) return UESIM_ERROR_INVALID_PARAM;
    memset(buf->data, 0, buf->capacity); buf->size = 0; buf->bit_offset = 0;
    asn1_encode_bits(buf, msg->rrc_transaction_id, 2);
    asn1_encode_bits(buf, 0, 6);
    asn1_encode_bits(buf, msg->target_pci, 16);
    asn1_encode_bits(buf, msg->target_cell_id, 28);
    asn1_encode_bits(buf, msg->new_c_rnti, 16);
    asn1_encode_octet_string(buf, msg->radio_bearer_config, msg->config_len);
    return UESIM_SUCCESS;
}

uesim_error_t rrc_decode_handover_command(const uint8_t* data, size_t len, rrc_handover_command_t* msg) {
    if (!data || !msg || len < 10) return UESIM_ERROR_INVALID_PARAM;
    size_t bit_offset = 0;
    uint32_t tid; asn1_decode_bits(data, &bit_offset, &tid, 2);
    msg->rrc_transaction_id = (uint8_t)tid;
    uint32_t spare; asn1_decode_bits(data, &bit_offset, &spare, 6);
    uint32_t pci, cell_id, crnti;
    asn1_decode_bits(data, &bit_offset, &pci, 16);
    asn1_decode_bits(data, &bit_offset, &cell_id, 28);
    asn1_decode_bits(data, &bit_offset, &crnti, 16);
    msg->target_pci = (uint16_t)pci;
    msg->target_cell_id = cell_id;
    msg->new_c_rnti = (uint8_t)crnti;
    size_t data_offset = (bit_offset + 7) / 8;
    if (data_offset < len) {
        msg->config_len = (len - data_offset < 512) ? len - data_offset : 512;
        memcpy(msg->radio_bearer_config, data + data_offset, msg->config_len);
    } else {
        msg->config_len = 0;
    }
    return UESIM_SUCCESS;
}

uesim_error_t rrc_encode_handover_confirm(asn1_buffer_t* buf, const rrc_handover_confirm_t* msg) {
    if (!buf || !msg) return UESIM_ERROR_INVALID_PARAM;
    memset(buf->data, 0, buf->capacity); buf->size = 0; buf->bit_offset = 0;
    asn1_encode_bits(buf, msg->rrc_transaction_id, 2);
    asn1_encode_bits(buf, 0, 6);
    return UESIM_SUCCESS;
}

/* ============== RRC UE Capability Messages ============== */

uesim_error_t rrc_encode_ue_cap_enquiry(asn1_buffer_t* buf, const rrc_ue_cap_enquiry_t* msg) {
    if (!buf || !msg) return UESIM_ERROR_INVALID_PARAM;
    memset(buf->data, 0, buf->capacity); buf->size = 0; buf->bit_offset = 0;
    asn1_encode_bits(buf, msg->enquiry_id, 4);
    asn1_encode_bits(buf, (uint32_t)msg->num_rat_types, 4);
    for (size_t i = 0; i < msg->num_rat_types && i < 8; i++) {
        asn1_encode_bits(buf, msg->rat_types[i], 4);
    }
    return UESIM_SUCCESS;
}

uesim_error_t rrc_decode_ue_cap_enquiry(const uint8_t* data, size_t len, rrc_ue_cap_enquiry_t* msg) {
    if (!data || !msg || len < 2) return UESIM_ERROR_INVALID_PARAM;
    size_t bit_offset = 0;
    uint32_t eid, num;
    asn1_decode_bits(data, &bit_offset, &eid, 4);
    asn1_decode_bits(data, &bit_offset, &num, 4);
    msg->enquiry_id = (uint8_t)eid;
    msg->num_rat_types = (num < 8) ? num : 8;
    for (size_t i = 0; i < msg->num_rat_types; i++) {
        uint32_t rat; asn1_decode_bits(data, &bit_offset, &rat, 4);
        msg->rat_types[i] = (uint8_t)rat;
    }
    return UESIM_SUCCESS;
}

uesim_error_t rrc_encode_ue_cap_info(asn1_buffer_t* buf, const rrc_ue_cap_info_t* msg) {
    if (!buf || !msg) return UESIM_ERROR_INVALID_PARAM;
    memset(buf->data, 0, buf->capacity); buf->size = 0; buf->bit_offset = 0;
    asn1_encode_bits(buf, msg->rat_type, 4);
    asn1_encode_bits(buf, 0, 4);
    asn1_encode_octet_string(buf, msg->capability_container, msg->container_len);
    return UESIM_SUCCESS;
}

/* ============== RRC Connection Release ============== */

uesim_error_t rrc_encode_connection_release(asn1_buffer_t* buf, const rrc_connection_release_t* msg) {
    if (!buf || !msg) return UESIM_ERROR_INVALID_PARAM;
    memset(buf->data, 0, buf->capacity); buf->size = 0; buf->bit_offset = 0;
    asn1_encode_bits(buf, msg->release_cause, 3);
    asn1_encode_boolean(buf, msg->redirect_carrier);
    if (msg->redirect_carrier) {
        asn1_encode_bits(buf, msg->redirect_earfcn, 16);
    }
    asn1_encode_bits(buf, 0, 5);
    return UESIM_SUCCESS;
}

uesim_error_t rrc_decode_connection_release(const uint8_t* data, size_t len, rrc_connection_release_t* msg) {
    if (!data || !msg || len < 2) return UESIM_ERROR_INVALID_PARAM;
    size_t bit_offset = 0;
    uint32_t cause; asn1_decode_bits(data, &bit_offset, &cause, 3);
    msg->release_cause = (uint8_t)cause;
    bool redirect; asn1_decode_boolean(data, &bit_offset, &redirect);
    msg->redirect_carrier = redirect;
    if (redirect) {
        uint32_t earfcn; asn1_decode_bits(data, &bit_offset, &earfcn, 16);
        msg->redirect_earfcn = (uint16_t)earfcn;
    } else {
        msg->redirect_earfcn = 0;
    }
    return UESIM_SUCCESS;
}

/* ============== Additional PER Primitives ============== */

uesim_error_t asn1_decode_length(const uint8_t* data, size_t* bit_offset, size_t* len) {
    if (!data || !bit_offset || !len) return UESIM_ERROR_INVALID_PARAM;
    
    uint32_t first_byte;
    size_t byte_idx = *bit_offset / 8;
    asn1_decode_bits(data, bit_offset, &first_byte, 8);
    
    if (first_byte < 128) {
        *len = first_byte;
    } else if (first_byte == 0x81) {
        uint32_t val;
        asn1_decode_bits(data, bit_offset, &val, 8);
        *len = val;
    } else if (first_byte == 0x82) {
        uint32_t val;
        asn1_decode_bits(data, bit_offset, &val, 16);
        *len = val;
    } else {
        *len = 0;
        return UESIM_ERROR_PROTOCOL;
    }
    return UESIM_SUCCESS;
}

uesim_error_t asn1_encode_bit_string(asn1_buffer_t* buf, const uint8_t* data, size_t num_bits, bool has_constraint, size_t fixed_size) {
    if (!buf) return UESIM_ERROR_INVALID_PARAM;
    
    if (has_constraint && fixed_size > 0) {
        /* Fixed-size bit string - no length encoding */
        for (size_t i = 0; i < num_bits; i++) {
            size_t byte_idx = i / 8;
            size_t bit_idx = 7 - (i % 8);
            uint8_t bit = (data[byte_idx] >> bit_idx) & 1;
            asn1_encode_bits(buf, bit, 1);
        }
    } else {
        /* Variable-size bit string with length */
        asn1_encode_length(buf, (num_bits + 7) / 8);
        size_t bytes = (num_bits + 7) / 8;
        if (buf->size + bytes > buf->capacity) return UESIM_ERROR_CAPACITY;
        if (data && bytes > 0) memcpy(buf->data + buf->size, data, bytes);
        buf->bit_offset += bytes * 8;
        buf->size += bytes;
    }
    return UESIM_SUCCESS;
}

uesim_error_t asn1_decode_bit_string(const uint8_t* data, size_t* bit_offset, uint8_t* out, size_t max_bytes, size_t* num_bits, bool has_constraint, size_t fixed_size) {
    if (!data || !bit_offset || !out || !num_bits) return UESIM_ERROR_INVALID_PARAM;
    
    size_t len;
    if (has_constraint && fixed_size > 0) {
        len = (fixed_size + 7) / 8;
        *num_bits = fixed_size;
    } else {
        if (asn1_decode_length(data, bit_offset, &len) != UESIM_SUCCESS) return UESIM_ERROR_PROTOCOL;
        *num_bits = len * 8;
    }
    
    if (len > max_bytes) return UESIM_ERROR_CAPACITY;
    
    size_t byte_idx = *bit_offset / 8;
    if (len > 0) memcpy(out, data + byte_idx, len);
    *bit_offset += len * 8;
    
    return UESIM_SUCCESS;
}

uesim_error_t asn1_encode_sequence(asn1_buffer_t* buf, const bool* optional_present, uint8_t num_optional) {
    if (!buf) return UESIM_ERROR_INVALID_PARAM;
    
    /* Encode presence bitmap for optional fields */
    if (optional_present && num_optional > 0) {
        for (uint8_t i = 0; i < num_optional; i++) {
            asn1_encode_boolean(buf, optional_present[i]);
        }
    }
    return UESIM_SUCCESS;
}

uesim_error_t asn1_decode_sequence(const uint8_t* data, size_t* bit_offset, bool* optional_present, uint8_t num_optional) {
    if (!data || !bit_offset) return UESIM_ERROR_INVALID_PARAM;
    
    /* Decode presence bitmap for optional fields */
    if (optional_present && num_optional > 0) {
        for (uint8_t i = 0; i < num_optional; i++) {
            asn1_decode_boolean(data, bit_offset, &optional_present[i]);
        }
    }
    return UESIM_SUCCESS;
}

uesim_error_t asn1_encode_sequence_of(asn1_buffer_t* buf, size_t count, size_t min_size, size_t max_size) {
    if (!buf) return UESIM_ERROR_INVALID_PARAM;
    
    if (min_size == max_size) {
        /* Fixed size - no length encoding needed */
        return UESIM_SUCCESS;
    }
    
    /* Encode length determinant */
    if (max_size <= 255) {
        asn1_encode_bits(buf, (uint32_t)count, 8);
    } else if (max_size <= 65535) {
        asn1_encode_bits(buf, 0x82, 8);
        asn1_encode_bits(buf, (uint32_t)count, 16);
    } else {
        asn1_encode_length(buf, count);
    }
    return UESIM_SUCCESS;
}

uesim_error_t asn1_decode_sequence_of(const uint8_t* data, size_t* bit_offset, size_t* count, size_t min_size, size_t max_size) {
    if (!data || !bit_offset || !count) return UESIM_ERROR_INVALID_PARAM;
    
    if (min_size == max_size) {
        *count = min_size;
        return UESIM_SUCCESS;
    }
    
    if (max_size <= 255) {
        uint32_t val;
        asn1_decode_bits(data, bit_offset, &val, 8);
        *count = val;
    } else if (max_size <= 65535) {
        uint32_t marker;
        asn1_decode_bits(data, bit_offset, &marker, 8);
        if (marker == 0x82) {
            uint32_t val;
            asn1_decode_bits(data, bit_offset, &val, 16);
            *count = val;
        } else {
            *count = marker;
        }
    } else {
        asn1_decode_length(data, bit_offset, count);
    }
    
    if (*count < min_size || *count > max_size) {
        return UESIM_ERROR_PROTOCOL;
    }
    return UESIM_SUCCESS;
}

uesim_error_t asn1_encode_choice(asn1_buffer_t* buf, uint32_t choice_index, uint32_t num_choices) {
    if (!buf || num_choices == 0) return UESIM_ERROR_INVALID_PARAM;
    
    /* Calculate bits needed for choice index */
    uint8_t bits = 0;
    uint32_t tmp = num_choices - 1;
    while (tmp) { bits++; tmp >>= 1; }
    
    if (bits > 0) {
        asn1_encode_bits(buf, choice_index, bits);
    }
    return UESIM_SUCCESS;
}

uesim_error_t asn1_decode_choice(const uint8_t* data, size_t* bit_offset, uint32_t* choice_index, uint32_t num_choices) {
    if (!data || !bit_offset || !choice_index || num_choices == 0) return UESIM_ERROR_INVALID_PARAM;
    
    if (num_choices == 1) {
        *choice_index = 0;
        return UESIM_SUCCESS;
    }
    
    uint8_t bits = 0;
    uint32_t tmp = num_choices - 1;
    while (tmp) { bits++; tmp >>= 1; }
    
    return asn1_decode_bits(data, bit_offset, choice_index, bits);
}

uesim_error_t asn1_encode_null(asn1_buffer_t* buf) {
    (void)buf;
    /* NULL type has no encoding */
    return UESIM_SUCCESS;
}

uesim_error_t asn1_decode_null(const uint8_t* data, size_t* bit_offset) {
    (void)data;
    (void)bit_offset;
    /* NULL type has no encoding to decode */
    return UESIM_SUCCESS;
}

uesim_error_t asn1_encode_constrained_integer(asn1_buffer_t* buf, int64_t value, int64_t min_val, int64_t max_val) {
    if (!buf || min_val > max_val) return UESIM_ERROR_INVALID_PARAM;
    
    if (value < min_val || value > max_val) {
        return UESIM_ERROR_PROTOCOL;
    }
    
    uint64_t range = (uint64_t)(max_val - min_val);
    uint64_t encoded_val = (uint64_t)(value - min_val);
    
    if (range == 0) {
        return UESIM_SUCCESS; /* Single value - no encoding needed */
    }
    
    /* Calculate bits needed */
    uint8_t bits = 0;
    uint64_t tmp = range;
    while (tmp) { bits++; tmp >>= 1; }
    
    return asn1_encode_bits(buf, (uint32_t)encoded_val, bits);
}

uesim_error_t asn1_decode_constrained_integer(const uint8_t* data, size_t* bit_offset, int64_t* value, int64_t min_val, int64_t max_val) {
    if (!data || !bit_offset || !value || min_val > max_val) return UESIM_ERROR_INVALID_PARAM;
    
    uint64_t range = (uint64_t)(max_val - min_val);
    
    if (range == 0) {
        *value = min_val;
        return UESIM_SUCCESS;
    }
    
    uint8_t bits = 0;
    uint64_t tmp = range;
    while (tmp) { bits++; tmp >>= 1; }
    
    uint32_t encoded_val;
    uesim_error_t ret = asn1_decode_bits(data, bit_offset, &encoded_val, bits);
    if (ret != UESIM_SUCCESS) return ret;
    
    *value = (int64_t)encoded_val + min_val;
    
    if (*value < min_val || *value > max_val) {
        return UESIM_ERROR_PROTOCOL;
    }
    return UESIM_SUCCESS;
}

/* ============== Utility Functions ============== */

/* Align to byte boundary */
uesim_error_t asn1_byte_align(asn1_buffer_t* buf) {
    if (!buf) return UESIM_ERROR_INVALID_PARAM;
    if (buf->bit_offset % 8 != 0) {
        buf->bit_offset = (buf->bit_offset + 7) & ~7;
        buf->size = buf->bit_offset / 8;
    }
    return UESIM_SUCCESS;
}

/* Skip padding bits to byte boundary */
void asn1_skip_to_byte_boundary(size_t* bit_offset) {
    if (bit_offset && *bit_offset % 8 != 0) {
        *bit_offset = (*bit_offset + 7) & ~7;
    }
}

/* Encode Object Identifier */
uesim_error_t asn1_encode_oid(asn1_buffer_t* buf, const uint32_t* oid_components, size_t num_components) {
    if (!buf || !oid_components || num_components < 2) return UESIM_ERROR_INVALID_PARAM;
    
    /* First two components encoded as 40*x + y */
    uint8_t first_byte = (uint8_t)(40 * oid_components[0] + oid_components[1]);
    asn1_encode_bits(buf, first_byte, 8);
    
    /* Encode remaining components using base-128 */
    for (size_t i = 2; i < num_components; i++) {
        uint32_t val = oid_components[i];
        if (val < 128) {
            asn1_encode_bits(buf, val, 8);
        } else if (val < 16384) {
            asn1_encode_bits(buf, (val >> 7) | 0x80, 8);
            asn1_encode_bits(buf, val & 0x7F, 8);
        } else if (val < 2097152) {
            asn1_encode_bits(buf, (val >> 14) | 0x80, 8);
            asn1_encode_bits(buf, ((val >> 7) & 0x7F) | 0x80, 8);
            asn1_encode_bits(buf, val & 0x7F, 8);
        } else {
            asn1_encode_bits(buf, (val >> 21) | 0x80, 8);
            asn1_encode_bits(buf, ((val >> 14) & 0x7F) | 0x80, 8);
            asn1_encode_bits(buf, ((val >> 7) & 0x7F) | 0x80, 8);
            asn1_encode_bits(buf, val & 0x7F, 8);
        }
    }
    return UESIM_SUCCESS;
}

/* Decode Object Identifier */
uesim_error_t asn1_decode_oid(const uint8_t* data, size_t* bit_offset, uint32_t* oid_components, size_t max_components, size_t* num_components) {
    if (!data || !bit_offset || !oid_components || !num_components) return UESIM_ERROR_INVALID_PARAM;
    
    *num_components = 0;
    if (max_components < 2) return UESIM_ERROR_CAPACITY;
    
    /* Decode first byte (40*x + y) */
    uint32_t first_byte;
    asn1_decode_bits(data, bit_offset, &first_byte, 8);
    oid_components[0] = first_byte / 40;
    oid_components[1] = first_byte % 40;
    *num_components = 2;
    
    /* Decode remaining components */
    while (*bit_offset / 8 < 1024 && *num_components < max_components) {
        uint32_t val = 0;
        uint32_t byte;
        bool more;
        
        do {
            asn1_decode_bits(data, bit_offset, &byte, 8);
            more = (byte & 0x80) != 0;
            val = (val << 7) | (byte & 0x7F);
        } while (more);
        
        oid_components[*num_components] = val;
        (*num_components)++;
        
        /* Check for end of OID (simple heuristic based on typical OID lengths) */
        if (*num_components >= 4) break;
    }
    
    return UESIM_SUCCESS;
}
