/*
 * 5G UE Simulation Application
 * NGAP PDU Validation - Message validation for NG Application Protocol
 * 
 * This module provides:
 * - NGAP message validation
 * - PDU structure checking
 * - Criticality validation
 * - IE presence verification
 */

#ifndef NGAP_VALIDATION_H
#define NGAP_VALIDATION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ============== Constants ============== */

#define NGAP_MAX_PDU_SIZE          65535
#define NGAP_MIN_HEADER_SIZE       4
#define NGAP_MAX_IE_COUNT          128
#define NGAP_MAX_NESTED_IES        16

/* ============== Validation Results ============== */

typedef enum {
    NGAP_VALIDATION_OK = 0,
    NGAP_VALIDATION_ERROR_LENGTH = -1,
    NGAP_VALIDATION_ERROR_MANDATORY_IE = -2,
    NGAP_VALIDATION_ERROR_VALUE_RANGE = -3,
    NGAP_VALIDATION_ERROR_CRITICALITY = -4,
    NGAP_VALIDATION_ERROR_PROCEDURE_CODE = -5,
    NGAP_VALIDATION_ERROR_MESSAGE_TYPE = -6,
    NGAP_VALIDATION_ERROR_IE_LENGTH = -7,
    NGAP_VALIDATION_ERROR_IE_TYPE = -8,
    NGAP_VALIDATION_ERROR_NULL_POINTER = -9,
    NGAP_VALIDATION_ERROR_BUFFER_TOO_SMALL = -10,
    NGAP_VALIDATION_ERROR_ENCODING = -11,
    NGAP_VALIDATION_ERROR_SEQUENCE = -12
} ngap_validation_result_t;

/* ============== NGAP Message Types ============== */

typedef enum {
    NGAP_MSG_NG_SETUP_REQUEST = 21,
    NGAP_MSG_NG_SETUP_RESPONSE = 22,
    NGAP_MSG_NG_SETUP_FAILURE = 23,
    NGAP_MSG_INITIAL_UE_MESSAGE = 25,
    NGAP_MSG_INITIAL_CONTEXT_SETUP_REQUEST = 26,
    NGAP_MSG_INITIAL_CONTEXT_SETUP_RESPONSE = 27,
    NGAP_MSG_INITIAL_CONTEXT_SETUP_FAILURE = 28,
    NGAP_MSG_UE_CONTEXT_RELEASE_REQUEST = 29,
    NGAP_MSG_UE_CONTEXT_RELEASE_COMMAND = 30,
    NGAP_MSG_UE_CONTEXT_RELEASE_COMPLETE = 31,
    NGAP_MSG_PDU_SESSION_SETUP_REQUEST = 32,
    NGAP_MSG_PDU_SESSION_SETUP_RESPONSE = 33,
    NGAP_MSG_PDU_SESSION_SETUP_FAILURE = 34,
    NGAP_MSG_PDU_SESSION_RELEASE_COMMAND = 35,
    NGAP_MSG_PDU_SESSION_RELEASE_RESPONSE = 36,
    NGAP_MSG_HANDOVER_PREPARATION = 37,
    NGAP_MSG_HANDOVER_REQUEST = 38,
    NGAP_MSG_HANDOVER_COMMAND = 39,
    NGAP_MSG_HANDOVER_SUCCESS = 40,
    NGAP_MSG_HANDOVER_FAILURE = 41,
    NGAP_MSG_ERROR_INDICATION = 0,
    NGAP_MSG_RESET = 1,
    NGAP_MSG_RESET_ACKNOWLEDGE = 2
} ngap_message_type_t;

/* ============== NGAP Procedure Codes ============== */

typedef enum {
    NGAP_PROC_NG_SETUP = 21,
    NGAP_PROC_INITIAL_UE = 25,
    NGAP_PROC_INITIAL_CONTEXT_SETUP = 26,
    NGAP_PROC_UE_CONTEXT_RELEASE = 29,
    NGAP_PROC_PDU_SESSION_SETUP = 32,
    NGAP_PROC_PDU_SESSION_RELEASE = 35,
    NGAP_PROC_HANDOVER_PREPARATION = 37,
    NGAP_PROC_HANDOVER_RESOURCE_ALLOCATION = 38,
    NGAP_PROC_ERROR_INDICATION = 0,
    NGAP_PROC_RESET = 1
} ngap_procedure_code_t;

