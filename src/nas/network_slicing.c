/*
 * 5G UE Simulation Application
 * Network Slicing Support Implementation
 * 
 * Implements S-NSSAI handling per 3GPP TS 23.501 and TS 24.501
 */

#include "nas.h"
#include "network_slicing.h"
#include "../core/memory.h"
#include <string.h>
#include <stdio.h>

// SST string descriptions
static const char* sst_strings[] = {
    [0] = "Reserved",
    [NAS_SST_EMBB] = "eMBB",
    [NAS_SST_URLLC] = "URLLC",
    [NAS_SST_MIOT] = "MIoT",
    [NAS_SST_V2X] = "V2X",
    [NAS_SST_HCI] = "HCI"
};

uesim_error_t nas_slice_init(struct nas_ue_context_t* nas_ctx) {
    if (nas_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Initialize slicing configuration in NAS context
    nas_network_slicing_t* slicing = &nas_ctx->slicing;
    
    slicing->slicing_enabled = true;
    slicing->default_slice_configured = false;
    slicing->num_configured = 0;
    slicing->num_allowed = 0;
    slicing->num_requested = 0;
    slicing->num_rejected = 0;
    
    // Initialize slice subscription
    nas_slice_subscription_t* subscription = &nas_ctx->slice_subscription;
    subscription->num_subscribed = 0;
    subscription->default_configured = false;
    
    // Initialize PDU session slice associations
    for (int i = 0; i < NAS_MAX_PDU_SESSIONS; i++) {
        nas_ctx->pdu_session_slices[i].pdu_session_id = i;
        nas_ctx->pdu_session_slices[i].associated = false;
        memset(&nas_ctx->pdu_session_slices[i].s_nssai, 0, sizeof(nas_s_nssai_t));
    }
    
    printf("NAS: Network slicing initialized\n");
    return UESIM_SUCCESS;
}

void nas_slice_cleanup(struct nas_ue_context_t* nas_ctx) {
    if (nas_ctx == NULL) {
        return;
    }
    
    // Clear slicing configuration
    nas_network_slicing_t* slicing = &nas_ctx->slicing;
    slicing->slicing_enabled = false;
    slicing->num_configured = 0;
    slicing->num_allowed = 0;
    slicing->num_requested = 0;
    slicing->num_rejected = 0;
    slicing->default_slice_configured = false;
    
    // Clear slice subscription
    nas_slice_subscription_t* subscription = &nas_ctx->slice_subscription;
    subscription->num_subscribed = 0;
    subscription->default_configured = false;
    
    // Clear PDU session slice associations
    for (int i = 0; i < NAS_MAX_PDU_SESSIONS; i++) {
        nas_ctx->pdu_session_slices[i].associated = false;
    }
    
    printf("NAS: Network slicing cleanup completed\n");
}

uesim_error_t nas_slice_create_s_nssai(uint8_t sst, uint32_t sd, bool sd_present,
                                      nas_s_nssai_t* slice) {
    if (slice == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    memset(slice, 0, sizeof(nas_s_nssai_t));
    slice->sst = sst;
    slice->sd = sd & 0x00FFFFFF;  // 24-bit SD
    slice->sd_present = sd_present;
    slice->mapped_sst_present = false;
    slice->mapped_sd_present = false;
    
    return UESIM_SUCCESS;
}

bool nas_slice_compare_s_nssai(const nas_s_nssai_t* slice1, const nas_s_nssai_t* slice2) {
    if (slice1 == NULL || slice2 == NULL) {
        return false;
    }
    
    // Compare SST
    if (slice1->sst != slice2->sst) {
        return false;
    }
    
    // Compare SD if present in both
    if (slice1->sd_present && slice2->sd_present) {
        if (slice1->sd != slice2->sd) {
            return false;
        }
    } else if (slice1->sd_present != slice2->sd_present) {
        // One has SD, other doesn't - not equal
        return false;
    }
    
    return true;
}

const char* nas_slice_sst_to_string(uint8_t sst) {
    if (sst > 0 && sst < sizeof(sst_strings) / sizeof(sst_strings[0])) {
        return sst_strings[sst];
    }
    return "Operator-Specific";
}

void nas_slice_print_s_nssai(const nas_s_nssai_t* slice) {
    if (slice == NULL) {
        printf("S-NSSAI: NULL\n");
        return;
    }
    
    printf("S-NSSAI: SST=%u (%s)", slice->sst, nas_slice_sst_to_string(slice->sst));
    if (slice->sd_present) {
        printf(", SD=0x%06X", slice->sd);
    }
    if (slice->mapped_sst_present) {
        printf(", Mapped SST=%u", slice->mapped_sst);
        if (slice->mapped_sd_present) {
            printf(", Mapped SD=0x%06X", slice->mapped_sd);
        }
    }
    printf("\n");
}

void nas_slice_print_nssai(const nas_s_nssai_t* slices, uint8_t count) {
    if (slices == NULL || count == 0) {
        printf("NSSAI: Empty\n");
        return;
    }
    
    printf("NSSAI (%u slices):\n", count);
    for (uint8_t i = 0; i < count; i++) {
        printf("  [%u] ", i);
        nas_slice_print_s_nssai(&slices[i]);
    }
}

uesim_error_t nas_slice_set_subscription(struct nas_ue_context_t* nas_ctx,
                                         const nas_s_nssai_t* slices,
                                         uint8_t count,
                                         const nas_s_nssai_t* default_slice) {
    if (nas_ctx == NULL || slices == NULL || count == 0) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (count > NAS_MAX_SNSSAI) {
        count = NAS_MAX_SNSSAI;
    }
    
    // Store subscribed S-NSSAIs in NAS context
    nas_slice_subscription_t* subscription = &nas_ctx->slice_subscription;
    
    for (uint8_t i = 0; i < count; i++) {
        subscription->subscribed[i] = slices[i];
    }
    subscription->num_subscribed = count;
    
    if (default_slice != NULL) {
        subscription->default_s_nssai = *default_slice;
        subscription->default_configured = true;
    }
    
    printf("NAS: Setting slice subscription (%u slices)\n", count);
    nas_slice_print_nssai(slices, count);
    
    if (default_slice != NULL) {
        printf("NAS: Default slice: ");
        nas_slice_print_s_nssai(default_slice);
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_slice_set_configured(struct nas_ue_context_t* nas_ctx,
                                       const nas_s_nssai_t* slices,
                                       uint8_t count) {
    if (nas_ctx == NULL || slices == NULL || count == 0) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (count > NAS_MAX_SNSSAI) {
        count = NAS_MAX_SNSSAI;
    }
    
    // Store configured S-NSSAIs in NAS context
    nas_network_slicing_t* slicing = &nas_ctx->slicing;
    
    for (uint8_t i = 0; i < count; i++) {
        slicing->configured[i] = slices[i];
    }
    slicing->num_configured = count;
    
    printf("NAS: Setting configured NSSAI (%u slices)\n", count);
    nas_slice_print_nssai(slices, count);
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_slice_set_allowed(struct nas_ue_context_t* nas_ctx,
                                    const nas_s_nssai_t* slices,
                                    uint8_t count) {
    if (nas_ctx == NULL || slices == NULL || count == 0) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (count > NAS_MAX_SNSSAI) {
        count = NAS_MAX_SNSSAI;
    }
    
    // Store allowed S-NSSAIs in NAS context
    nas_network_slicing_t* slicing = &nas_ctx->slicing;
    
    for (uint8_t i = 0; i < count; i++) {
        slicing->allowed[i] = slices[i];
    }
    slicing->num_allowed = count;
    
    printf("NAS: Setting allowed NSSAI (%u slices)\n", count);
    nas_slice_print_nssai(slices, count);
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_slice_set_requested(struct nas_ue_context_t* nas_ctx,
                                      const nas_s_nssai_t* slices,
                                      uint8_t count) {
    if (nas_ctx == NULL || slices == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (count > NAS_MAX_SNSSAI) {
        count = NAS_MAX_SNSSAI;
    }
    
    // Store requested S-NSSAIs in NAS context
    nas_network_slicing_t* slicing = &nas_ctx->slicing;
    
    for (uint8_t i = 0; i < count; i++) {
        slicing->requested[i] = slices[i];
    }
    slicing->num_requested = count;
    
    printf("NAS: Setting requested NSSAI (%u slices)\n", count);
    nas_slice_print_nssai(slices, count);
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_slice_add_rejected(struct nas_ue_context_t* nas_ctx,
                                     const nas_s_nssai_t* slice) {
    if (nas_ctx == NULL || slice == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Add to rejected list in NAS context
    nas_network_slicing_t* slicing = &nas_ctx->slicing;
    
    if (slicing->num_rejected < NAS_MAX_SNSSAI) {
        slicing->rejected[slicing->num_rejected] = *slice;
        slicing->num_rejected++;
    }
    
    printf("NAS: Adding rejected S-NSSAI: ");
    nas_slice_print_s_nssai(slice);
    
    return UESIM_SUCCESS;
}

bool nas_slice_is_allowed(struct nas_ue_context_t* nas_ctx, const nas_s_nssai_t* slice) {
    if (nas_ctx == NULL || slice == NULL) {
        return false;
    }
    
    nas_network_slicing_t* slicing = &nas_ctx->slicing;
    
    // Check against allowed list
    for (uint8_t i = 0; i < slicing->num_allowed; i++) {
        if (nas_slice_compare_s_nssai(&slicing->allowed[i], slice)) {
            return true;
        }
    }
    
    // If no allowed slices configured, accept valid SST values
    if (slicing->num_allowed == 0) {
        return (slice->sst >= NAS_SST_EMBB && slice->sst <= NAS_SST_HCI);
    }
    
    return false;
}

bool nas_slice_is_subscribed(struct nas_ue_context_t* nas_ctx, const nas_s_nssai_t* slice) {
    if (nas_ctx == NULL || slice == NULL) {
        return false;
    }
    
    nas_slice_subscription_t* subscription = &nas_ctx->slice_subscription;
    
    // Check against subscribed list
    for (uint8_t i = 0; i < subscription->num_subscribed; i++) {
        if (nas_slice_compare_s_nssai(&subscription->subscribed[i], slice)) {
            return true;
        }
    }
    
    return false;
}

bool nas_slice_is_requested(struct nas_ue_context_t* nas_ctx, const nas_s_nssai_t* slice) {
    if (nas_ctx == NULL || slice == NULL) {
        return false;
    }
    
    nas_network_slicing_t* slicing = &nas_ctx->slicing;
    
    // Check against requested list
    for (uint8_t i = 0; i < slicing->num_requested; i++) {
        if (nas_slice_compare_s_nssai(&slicing->requested[i], slice)) {
            return true;
        }
    }
    
    return false;
}

bool nas_slice_is_rejected(struct nas_ue_context_t* nas_ctx, const nas_s_nssai_t* slice) {
    if (nas_ctx == NULL || slice == NULL) {
        return false;
    }
    
    nas_network_slicing_t* slicing = &nas_ctx->slicing;
    
    // Check against rejected list
    for (uint8_t i = 0; i < slicing->num_rejected; i++) {
        if (nas_slice_compare_s_nssai(&slicing->rejected[i], slice)) {
            return true;
        }
    }
    
    return false;
}

nas_s_nssai_t* nas_slice_get_default(struct nas_ue_context_t* nas_ctx) {
    if (nas_ctx == NULL) {
        return NULL;
    }
    
    nas_network_slicing_t* slicing = &nas_ctx->slicing;
    nas_slice_subscription_t* subscription = &nas_ctx->slice_subscription;
    
    // Return configured default slice if available
    if (slicing->default_slice_configured) {
        return &slicing->default_slice;
    }
    
    // Return subscription default if available
    if (subscription->default_configured) {
        return &subscription->default_s_nssai;
    }
    
    // Return first allowed slice if available
    if (slicing->num_allowed > 0) {
        return &slicing->allowed[0];
    }
    
    // Return first subscribed slice if available
    if (subscription->num_subscribed > 0) {
        return &subscription->subscribed[0];
    }
    
    // Return default eMBB slice as fallback
    static nas_s_nssai_t default_slice = {0};
    if (default_slice.sst == 0) {
        nas_slice_create_s_nssai(NAS_SST_EMBB, 0, false, &default_slice);
    }
    
    return &default_slice;
}

nas_s_nssai_t* nas_slice_get_allowed(struct nas_ue_context_t* nas_ctx, uint8_t index) {
    if (nas_ctx == NULL || index >= NAS_MAX_SNSSAI) {
        return NULL;
    }
    
    nas_network_slicing_t* slicing = &nas_ctx->slicing;
    
    if (index < slicing->num_allowed) {
        return &slicing->allowed[index];
    }
    
    return NULL;
}

uint8_t nas_slice_get_allowed_count(struct nas_ue_context_t* nas_ctx) {
    if (nas_ctx == NULL) {
        return 0;
    }
    
    return nas_ctx->slicing.num_allowed;
}

uint8_t nas_slice_get_requested_count(struct nas_ue_context_t* nas_ctx) {
    if (nas_ctx == NULL) {
        return 0;
    }
    
    return nas_ctx->slicing.num_requested;
}

nas_slice_select_result_t nas_slice_select_for_pdu_session(struct nas_ue_context_t* nas_ctx,
                                                          uint8_t pdu_session_id,
                                                          nas_s_nssai_t* selected_slice) {
    if (nas_ctx == NULL || selected_slice == NULL) {
        return NAS_SLICE_SELECT_NO_DEFAULT;
    }
    
    // Get default slice
    nas_s_nssai_t* default_slice = nas_slice_get_default(nas_ctx);
    if (default_slice == NULL) {
        return NAS_SLICE_SELECT_NO_DEFAULT;
    }
    
    // Check if default slice is allowed
    if (!nas_slice_is_allowed(nas_ctx, default_slice)) {
        return NAS_SLICE_SELECT_NOT_ALLOWED;
    }
    
    // Check if rejected
    if (nas_slice_is_rejected(nas_ctx, default_slice)) {
        return NAS_SLICE_SELECT_REJECTED;
    }
    
    // Return default slice
    *selected_slice = *default_slice;
    
    printf("NAS: Selected slice for PDU session %u: ", pdu_session_id);
    nas_slice_print_s_nssai(selected_slice);
    
    return NAS_SLICE_SELECT_SUCCESS;
}

uesim_error_t nas_slice_associate_pdu_session(struct nas_ue_context_t* nas_ctx,
                                              uint8_t pdu_session_id,
                                              const nas_s_nssai_t* slice) {
    if (nas_ctx == NULL || slice == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pdu_session_id >= NAS_MAX_PDU_SESSIONS) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Store association in NAS context
    nas_pdu_session_slice_t* session_slice = &nas_ctx->pdu_session_slices[pdu_session_id];
    session_slice->pdu_session_id = pdu_session_id;
    session_slice->s_nssai = *slice;
    session_slice->associated = true;
    
    printf("NAS: Associating PDU session %u with slice: ", pdu_session_id);
    nas_slice_print_s_nssai(slice);
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_slice_disassociate_pdu_session(struct nas_ue_context_t* nas_ctx,
                                                 uint8_t pdu_session_id) {
    if (nas_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pdu_session_id >= NAS_MAX_PDU_SESSIONS) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Clear association in NAS context
    nas_pdu_session_slice_t* session_slice = &nas_ctx->pdu_session_slices[pdu_session_id];
    session_slice->associated = false;
    memset(&session_slice->s_nssai, 0, sizeof(nas_s_nssai_t));
    
    printf("NAS: Disassociating PDU session %u from slice\n", pdu_session_id);
    
    return UESIM_SUCCESS;
}

nas_s_nssai_t* nas_slice_get_pdu_session_slice(struct nas_ue_context_t* nas_ctx,
                                               uint8_t pdu_session_id) {
    if (nas_ctx == NULL || pdu_session_id >= NAS_MAX_PDU_SESSIONS) {
        return NULL;
    }
    
    nas_pdu_session_slice_t* session_slice = &nas_ctx->pdu_session_slices[pdu_session_id];
    
    // Return associated slice if available
    if (session_slice->associated) {
        return &session_slice->s_nssai;
    }
    
    // Return default slice as fallback
    return nas_slice_get_default(nas_ctx);
}

uesim_error_t nas_slice_encode_s_nssai(const nas_s_nssai_t* slice,
                                       uint8_t* buffer,
                                       size_t buffer_size,
                                       size_t* encoded_len) {
    if (slice == NULL || buffer == NULL || encoded_len == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    size_t len = 0;
    
    // SST (1 byte)
    if (buffer_size < 1) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    buffer[len++] = slice->sst;
    
    // SD (3 bytes, optional)
    if (slice->sd_present) {
        if (buffer_size < len + 3) {
            return UESIM_ERROR_INVALID_PARAM;
        }
        buffer[len++] = (slice->sd >> 16) & 0xFF;
        buffer[len++] = (slice->sd >> 8) & 0xFF;
        buffer[len++] = slice->sd & 0xFF;
    }
    
    // Mapped SST and SD (for interworking, optional)
    if (slice->mapped_sst_present) {
        if (buffer_size < len + 1) {
            return UESIM_ERROR_INVALID_PARAM;
        }
        buffer[len++] = slice->mapped_sst;
        
        if (slice->mapped_sd_present) {
            if (buffer_size < len + 3) {
                return UESIM_ERROR_INVALID_PARAM;
            }
            buffer[len++] = (slice->mapped_sd >> 16) & 0xFF;
            buffer[len++] = (slice->mapped_sd >> 8) & 0xFF;
            buffer[len++] = slice->mapped_sd & 0xFF;
        }
    }
    
    *encoded_len = len;
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_slice_decode_s_nssai(const uint8_t* buffer,
                                      size_t buffer_len,
                                      nas_s_nssai_t* slice,
                                      size_t* decoded_len) {
    if (buffer == NULL || slice == NULL || decoded_len == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    memset(slice, 0, sizeof(nas_s_nssai_t));
    size_t len = 0;
    
    // Minimum length is 1 byte (SST only)
    if (buffer_len < 1) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // SST (1 byte)
    slice->sst = buffer[len++];
    
    // Check if SD is present (based on remaining length)
    // In real implementation, this would be indicated by a length field
    if (buffer_len >= len + 3) {
        slice->sd_present = true;
        slice->sd = ((uint32_t)buffer[len] << 16) | 
                    ((uint32_t)buffer[len + 1] << 8) | 
                    buffer[len + 2];
        len += 3;
    }
    
    *decoded_len = len;
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_slice_encode_nssai(const nas_s_nssai_t* slices,
                                     uint8_t count,
                                     uint8_t* buffer,
                                     size_t buffer_size,
                                     size_t* encoded_len) {
    if (slices == NULL || buffer == NULL || encoded_len == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (count == 0 || count > NAS_MAX_SNSSAI) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    size_t total_len = 0;
    
    // Length byte (will be filled later)
    if (buffer_size < 1) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    size_t length_pos = total_len++;
    
    // Encode each S-NSSAI
    for (uint8_t i = 0; i < count; i++) {
        size_t slice_len = 0;
        uesim_error_t result = nas_slice_encode_s_nssai(&slices[i],
                                                        buffer + total_len,
                                                        buffer_size - total_len,
                                                        &slice_len);
        if (result != UESIM_SUCCESS) {
            return result;
        }
        total_len += slice_len;
    }
    
    // Fill length byte
    buffer[length_pos] = (uint8_t)(total_len - 1);
    
    *encoded_len = total_len;
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_slice_decode_nssai(const uint8_t* buffer,
                                     size_t buffer_len,
                                     nas_s_nssai_t* slices,
                                     uint8_t max_count,
                                     uint8_t* decoded_count) {
    if (buffer == NULL || slices == NULL || decoded_count == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (buffer_len < 1) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    size_t len = 0;
    uint8_t count = 0;
    
    // Length byte
    uint8_t nssai_len = buffer[len++];
    
    if (nssai_len > buffer_len - 1) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Decode each S-NSSAI
    while (len < (size_t)(nssai_len + 1) && count < max_count) {
        size_t slice_len = 0;
        uesim_error_t result = nas_slice_decode_s_nssai(buffer + len,
                                                        buffer_len - len,
                                                        &slices[count],
                                                        &slice_len);
        if (result != UESIM_SUCCESS) {
            break;
        }
        len += slice_len;
        count++;
    }
    
    *decoded_count = count;
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_slice_prepare_registration_nssai(struct nas_ue_context_t* nas_ctx,
                                                   uint8_t* buffer,
                                                   size_t buffer_size,
                                                   size_t* encoded_len) {
    if (nas_ctx == NULL || buffer == NULL || encoded_len == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    nas_slice_subscription_t* subscription = &nas_ctx->slice_subscription;
    
    // Prepare requested NSSAI for registration
    nas_s_nssai_t requested_slices[NAS_MAX_SNSSAI];
    uint8_t count = 0;
    
    // Use subscribed slices if available
    if (subscription->num_subscribed > 0) {
        for (uint8_t i = 0; i < subscription->num_subscribed && count < NAS_MAX_SNSSAI; i++) {
            requested_slices[count++] = subscription->subscribed[i];
        }
    } else {
        // Add default eMBB slice
        nas_slice_create_s_nssai(NAS_SST_EMBB, 0, false, &requested_slices[count++]);
    }
    
    // Store requested slices in context
    nas_slice_set_requested(nas_ctx, requested_slices, count);
    
    // Encode NSSAI
    return nas_slice_encode_nssai(requested_slices, count, buffer, buffer_size, encoded_len);
}

uesim_error_t nas_slice_process_registration_accept_nssai(struct nas_ue_context_t* nas_ctx,
                                                          const uint8_t* buffer,
                                                          size_t buffer_len) {
    if (nas_ctx == NULL || buffer == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (buffer_len == 0) {
        // No NSSAI in registration accept - use default
        printf("NAS: No NSSAI in Registration Accept, using default\n");
        return UESIM_SUCCESS;
    }
    
    // Decode allowed NSSAI
    nas_s_nssai_t allowed_slices[NAS_MAX_SNSSAI];
    uint8_t count = 0;
    
    uesim_error_t result = nas_slice_decode_nssai(buffer, buffer_len,
                                                   allowed_slices, NAS_MAX_SNSSAI, &count);
    if (result != UESIM_SUCCESS) {
        printf("NAS: Failed to decode allowed NSSAI\n");
        return result;
    }
    
    printf("NAS: Received allowed NSSAI (%u slices)\n", count);
    nas_slice_print_nssai(allowed_slices, count);
    
    // Store allowed NSSAI
    return nas_slice_set_allowed(nas_ctx, allowed_slices, count);
}