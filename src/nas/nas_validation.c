/*
 * 5G UE Simulation Application
 * NAS PDU Validation - Implementation
 */

#include "nas_validation.h"
#include <string.h>
#include <stdio.h>

/* ============== Internal State ============== */

static nas_validation_config_t g_config = {0};
static bool g_initialized = false;

/* ============== IE Type Definitions ============== */

/* NAS IE Types (3GPP TS 24.501) */
#define NAS_IE_EXTENDED_PROTOCOL_DISCRIMINATOR   0x7E
#define NAS_IE_PDU_SESSION_ID                    0x12
#define NAS_IE_PTI                                0x19
#define NAS_IE_5GMM_CAPABILITY                    0x10
#define NAS_IE_5GSM_CAPABILITY                    0x11
#define NAS_IE_5GS_DRX_PARAMETERS                  0x13
#define NAS_IE_5GS_MOBILE_IDENTITY                0x15
#define NAS_IE_5GS_NETWORK_FEATURE_SUPPORT        0x16
#define NAS_IE_5GS_NETWORK_NAME                   0x17
#define NAS_IE_5GS_REGISTRATION_RESULT            0x18
#define NAS_IE_5GS_TRACKING_AREA_IDENTITY         0x1A
#define NAS_IE_5GS_UPDATE_TYPE                    0x1B
#define NAS_IE_ALLOWED_NSSAI                      0x1C
#define NAS_IE_AUTHENTICATION_PARAMETER_AUTN       0x20
#define NAS_IE_AUTHENTICATION_PARAMETER_RAND       0x21
#define NAS_IE_EAP_MESSAGE                        0x2A
#define NAS_IE_GPRS_TIMER                         0x2B
#define NAS_IE_GPRS_TIMER_2                        0x2C
#define NAS_IE_GPRS_TIMER_3                        0x2D
#define NAS_IE_IMEISV_REQUEST                     0x2E
#define NAS_IE_5GS_MOBILE_STATION_CLASSMARK_2      0x2F
#define NAS_IE_NAS_KEY_SET_IDENTIFIER             0x30
#define NAS_IE_5GS_NETWORK_FEATURE_SUPPORT_2      0x31
#define NAS_IE_NON_3GPP_NW_PROVIDED_POLICIES      0x32
#define NAS_IE_PDU_SESSION_REACTIVATION_RESULT    0x33
#define NAS_IE_PDU_SESSION_STATUS                 0x34
#define NAS_IE_PDU_ADDRESS                        0x35
#define NAS_IE_QOS_RULES                          0x36
#define NAS_IE_SESSION_AMBR                       0x37
#define NAS_IE_S_NSSAI                            0x38
#define NAS_IE_SERVICE_AREA_LIST                   0x39
#define NAS_IE_TAI_LIST                           0x3A
#define NAS_IE_5GMM_CAUSE                         0x3B
#define NAS_IE_5GSM_CAUSE                         0x3C
#define NAS_IE_S1_UE_NETWORK_CAPABILITY           0x3D
#define NAS_IE_USER_LOCATION_INFORMATION          0x3E
#define NAS_IE_5GSM_NETWORK_FEATURE_SUPPORT       0x3F
#define NAS_IE_PAYLOAD_CONTAINER                   0x40
#define NAS_IE_PAYLOAD_CONTAINER_TYPE              0x41
#define NAS_IE_5G_ACCESS_TYPE                      0x42
#define NAS_IE_REJECTED_NSSAI                     0x43
#define NAS_IE_SOR_TRANSPARENT_CONTAINER          0x44
#define NAS_IE_UP_INTERWORKING_INFO               0x45
#define NAS_IE_UPLINK_DATA_STATUS                 0x46
#define NAS_IE_ALLOWED_PDU_SESSION_STATUS         0x47
#define NAS_IE_CONFIGURATION_INDICATOR            0x48
#define NAS_IE_DNN                                0x49
#define NAS_IE_EMERGENCY_NUMBER_LIST              0x4A
#define NAS_IE_EXTENDED_EMERGENCY_NUMBER_LIST     0x4B
#define NAS_IE_EXTENDED_DRX_PARAMETERS            0x4C
#define NAS_IE_HEADER_COMPRESSION_CONFIGURATION   0x4D
#define NAS_IE_MICO_INDICATION                    0x4E
#define NAS_IE_NETWORK_SLICING_INDICATION         0x4F
#define NAS_IE_NETWORK_SLICING_SUBSCRIPTION_DIFF  0x50
#define NAS_IE_OPERATOR_DEFINED_ACCESS_CATEGORY    0x51
#define NAS_IE_PLMN_IDENTITY                      0x52
#define NAS_IE_PDU_SESSION_TYPE                   0x53
#define NAS_IE_QOS_FLOW_DESCRIPTIONS              0x54
#define NAS_IE_QOS_FLOW_IDENTIFIER                0x55
#define NAS_IE_RQ_TIMER                           0x56
#define NAS_IE_SSC_MODE                          0x57
#define NAS_IE_SERVICE_LEVEL_AA_CONTAINER         0x58
#define NAS_IE_TAI                                0x59
#define NAS_IE_TARGET_TAI_LIST                    0x5A
#define NAS_IE_UPLINK_TIME_SYNCHRONIZATION_BEARER 0x5B
#define NAS_IE_WUS_ASSISTANCE_INFORMATION         0x5C
#define NAS_IE_ACCESS_TYPE                        0x5D
#define NAS_IE_5GS_REGISTRATION_TYPE              0x5E
#define NAS_IE_5GS_IDENTITY_TYPE                  0x5F
#define NAS_IE_USER_LOCATION_INFORMATION_PLMN     0x60
#define NAS_IE_USER_LOCATION_INFORMATION_TAI      0x61
#define NAS_IE_USER_LOCATION_INFORMATION_ECGI     0x62
#define NAS_IE_USER_LOCATION_INFORMATION_NCGI    0x63
#define NAS_IE_USER_LOCATION_INFORMATION_GCI     0x64
#define NAS_IE_USER_LOCATION_INFORMATION_AI       0x65
#define NAS_IE_USER_LOCATION_INFORMATION_SAI      0x66
#define NAS_IE_USER_LOCATION_INFORMATION_CGI      0x67
#define NAS_IE_USER_LOCATION_INFORMATION_LAI      0x68
#define NAS_IE_USER_LOCATION_INFORMATION_TAI_1    0x69
#define NAS_IE_USER_LOCATION_INFORMATION_ECGI_1   0x6A
#define NAS_IE_USER_LOCATION_INFORMATION_NCGI_1   0x6B
#define NAS_IE_USER_LOCATION_INFORMATION_GCI_1    0x6C
#define NAS_IE_USER_LOCATION_INFORMATION_AI_1      0x6D
#define NAS_IE_USER_LOCATION_INFORMATION_SAI_1     0x6E
#define NAS_IE_USER_LOCATION_INFORMATION_CGI_1     0x6F
#define NAS_IE_USER_LOCATION_INFORMATION_LAI_1     0x70

