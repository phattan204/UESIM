/*
 * 5G UE Simulation Application
 * Network Slicing Support Header
 * 
 * Implements S-NSSAI handling per 3GPP TS 23.501 and TS 24.501
 */

#ifndef NETWORK_SLICING_H
#define NETWORK_SLICING_H

#include "../uesim.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Forward declaration to avoid circular dependency
struct nas_ue_context_t;

// Maximum number of S-NSSAIs
#define NAS_MAX_SNSSAI 8

// Slice Service Type (SST) values per TS 23.501
typedef enum {
    NAS_SST_EMBB = 1,        // Enhanced Mobile Broadband
    NAS_SST_URLLC = 2,       // Ultra-Reliable Low-Latency Communications
    NAS_SST_MIOT = 3,        // Massive IoT
    NAS_SST_V2X = 4,         // Vehicle-to-Everything
    NAS_SST_HCI = 5          // High Capacity Immersive
} nas_slice_service_type_t;

// S-NSSAI structure (Single Network Slice Selection Assistance Information)
typedef struct {
    uint8_t sst;              // Slice/Service Type (1 byte standard, 4 bytes operator-specific)
    uint32_t sd;              // Slice Differentiator (24 bits, optional)
    bool sd_present;          // SD is present flag
    bool mapped_sst_present;  // Mapped SST for interworking
    uint8_t mapped_sst;
    uint32_t mapped_sd;
    bool mapped_sd_present;
} nas_s_nssai_t;

// Network Slicing Subscription
typedef struct {
    nas_s_nssai_t subscribed[NAS_MAX_SNSSAI];  // Subscribed S-NSSAIs
    uint8_t num_subscribed;
    nas_s_nssai_t default_s_nssai;              // Default S-NSSAI
    bool default_configured;
} nas_slice_subscription_t;

// Network Slicing Configuration
typedef struct {
    nas_s_nssai_t configured[NAS_MAX_SNSSAI];  // Configured S-NSSAIs (from AMF)
    nas_s_nssai_t allowed[NAS_MAX_SNSSAI];      // Allowed S-NSSAIs (from AMF)
    nas_s_nssai_t requested[NAS_MAX_SNSSAI];   // Requested S-NSSAIs (to AMF)
    nas_s_nssai_t rejected[NAS_MAX_SNSSAI];    // Rejected S-NSSAIs
    uint8_t num_configured;
    uint8_t num_allowed;
    uint8_t num_requested;
    uint8_t num_rejected;
    nas_s_nssai_t default_slice;               // Default S-NSSAI for registration
    bool slicing_enabled;                       // Network slicing support flag
    bool default_slice_configured;
} nas_network_slicing_t;

// Slice selection result
typedef enum {
    NAS_SLICE_SELECT_SUCCESS = 0,
    NAS_SLICE_SELECT_NOT_ALLOWED = 1,
    NAS_SLICE_SELECT_NOT_SUBSCRIBED = 2,
    NAS_SLICE_SELECT_REJECTED = 3,
    NAS_SLICE_SELECT_NO_DEFAULT = 4
} nas_slice_select_result_t;

// Slice association for PDU session
typedef struct {
    uint8_t pdu_session_id;
    nas_s_nssai_t s_nssai;
    bool associated;
} nas_pdu_session_slice_t;

// Initialization and cleanup
uesim_error_t nas_slice_init(struct nas_ue_context_t* nas_ctx);
void nas_slice_cleanup(struct nas_ue_context_t* nas_ctx);

// Slice configuration
uesim_error_t nas_slice_set_subscription(struct nas_ue_context_t* nas_ctx,
                                         const nas_s_nssai_t* slices,
                                         uint8_t count,
                                         const nas_s_nssai_t* default_slice);
uesim_error_t nas_slice_set_configured(struct nas_ue_context_t* nas_ctx,
                                       const nas_s_nssai_t* slices,
                                       uint8_t count);
uesim_error_t nas_slice_set_allowed(struct nas_ue_context_t* nas_ctx,
                                    const nas_s_nssai_t* slices,
                                    uint8_t count);
