/*
 * 5G UE Simulation Application
 * NAS PDU Validation - Message validation and integrity checking
 * 
 * This module provides:
 * - NAS message validation
 * - PDU length checking
 * - Mandatory IE verification
 * - Value range validation
 */

#ifndef NAS_VALIDATION_H
#define NAS_VALIDATION_H

#include "nas.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ============== Constants ============== */

#define NAS_MAX_PDU_SIZE            65535
#define NAS_MIN_HEADER_SIZE         3
#define NAS_MAX_IE_COUNT            64

/* ============== Validation Results ============== */

typedef enum {
    NAS_VALIDATION_OK = 0,
    NAS_VALIDATION_ERROR_LENGTH = -1,
    NAS_VALIDATION_ERROR_MANDATORY_IE = -2,
    NAS_VALIDATION_ERROR_VALUE_RANGE = -3,
    NAS_VALIDATION_ERROR_INTEGRITY = -4,
    NAS_VALIDATION_ERROR_SEQUENCE = -5,
    NAS_VALIDATION_ERROR_PROTOCOL_DISCRIMINATOR = -6,
    NAS_VALIDATION_ERROR_MESSAGE_TYPE = -7,
    NAS_VALIDATION_ERROR_SECURITY_HEADER = -8,
    NAS_VALIDATION_ERROR_IE_LENGTH = -9,
    NAS_VALIDATION_ERROR_IE_TYPE = -10,
    NAS_VALIDATION_ERROR_NULL_POINTER = -11,
    NAS_VALIDATION_ERROR_BUFFER_TOO_SMALL = -12
} nas_validation_result_t;

/* ============== Validation Context ============== */

typedef struct {
    nas_validation_result_t result;
    uint8_t error_ie_type;          /* IE type that caused error */
    uint16_t error_ie_offset;       /* Offset of problematic IE */
    char error_message[128];        /* Human-readable error */
    uint32_t validated_bytes;       /* Bytes successfully validated */
    uint8_t ie_count;               /* Number of IEs validated */
} nas_validation_context_t;

/* ============== Validation Configuration ============== */

typedef struct {
    bool check_mandatory_ie;
    bool check_value_ranges;
    bool check_sequence;
    bool strict_mode;               /* Fail on warnings */
    bool allow_unknown_ie;          /* Skip unknown IEs */
    uint32_t max_message_size;
} nas_validation_config_t;

/* ============== Initialization ============== */

/**
 * Initialize NAS validation module
 * @param config Configuration (NULL for defaults)
 * @return NAS_VALIDATION_OK or error
 */
nas_validation_result_t nas_validation_init(const nas_validation_config_t* config);

/**
 * Get default validation configuration
 * @param config Configuration to fill
 */
void nas_validation_get_default_config(nas_validation_config_t* config);

/**
 * Cleanup NAS validation module
 */
void nas_validation_cleanup(void);

/* ============== Message Validation ============== */

/**
 * Validate NAS message header
 * @param data Raw message data
 * @param length Data length
 * @param ctx Validation context (output)
 * @return NAS_VALIDATION_OK or error
 */
nas_validation_result_t nas_validate_message_header(const uint8_t* data, size_t length,
                                                     nas_validation_context_t* ctx);

/**
 * Validate complete NAS message
 * @param data Raw message data
 * @param length Data length
 * @param msg_type Expected message type (0 for auto-detect)
 * @param ctx Validation context (output)
 * @return NAS_VALIDATION_OK or error
 */
nas_validation_result_t nas_validate_message(const uint8_t* data, size_t length,
                                              nas_message_type_t msg_type,
                                              nas_validation_context_t* ctx);

/**
 * Validate NAS PDU length
 * @param data Raw PDU data
 * @param length PDU length
 * @param expected_type Expected message type
 * @return NAS_VALIDATION_OK or error
 */
nas_validation_result_t nas_validate_pdu_length(const uint8_t* data, size_t length,
                                                 nas_message_type_t expected_type);

/* ============== Message-Specific Validation ============== */

/**
 * Validate registration request message
 * @param data Message data (after header)
 * @param length Data length
 * @param ctx Validation context
 * @return NAS_VALIDATION_OK or error
 */