/* ============== Initialization ============== */

nas_validation_result_t nas_validation_init(const nas_validation_config_t* config) {
    if (config != NULL) {
        memcpy(&g_config, config, sizeof(nas_validation_config_t));
    } else {
        nas_validation_get_default_config(&g_config);
    }
    g_initialized = true;
    return NAS_VALIDATION_OK;
}

void nas_validation_get_default_config(nas_validation_config_t* config) {
    if (config == NULL) return;
    
    config->check_mandatory_ie = true;
    config->check_value_ranges = true;
    config->check_sequence = true;
    config->strict_mode = false;
    config->allow_unknown_ie = true;
    config->max_message_size = NAS_MAX_PDU_SIZE;
}

void nas_validation_cleanup(void) {
    memset(&g_config, 0, sizeof(g_config));
    g_initialized = false;
}

/* ============== Validation Context Helpers ============== */

void nas_validation_context_init(nas_validation_context_t* ctx) {
    if (ctx == NULL) return;
    memset(ctx, 0, sizeof(nas_validation_context_t));
    ctx->result = NAS_VALIDATION_OK;
}

void nas_validation_set_error(nas_validation_context_t* ctx,
                               nas_validation_result_t result,
                               uint8_t ie_type,
                               const char* message) {
    if (ctx == NULL) return;
    
    ctx->result = result;
    ctx->error_ie_type = ie_type;
    if (message != NULL) {
        strncpy(ctx->error_message, message, sizeof(ctx->error_message) - 1);
        ctx->error_message[sizeof(ctx->error_message) - 1] = '\0';
    }
}

void nas_validation_context_print(const nas_validation_context_t* ctx) {
    if (ctx == NULL) return;
    
    printf("NAS Validation Context:\n");
    printf("  Result: %s\n", nas_validation_result_to_string(ctx->result));
    printf("  Validated Bytes: %u\n", ctx->validated_bytes);
    printf("  IE Count: %u\n", ctx->ie_count);
    if (ctx->result != NAS_VALIDATION_OK) {
        printf("  Error IE Type: 0x%02X (%s)\n", 
               ctx->error_ie_type, nas_ie_type_to_string(ctx->error_ie_type));
        printf("  Error Message: %s\n", ctx->error_message);
    }
}

/* ============== Message Validation ============== */

nas_validation_result_t nas_validate_message_header(const uint8_t* data, size_t length,
                                                     nas_validation_context_t* ctx) {
    nas_validation_context_t local_ctx;
    if (ctx == NULL) {
        ctx = &local_ctx;
        nas_validation_context_init(ctx);
    }
    
    if (data == NULL) {
        nas_validation_set_error(ctx, NAS_VALIDATION_ERROR_NULL_POINTER, 0, "NULL data pointer");
        return NAS_VALIDATION_ERROR_NULL_POINTER;
    }
    
    if (length < NAS_MIN_HEADER_SIZE) {
        nas_validation_set_error(ctx, NAS_VALIDATION_ERROR_BUFFER_TOO_SMALL, 0, 
                                 "Buffer too small for header");
        return NAS_VALIDATION_ERROR_BUFFER_TOO_SMALL;
    }
    
    /* Check extended protocol discriminator */
    uint8_t epd = data[0];
    if (!nas_is_valid_protocol_discriminator(epd)) {
        nas_validation_set_error(ctx, NAS_VALIDATION_ERROR_PROTOCOL_DISCRIMINATOR, 
                                 NAS_IE_EXTENDED_PROTOCOL_DISCRIMINATOR,
                                 "Invalid protocol discriminator");
        return NAS_VALIDATION_ERROR_PROTOCOL_DISCRIMINATOR;
    }
    
    /* Check security header type */
    uint8_t sec_header = data[1] & 0x0F;
    nas_validation_result_t sec_result = nas_validate_security_header(sec_header, ctx);
    if (sec_result != NAS_VALIDATION_OK) {
        return sec_result;
    }
    
    /* Check message type */
    uint8_t msg_type = data[2];
    if (!nas_is_valid_message_type(msg_type)) {
        nas_validation_set_error(ctx, NAS_VALIDATION_ERROR_MESSAGE_TYPE, 0,
                                 "Invalid message type");
        return NAS_VALIDATION_ERROR_MESSAGE_TYPE;
    }
    
    ctx->validated_bytes = 3;
    ctx->ie_count = 0;
    
    return NAS_VALIDATION_OK;
}