uesim_error_t nas_slice_set_requested(struct nas_ue_context_t* nas_ctx,
                                      const nas_s_nssai_t* slices,
                                      uint8_t count);
uesim_error_t nas_slice_add_rejected(struct nas_ue_context_t* nas_ctx,
                                     const nas_s_nssai_t* slice);

// Slice queries
bool nas_slice_is_allowed(struct nas_ue_context_t* nas_ctx, const nas_s_nssai_t* slice);
bool nas_slice_is_subscribed(struct nas_ue_context_t* nas_ctx, const nas_s_nssai_t* slice);
bool nas_slice_is_requested(struct nas_ue_context_t* nas_ctx, const nas_s_nssai_t* slice);
bool nas_slice_is_rejected(struct nas_ue_context_t* nas_ctx, const nas_s_nssai_t* slice);
nas_s_nssai_t* nas_slice_get_default(struct nas_ue_context_t* nas_ctx);
nas_s_nssai_t* nas_slice_get_allowed(struct nas_ue_context_t* nas_ctx, uint8_t index);
uint8_t nas_slice_get_allowed_count(struct nas_ue_context_t* nas_ctx);
uint8_t nas_slice_get_requested_count(struct nas_ue_context_t* nas_ctx);

// Slice selection
nas_slice_select_result_t nas_slice_select_for_pdu_session(struct nas_ue_context_t* nas_ctx,
                                                          uint8_t pdu_session_id,
                                                          nas_s_nssai_t* selected_slice);
uesim_error_t nas_slice_associate_pdu_session(struct nas_ue_context_t* nas_ctx,
                                              uint8_t pdu_session_id,
                                              const nas_s_nssai_t* slice);
uesim_error_t nas_slice_disassociate_pdu_session(struct nas_ue_context_t* nas_ctx,
                                                 uint8_t pdu_session_id);
nas_s_nssai_t* nas_slice_get_pdu_session_slice(struct nas_ue_context_t* nas_ctx,
                                               uint8_t pdu_session_id);

// S-NSSAI encoding/decoding for NAS messages
uesim_error_t nas_slice_encode_s_nssai(const nas_s_nssai_t* slice,
                                       uint8_t* buffer,
                                       size_t buffer_size,
                                       size_t* encoded_len);
uesim_error_t nas_slice_decode_s_nssai(const uint8_t* buffer,
                                       size_t buffer_len,
                                       nas_s_nssai_t* slice,
                                       size_t* decoded_len);

// NSSAI encoding/decoding (list of S-NSSAIs)
uesim_error_t nas_slice_encode_nssai(const nas_s_nssai_t* slices,
                                     uint8_t count,
                                     uint8_t* buffer,
                                     size_t buffer_size,
                                     size_t* encoded_len);
uesim_error_t nas_slice_decode_nssai(const uint8_t* buffer,
                                     size_t buffer_len,
                                     nas_s_nssai_t* slices,
                                     uint8_t max_count,
                                     uint8_t* decoded_count);

// Utility functions
uesim_error_t nas_slice_create_s_nssai(uint8_t sst, uint32_t sd, bool sd_present,
                                      nas_s_nssai_t* slice);
bool nas_slice_compare_s_nssai(const nas_s_nssai_t* slice1, const nas_s_nssai_t* slice2);
const char* nas_slice_sst_to_string(uint8_t sst);
void nas_slice_print_s_nssai(const nas_s_nssai_t* slice);
void nas_slice_print_nssai(const nas_s_nssai_t* slices, uint8_t count);

// Registration integration
uesim_error_t nas_slice_prepare_registration_nssai(struct nas_ue_context_t* nas_ctx,
                                                   uint8_t* buffer,
                                                   size_t buffer_size,
                                                   size_t* encoded_len);
uesim_error_t nas_slice_process_registration_accept_nssai(struct nas_ue_context_t* nas_ctx,
                                                          const uint8_t* buffer,
                                                          size_t buffer_len);

#endif // NETWORK_SLICING_H