/*
 * 5G UE Simulation Application
 * ASN.1 PER Encoding/Decoding Header
 */

#ifndef ASN1_PER_H
#define ASN1_PER_H

#include "../uesim.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ASN.1 encoding buffer */
typedef struct {
    uint8_t* data;
    size_t size;
    size_t capacity;
    size_t bit_offset;
    bool own_data;
} asn1_buffer_t;

/* RRC message types */
typedef enum {
    RRC_MSG_RRC_SETUP_REQUEST = 0,
    RRC_MSG_RRC_SETUP = 1,
    RRC_MSG_RRC_SETUP_COMPLETE = 2,
    RRC_MSG_RRC_REESTABLISHMENT_REQUEST = 3,
    RRC_MSG_RRC_REESTABLISHMENT = 4,
    RRC_MSG_RRC_REESTABLISHMENT_COMPLETE = 5,
    RRC_MSG_RRC_RECONFIGURATION = 6,
    RRC_MSG_RRC_RECONFIGURATION_COMPLETE = 7,
    RRC_MSG_RRC_MEASUREMENT_REPORT = 8,
    RRC_MSG_RRC_HANDOVER_COMMAND = 9,
    RRC_MSG_RRC_HANDOVER_CONFIRMATION = 10,
    RRC_MSG_RRC_UE_CAPABILITY_ENQUIRY = 11,
    RRC_MSG_RRC_UE_CAPABILITY_INFORMATION = 12,
    RRC_MSG_RRC_CONNECTION_RELEASE = 13
} rrc_message_id_t;

/* RRC UE Identity */
typedef struct {
    uint8_t type;
    uint64_t random_value;
} rrc_ue_identity_t;

/* RRC Establishment Cause */
typedef enum {
    RRC_EST_CAUSE_EMERGENCY = 0,
    RRC_EST_CAUSE_MT_ACCESS = 1,
    RRC_EST_CAUSE_MO_SIGNALLING = 2,
    RRC_EST_CAUSE_MO_DATA = 3,
    RRC_EST_CAUSE_MT_DATA = 4,
    RRC_EST_CAUSE_DELAY_TOLERANT = 5
} rrc_establishment_cause_t;

/* RRC Reestablishment Cause */
typedef enum {
    RRC_REEST_CAUSE_RECONFIG_FAILURE = 0,
    RRC_REEST_CAUSE_HANDOVER_FAILURE = 1,
    RRC_REEST_CAUSE_OTHER_FAILURE = 2
} rrc_reestablishment_cause_t;

/* RRC Setup Request */
typedef struct {
    rrc_ue_identity_t ue_identity;
    rrc_establishment_cause_t establishment_cause;
} rrc_setup_request_t;

/* RRC Setup */
typedef struct {
    uint8_t rrc_transaction_id;
    uint8_t radio_bearer_config[256];
    size_t config_len;
    uint8_t master_cell_group[512];
    size_t cell_group_len;
} rrc_setup_t;

/* RRC Setup Complete */
typedef struct {
    uint8_t rrc_transaction_id;
    uint8_t selected_plmn;
    uint8_t nas_pdu[1024];
    size_t nas_pdu_len;
} rrc_setup_complete_t;

/* RRC Reestablishment Request */
typedef struct {
    rrc_ue_identity_t ue_identity;
    rrc_reestablishment_cause_t reestablishment_cause;
    uint16_t pci;
    uint32_t c_rnti;
    uint8_t short_mac_i[2];
} rrc_reest_request_t;

/* RRC Reestablishment */
typedef struct {
    uint8_t rrc_transaction_id;
    uint8_t radio_bearer_config[256];
    size_t config_len;
} rrc_reestablishment_t;

/* RRC Reestablishment Complete */
typedef struct {
    uint8_t rrc_transaction_id;
} rrc_reest_complete_t;

/* RRC Reconfiguration */
typedef struct {
    uint8_t rrc_transaction_id;
    uint8_t radio_bearer_config[512];
    size_t config_len;
    uint8_t meas_config[256];
    size_t meas_config_len;
    bool has_mobility_config;
    uint8_t mobility_config[128];
    size_t mobility_config_len;
} rrc_reconfiguration_t;

/* RRC Reconfiguration Complete */
typedef struct {
    uint8_t rrc_transaction_id;
} rrc_reconfig_complete_t;

/* RRC Measurement Report */
typedef struct {
    uint8_t meas_id;
    int32_t rsrp;
    int32_t rsrq;
    uint16_t pci;
    uint32_t cell_id;
} rrc_measurement_report_t;

/* RRC Handover Command */
typedef struct {
    uint8_t rrc_transaction_id;
    uint16_t target_pci;
    uint32_t target_cell_id;
    uint8_t new_c_rnti;
    uint8_t radio_bearer_config[512];
    size_t config_len;
} rrc_handover_command_t;