nas_validation_result_t nas_validate_message(const uint8_t* data, size_t length,
                                              nas_message_type_t msg_type,
                                              nas_validation_context_t* ctx) {
    nas_validation_context_t local_ctx;
    if (ctx == NULL) {
        ctx = &local_ctx;
        nas_validation_context_init(ctx);
    }
    
    if (data == NULL) {
        nas_validation_set_error(ctx, NAS_VALIDATION_ERROR_NULL_POINTER, 0, "NULL data pointer");
        return NAS_VALIDATION_ERROR_NULL_POINTER;
    }
    
    if (length > g_config.max_message_size) {
        nas_validation_set_error(ctx, NAS_VALIDATION_ERROR_LENGTH, 0, "Message exceeds max size");
        return NAS_VALIDATION_ERROR_LENGTH;
    }
    
    /* Validate header first */
    nas_validation_result_t result = nas_validate_message_header(data, length, ctx);
    if (result != NAS_VALIDATION_OK) {
        return result;
    }
    
    /* Get actual message type from header if not specified */
    if (msg_type == 0) {
        msg_type = (nas_message_type_t)data[2];
    }
    
    /* Validate minimum length */
    size_t min_len = nas_get_min_message_length(msg_type);
    if (length < min_len) {
        nas_validation_set_error(ctx, NAS_VALIDATION_ERROR_LENGTH, 0,
                                 "Message shorter than minimum");
        return NAS_VALIDATION_ERROR_LENGTH;
    }
    
    /* Message-specific validation */
    const uint8_t* payload = data + 3;
    size_t payload_len = length - 3;
    
    switch (msg_type) {
        case NAS_MSG_TYPE_REGISTRATION_REQUEST:
            return nas_validate_registration_request(payload, payload_len, ctx);
            
        case NAS_MSG_TYPE_REGISTRATION_ACCEPT:
            return nas_validate_registration_accept(payload, payload_len, ctx);
            
        case NAS_MSG_TYPE_AUTHENTICATION_REQUEST:
            return nas_validate_authentication_request(payload, payload_len, ctx);
            
        case NAS_MSG_TYPE_SECURITY_MODE_COMMAND:
            return nas_validate_security_mode_command(payload, payload_len, ctx);
            
        case NAS_MSG_TYPE_PDU_SESSION_ESTABLISHMENT_REQUEST:
            return nas_validate_pdu_session_establishment_request(payload, payload_len, ctx);
            
        case NAS_MSG_TYPE_PDU_SESSION_ESTABLISHMENT_ACCEPT:
            return nas_validate_pdu_session_establishment_accept(payload, payload_len, ctx);
            
        case NAS_MSG_TYPE_UL_NAS_TRANSPORT:
            return nas_validate_ul_nas_transport(payload, payload_len, ctx);
            
        case NAS_MSG_TYPE_DL_NAS_TRANSPORT:
            return nas_validate_dl_nas_transport(payload, payload_len, ctx);
            
        default:
            /* Unknown message type - skip specific validation */
            ctx->validated_bytes = length;
            return NAS_VALIDATION_OK;
    }
}

nas_validation_result_t nas_validate_pdu_length(const uint8_t* data, size_t length,
                                                 nas_message_type_t expected_type) {
    if (data == NULL) {
        return NAS_VALIDATION_ERROR_NULL_POINTER;
    }
    
    size_t min_len = nas_get_min_message_length(expected_type);
    if (length < min_len) {
        return NAS_VALIDATION_ERROR_LENGTH;
    }
    
    if (length > NAS_MAX_PDU_SIZE) {
        return NAS_VALIDATION_ERROR_LENGTH;
    }
    
    return NAS_VALIDATION_OK;
}

/* ============== Message-Specific Validation ============== */

nas_validation_result_t nas_validate_registration_request(const uint8_t* data, size_t length,
                                                           nas_validation_context_t* ctx) {
    if (data == NULL || length < 5) {
        nas_validation_set_error(ctx, NAS_VALIDATION_ERROR_LENGTH, 0,
                                 "Registration request too short");
        return NAS_VALIDATION_ERROR_LENGTH;
    }
    
    /* Check mandatory IEs: 5GS registration type, 5GS mobile identity */
    if (g_config.check_mandatory_ie) {
        /* 5GS registration type is mandatory */
        nas_validation_result_t result = nas_validate_mandatory_ie(data, length, 
                                                                   NAS_IE_5GS_REGISTRATION_TYPE, ctx);
        if (result != NAS_VALIDATION_OK) {
            return result;
        }
        
        /* 5GS mobile identity is mandatory */
        result = nas_validate_mandatory_ie(data, length, NAS_IE_5GS_MOBILE_IDENTITY, ctx);
        if (result != NAS_VALIDATION_OK) {
            return result;
        }
    }
    
    ctx->validated_bytes = length;
    return NAS_VALIDATION_OK;
}