nas_validation_result_t nas_validate_registration_request(const uint8_t* data, size_t length,
                                                           nas_validation_context_t* ctx);

/**
 * Validate registration accept message
 * @param data Message data (after header)
 * @param length Data length
 * @param ctx Validation context
 * @return NAS_VALIDATION_OK or error
 */
nas_validation_result_t nas_validate_registration_accept(const uint8_t* data, size_t length,
                                                          nas_validation_context_t* ctx);

/**
 * Validate authentication request message
 * @param data Message data (after header)
 * @param length Data length
 * @param ctx Validation context
 * @return NAS_VALIDATION_OK or error
 */
nas_validation_result_t nas_validate_authentication_request(const uint8_t* data, size_t length,
                                                             nas_validation_context_t* ctx);

/**
 * Validate security mode command message
 * @param data Message data (after header)
 * @param length Data length
 * @param ctx Validation context
 * @return NAS_VALIDATION_OK or error
 */
nas_validation_result_t nas_validate_security_mode_command(const uint8_t* data, size_t length,
                                                           nas_validation_context_t* ctx);

/**
 * Validate PDU session establishment request
 * @param data Message data (after header)
 * @param length Data length
 * @param ctx Validation context
 * @return NAS_VALIDATION_OK or error
 */
nas_validation_result_t nas_validate_pdu_session_establishment_request(const uint8_t* data, size_t length,
                                                                          nas_validation_context_t* ctx);

/**
 * Validate PDU session establishment accept
 * @param data Message data (after header)
 * @param length Data length
 * @param ctx Validation context
 * @return NAS_VALIDATION_OK or error
 */
nas_validation_result_t nas_validate_pdu_session_establishment_accept(const uint8_t* data, size_t length,
                                                                        nas_validation_context_t* ctx);

/**
 * Validate UL NAS transport message
 * @param data Message data (after header)
 * @param length Data length
 * @param ctx Validation context
 * @return NAS_VALIDATION_OK or error
 */
nas_validation_result_t nas_validate_ul_nas_transport(const uint8_t* data, size_t length,
                                                       nas_validation_context_t* ctx);

/**
 * Validate DL NAS transport message
 * @param data Message data (after header)
 * @param length Data length
 * @param ctx Validation context
 * @return NAS_VALIDATION_OK or error
 */
nas_validation_result_t nas_validate_dl_nas_transport(const uint8_t* data, size_t length,
                                                       nas_validation_context_t* ctx);

/* ============== IE Validation ============== */

/**
 * Validate mandatory IE presence
 * @param data Message data
 * @param length Data length
 * @param ie_type IE type to check
 * @param ctx Validation context
 * @return NAS_VALIDATION_OK or error
 */
nas_validation_result_t nas_validate_mandatory_ie(const uint8_t* data, size_t length,
                                                   uint8_t ie_type,
                                                   nas_validation_context_t* ctx);

/**
 * Validate IE value range
 * @param value Value to validate
 * @param min_val Minimum allowed value
 * @param max_val Maximum allowed value
 * @param ie_type IE type (for error reporting)
 * @return NAS_VALIDATION_OK or error
 */
nas_validation_result_t nas_validate_value_range(uint32_t value, uint32_t min_val, 
                                                   uint32_t max_val, uint8_t ie_type);

/**
 * Validate IE length
 * @param actual_len Actual IE length
 * @param expected_len Expected length (0 for variable)
 * @param min_len Minimum length for variable IE
 * @param max_len Maximum length for variable IE
 * @return NAS_VALIDATION_OK or error
 */
nas_validation_result_t nas_validate_ie_length(uint16_t actual_len, uint16_t expected_len,
                                                uint16_t min_len, uint16_t max_len);

/* ============== Security Validation ============== */

/**
 * Validate NAS security header
 * @param header Security header byte
 * @param ctx Validation context
 * @return NAS_VALIDATION_OK or error
 */
nas_validation_result_t nas_validate_security_header(uint8_t header,
                                                      nas_validation_context_t* ctx);