/* ============== NGAP Criticality ============== */

typedef enum {
    NGAP_CRITICALITY_REJECT = 0,
    NGAP_CRITICALITY_IGNORE = 1,
    NGAP_CRITICALITY_NOTIFY = 2
} ngap_criticality_t;

/* ============== NGAP Presence ============== */

typedef enum {
    NGAP_PRESENCE_OPTIONAL = 0,
    NGAP_PRESENCE_CONDITIONAL = 1,
    NGAP_PRESENCE_MANDATORY = 2
} ngap_presence_t;

/* ============== Validation Context ============== */

typedef struct {
    ngap_validation_result_t result;
    uint32_t error_ie_id;           /* IE ID that caused error */
    uint16_t error_ie_offset;       /* Offset of problematic IE */
    char error_message[128];        /* Human-readable error */
    uint32_t validated_bytes;       /* Bytes successfully validated */
    uint8_t ie_count;               /* Number of IEs validated */
    uint8_t nested_depth;           /* Current nesting depth */
} ngap_validation_context_t;

/* ============== Validation Configuration ============== */

typedef struct {
    bool check_mandatory_ie;
    bool check_criticality;
    bool check_value_ranges;
    bool strict_mode;               /* Fail on warnings */
    bool allow_unknown_ie;          /* Skip unknown IEs */
    uint32_t max_message_size;
    uint8_t max_nested_depth;
} ngap_validation_config_t;

/* ============== Initialization ============== */

/**
 * Initialize NGAP validation module
 * @param config Configuration (NULL for defaults)
 * @return NGAP_VALIDATION_OK or error
 */
ngap_validation_result_t ngap_validation_init(const ngap_validation_config_t* config);

/**
 * Get default validation configuration
 * @param config Configuration to fill
 */
void ngap_validation_get_default_config(ngap_validation_config_t* config);

/**
 * Cleanup NGAP validation module
 */
void ngap_validation_cleanup(void);

/* ============== PDU Validation ============== */

/**
 * Validate NGAP PDU header
 * @param data Raw PDU data
 * @param length Data length
 * @param ctx Validation context (output)
 * @return NGAP_VALIDATION_OK or error
 */
ngap_validation_result_t ngap_validate_pdu_header(const uint8_t* data, size_t length,
                                                    ngap_validation_context_t* ctx);

/**
 * Validate complete NGAP PDU
 * @param data Raw PDU data
 * @param length Data length
 * @param ctx Validation context (output)
 * @return NGAP_VALIDATION_OK or error
 */
ngap_validation_result_t ngap_validate_pdu(const uint8_t* data, size_t length,
                                            ngap_validation_context_t* ctx);

/**
 * Validate NGAP PDU length
 * @param length PDU length
 * @param expected_type Expected message type
 * @return NGAP_VALIDATION_OK or error
 */
ngap_validation_result_t ngap_validate_pdu_length(size_t length, 
                                                    ngap_message_type_t expected_type);

/* ============== Message-Specific Validation ============== */

/**
 * Validate NG Setup Request
 * @param data Message data
 * @param length Data length
 * @param ctx Validation context
 * @return NGAP_VALIDATION_OK or error
 */
ngap_validation_result_t ngap_validate_ng_setup_request(const uint8_t* data, size_t length,
                                                         ngap_validation_context_t* ctx);

/**
 * Validate NG Setup Response
 * @param data Message data
 * @param length Data length
 * @param ctx Validation context
 * @return NGAP_VALIDATION_OK or error
 */
ngap_validation_result_t ngap_validate_ng_setup_response(const uint8_t* data, size_t length,
                                                           ngap_validation_context_t* ctx);

/**
 * Validate Initial UE Message
 * @param data Message data
 * @param length Data length
 * @param ctx Validation context
 * @return NGAP_VALIDATION_OK or error
 */