nas_validation_result_t nas_validate_registration_accept(const uint8_t* data, size_t length,
                                                          nas_validation_context_t* ctx) {
    if (data == NULL || length < 3) {
        nas_validation_set_error(ctx, NAS_VALIDATION_ERROR_LENGTH, 0,
                                 "Registration accept too short");
        return NAS_VALIDATION_ERROR_LENGTH;
    }
    
    /* Check 5GS registration result */
    if (g_config.check_mandatory_ie) {
        nas_validation_result_t result = nas_validate_mandatory_ie(data, length,
                                                                   NAS_IE_5GS_REGISTRATION_RESULT, ctx);
        if (result != NAS_VALIDATION_OK) {
            return result;
        }
    }
    
    ctx->validated_bytes = length;
    return NAS_VALIDATION_OK;
}

nas_validation_result_t nas_validate_authentication_request(const uint8_t* data, size_t length,
                                                             nas_validation_context_t* ctx) {
    if (data == NULL || length < 35) {  /* RAND(16) + AUTN(16) + header */
        nas_validation_set_error(ctx, NAS_VALIDATION_ERROR_LENGTH, 0,
                                 "Authentication request too short");
        return NAS_VALIDATION_ERROR_LENGTH;
    }
    
    /* Check RAND and AUTN presence */
    if (g_config.check_mandatory_ie) {
        nas_validation_result_t result = nas_validate_mandatory_ie(data, length,
                                                                   NAS_IE_AUTHENTICATION_PARAMETER_RAND, ctx);
        if (result != NAS_VALIDATION_OK) {
            return result;
        }
        
        result = nas_validate_mandatory_ie(data, length,
                                           NAS_IE_AUTHENTICATION_PARAMETER_AUTN, ctx);
        if (result != NAS_VALIDATION_OK) {
            return result;
        }
    }
    
    ctx->validated_bytes = length;
    return NAS_VALIDATION_OK;
}

nas_validation_result_t nas_validate_security_mode_command(const uint8_t* data, size_t length,
                                                           nas_validation_context_t* ctx) {
    if (data == NULL || length < 5) {
        nas_validation_set_error(ctx, NAS_VALIDATION_ERROR_LENGTH, 0,
                                 "Security mode command too short");
        return NAS_VALIDATION_ERROR_LENGTH;
    }
    
    /* Check NAS key set identifier */
    if (g_config.check_mandatory_ie) {
        nas_validation_result_t result = nas_validate_mandatory_ie(data, length,
                                                                   NAS_IE_NAS_KEY_SET_IDENTIFIER, ctx);
        if (result != NAS_VALIDATION_OK) {
            return result;
        }
    }
    
    ctx->validated_bytes = length;
    return NAS_VALIDATION_OK;
}

nas_validation_result_t nas_validate_pdu_session_establishment_request(const uint8_t* data, size_t length,
                                                                          nas_validation_context_t* ctx) {
    if (data == NULL || length < 5) {
        nas_validation_set_error(ctx, NAS_VALIDATION_ERROR_LENGTH, 0,
                                 "PDU session establishment request too short");
        return NAS_VALIDATION_ERROR_LENGTH;
    }
    
    /* Check mandatory IEs */
    if (g_config.check_mandatory_ie) {
        nas_validation_result_t result = nas_validate_mandatory_ie(data, length,
                                                                   NAS_IE_PDU_SESSION_ID, ctx);
        if (result != NAS_VALIDATION_OK) {
            return result;
        }
        
        result = nas_validate_mandatory_ie(data, length, NAS_IE_PTI, ctx);
        if (result != NAS_VALIDATION_OK) {
            return result;
        }
    }
    
    ctx->validated_bytes = length;
    return NAS_VALIDATION_OK;
}

nas_validation_result_t nas_validate_pdu_session_establishment_accept(const uint8_t* data, size_t length,
                                                                        nas_validation_context_t* ctx) {
    if (data == NULL || length < 10) {
        nas_validation_set_error(ctx, NAS_VALIDATION_ERROR_LENGTH, 0,
                                 "PDU session establishment accept too short");
        return NAS_VALIDATION_ERROR_LENGTH;
    }
    
    /* Check mandatory IEs */
    if (g_config.check_mandatory_ie) {
        nas_validation_result_t result = nas_validate_mandatory_ie(data, length,
                                                                   NAS_IE_PDU_SESSION_ID, ctx);
        if (result != NAS_VALIDATION_OK) {
            return result;
        }
        
        result = nas_validate_mandatory_ie(data, length, NAS_IE_PDU_ADDRESS, ctx);
        if (result != NAS_VALIDATION_OK) {
            return result;
        }
    }
    
    ctx->validated_bytes = length;
    return NAS_VALIDATION_OK;
}