/**
 * Validate message authentication code (MAC)
 * @param data Message data
 * @param length Data length
 * @param expected_mac Expected MAC value
 * @param ctx Validation context
 * @return NAS_VALIDATION_OK or error
 */
nas_validation_result_t nas_validate_mac(const uint8_t* data, size_t length,
                                          uint32_t expected_mac,
                                          nas_validation_context_t* ctx);

/* ============== Utility Functions ============== */

/**
 * Convert validation result to string
 * @param result Validation result
 * @return Result string
 */
const char* nas_validation_result_to_string(nas_validation_result_t result);

/**
 * Get IE name by type
 * @param ie_type IE type
 * @return IE name string
 */
const char* nas_ie_type_to_string(uint8_t ie_type);

/**
 * Check if message type is valid
 * @param msg_type Message type
 * @return true if valid
 */
bool nas_is_valid_message_type(uint8_t msg_type);

/**
 * Check if protocol discriminator is valid
 * @param pd Protocol discriminator
 * @return true if valid
 */
bool nas_is_valid_protocol_discriminator(uint8_t pd);

/**
 * Get minimum message length for type
 * @param msg_type Message type
 * @return Minimum length (0 if unknown type)
 */
size_t nas_get_min_message_length(nas_message_type_t msg_type);

/* ============== Validation Context Helpers ============== */

/**
 * Initialize validation context
 * @param ctx Context to initialize
 */
void nas_validation_context_init(nas_validation_context_t* ctx);

/**
 * Set validation error in context
 * @param ctx Validation context
 * @param result Error result
 * @param ie_type IE type that caused error
 * @param message Error message
 */
void nas_validation_set_error(nas_validation_context_t* ctx,
                               nas_validation_result_t result,
                               uint8_t ie_type,
                               const char* message);

/**
 * Print validation context
 * @param ctx Validation context
 */
void nas_validation_context_print(const nas_validation_context_t* ctx);

/* ============== Message Sequence Validation ============== */

/**
 * NAS message sequence state
 */
typedef enum {
    NAS_SEQ_STATE_IDLE = 0,
    NAS_SEQ_STATE_REGISTERING,
    NAS_SEQ_STATE_AUTHENTICATING,
    NAS_SEQ_STATE_SECURITY_MODE,
    NAS_SEQ_STATE_REGISTERED,
    NAS_SEQ_STATE_PDU_SESSION_PENDING,
    NAS_SEQ_STATE_CONNECTED,
    NAS_SEQ_STATE_DEREGISTERING
} nas_seq_state_t;

/**
 * Message sequence context
 */
typedef struct {
    nas_seq_state_t current_state;
    nas_seq_state_t expected_next;
    uint8_t last_msg_type;
    uint32_t sequence_number;
    uint32_t expected_sequence;
    bool sequence_valid;
    char error_msg[128];
} nas_sequence_context_t;

/**
 * Initialize sequence context
 * @param ctx Context to initialize
 */
void nas_sequence_context_init(nas_sequence_context_t* ctx);

/**
 * Validate message sequence
 * @param ctx Sequence context
 * @param msg_type Received message type
 * @param sequence_num Message sequence number
 * @return NAS_VALIDATION_OK or NAS_VALIDATION_ERROR_SEQUENCE
 */
nas_validation_result_t nas_validate_sequence(nas_sequence_context_t* ctx,
                                               uint8_t msg_type,
                                               uint32_t sequence_num);

/**
 * Get expected next message type
 * @param ctx Sequence context
 * @return Expected message type (0 if any)
 */
uint8_t nas_get_expected_next_msg_type(const nas_sequence_context_t* ctx);

/**
 * Check if state transition is valid
 * @param from Current state
 * @param to Target state
 * @param msg_type Message type causing transition
 * @return true if valid transition
 */
bool nas_is_valid_state_transition(nas_seq_state_t from, 
                                    nas_seq_state_t to,
                                    uint8_t msg_type);

/**
 * Convert sequence state to string
 * @param state Sequence state
 * @return State string
 */
const char* nas_seq_state_to_string(nas_seq_state_t state);

#endif /* NAS_VALIDATION_H */