/* RRC Handover Confirmation */
typedef struct {
    uint8_t rrc_transaction_id;
} rrc_handover_confirm_t;

/* RRC UE Capability Enquiry */
typedef struct {
    uint8_t enquiry_id;
    uint8_t rat_types[8];
    size_t num_rat_types;
} rrc_ue_cap_enquiry_t;

/* RRC UE Capability Information */
typedef struct {
    uint8_t rat_type;
    uint8_t capability_container[1024];
    size_t container_len;
} rrc_ue_cap_info_t;

/* RRC Connection Release */
typedef struct {
    uint8_t release_cause;
    bool redirect_carrier;
    uint16_t redirect_earfcn;
} rrc_connection_release_t;

/* RRC Handover Preparation */
typedef struct {
    uint8_t meas_id;
    int32_t rsrp;
    int32_t rsrq;
    uint16_t pci;
    uint32_t cell_id;
} rrc_handover_prep_t;

/* RRC Security Mode Command */
typedef struct {
    uint8_t rrc_transaction_id;
    uint8_t ciphering_alg;
    uint8_t integrity_alg;
    uint8_t security_capabilities[4];
    size_t capabilities_len;
} rrc_security_mode_cmd_t;

/* RRC Security Mode Complete */
typedef struct {
    uint8_t rrc_transaction_id;
} rrc_security_mode_complete_t;

/* Buffer management */
uesim_error_t asn1_buffer_init(asn1_buffer_t* buf, uint8_t* data, size_t capacity);
uesim_error_t asn1_buffer_alloc(asn1_buffer_t* buf, size_t initial_capacity);
void asn1_buffer_free(asn1_buffer_t* buf);
size_t asn1_buffer_length(const asn1_buffer_t* buf);

/* Bit operations */
uesim_error_t asn1_encode_bits(asn1_buffer_t* buf, uint32_t value, uint8_t num_bits);
uesim_error_t asn1_decode_bits(const uint8_t* data, size_t* bit_offset, uint32_t* value, uint8_t num_bits);

/* PER primitives */
uesim_error_t asn1_encode_boolean(asn1_buffer_t* buf, bool value);
uesim_error_t asn1_encode_integer(asn1_buffer_t* buf, int64_t value, uint8_t num_bits);
uesim_error_t asn1_encode_enumerated(asn1_buffer_t* buf, uint32_t value, uint32_t num_values);
uesim_error_t asn1_encode_octet_string(asn1_buffer_t* buf, const uint8_t* data, size_t len);
uesim_error_t asn1_encode_length(asn1_buffer_t* buf, size_t len);

uesim_error_t asn1_decode_boolean(const uint8_t* data, size_t* bit_offset, bool* value);
uesim_error_t asn1_decode_integer(const uint8_t* data, size_t* bit_offset, int64_t* value, uint8_t num_bits);
uesim_error_t asn1_decode_enumerated(const uint8_t* data, size_t* bit_offset, uint32_t* value, uint32_t num_values);
uesim_error_t asn1_decode_octet_string(const uint8_t* data, size_t* bit_offset, uint8_t* out, size_t max_len, size_t* out_len);

/* RRC Setup messages */
uesim_error_t rrc_encode_setup_request(asn1_buffer_t* buf, const rrc_setup_request_t* msg);
uesim_error_t rrc_decode_setup_request(const uint8_t* data, size_t len, rrc_setup_request_t* msg);
uesim_error_t rrc_encode_setup(asn1_buffer_t* buf, const rrc_setup_t* msg);
uesim_error_t rrc_decode_setup(const uint8_t* data, size_t len, rrc_setup_t* msg);
uesim_error_t rrc_encode_setup_complete(asn1_buffer_t* buf, const rrc_setup_complete_t* msg);
uesim_error_t rrc_decode_setup_complete(const uint8_t* data, size_t len, rrc_setup_complete_t* msg);

/* RRC Reestablishment messages */
uesim_error_t rrc_encode_reest_request(asn1_buffer_t* buf, const rrc_reest_request_t* msg);
uesim_error_t rrc_decode_reest_request(const uint8_t* data, size_t len, rrc_reest_request_t* msg);
uesim_error_t rrc_encode_reestablishment(asn1_buffer_t* buf, const rrc_reestablishment_t* msg);
uesim_error_t rrc_decode_reestablishment(const uint8_t* data, size_t len, rrc_reestablishment_t* msg);
uesim_error_t rrc_encode_reest_complete(asn1_buffer_t* buf, const rrc_reest_complete_t* msg);