nas_validation_result_t nas_validate_ul_nas_transport(const uint8_t* data, size_t length,
                                                       nas_validation_context_t* ctx) {
    if (data == NULL || length < 5) {
        nas_validation_set_error(ctx, NAS_VALIDATION_ERROR_LENGTH, 0,
                                 "UL NAS transport too short");
        return NAS_VALIDATION_ERROR_LENGTH;
    }
    
    /* Check payload container */
    if (g_config.check_mandatory_ie) {
        nas_validation_result_t result = nas_validate_mandatory_ie(data, length,
                                                                   NAS_IE_PAYLOAD_CONTAINER, ctx);
        if (result != NAS_VALIDATION_OK) {
            return result;
        }
        
        result = nas_validate_mandatory_ie(data, length, NAS_IE_PAYLOAD_CONTAINER_TYPE, ctx);
        if (result != NAS_VALIDATION_OK) {
            return result;
        }
    }
    
    ctx->validated_bytes = length;
    return NAS_VALIDATION_OK;
}

nas_validation_result_t nas_validate_dl_nas_transport(const uint8_t* data, size_t length,
                                                       nas_validation_context_t* ctx) {
    /* Same as UL NAS transport */
    return nas_validate_ul_nas_transport(data, length, ctx);
}

/* ============== IE Validation ============== */

nas_validation_result_t nas_validate_mandatory_ie(const uint8_t* data, size_t length,
                                                   uint8_t ie_type,
                                                   nas_validation_context_t* ctx) {
    if (data == NULL) {
        return NAS_VALIDATION_ERROR_NULL_POINTER;
    }
    
    /* Scan for IE */
    size_t offset = 0;
    while (offset + 2 <= length) {
        uint8_t found_ie_type = data[offset];
        
        if (found_ie_type == ie_type) {
            return NAS_VALIDATION_OK;  /* Found */
        }
        
        /* Skip to next IE */
        if (offset + 3 <= length) {
            uint16_t ie_len = data[offset + 1];
            offset += 2 + ie_len;
        } else {
            break;
        }
    }
    
    nas_validation_set_error(ctx, NAS_VALIDATION_ERROR_MANDATORY_IE, ie_type,
                             "Mandatory IE not found");
    return NAS_VALIDATION_ERROR_MANDATORY_IE;
}

nas_validation_result_t nas_validate_value_range(uint32_t value, uint32_t min_val, 
                                                   uint32_t max_val, uint8_t ie_type) {
    if (value < min_val || value > max_val) {
        return NAS_VALIDATION_ERROR_VALUE_RANGE;
    }
    return NAS_VALIDATION_OK;
}

nas_validation_result_t nas_validate_ie_length(uint16_t actual_len, uint16_t expected_len,
                                                uint16_t min_len, uint16_t max_len) {
    if (expected_len > 0) {
        /* Fixed length IE */
        if (actual_len != expected_len) {
            return NAS_VALIDATION_ERROR_IE_LENGTH;
        }
    } else {
        /* Variable length IE */
        if (actual_len < min_len || actual_len > max_len) {
            return NAS_VALIDATION_ERROR_IE_LENGTH;
        }
    }
    return NAS_VALIDATION_OK;
}

/* ============== Security Validation ============== */

nas_validation_result_t nas_validate_security_header(uint8_t header,
                                                      nas_validation_context_t* ctx) {
    switch (header) {
        case NAS_SECURITY_HEADER_PLAIN:
        case NAS_SECURITY_HEADER_INTEGRITY_PROTECTED:
        case NAS_SECURITY_HEADER_INTEGRITY_PROTECTED_CIPHERED:
        case NAS_SECURITY_HEADER_INTEGRITY_PROTECTED_NEW:
        case NAS_SECURITY_HEADER_INTEGRITY_PROTECTED_CIPHERED_NEW:
        case NAS_SECURITY_HEADER_SERVICE_REQUEST:
            return NAS_VALIDATION_OK;
            
        default:
            if (ctx != NULL) {
                nas_validation_set_error(ctx, NAS_VALIDATION_ERROR_SECURITY_HEADER, 0,
                                         "Invalid security header type");
            }
            return NAS_VALIDATION_ERROR_SECURITY_HEADER;
    }
}

nas_validation_result_t nas_validate_mac(const uint8_t* data, size_t length,
                                          uint32_t expected_mac,
                                          nas_validation_context_t* ctx) {
    if (data == NULL || length < 4) {
        nas_validation_set_error(ctx, NAS_VALIDATION_ERROR_INTEGRITY, 0,
                                 "Data too short for MAC validation");
        return NAS_VALIDATION_ERROR_INTEGRITY;
    }
    
    /* Extract MAC from message (last 4 bytes before any padding) */
    uint32_t actual_mac = ((uint32_t)data[length - 4] << 24) |
                          ((uint32_t)data[length - 3] << 16) |
                          ((uint32_t)data[length - 2] << 8) |
                          (uint32_t)data[length - 1];
    
    if (actual_mac != expected_mac) {
        nas_validation_set_error(ctx, NAS_VALIDATION_ERROR_INTEGRITY, 0,
                                 "MAC verification failed");
        return NAS_VALIDATION_ERROR_INTEGRITY;
    }
    
    return NAS_VALIDATION_OK;
}

/* ============== Utility Functions ============== */