ngap_validation_result_t ngap_validate_initial_ue_message(const uint8_t* data, size_t length,
                                                           ngap_validation_context_t* ctx);

/**
 * Validate Initial Context Setup Request
 * @param data Message data
 * @param length Data length
 * @param ctx Validation context
 * @return NGAP_VALIDATION_OK or error
 */
ngap_validation_result_t ngap_validate_initial_context_setup_request(const uint8_t* data, size_t length,
                                                                      ngap_validation_context_t* ctx);

/**
 * Validate PDU Session Setup Request
 * @param data Message data
 * @param length Data length
 * @param ctx Validation context
 * @return NGAP_VALIDATION_OK or error
 */
ngap_validation_result_t ngap_validate_pdu_session_setup_request(const uint8_t* data, size_t length,
                                                                  ngap_validation_context_t* ctx);

/**
 * Validate Error Indication
 * @param data Message data
 * @param length Data length
 * @param ctx Validation context
 * @return NGAP_VALIDATION_OK or error
 */
ngap_validation_result_t ngap_validate_error_indication(const uint8_t* data, size_t length,
                                                         ngap_validation_context_t* ctx);

/* ============== IE Validation ============== */

/**
 * Validate mandatory IE presence
 * @param data Message data
 * @param length Data length
 * @param ie_id IE ID to check
 * @param ctx Validation context
 * @return NGAP_VALIDATION_OK or error
 */
ngap_validation_result_t ngap_validate_mandatory_ie(const uint8_t* data, size_t length,
                                                    uint32_t ie_id,
                                                    ngap_validation_context_t* ctx);

/**
 * Validate IE criticality
 * @param criticality IE criticality value
 * @param ctx Validation context
 * @return NGAP_VALIDATION_OK or error
 */
ngap_validation_result_t ngap_validate_criticality(uint8_t criticality,
                                                    ngap_validation_context_t* ctx);

/**
 * Validate IE length
 * @param actual_len Actual IE length
 * @param expected_len Expected length (0 for variable)
 * @param min_len Minimum length for variable IE
 * @param max_len Maximum length for variable IE
 * @return NGAP_VALIDATION_OK or error
 */
ngap_validation_result_t ngap_validate_ie_length(uint16_t actual_len, uint16_t expected_len,
                                                  uint16_t min_len, uint16_t max_len);

/* ============== Utility Functions ============== */

/**
 * Convert validation result to string
 * @param result Validation result
 * @return Result string
 */
const char* ngap_validation_result_to_string(ngap_validation_result_t result);

/**
 * Get IE name by ID
 * @param ie_id IE ID
 * @return IE name string
 */
const char* ngap_ie_id_to_string(uint32_t ie_id);

/**
 * Get message name by type
 * @param msg_type Message type
 * @return Message name string
 */
const char* ngap_message_type_to_string(ngap_message_type_t msg_type);

/**
 * Check if message type is valid
 * @param msg_type Message type
 * @return true if valid
 */
bool ngap_is_valid_message_type(uint8_t msg_type);

/**
 * Check if procedure code is valid
 * @param proc_code Procedure code
 * @return true if valid
 */
bool ngap_is_valid_procedure_code(uint8_t proc_code);

/**
 * Get minimum message length for type
 * @param msg_type Message type
 * @return Minimum length (0 if unknown type)
 */
size_t ngap_get_min_message_length(ngap_message_type_t msg_type);

/* ============== Validation Context Helpers ============== */

/**
 * Initialize validation context
 * @param ctx Context to initialize
 */
void ngap_validation_context_init(ngap_validation_context_t* ctx);

/**
 * Set validation error in context
 * @param ctx Validation context
 * @param result Error result
 * @param ie_id IE ID that caused error
 * @param message Error message
 */
void ngap_validation_set_error(ngap_validation_context_t* ctx,
                                ngap_validation_result_t result,
                                uint32_t ie_id,
                                const char* message);

/**
 * Print validation context
 * @param ctx Validation context
 */
void ngap_validation_context_print(const ngap_validation_context_t* ctx);

#endif /* NGAP_VALIDATION_H */