/* RRC Reconfiguration messages */
uesim_error_t rrc_encode_reconfiguration(asn1_buffer_t* buf, const rrc_reconfiguration_t* msg);
uesim_error_t rrc_decode_reconfiguration(const uint8_t* data, size_t len, rrc_reconfiguration_t* msg);
uesim_error_t rrc_encode_reconfig_complete(asn1_buffer_t* buf, const rrc_reconfig_complete_t* msg);
uesim_error_t rrc_decode_reconfig_complete(const uint8_t* data, size_t len, rrc_reconfig_complete_t* msg);

/* RRC Measurement messages */
uesim_error_t rrc_encode_measurement_report(asn1_buffer_t* buf, const rrc_measurement_report_t* msg);
uesim_error_t rrc_decode_measurement_report(const uint8_t* data, size_t len, rrc_measurement_report_t* msg);

/* RRC Handover messages */
uesim_error_t rrc_encode_handover_command(asn1_buffer_t* buf, const rrc_handover_command_t* msg);
uesim_error_t rrc_decode_handover_command(const uint8_t* data, size_t len, rrc_handover_command_t* msg);
uesim_error_t rrc_encode_handover_confirm(asn1_buffer_t* buf, const rrc_handover_confirm_t* msg);

/* RRC UE Capability messages */
uesim_error_t rrc_encode_ue_cap_enquiry(asn1_buffer_t* buf, const rrc_ue_cap_enquiry_t* msg);
uesim_error_t rrc_decode_ue_cap_enquiry(const uint8_t* data, size_t len, rrc_ue_cap_enquiry_t* msg);
uesim_error_t rrc_encode_ue_cap_info(asn1_buffer_t* buf, const rrc_ue_cap_info_t* msg);

/* RRC Connection Release */
uesim_error_t rrc_encode_connection_release(asn1_buffer_t* buf, const rrc_connection_release_t* msg);
uesim_error_t rrc_decode_connection_release(const uint8_t* data, size_t len, rrc_connection_release_t* msg);

/* RRC Handover Preparation */
uesim_error_t rrc_encode_handover_prep(asn1_buffer_t* buf, const rrc_handover_prep_t* msg);

/* RRC Security Mode Command */
uesim_error_t rrc_encode_security_mode_cmd(asn1_buffer_t* buf, const rrc_security_mode_cmd_t* msg);

/* RRC Security Mode Complete */
uesim_error_t rrc_encode_security_mode_complete(asn1_buffer_t* buf, const rrc_security_mode_complete_t* msg);

/* Length encoding/decoding */
uesim_error_t asn1_decode_length(const uint8_t* data, size_t* bit_offset, size_t* len);

/* BIT STRING */
uesim_error_t asn1_encode_bit_string(asn1_buffer_t* buf, const uint8_t* data, size_t num_bits, bool has_constraint, size_t fixed_size);
uesim_error_t asn1_decode_bit_string(const uint8_t* data, size_t* bit_offset, uint8_t* out, size_t max_bytes, size_t* num_bits, bool has_constraint, size_t fixed_size);

/* SEQUENCE */
uesim_error_t asn1_encode_sequence(asn1_buffer_t* buf, const bool* optional_present, uint8_t num_optional);
uesim_error_t asn1_decode_sequence(const uint8_t* data, size_t* bit_offset, bool* optional_present, uint8_t num_optional);

/* SEQUENCE OF */
uesim_error_t asn1_encode_sequence_of(asn1_buffer_t* buf, size_t count, size_t min_size, size_t max_size);
uesim_error_t asn1_decode_sequence_of(const uint8_t* data, size_t* bit_offset, size_t* count, size_t min_size, size_t max_size);

/* CHOICE */
uesim_error_t asn1_encode_choice(asn1_buffer_t* buf, uint32_t choice_index, uint32_t num_choices);
uesim_error_t asn1_decode_choice(const uint8_t* data, size_t* bit_offset, uint32_t* choice_index, uint32_t num_choices);

/* NULL */
uesim_error_t asn1_encode_null(asn1_buffer_t* buf);
uesim_error_t asn1_decode_null(const uint8_t* data, size_t* bit_offset);

/* Constrained Integer */
uesim_error_t asn1_encode_constrained_integer(asn1_buffer_t* buf, int64_t value, int64_t min_val, int64_t max_val);
uesim_error_t asn1_decode_constrained_integer(const uint8_t* data, size_t* bit_offset, int64_t* value, int64_t min_val, int64_t max_val);

/* Object Identifier */
uesim_error_t asn1_encode_oid(asn1_buffer_t* buf, const uint32_t* oid_components, size_t num_components);
uesim_error_t asn1_decode_oid(const uint8_t* data, size_t* bit_offset, uint32_t* oid_components, size_t max_components, size_t* num_components);

/* Byte alignment */
uesim_error_t asn1_byte_align(asn1_buffer_t* buf);
void asn1_skip_to_byte_boundary(size_t* bit_offset);

/* Utility */
uint8_t asn1_count_bits(uint64_t value);

#endif /* ASN1_PER_H */