const char* nas_validation_result_to_string(nas_validation_result_t result) {
    switch (result) {
        case NAS_VALIDATION_OK: return "OK";
        case NAS_VALIDATION_ERROR_LENGTH: return "Length error";
        case NAS_VALIDATION_ERROR_MANDATORY_IE: return "Mandatory IE missing";
        case NAS_VALIDATION_ERROR_VALUE_RANGE: return "Value out of range";
        case NAS_VALIDATION_ERROR_INTEGRITY: return "Integrity check failed";
        case NAS_VALIDATION_ERROR_SEQUENCE: return "Sequence error";
        case NAS_VALIDATION_ERROR_PROTOCOL_DISCRIMINATOR: return "Invalid protocol discriminator";
        case NAS_VALIDATION_ERROR_MESSAGE_TYPE: return "Invalid message type";
        case NAS_VALIDATION_ERROR_SECURITY_HEADER: return "Invalid security header";
        case NAS_VALIDATION_ERROR_IE_LENGTH: return "IE length error";
        case NAS_VALIDATION_ERROR_IE_TYPE: return "Invalid IE type";
        case NAS_VALIDATION_ERROR_NULL_POINTER: return "NULL pointer";
        case NAS_VALIDATION_ERROR_BUFFER_TOO_SMALL: return "Buffer too small";
        default: return "Unknown error";
    }
}

const char* nas_ie_type_to_string(uint8_t ie_type) {
    switch (ie_type) {
        case NAS_IE_EXTENDED_PROTOCOL_DISCRIMINATOR: return "Extended Protocol Discriminator";
        case NAS_IE_PDU_SESSION_ID: return "PDU Session ID";
        case NAS_IE_PTI: return "PTI";
        case NAS_IE_5GMM_CAPABILITY: return "5GMM Capability";
        case NAS_IE_5GSM_CAPABILITY: return "5GSM Capability";
        case NAS_IE_5GS_DRX_PARAMETERS: return "5GS DRX Parameters";
        case NAS_IE_5GS_MOBILE_IDENTITY: return "5GS Mobile Identity";
        case NAS_IE_5GS_NETWORK_FEATURE_SUPPORT: return "5GS Network Feature Support";
        case NAS_IE_5GS_REGISTRATION_RESULT: return "5GS Registration Result";
        case NAS_IE_5GS_REGISTRATION_TYPE: return "5GS Registration Type";
        case NAS_IE_AUTHENTICATION_PARAMETER_AUTN: return "Authentication Parameter AUTN";
        case NAS_IE_AUTHENTICATION_PARAMETER_RAND: return "Authentication Parameter RAND";
        case NAS_IE_NAS_KEY_SET_IDENTIFIER: return "NAS Key Set Identifier";
        case NAS_IE_PDU_ADDRESS: return "PDU Address";
        case NAS_IE_PAYLOAD_CONTAINER: return "Payload Container";
        case NAS_IE_PAYLOAD_CONTAINER_TYPE: return "Payload Container Type";
        case NAS_IE_DNN: return "DNN";
        case NAS_IE_SSC_MODE: return "SSC Mode";
        case NAS_IE_S_NSSAI: return "S-NSSAI";
        case NAS_IE_SESSION_AMBR: return "Session AMBR";
        case NAS_IE_QOS_RULES: return "QoS Rules";
        case NAS_IE_QOS_FLOW_DESCRIPTIONS: return "QoS Flow Descriptions";
        case NAS_IE_5GMM_CAUSE: return "5GMM Cause";
        case NAS_IE_5GSM_CAUSE: return "5GSM Cause";
        default: return "Unknown IE";
    }
}

bool nas_is_valid_message_type(uint8_t msg_type) {
    /* 5GMM messages: 0x41-0x6F */
    /* 5GSM messages: 0xC1-0xDF */
    return ((msg_type >= 0x41 && msg_type <= 0x6F) ||
            (msg_type >= 0xC1 && msg_type <= 0xDF));
}

bool nas_is_valid_protocol_discriminator(uint8_t pd) {
    /* 5GMM: 0x7E, 5GSM: 0x2E */
    return (pd == 0x7E || pd == 0x2E);
}

size_t nas_get_min_message_length(nas_message_type_t msg_type) {
    switch (msg_type) {
        case NAS_MSG_TYPE_REGISTRATION_REQUEST:
            return 20;  /* Minimum with mandatory IEs */
        case NAS_MSG_TYPE_REGISTRATION_ACCEPT:
            return 10;
        case NAS_MSG_TYPE_AUTHENTICATION_REQUEST:
            return 35;  /* RAND + AUTN + headers */
        case NAS_MSG_TYPE_AUTHENTICATION_RESPONSE:
            return 18;  /* RES */
        case NAS_MSG_TYPE_SECURITY_MODE_COMMAND:
            return 10;
        case NAS_MSG_TYPE_SECURITY_MODE_COMPLETE:
            return 5;
        case NAS_MSG_TYPE_PDU_SESSION_ESTABLISHMENT_REQUEST:
            return 10;
        case NAS_MSG_TYPE_PDU_SESSION_ESTABLISHMENT_ACCEPT:
            return 20;
        case NAS_MSG_TYPE_UL_NAS_TRANSPORT:
        case NAS_MSG_TYPE_DL_NAS_TRANSPORT:
            return 10;
        case NAS_MSG_TYPE_REGISTRATION_COMPLETE:
            return 5;
        case NAS_MSG_TYPE_DEREGISTRATION_REQUEST:
            return 10;
        default:
            return NAS_MIN_HEADER_SIZE;
    }
}

/* ============== Message Sequence Validation ============== */

void nas_sequence_context_init(nas_sequence_context_t* ctx) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(nas_sequence_context_t));
    ctx->current_state = NAS_SEQ_STATE_IDLE;
    ctx->sequence_valid = true;
}

nas_validation_result_t nas_validate_sequence(nas_sequence_context_t* ctx,
                                               uint8_t msg_type,
                                               uint32_t sequence_num) {
    if (!ctx) return NAS_VALIDATION_ERROR_NULL_POINTER;
    
    nas_seq_state_t new_state = ctx->current_state;
    
    /* Determine new state based on message type */
    switch (msg_type) {
        case NAS_MSG_TYPE_REGISTRATION_REQUEST:
            if (ctx->current_state != NAS_SEQ_STATE_IDLE &&
                ctx->current_state != NAS_SEQ_STATE_REGISTERED) {
                snprintf(ctx->error_msg, sizeof(ctx->error_msg) - 1,
                         "Invalid registration request in state %s",
                         nas_seq_state_to_string(ctx->current_state));
                ctx->sequence_valid = false;
                return NAS_VALIDATION_ERROR_SEQUENCE;
            }
            new_state = NAS_SEQ_STATE_REGISTERING;
            break;
            
        case NAS_MSG_TYPE_AUTHENTICATION_REQUEST:
            if (ctx->current_state != NAS_SEQ_STATE_REGISTERING) {
                snprintf(ctx->error_msg, sizeof(ctx->error_msg) - 1,
                         "Invalid auth request in state %s",
                         nas_seq_state_to_string(ctx->current_state));
                ctx->sequence_valid = false;
                return NAS_VALIDATION_ERROR_SEQUENCE;
            }
            new_state = NAS_SEQ_STATE_AUTHENTICATING;
            break;
            
        case NAS_MSG_TYPE_AUTHENTICATION_RESPONSE:
            if (ctx->current_state != NAS_SEQ_STATE_AUTHENTICATING) {
                snprintf(ctx->error_msg, sizeof(ctx->error_msg) - 1,
                         "Invalid auth response in state %s",
                         nas_seq_state_to_string(ctx->current_state));
                ctx->sequence_valid = false;
                return NAS_VALIDATION_ERROR_SEQUENCE;
            }
            new_state = NAS_SEQ_STATE_SECURITY_MODE;
            break;
            
        case NAS_MSG_TYPE_SECURITY_MODE_COMMAND:
            if (ctx->current_state != NAS_SEQ_STATE_SECURITY_MODE) {
                snprintf(ctx->error_msg, sizeof(ctx->error_msg) - 1,
                         "Invalid security mode command in state %s",
                         nas_seq_state_to_string(ctx->current_state));
                ctx->sequence_valid = false;
                return NAS_VALIDATION_ERROR_SEQUENCE;
            }
            break;
            
        case NAS_MSG_TYPE_SECURITY_MODE_COMPLETE:
            if (ctx->current_state != NAS_SEQ_STATE_SECURITY_MODE) {
                snprintf(ctx->error_msg, sizeof(ctx->error_msg) - 1,
                         "Invalid security mode complete in state %s",
                         nas_seq_state_to_string(ctx->current_state));
                ctx->sequence_valid = false;
                return NAS_VALIDATION_ERROR_SEQUENCE;
            }
            new_state = NAS_SEQ_STATE_REGISTERED;
            break;
            
        case NAS_MSG_TYPE_REGISTRATION_ACCEPT:
            if (ctx->current_state != NAS_SEQ_STATE_REGISTERING &&
                ctx->current_state != NAS_SEQ_STATE_SECURITY_MODE) {
                snprintf(ctx->error_msg, sizeof(ctx->error_msg) - 1,
                         "Invalid registration accept in state %s",
                         nas_seq_state_to_string(ctx->current_state));
                ctx->sequence_valid = false;
                return NAS_VALIDATION_ERROR_SEQUENCE;
            }
            new_state = NAS_SEQ_STATE_REGISTERED;
            break;
            
        case NAS_MSG_TYPE_PDU_SESSION_ESTABLISHMENT_REQUEST:
            if (ctx->current_state != NAS_SEQ_STATE_REGISTERED &&
                ctx->current_state != NAS_SEQ_STATE_CONNECTED) {
                snprintf(ctx->error_msg, sizeof(ctx->error_msg) - 1,
                         "Invalid PDU session request in state %s",
                         nas_seq_state_to_string(ctx->current_state));
                ctx->sequence_valid = false;
                return NAS_VALIDATION_ERROR_SEQUENCE;
            }
            new_state = NAS_SEQ_STATE_PDU_SESSION_PENDING;
            break;
            
        case NAS_MSG_TYPE_PDU_SESSION_ESTABLISHMENT_ACCEPT:
            if (ctx->current_state != NAS_SEQ_STATE_PDU_SESSION_PENDING) {
                snprintf(ctx->error_msg, sizeof(ctx->error_msg) - 1,
                         "Invalid PDU session accept in state %s",
                         nas_seq_state_to_string(ctx->current_state));
                ctx->sequence_valid = false;
                return NAS_VALIDATION_ERROR_SEQUENCE;
            }
            new_state = NAS_SEQ_STATE_CONNECTED;
            break;
            
        case NAS_MSG_TYPE_DEREGISTRATION_REQUEST:
            if (ctx->current_state != NAS_SEQ_STATE_REGISTERED &&
                ctx->current_state != NAS_SEQ_STATE_CONNECTED) {
                snprintf(ctx->error_msg, sizeof(ctx->error_msg) - 1,
                         "Invalid deregistration request in state %s",
                         nas_seq_state_to_string(ctx->current_state));
                ctx->sequence_valid = false;
                return NAS_VALIDATION_ERROR_SEQUENCE;
            }
            new_state = NAS_SEQ_STATE_DEREGISTERING;
            break;
            
        default:
            /* Unknown message type - allow but don't change state */
            break;
    }
    
    /* Check sequence number if expected */
    if (ctx->expected_sequence > 0 && sequence_num != ctx->expected_sequence) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg) - 1,
                 "Sequence number mismatch: expected %u, got %u",
                 ctx->expected_sequence, sequence_num);
        ctx->sequence_valid = false;
        return NAS_VALIDATION_ERROR_SEQUENCE;
    }
    
    /* Update context */
    ctx->last_msg_type = msg_type;
    ctx->sequence_number = sequence_num;
    ctx->current_state = new_state;
    
    return NAS_VALIDATION_OK;
}

uint8_t nas_get_expected_next_msg_type(const nas_sequence_context_t* ctx) {
    if (!ctx) return 0;
    
    switch (ctx->current_state) {
        case NAS_SEQ_STATE_IDLE:
            return NAS_MSG_TYPE_REGISTRATION_REQUEST;
        case NAS_SEQ_STATE_REGISTERING:
            return NAS_MSG_TYPE_AUTHENTICATION_REQUEST;
        case NAS_SEQ_STATE_AUTHENTICATING:
            return NAS_MSG_TYPE_AUTHENTICATION_RESPONSE;
        case NAS_SEQ_STATE_SECURITY_MODE:
            return NAS_MSG_TYPE_SECURITY_MODE_COMPLETE;
        case NAS_SEQ_STATE_REGISTERED:
            return NAS_MSG_TYPE_PDU_SESSION_ESTABLISHMENT_REQUEST;
        case NAS_SEQ_STATE_PDU_SESSION_PENDING:
            return NAS_MSG_TYPE_PDU_SESSION_ESTABLISHMENT_ACCEPT;
        default:
            return 0;
    }
}

bool nas_is_valid_state_transition(nas_seq_state_t from, 
                                    nas_seq_state_t to,
                                    uint8_t msg_type) {
    /* Define valid transitions */
    switch (from) {
        case NAS_SEQ_STATE_IDLE:
            return to == NAS_SEQ_STATE_REGISTERING && 
                   msg_type == NAS_MSG_TYPE_REGISTRATION_REQUEST;
            
        case NAS_SEQ_STATE_REGISTERING:
            return (to == NAS_SEQ_STATE_AUTHENTICATING && 
                    msg_type == NAS_MSG_TYPE_AUTHENTICATION_REQUEST) ||
                   (to == NAS_SEQ_STATE_REGISTERED &&
                    msg_type == NAS_MSG_TYPE_REGISTRATION_ACCEPT);
            
        case NAS_SEQ_STATE_AUTHENTICATING:
            return to == NAS_SEQ_STATE_SECURITY_MODE &&
                   msg_type == NAS_MSG_TYPE_AUTHENTICATION_RESPONSE;
            
        case NAS_SEQ_STATE_SECURITY_MODE:
            return to == NAS_SEQ_STATE_REGISTERED &&
                   msg_type == NAS_MSG_TYPE_SECURITY_MODE_COMPLETE;
            
        case NAS_SEQ_STATE_REGISTERED:
            return (to == NAS_SEQ_STATE_PDU_SESSION_PENDING &&
                    msg_type == NAS_MSG_TYPE_PDU_SESSION_ESTABLISHMENT_REQUEST) ||
                   (to == NAS_SEQ_STATE_DEREGISTERING &&
                    msg_type == NAS_MSG_TYPE_DEREGISTRATION_REQUEST);
            
        case NAS_SEQ_STATE_PDU_SESSION_PENDING:
            return to == NAS_SEQ_STATE_CONNECTED &&
                   msg_type == NAS_MSG_TYPE_PDU_SESSION_ESTABLISHMENT_ACCEPT;
            
        case NAS_SEQ_STATE_CONNECTED:
            return to == NAS_SEQ_STATE_DEREGISTERING &&
                   msg_type == NAS_MSG_TYPE_DEREGISTRATION_REQUEST;
            
        case NAS_SEQ_STATE_DEREGISTERING:
            return to == NAS_SEQ_STATE_IDLE;
            
        default:
            return false;
    }
}

const char* nas_seq_state_to_string(nas_seq_state_t state) {
    switch (state) {
        case NAS_SEQ_STATE_IDLE: return "IDLE";
        case NAS_SEQ_STATE_REGISTERING: return "REGISTERING";
        case NAS_SEQ_STATE_AUTHENTICATING: return "AUTHENTICATING";
        case NAS_SEQ_STATE_SECURITY_MODE: return "SECURITY_MODE";
        case NAS_SEQ_STATE_REGISTERED: return "REGISTERED";
        case NAS_SEQ_STATE_PDU_SESSION_PENDING: return "PDU_SESSION_PENDING";
        case NAS_SEQ_STATE_CONNECTED: return "CONNECTED";
        case NAS_SEQ_STATE_DEREGISTERING: return "DEREGISTERING";
        default: return "UNKNOWN";
    }
}
