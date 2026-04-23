/*
 * 5G UE Simulation Application
 * NAS (Non-Access Stratum) Layer Implementation
 */

#include "nas.h"
#include "../core/memory.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

// Global NAS context
static atomic_uint g_nas_ue_counter = 0;

uesim_error_t nas_init(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("NAS initialized for UE %u\n", ue_ctx->ue_id);
    return UESIM_SUCCESS;
}

void nas_cleanup(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return;
    }
    
    printf("NAS cleanup completed for UE %u\n", ue_ctx->ue_id);
}

uesim_error_t nas_create_ue_context(ue_context_t* ue_ctx, nas_ue_context_t** nas_ctx) {
    if (ue_ctx == NULL || nas_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Allocate NAS UE context
    nas_ue_context_t* nas_ue_ctx = (nas_ue_context_t*)uesim_calloc(1, sizeof(nas_ue_context_t));
    if (nas_ue_ctx == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    // Initialize context
    nas_ue_ctx->ue_id = atomic_fetch_add(&g_nas_ue_counter, 1);
    nas_ue_ctx->mm_state = NAS_5GMM_NULL;
    nas_ue_ctx->active = false;
    
    // Initialize atomic counters
    atomic_init(&nas_ue_ctx->message_counter, 0);
    
    // Initialize mutex and condition variable
    if (pthread_mutex_init(&nas_ue_ctx->nas_mutex, NULL) != 0) {
        uesim_free(nas_ue_ctx);
        return UESIM_ERROR_THREAD;
    }
    
    if (pthread_cond_init(&nas_ue_ctx->nas_cond, NULL) != 0) {
        pthread_mutex_destroy(&nas_ue_ctx->nas_mutex);
        uesim_free(nas_ue_ctx);
        return UESIM_ERROR_THREAD;
    }
    
    // Initialize security context mutex
    if (pthread_mutex_init(&nas_ue_ctx->security_context.security_mutex, NULL) != 0) {
        pthread_cond_destroy(&nas_ue_ctx->nas_cond);
        pthread_mutex_destroy(&nas_ue_ctx->nas_mutex);
        uesim_free(nas_ue_ctx);
        return UESIM_ERROR_THREAD;
    }
    
    // Initialize PDU session mutexes
    for (int i = 0; i < NAS_MAX_PDU_SESSIONS; i++) {
        if (pthread_mutex_init(&nas_ue_ctx->pdu_sessions[i].session_mutex, NULL) != 0) {
            // Cleanup already initialized session mutexes
            for (int j = 0; j < i; j++) {
                pthread_mutex_destroy(&nas_ue_ctx->pdu_sessions[j].session_mutex);
            }
            pthread_mutex_destroy(&nas_ue_ctx->security_context.security_mutex);
            pthread_cond_destroy(&nas_ue_ctx->nas_cond);
            pthread_mutex_destroy(&nas_ue_ctx->nas_mutex);
            uesim_free(nas_ue_ctx);
            return UESIM_ERROR_THREAD;
        }
    }
    
    // Set default configuration
    nas_set_default_config(nas_ue_ctx);
    
    nas_ue_ctx->mm_state = NAS_5GMM_DEREGISTERED;
    *nas_ctx = nas_ue_ctx;
    
    printf("NAS UE context created: ID=%u\n", nas_ue_ctx->ue_id);
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_destroy_ue_context(ue_context_t* ue_ctx, nas_ue_context_t* nas_ctx) {
    if (ue_ctx == NULL || nas_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Destroy PDU session mutexes
    for (int i = 0; i < NAS_MAX_PDU_SESSIONS; i++) {
        pthread_mutex_destroy(&nas_ctx->pdu_sessions[i].session_mutex);
    }
    
    // Destroy security context mutex
    pthread_mutex_destroy(&nas_ctx->security_context.security_mutex);
    
    // Destroy synchronization primitives
    pthread_cond_destroy(&nas_ctx->nas_cond);
    pthread_mutex_destroy(&nas_ctx->nas_mutex);
    
    // Free context
    uesim_free(nas_ctx);
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_activate_ue_context(nas_ue_context_t* nas_ctx) {
    if (nas_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&nas_ctx->nas_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    nas_ctx->active = true;
    
    pthread_mutex_unlock(&nas_ctx->nas_mutex);
    
    printf("NAS UE context %u activated\n", nas_ctx->ue_id);
    return UESIM_SUCCESS;
}

uesim_error_t nas_deactivate_ue_context(nas_ue_context_t* nas_ctx) {
    if (nas_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&nas_ctx->nas_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    nas_ctx->active = false;
    
    pthread_mutex_unlock(&nas_ctx->nas_mutex);
    
    printf("NAS UE context %u deactivated\n", nas_ctx->ue_id);
    return UESIM_SUCCESS;
}

uesim_error_t nas_update_5gmm_state(nas_ue_context_t* nas_ctx, nas_5gmm_state_t new_state) {
    if (nas_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&nas_ctx->nas_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    nas_5gmm_state_t old_state = nas_ctx->mm_state;
    nas_ctx->mm_state = new_state;
    
    pthread_mutex_unlock(&nas_ctx->nas_mutex);
    
    printf("NAS UE context %u: 5GMM state changed from %d to %d\n", 
           nas_ctx->ue_id, old_state, new_state);
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_update_5gsm_state(nas_ue_context_t* nas_ctx, uint8_t pdu_session_id, 
                                   nas_5gsm_state_t new_state) {
    if (nas_ctx == NULL || pdu_session_id >= NAS_MAX_PDU_SESSIONS) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    nas_5gsm_state_t old_state = nas_ctx->pdu_sessions[pdu_session_id].state;
    nas_ctx->pdu_sessions[pdu_session_id].state = new_state;
    
    pthread_mutex_unlock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex);
    
    printf("NAS UE context %u: PDU session %u state changed from %d to %d\n", 
           nas_ctx->ue_id, pdu_session_id, old_state, new_state);
    
    return UESIM_SUCCESS;
}

// NAS Message Processing
uesim_error_t nas_process_message(nas_ue_context_t* nas_ctx, const nas_message_t* message) {
    if (nas_ctx == NULL || message == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if context is active
    if (!nas_ctx->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Verify integrity protection if present
    if (message->integrity_protected) {
        bool valid = false;
        uesim_error_t result = nas_verify_integrity_protection(nas_ctx, message, &valid);
        if (result != UESIM_SUCCESS) {
            return result;
        }
        
        if (!valid) {
            printf("NAS: Integrity verification failed for message type 0x%02x\n", 
                   message->header.message_type);
            return UESIM_ERROR_PROTOCOL;
        }
    }
    
    // Decipher message if ciphered
    if (message->ciphered) {
        nas_message_t temp_message = *message;
        uesim_error_t result = nas_decipher_message(nas_ctx, &temp_message);
        if (result != UESIM_SUCCESS) {
            return result;
        }
        // Process deciphered message (in real implementation)
    }
    
    // Process message based on type
    switch (message->header.message_type) {
        case NAS_MSG_TYPE_REGISTRATION_ACCEPT:
            printf("NAS: Processing Registration Accept message\n");
            break;
        case NAS_MSG_TYPE_AUTHENTICATION_REQUEST:
            printf("NAS: Processing Authentication Request message\n");
            break;
        case NAS_MSG_TYPE_SECURITY_MODE_COMMAND:
            printf("NAS: Processing Security Mode Command message\n");
            break;
        case NAS_MSG_TYPE_PDU_SESSION_ESTABLISHMENT_ACCEPT:
            printf("NAS: Processing PDU Session Establishment Accept message\n");
            break;
        default:
            printf("NAS: Processing unknown message type 0x%02x\n", 
                   message->header.message_type);
            break;
    }
    
    // Update statistics
    if (pthread_mutex_lock(&nas_ctx->nas_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    nas_ctx->stats.messages_received++;
    
    pthread_mutex_unlock(&nas_ctx->nas_mutex);
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_send_message(nas_ue_context_t* nas_ctx, nas_message_t* message) {
    if (nas_ctx == NULL || message == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if context is active
    if (!nas_ctx->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Apply security if security context is valid
    if (nas_ctx->security_context.security_context_valid) {
        // Apply integrity protection
        uesim_error_t result = nas_integrity_protect_message(nas_ctx, message);
        if (result != UESIM_SUCCESS) {
            return result;
        }
        
        // Apply ciphering if required
        if (nas_ctx->security_context.ciphering_alg != NAS_CIPHERING_ALG_NEA0) {
            result = nas_cipher_message(nas_ctx, message);
            if (result != UESIM_SUCCESS) {
                return result;
            }
        }
    }
    
    // Send message (in real implementation, this would go to lower layers)
    printf("NAS: Sending message type 0x%02x, length=%zu\n", 
           message->header.message_type, message->message_length);
    
    // Update statistics
    if (pthread_mutex_lock(&nas_ctx->nas_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    nas_ctx->stats.messages_sent++;
    
    pthread_mutex_unlock(&nas_ctx->nas_mutex);
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_encode_message(const nas_message_t* message, uint8_t** encoded_data, 
                                size_t* encoded_length) {
    if (message == NULL || encoded_data == NULL || encoded_length == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Calculate encoded length
    *encoded_length = sizeof(nas_message_header_t) + message->message_length;
    
    // Allocate encoded data buffer
    *encoded_data = (uint8_t*)uesim_malloc(*encoded_length);
    if (*encoded_data == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    // Encode header
    uint8_t* ptr = *encoded_data;
    *ptr++ = message->header.extended_protocol_discriminator;
    *ptr++ = (message->header.security_header_type << 4) | message->header.message_type;
    
    // Encode security header if present
    if (message->header.security_header_type != NAS_SECURITY_HEADER_PLAIN) {
        memcpy(ptr, &message->header.message_authentication_code, 4);
        ptr += 4;
        *ptr++ = message->header.sequence_number;
    }
    
    // Encode message data
    if (message->message_length > 0) {
        memcpy(ptr, message->message_data, message->message_length);
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_decode_message(const uint8_t* encoded_data, size_t encoded_length, 
                                nas_message_t* message) {
    if (encoded_data == NULL || message == NULL || encoded_length == 0) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Decode header
    const uint8_t* ptr = encoded_data;
    message->header.extended_protocol_discriminator = *ptr++;
    
    uint8_t type_byte = *ptr++;
    message->header.security_header_type = (type_byte >> 4) & 0x0F;
    message->header.message_type = type_byte & 0x0F;
    
    // Decode security header if present
    if (message->header.security_header_type != NAS_SECURITY_HEADER_PLAIN) {
        if (encoded_length < sizeof(nas_message_header_t)) {
            return UESIM_ERROR_INVALID_PARAM;
        }
        memcpy(&message->header.message_authentication_code, ptr, 4);
        ptr += 4;
        message->header.sequence_number = *ptr++;
    }
    
    // Calculate message data length
    size_t header_length = ptr - encoded_data;
    message->message_length = encoded_length - header_length;
    
    // Allocate message data buffer
    if (message->message_length > 0) {
        message->message_data = (uint8_t*)uesim_malloc(message->message_length);
        if (message->message_data == NULL) {
            return UESIM_ERROR_MEMORY;
        }
        memcpy(message->message_data, ptr, message->message_length);
    }
    
    message->arrival_time = (uint32_t)time(NULL);
    message->integrity_protected = false;
    message->ciphered = false;
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_create_message(nas_message_type_t msg_type, const void* msg_data, 
                                size_t msg_length, nas_message_t** message) {
    if (message == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Allocate message
    nas_message_t* new_message = (nas_message_t*)uesim_calloc(1, sizeof(nas_message_t));
    if (new_message == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    // Initialize header
    new_message->header.extended_protocol_discriminator = 0x7E; // 5GMM
    new_message->header.security_header_type = NAS_SECURITY_HEADER_PLAIN;
    new_message->header.message_type = msg_type;
    new_message->header.message_authentication_code = 0;
    new_message->header.sequence_number = 0;
    
    // Allocate message data if provided
    if (msg_data != NULL && msg_length > 0) {
        new_message->message_data = (uint8_t*)uesim_malloc(msg_length);
        if (new_message->message_data == NULL) {
            uesim_free(new_message);
            return UESIM_ERROR_MEMORY;
        }
        memcpy(new_message->message_data, msg_data, msg_length);
        new_message->message_length = msg_length;
    }
    
    new_message->arrival_time = (uint32_t)time(NULL);
    new_message->integrity_protected = false;
    new_message->ciphered = false;
    
    *message = new_message;
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_destroy_message(nas_message_t* message) {
    if (message == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (message->message_data != NULL) {
        uesim_free(message->message_data);
    }
    
    uesim_free(message);
    
    return UESIM_SUCCESS;
}

// NAS Registration Procedures
uesim_error_t nas_initiate_registration(nas_ue_context_t* nas_ctx, 
                                       nas_registration_type_t reg_type) {
    if (nas_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if context is active
    if (!nas_ctx->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Create registration request
    nas_registration_request_t reg_request = {0};
    reg_request.registration_type = reg_type;
    reg_request.ngksi_tsc = false;
    reg_request.ngksi_value = 0x07; // No key available
    reg_request.identity_type = NAS_IDENTITY_TYPE_SUCI;
    strcpy(reg_request.mobile_identity, nas_ctx->identity.suci);
    
    // Create NAS message
    nas_message_t* message = NULL;
    uesim_error_t result = nas_create_message(NAS_MSG_TYPE_REGISTRATION_REQUEST, 
                                             &reg_request, sizeof(reg_request), &message);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    // Send message
    result = nas_send_message(nas_ctx, message);
    if (result != UESIM_SUCCESS) {
        nas_destroy_message(message);
        return result;
    }
    
    // Update state
    nas_update_5gmm_state(nas_ctx, NAS_5GMM_REGISTERED_INITIATED);
    
    // Update statistics
    if (pthread_mutex_lock(&nas_ctx->nas_mutex) != 0) {
        nas_destroy_message(message);
        return UESIM_ERROR_THREAD;
    }
    
    nas_ctx->stats.registration_requests++;
    
    pthread_mutex_unlock(&nas_ctx->nas_mutex);
    
    nas_destroy_message(message);
    
    printf("NAS: Initiated registration procedure, type=%d\n", reg_type);
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_handle_registration_request(nas_ue_context_t* nas_ctx, 
                                             const nas_registration_request_t* request) {
    if (nas_ctx == NULL || request == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if context is active
    if (!nas_ctx->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Process registration request
    printf("NAS: Handling registration request, type=%d, identity=%s\n", 
           request->registration_type, request->mobile_identity);
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_handle_registration_accept(nas_ue_context_t* nas_ctx, 
                                            const nas_registration_accept_t* accept) {
    if (nas_ctx == NULL || accept == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if context is active
    if (!nas_ctx->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Process registration accept
    printf("NAS: Handling registration accept, GUTI=%s\n", accept->guti);
    
    // Update UE identity with assigned GUTI
    nas_update_ue_identity(nas_ctx, &nas_ctx->identity);
    
    // Update state
    nas_update_5gmm_state(nas_ctx, NAS_5GMM_REGISTERED);
    
    // Update statistics
    if (pthread_mutex_lock(&nas_ctx->nas_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    nas_ctx->stats.registration_accepts++;
    
    pthread_mutex_unlock(&nas_ctx->nas_mutex);
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_send_registration_complete(nas_ue_context_t* nas_ctx) {
    if (nas_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if context is active
    if (!nas_ctx->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Create registration complete message (empty payload)
    nas_message_t* message = NULL;
    uesim_error_t result = nas_create_message(NAS_MSG_TYPE_REGISTRATION_COMPLETE, 
                                             NULL, 0, &message);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    // Send message
    result = nas_send_message(nas_ctx, message);
    if (result != UESIM_SUCCESS) {
        nas_destroy_message(message);
        return result;
    }
    
    nas_destroy_message(message);
    
    printf("NAS: Sent registration complete\n");
    
    return UESIM_SUCCESS;
}

// NAS Authentication Procedures
uesim_error_t nas_initiate_authentication(nas_ue_context_t* nas_ctx) {
    if (nas_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if context is active
    if (!nas_ctx->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Authentication is typically initiated by the network
    // This function would be called when receiving an authentication request
    printf("NAS: Authentication procedure initiated\n");
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_handle_authentication_request(nas_ue_context_t* nas_ctx, 
                                               const uint8_t* auth_data, size_t data_length) {
    if (nas_ctx == NULL || auth_data == NULL || data_length == 0) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if context is active
    if (!nas_ctx->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Process authentication request
    printf("NAS: Handling authentication request, data length=%zu\n", data_length);
    
    // Update statistics
    if (pthread_mutex_lock(&nas_ctx->nas_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    nas_ctx->stats.authentication_requests++;
    
    pthread_mutex_unlock(&nas_ctx->nas_mutex);
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_send_authentication_response(nas_ue_context_t* nas_ctx, 
                                              const uint8_t* response_data, size_t data_length) {
    if (nas_ctx == NULL || response_data == NULL || data_length == 0) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if context is active
    if (!nas_ctx->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Create authentication response message
    nas_message_t* message = NULL;
    uesim_error_t result = nas_create_message(NAS_MSG_TYPE_AUTHENTICATION_RESPONSE, 
                                             response_data, data_length, &message);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    // Send message
    result = nas_send_message(nas_ctx, message);
    if (result != UESIM_SUCCESS) {
        nas_destroy_message(message);
        return result;
    }
    
    // Update statistics
    if (pthread_mutex_lock(&nas_ctx->nas_mutex) != 0) {
        nas_destroy_message(message);
        return UESIM_ERROR_THREAD;
    }
    
    nas_ctx->stats.authentication_responses++;
    
    pthread_mutex_unlock(&nas_ctx->nas_mutex);
    
    nas_destroy_message(message);
    
    printf("NAS: Sent authentication response\n");
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_handle_authentication_result(nas_ue_context_t* nas_ctx, 
                                              const uint8_t* result_data, size_t data_length) {
    if (nas_ctx == NULL || result_data == NULL || data_length == 0) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if context is active
    if (!nas_ctx->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Process authentication result
    printf("NAS: Handling authentication result, data length=%zu\n", data_length);
    
    // Update authentication status
    if (pthread_mutex_lock(&nas_ctx->nas_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    nas_ctx->auth_context.authenticated = true;
    nas_ctx->stats.authentication_success++;
    
    pthread_mutex_unlock(&nas_ctx->nas_mutex);
    
    return UESIM_SUCCESS;
}

// NAS Security Mode Control
uesim_error_t nas_initiate_security_mode_control(nas_ue_context_t* nas_ctx) {
    if (nas_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if context is active
    if (!nas_ctx->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Security mode control is typically initiated by the network
    // This function would be called when receiving a security mode command
    printf("NAS: Security mode control procedure initiated\n");
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_handle_security_mode_command(nas_ue_context_t* nas_ctx, 
                                              const nas_security_mode_command_t* command) {
    if (nas_ctx == NULL || command == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if context is active
    if (!nas_ctx->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Process security mode command
    printf("NAS: Handling security mode command, ciphering alg=%d, integrity alg=%d\n", 
           command->selected_nas_security_algorithms, command->selected_nas_integrity_algorithm);
    
    // Update security context
    if (pthread_mutex_lock(&nas_ctx->security_context.security_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    nas_ctx->security_context.ciphering_alg = command->selected_nas_security_algorithms;
    nas_ctx->security_context.integrity_alg = command->selected_nas_integrity_algorithm;
    nas_ctx->security_context.security_context_valid = true;
    
    pthread_mutex_unlock(&nas_ctx->security_context.security_mutex);
    
    // Update statistics
    if (pthread_mutex_lock(&nas_ctx->nas_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    nas_ctx->stats.security_mode_commands++;
    
    pthread_mutex_unlock(&nas_ctx->nas_mutex);
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_send_security_mode_complete(nas_ue_context_t* nas_ctx) {
    if (nas_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if context is active
    if (!nas_ctx->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Create security mode complete message (empty payload)
    nas_message_t* message = NULL;
    uesim_error_t result = nas_create_message(NAS_MSG_TYPE_SECURITY_MODE_COMPLETE, 
                                             NULL, 0, &message);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    // Send message
    result = nas_send_message(nas_ctx, message);
    if (result != UESIM_SUCCESS) {
        nas_destroy_message(message);
        return result;
    }
    
    // Update statistics
    if (pthread_mutex_lock(&nas_ctx->nas_mutex) != 0) {
        nas_destroy_message(message);
        return UESIM_ERROR_THREAD;
    }
    
    nas_ctx->stats.security_mode_completes++;
    
    pthread_mutex_unlock(&nas_ctx->nas_mutex);
    
    nas_destroy_message(message);
    
    printf("NAS: Sent security mode complete\n");
    
    return UESIM_SUCCESS;
}

// NAS PDU Session Management
uesim_error_t nas_initiate_pdu_session_establishment(nas_ue_context_t* nas_ctx, 
                                                    uint8_t pdu_session_id,
                                                    nas_pdu_session_type_t session_type) {
    if (nas_ctx == NULL || pdu_session_id >= NAS_MAX_PDU_SESSIONS) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if context is active
    if (!nas_ctx->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate PDU session ID range (1-15 for 5G)
    if (pdu_session_id < 1 || pdu_session_id > 15) {
        printf("NAS: Invalid PDU session ID %u, must be 1-15\n", pdu_session_id);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if session already exists and is active
    if (pthread_mutex_lock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    if (nas_ctx->pdu_sessions[pdu_session_id].active) {
        pthread_mutex_unlock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex);
        printf("NAS: PDU session %u already active\n", pdu_session_id);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_unlock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex);
    
    // Create PDU session establishment request
    nas_pdu_session_establishment_request_t session_request = {0};
    session_request.pdu_session_id = pdu_session_id;
    session_request.pdu_session_type = session_type;
    session_request.ssc_mode = NAS_SSC_MODE_1;
    session_request.ptis = 0x01; // Procedure Transaction ID
    
    // Create NAS message
    nas_message_t* message = NULL;
    uesim_error_t result = nas_create_message(NAS_MSG_TYPE_PDU_SESSION_ESTABLISHMENT_REQUEST, 
                                             &session_request, sizeof(session_request), &message);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    // Send message
    result = nas_send_message(nas_ctx, message);
    if (result != UESIM_SUCCESS) {
        nas_destroy_message(message);
        return result;
    }
    
    // Update PDU session state
    nas_update_5gsm_state(nas_ctx, pdu_session_id, NAS_5GSM_PDU_SESSION_ACTIVE_PENDING);
    
    // Update statistics
    if (pthread_mutex_lock(&nas_ctx->nas_mutex) != 0) {
        nas_destroy_message(message);
        return UESIM_ERROR_THREAD;
    }
    
    nas_ctx->stats.pdu_session_est_requests++;
    
    pthread_mutex_unlock(&nas_ctx->nas_mutex);
    
    nas_destroy_message(message);
    
    printf("NAS: Initiated PDU session establishment, session ID=%u, type=%d\n", 
           pdu_session_id, session_type);
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_handle_pdu_session_establishment_request(nas_ue_context_t* nas_ctx, 
                                                          const nas_pdu_session_establishment_request_t* request) {
    if (nas_ctx == NULL || request == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if context is active
    if (!nas_ctx->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate PDU session ID
    if (request->pdu_session_id >= NAS_MAX_PDU_SESSIONS) {
        printf("NAS: Invalid PDU session ID %u\n", request->pdu_session_id);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Process PDU session establishment request
    printf("NAS: Handling PDU session establishment request, session ID=%u, type=%d\n", 
           request->pdu_session_id, request->pdu_session_type);
    
    // Update PDU session configuration
    if (pthread_mutex_lock(&nas_ctx->pdu_sessions[request->pdu_session_id].session_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    nas_ctx->pdu_sessions[request->pdu_session_id].pdu_session_id = request->pdu_session_id;
    nas_ctx->pdu_sessions[request->pdu_session_id].session_type = request->pdu_session_type;
    nas_ctx->pdu_sessions[request->pdu_session_id].ssc_mode = request->ssc_mode;
    nas_ctx->pdu_sessions[request->pdu_session_id].ptis = request->ptis;
    
    pthread_mutex_unlock(&nas_ctx->pdu_sessions[request->pdu_session_id].session_mutex);
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_send_pdu_session_establishment_accept(nas_ue_context_t* nas_ctx, 
                                                       uint8_t pdu_session_id) {
    if (nas_ctx == NULL || pdu_session_id >= NAS_MAX_PDU_SESSIONS) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if context is active
    if (!nas_ctx->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate PDU session ID
    if (pdu_session_id < 1 || pdu_session_id > 15) {
        printf("NAS: Invalid PDU session ID %u for establishment accept\n", pdu_session_id);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Create PDU session establishment accept with complete session information
    uint8_t accept_data[128] = {0};
    size_t offset = 0;
    
    // Add session ID
    accept_data[offset++] = pdu_session_id;
    
    // Add session type
    accept_data[offset++] = nas_ctx->pdu_sessions[pdu_session_id].session_type;
    
    // Add PDU address (simplified)
    uint32_t pdu_address = 0xC0A80101; // 192.168.1.1
    memcpy(&accept_data[offset], &pdu_address, 4);
    offset += 4;
    
    // Add QoS rules (simplified)
    accept_data[offset++] = 0x01; // Default QFI
    accept_data[offset++] = 0x08; // QoS rule length
    accept_data[offset++] = 0x01; // QFI
    accept_data[offset++] = 0x01; // ARP
    accept_data[offset++] = 0x05; // QCI (5 - Video)
    accept_data[offset++] = 0x00; // GBR UL
    accept_data[offset++] = 0x64; // GBR UL (100 kbps)
    accept_data[offset++] = 0x00; // GBR DL
    accept_data[offset++] = 0xC8; // GBR DL (200 kbps)
    
    // Create NAS message
    nas_message_t* message = NULL;
    uesim_error_t result = nas_create_message(NAS_MSG_TYPE_PDU_SESSION_ESTABLISHMENT_ACCEPT, 
                                             accept_data, offset, &message);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    // Send message
    result = nas_send_message(nas_ctx, message);
    if (result != UESIM_SUCCESS) {
        nas_destroy_message(message);
        return result;
    }
    
    // Update PDU session state and configuration
    if (pthread_mutex_lock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex) != 0) {
        nas_destroy_message(message);
        return UESIM_ERROR_THREAD;
    }
    
    // Activate session
    nas_ctx->pdu_sessions[pdu_session_id].active = true;
    nas_ctx->pdu_sessions[pdu_session_id].pdu_address = pdu_address;
    nas_ctx->pdu_sessions[pdu_session_id].default_qfi = 1;
    nas_ctx->pdu_sessions[pdu_session_id].session_ambr_ul = 1000; // 1000 kbps
    nas_ctx->pdu_sessions[pdu_session_id].session_ambr_dl = 2000; // 2000 kbps
    nas_ctx->pdu_sessions[pdu_session_id].ul_teid = 0x12345678;
    nas_ctx->pdu_sessions[pdu_session_id].dl_teid = 0x87654321;
    nas_ctx->pdu_sessions[pdu_session_id].upf_ip = 0xC0A80102; // 192.168.1.2
    
    // Add default QoS flow
    nas_ctx->pdu_sessions[pdu_session_id].qos_flows[0].qfi = 1;
    nas_ctx->pdu_sessions[pdu_session_id].qos_flows[0].arp = 1;
    nas_ctx->pdu_sessions[pdu_session_id].qos_flows[0].qci = 5;
    nas_ctx->pdu_sessions[pdu_session_id].qos_flows[0].gbr_ul = 100;
    nas_ctx->pdu_sessions[pdu_session_id].qos_flows[0].gbr_dl = 200;
    nas_ctx->pdu_sessions[pdu_session_id].qos_flows[0].mbr_ul = 1000;
    nas_ctx->pdu_sessions[pdu_session_id].qos_flows[0].mbr_dl = 2000;
    nas_ctx->pdu_sessions[pdu_session_id].qos_flows[0].active = true;
    nas_ctx->pdu_sessions[pdu_session_id].num_qos_flows = 1;
    
    pthread_mutex_unlock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex);
    
    // Update PDU session state
    nas_update_5gsm_state(nas_ctx, pdu_session_id, NAS_5GSM_PDU_SESSION_ACTIVE);
    
    // Update statistics
    if (pthread_mutex_lock(&nas_ctx->nas_mutex) != 0) {
        nas_destroy_message(message);
        return UESIM_ERROR_THREAD;
    }
    
    nas_ctx->stats.pdu_session_est_accepts++;
    nas_ctx->num_active_sessions++;
    
    pthread_mutex_unlock(&nas_ctx->nas_mutex);
    
    nas_destroy_message(message);
    
    printf("NAS: Sent PDU session establishment accept, session ID=%u, IP=%u.%u.%u.%u\n", 
           pdu_session_id,
           (pdu_address >> 24) & 0xFF,
           (pdu_address >> 16) & 0xFF,
           (pdu_address >> 8) & 0xFF,
           pdu_address & 0xFF);
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_handle_pdu_session_establishment_reject(nas_ue_context_t* nas_ctx, 
                                                         uint8_t pdu_session_id, uint8_t cause) {
    if (nas_ctx == NULL || pdu_session_id >= NAS_MAX_PDU_SESSIONS) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if context is active
    if (!nas_ctx->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Process PDU session establishment reject
    printf("NAS: Handling PDU session establishment reject, session ID=%u, cause=%u\n", 
           pdu_session_id, cause);
    
    // Update PDU session state
    nas_update_5gsm_state(nas_ctx, pdu_session_id, NAS_5GSM_PDU_SESSION_INACTIVE);
    
    return UESIM_SUCCESS;
}

// Enhanced PDU Session Management Functions
uesim_error_t nas_initiate_pdu_session_modification(nas_ue_context_t* nas_ctx, 
                                                   uint8_t pdu_session_id) {
    if (nas_ctx == NULL || pdu_session_id >= NAS_MAX_PDU_SESSIONS) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if context is active
    if (!nas_ctx->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if session exists and is active
    if (pthread_mutex_lock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    if (!nas_ctx->pdu_sessions[pdu_session_id].active) {
        pthread_mutex_unlock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex);
        printf("NAS: PDU session %u not active for modification\n", pdu_session_id);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_unlock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex);
    
    // Create PDU session modification request
    uint8_t mod_request_data[64] = {0};
    mod_request_data[0] = pdu_session_id;
    mod_request_data[1] = 0x01; // Modification type
    
    // Create NAS message
    nas_message_t* message = NULL;
    uesim_error_t result = nas_create_message(NAS_MSG_TYPE_PDU_SESSION_MODIFICATION_REQUEST, 
                                             mod_request_data, 2, &message);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    // Send message
    result = nas_send_message(nas_ctx, message);
    if (result != UESIM_SUCCESS) {
        nas_destroy_message(message);
        return result;
    }
    
    // Update PDU session state
    nas_update_5gsm_state(nas_ctx, pdu_session_id, NAS_5GSM_PDU_SESSION_MODIFICATION_PENDING);
    
    nas_destroy_message(message);
    
    printf("NAS: Initiated PDU session modification, session ID=%u\n", pdu_session_id);
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_initiate_pdu_session_release(nas_ue_context_t* nas_ctx, 
                                              uint8_t pdu_session_id) {
    if (nas_ctx == NULL || pdu_session_id >= NAS_MAX_PDU_SESSIONS) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if context is active
    if (!nas_ctx->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if session exists and is active
    if (pthread_mutex_lock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    if (!nas_ctx->pdu_sessions[pdu_session_id].active) {
        pthread_mutex_unlock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex);
        printf("NAS: PDU session %u not active for release\n", pdu_session_id);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_unlock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex);
    
    // Create PDU session release request
    uint8_t release_request_data[32] = {0};
    release_request_data[0] = pdu_session_id;
    release_request_data[1] = 0x00; // Cause
    
    // Create NAS message
    nas_message_t* message = NULL;
    uesim_error_t result = nas_create_message(NAS_MSG_TYPE_PDU_SESSION_RELEASE_REQUEST, 
                                             release_request_data, 2, &message);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    // Send message
    result = nas_send_message(nas_ctx, message);
    if (result != UESIM_SUCCESS) {
        nas_destroy_message(message);
        return result;
    }
    
    // Update PDU session state
    nas_update_5gsm_state(nas_ctx, pdu_session_id, NAS_5GSM_PDU_SESSION_RELEASED_PENDING);
    
    nas_destroy_message(message);
    
    printf("NAS: Initiated PDU session release, session ID=%u\n", pdu_session_id);
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_handle_pdu_session_modification_command(nas_ue_context_t* nas_ctx, 
                                                         uint8_t pdu_session_id,
                                                         const uint8_t* command_data, 
                                                         size_t data_length) {
    if (nas_ctx == NULL || pdu_session_id >= NAS_MAX_PDU_SESSIONS || 
        command_data == NULL || data_length == 0) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if context is active
    if (!nas_ctx->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Process PDU session modification command
    printf("NAS: Handling PDU session modification command, session ID=%u\n", pdu_session_id);
    
    // Update PDU session configuration based on command
    if (pthread_mutex_lock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    // Parse modification command and update session parameters
    if (data_length >= 4) {
        // Update AMBR values
        memcpy(&nas_ctx->pdu_sessions[pdu_session_id].session_ambr_ul, &command_data[0], 2);
        memcpy(&nas_ctx->pdu_sessions[pdu_session_id].session_ambr_dl, &command_data[2], 2);
    }
    
    pthread_mutex_unlock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex);
    
    // Send modification complete
    uint8_t complete_data[16] = {0};
    complete_data[0] = pdu_session_id;
    
    nas_message_t* message = NULL;
    uesim_error_t result = nas_create_message(NAS_MSG_TYPE_PDU_SESSION_MODIFICATION_COMPLETE, 
                                             complete_data, 1, &message);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    result = nas_send_message(nas_ctx, message);
    if (result != UESIM_SUCCESS) {
        nas_destroy_message(message);
        return result;
    }
    
    // Update PDU session state back to active
    nas_update_5gsm_state(nas_ctx, pdu_session_id, NAS_5GSM_PDU_SESSION_ACTIVE);
    
    nas_destroy_message(message);
    
    printf("NAS: Completed PDU session modification, session ID=%u\n", pdu_session_id);
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_handle_pdu_session_release_command(nas_ue_context_t* nas_ctx, 
                                                    uint8_t pdu_session_id,
                                                    const uint8_t* command_data, 
                                                    size_t data_length) {
    if (nas_ctx == NULL || pdu_session_id >= NAS_MAX_PDU_SESSIONS) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if context is active
    if (!nas_ctx->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Process PDU session release command
    printf("NAS: Handling PDU session release command, session ID=%u\n", pdu_session_id);
    
    // Deactivate PDU session
    if (pthread_mutex_lock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    nas_ctx->pdu_sessions[pdu_session_id].active = false;
    nas_ctx->pdu_sessions[pdu_session_id].state = NAS_5GSM_PDU_SESSION_INACTIVE;
    
    pthread_mutex_unlock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex);
    
    // Update global session count
    if (pthread_mutex_lock(&nas_ctx->nas_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    if (nas_ctx->num_active_sessions > 0) {
        nas_ctx->num_active_sessions--;
    }
    
    pthread_mutex_unlock(&nas_ctx->nas_mutex);
    
    // Send release complete
    uint8_t complete_data[16] = {0};
    complete_data[0] = pdu_session_id;
    
    nas_message_t* message = NULL;
    uesim_error_t result = nas_create_message(NAS_MSG_TYPE_PDU_SESSION_RELEASE_COMPLETE, 
                                             complete_data, 1, &message);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    result = nas_send_message(nas_ctx, message);
    if (result != UESIM_SUCCESS) {
        nas_destroy_message(message);
        return result;
    }
    
    nas_destroy_message(message);
    
    printf("NAS: Completed PDU session release, session ID=%u\n", pdu_session_id);
    
    return UESIM_SUCCESS;
}

// QoS Flow Management Functions
uesim_error_t nas_add_qos_flow(nas_ue_context_t* nas_ctx, uint8_t pdu_session_id, 
                              const nas_qos_flow_t* qos_flow) {
    if (nas_ctx == NULL || pdu_session_id >= NAS_MAX_PDU_SESSIONS || qos_flow == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if context is active
    if (!nas_ctx->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Add QoS flow to PDU session
    if (pthread_mutex_lock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    // Check if session is active
    if (!nas_ctx->pdu_sessions[pdu_session_id].active) {
        pthread_mutex_unlock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex);
        printf("NAS: Cannot add QoS flow to inactive PDU session %u\n", pdu_session_id);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if we have space for more QoS flows
    if (nas_ctx->pdu_sessions[pdu_session_id].num_qos_flows >= 8) {
        pthread_mutex_unlock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex);
        printf("NAS: Maximum QoS flows reached for PDU session %u\n", pdu_session_id);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Add QoS flow
    int flow_index = nas_ctx->pdu_sessions[pdu_session_id].num_qos_flows;
    nas_ctx->pdu_sessions[pdu_session_id].qos_flows[flow_index] = *qos_flow;
    nas_ctx->pdu_sessions[pdu_session_id].qos_flows[flow_index].active = true;
    nas_ctx->pdu_sessions[pdu_session_id].num_qos_flows++;
    
    pthread_mutex_unlock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex);
    
    printf("NAS: Added QoS flow QFI=%u to PDU session %u\n", 
           qos_flow->qfi, pdu_session_id);
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_remove_qos_flow(nas_ue_context_t* nas_ctx, uint8_t pdu_session_id, 
                                 uint8_t qfi) {
    if (nas_ctx == NULL || pdu_session_id >= NAS_MAX_PDU_SESSIONS) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if context is active
    if (!nas_ctx->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Remove QoS flow from PDU session
    if (pthread_mutex_lock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    // Find and remove QoS flow
    for (int i = 0; i < nas_ctx->pdu_sessions[pdu_session_id].num_qos_flows; i++) {
        if (nas_ctx->pdu_sessions[pdu_session_id].qos_flows[i].qfi == qfi) {
            // Shift remaining flows
            for (int j = i; j < nas_ctx->pdu_sessions[pdu_session_id].num_qos_flows - 1; j++) {
                nas_ctx->pdu_sessions[pdu_session_id].qos_flows[j] = 
                    nas_ctx->pdu_sessions[pdu_session_id].qos_flows[j + 1];
            }
            nas_ctx->pdu_sessions[pdu_session_id].num_qos_flows--;
            break;
        }
    }
    
    pthread_mutex_unlock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex);
    
    printf("NAS: Removed QoS flow QFI=%u from PDU session %u\n", qfi, pdu_session_id);
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_modify_qos_flow(nas_ue_context_t* nas_ctx, uint8_t pdu_session_id, 
                                 const nas_qos_flow_t* qos_flow) {
    if (nas_ctx == NULL || pdu_session_id >= NAS_MAX_PDU_SESSIONS || qos_flow == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if context is active
    if (!nas_ctx->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Modify QoS flow in PDU session
    if (pthread_mutex_lock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    // Find and modify QoS flow
    for (int i = 0; i < nas_ctx->pdu_sessions[pdu_session_id].num_qos_flows; i++) {
        if (nas_ctx->pdu_sessions[pdu_session_id].qos_flows[i].qfi == qos_flow->qfi) {
            nas_ctx->pdu_sessions[pdu_session_id].qos_flows[i] = *qos_flow;
            nas_ctx->pdu_sessions[pdu_session_id].qos_flows[i].active = true;
            break;
        }
    }
    
    pthread_mutex_unlock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex);
    
    printf("NAS: Modified QoS flow QFI=%u in PDU session %u\n", qos_flow->qfi, pdu_session_id);
    
    return UESIM_SUCCESS;
}

// PDU Session Utility Functions
bool nas_is_pdu_session_active(nas_ue_context_t* nas_ctx, uint8_t pdu_session_id) {
    if (nas_ctx == NULL || pdu_session_id >= NAS_MAX_PDU_SESSIONS) {
        return false;
    }
    
    if (pthread_mutex_lock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex) != 0) {
        return false;
    }
    
    bool active = nas_ctx->pdu_sessions[pdu_session_id].active;
    
    pthread_mutex_unlock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex);
    
    return active;
}

uesim_error_t nas_get_pdu_session_info(nas_ue_context_t* nas_ctx, uint8_t pdu_session_id,
                                      nas_pdu_session_t* session_info) {
    if (nas_ctx == NULL || pdu_session_id >= NAS_MAX_PDU_SESSIONS || session_info == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    *session_info = nas_ctx->pdu_sessions[pdu_session_id];
    
    pthread_mutex_unlock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex);
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_get_all_active_pdu_sessions(nas_ue_context_t* nas_ctx,
                                             uint8_t* session_ids,
                                             uint8_t* num_sessions) {
    if (nas_ctx == NULL || session_ids == NULL || num_sessions == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    *num_sessions = 0;
    
    for (int i = 0; i < NAS_MAX_PDU_SESSIONS; i++) {
        if (nas_is_pdu_session_active(nas_ctx, i)) {
            session_ids[*num_sessions] = i;
            (*num_sessions)++;
            if (*num_sessions >= NAS_MAX_PDU_SESSIONS) {
                break;
            }
        }
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_update_pdu_session_ambr(nas_ue_context_t* nas_ctx, uint8_t pdu_session_id,
                                         uint16_t ul_ambr, uint16_t dl_ambr) {
    if (nas_ctx == NULL || pdu_session_id >= NAS_MAX_PDU_SESSIONS) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    nas_ctx->pdu_sessions[pdu_session_id].session_ambr_ul = ul_ambr;
    nas_ctx->pdu_sessions[pdu_session_id].session_ambr_dl = dl_ambr;
    
    pthread_mutex_unlock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex);
    
    printf("NAS: Updated PDU session %u AMBR: UL=%u kbps, DL=%u kbps\n", 
           pdu_session_id, ul_ambr, dl_ambr);
    
    return UESIM_SUCCESS;
}

// NAS Security Functions
uesim_error_t nas_create_security_context(nas_ue_context_t* nas_ctx, 
                                         nas_ciphering_algorithm_t cipher_alg,
                                         nas_integrity_algorithm_t integrity_alg) {
    if (nas_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&nas_ctx->security_context.security_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    nas_ctx->security_context.ciphering_alg = cipher_alg;
    nas_ctx->security_context.integrity_alg = integrity_alg;
    nas_ctx->security_context.ksi = 0;
    nas_ctx->security_context.downlink_count = 0;
    nas_ctx->security_context.uplink_count = 0;
    nas_ctx->security_context.security_context_valid = true;
    
    pthread_mutex_unlock(&nas_ctx->security_context.security_mutex);
    
    printf("NAS: Created security context, ciphering=%d, integrity=%d\n", 
           cipher_alg, integrity_alg);
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_destroy_security_context(nas_ue_context_t* nas_ctx) {
    if (nas_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&nas_ctx->security_context.security_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    nas_ctx->security_context.security_context_valid = false;
    memset(nas_ctx->security_context.knas_enc, 0, 16);
    memset(nas_ctx->security_context.knas_int, 0, 16);
    nas_ctx->security_context.downlink_count = 0;
    nas_ctx->security_context.uplink_count = 0;
    
    pthread_mutex_unlock(&nas_ctx->security_context.security_mutex);
    
    printf("NAS: Destroyed security context\n");
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_update_security_context(nas_ue_context_t* nas_ctx, 
                                         nas_ciphering_algorithm_t new_cipher_alg,
                                         nas_integrity_algorithm_t new_integrity_alg) {
    if (nas_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&nas_ctx->security_context.security_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    nas_ctx->security_context.ciphering_alg = new_cipher_alg;
    nas_ctx->security_context.integrity_alg = new_integrity_alg;
    
    pthread_mutex_unlock(&nas_ctx->security_context.security_mutex);
    
    printf("NAS: Updated security context, ciphering=%d, integrity=%d\n", 
           new_cipher_alg, new_integrity_alg);
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_integrity_protect_message(nas_ue_context_t* nas_ctx, 
                                           nas_message_t* message) {
    if (nas_ctx == NULL || message == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if security context is valid
    if (!nas_ctx->security_context.security_context_valid) {
        return UESIM_SUCCESS; // No security context, nothing to do
    }
    
    // Apply integrity protection based on selected algorithm
    if (nas_ctx->security_context.integrity_alg == NAS_INTEGRITY_ALG_NIA0) {
        message->integrity_protected = false;
        return UESIM_SUCCESS; // NULL algorithm
    }
    
    // In a real implementation, this would compute the MAC using the selected algorithm
    // For now, we'll just set the flag
    message->integrity_protected = true;
    message->header.message_authentication_code = 0x12345678; // Dummy MAC
    
    printf("NAS: Applied integrity protection to message type 0x%02x\n", 
           message->header.message_type);
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_verify_integrity_protection(nas_ue_context_t* nas_ctx, 
                                             const nas_message_t* message, bool* valid) {
    if (nas_ctx == NULL || message == NULL || valid == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if security context is valid
    if (!nas_ctx->security_context.security_context_valid) {
        *valid = true; // No security context, accept as valid
        return UESIM_SUCCESS;
    }
    
    // Check if message has integrity protection
    if (!message->integrity_protected) {
        *valid = true; // No integrity protection, accept as valid
        return UESIM_SUCCESS;
    }
    
    // Verify integrity protection based on selected algorithm
    if (nas_ctx->security_context.integrity_alg == NAS_INTEGRITY_ALG_NIA0) {
        *valid = true; // NULL algorithm, accept as valid
        return UESIM_SUCCESS;
    }
    
    // In a real implementation, this would verify the MAC using the selected algorithm
    // For now, we'll just accept the dummy MAC
    *valid = (message->header.message_authentication_code == 0x12345678);
    
    printf("NAS: Verified integrity protection for message type 0x%02x, valid=%s\n", 
           message->header.message_type, *valid ? "true" : "false");
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_cipher_message(nas_ue_context_t* nas_ctx, nas_message_t* message) {
    if (nas_ctx == NULL || message == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if security context is valid
    if (!nas_ctx->security_context.security_context_valid) {
        return UESIM_SUCCESS; // No security context, nothing to do
    }
    
    // Apply ciphering based on selected algorithm
    if (nas_ctx->security_context.ciphering_alg == NAS_CIPHERING_ALG_NEA0) {
        message->ciphered = false;
        return UESIM_SUCCESS; // NULL algorithm
    }
    
    // In a real implementation, this would apply ciphering using the selected algorithm
    // For now, we'll just set the flag
    message->ciphered = true;
    
    printf("NAS: Applied ciphering to message type 0x%02x\n", message->header.message_type);
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_decipher_message(nas_ue_context_t* nas_ctx, nas_message_t* message) {
    if (nas_ctx == NULL || message == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if security context is valid
    if (!nas_ctx->security_context.security_context_valid) {
        return UESIM_SUCCESS; // No security context, nothing to do
    }
    
    // Apply deciphering based on selected algorithm
    if (nas_ctx->security_context.ciphering_alg == NAS_CIPHERING_ALG_NEA0) {
        message->ciphered = false;
        return UESIM_SUCCESS; // NULL algorithm
    }
    
    // In a real implementation, this would apply deciphering using the selected algorithm
    // For now, we'll just clear the flag
    message->ciphered = false;
    
    printf("NAS: Applied deciphering to message type 0x%02x\n", message->header.message_type);
    
    return UESIM_SUCCESS;
}

// NAS Authentication Functions
uesim_error_t nas_generate_authentication_vector(nas_ue_context_t* nas_ctx, 
                                                nas_auth_vector_t* vector) {
    if (nas_ctx == NULL || vector == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Generate dummy authentication vector (in real implementation, this would use AUC)
    for (int i = 0; i < 16; i++) {
        vector->rand[i] = (uint8_t)(rand() & 0xFF);
        vector->xres[i] = (uint8_t)(rand() & 0xFF);
        vector->autn[i] = (uint8_t)(rand() & 0xFF);
    }
    
    for (int i = 0; i < 32; i++) {
        vector->kasme[i] = (uint8_t)(rand() & 0xFF);
    }
    
    vector->used = false;
    
    printf("NAS: Generated authentication vector\n");
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_validate_authentication_response(nas_ue_context_t* nas_ctx, 
                                                  const uint8_t* response, size_t response_length,
                                                  bool* valid) {
    if (nas_ctx == NULL || response == NULL || response_length == 0 || valid == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // In a real implementation, this would validate the response against the expected XRES
    // For now, we'll just accept any response
    *valid = true;
    
    printf("NAS: Validated authentication response, valid=%s\n", *valid ? "true" : "false");
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_derive_nas_keys(nas_ue_context_t* nas_ctx, const uint8_t* kasme) {
    if (nas_ctx == NULL || kasme == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // In a real implementation, this would derive NAS keys from KASME
    // For now, we'll just copy dummy keys
    if (pthread_mutex_lock(&nas_ctx->security_context.security_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    for (int i = 0; i < 16; i++) {
        nas_ctx->security_context.knas_enc[i] = (uint8_t)(rand() & 0xFF);
        nas_ctx->security_context.knas_int[i] = (uint8_t)(rand() & 0xFF);
    }
    
    pthread_mutex_unlock(&nas_ctx->security_context.security_mutex);
    
    printf("NAS: Derived NAS keys\n");
    
    return UESIM_SUCCESS;
}

// NAS Timer Functions
uesim_error_t nas_start_timer(nas_ue_context_t* nas_ctx, uint16_t timer_id, uint32_t timeout_ms) {
    if (nas_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Start timer based on timer ID
    switch (timer_id) {
        case 3412: // T3412 - Periodic registration update timer
            if (pthread_mutex_lock(&nas_ctx->nas_mutex) != 0) {
                return UESIM_ERROR_THREAD;
            }
            nas_ctx->t3412_timer = timeout_ms / 1000; // Convert to seconds
            nas_ctx->t3412_running = true;
            pthread_mutex_unlock(&nas_ctx->nas_mutex);
            printf("NAS: Started T3412 timer, timeout=%u seconds\n", timeout_ms / 1000);
            break;
            
        case 3422: // T3422 - Registration accept timer
            if (pthread_mutex_lock(&nas_ctx->nas_mutex) != 0) {
                return UESIM_ERROR_THREAD;
            }
            nas_ctx->t3422_timer = timeout_ms / 1000; // Convert to seconds
            nas_ctx->t3422_running = true;
            pthread_mutex_unlock(&nas_ctx->nas_mutex);
            printf("NAS: Started T3422 timer, timeout=%u seconds\n", timeout_ms / 1000);
            break;
            
        case 3450: // T3450 - Registration complete timer
            if (pthread_mutex_lock(&nas_ctx->nas_mutex) != 0) {
                return UESIM_ERROR_THREAD;
            }
            nas_ctx->t3450_timer = timeout_ms / 1000; // Convert to seconds
            nas_ctx->t3450_running = true;
            pthread_mutex_unlock(&nas_ctx->nas_mutex);
            printf("NAS: Started T3450 timer, timeout=%u seconds\n", timeout_ms / 1000);
            break;
            
        default:
            printf("NAS: Unknown timer ID %u\n", timer_id);
            return UESIM_ERROR_INVALID_PARAM;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_stop_timer(nas_ue_context_t* nas_ctx, uint16_t timer_id) {
    if (nas_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Stop timer based on timer ID
    switch (timer_id) {
        case 3412: // T3412 - Periodic registration update timer
            if (pthread_mutex_lock(&nas_ctx->nas_mutex) != 0) {
                return UESIM_ERROR_THREAD;
            }
            nas_ctx->t3412_running = false;
            pthread_mutex_unlock(&nas_ctx->nas_mutex);
            printf("NAS: Stopped T3412 timer\n");
            break;
            
        case 3422: // T3422 - Registration accept timer
            if (pthread_mutex_lock(&nas_ctx->nas_mutex) != 0) {
                return UESIM_ERROR_THREAD;
            }
            nas_ctx->t3422_running = false;
            pthread_mutex_unlock(&nas_ctx->nas_mutex);
            printf("NAS: Stopped T3422 timer\n");
            break;
            
        case 3450: // T3450 - Registration complete timer
            if (pthread_mutex_lock(&nas_ctx->nas_mutex) != 0) {
                return UESIM_ERROR_THREAD;
            }
            nas_ctx->t3450_running = false;
            pthread_mutex_unlock(&nas_ctx->nas_mutex);
            printf("NAS: Stopped T3450 timer\n");
            break;
            
        default:
            printf("NAS: Unknown timer ID %u\n", timer_id);
            return UESIM_ERROR_INVALID_PARAM;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_handle_timer_expiry(nas_ue_context_t* nas_ctx, uint16_t timer_id) {
    if (nas_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Handle timer expiry based on timer ID
    switch (timer_id) {
        case 3412: // T3412 - Periodic registration update timer
            printf("NAS: T3412 timer expired, initiating periodic registration\n");
            // In a real implementation, this would trigger periodic registration
            break;
            
        case 3422: // T3422 - Registration accept timer
            printf("NAS: T3422 timer expired, registration accept timeout\n");
            // In a real implementation, this would handle registration accept timeout
            break;
            
        case 3450: // T3450 - Registration complete timer
            printf("NAS: T3450 timer expired, registration complete timeout\n");
            // In a real implementation, this would handle registration complete timeout
            break;
            
        default:
            printf("NAS: Unknown timer ID %u expired\n", timer_id);
            return UESIM_ERROR_INVALID_PARAM;
    }
    
    return UESIM_SUCCESS;
}

// NAS Utility Functions
uesim_error_t nas_update_ue_identity(nas_ue_context_t* nas_ctx, const nas_ue_identity_t* identity) {
    if (nas_ctx == NULL || identity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&nas_ctx->nas_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    nas_ctx->identity = *identity;
    
    pthread_mutex_unlock(&nas_ctx->nas_mutex);
    
    printf("NAS: Updated UE identity\n");
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_get_ue_identity(nas_ue_context_t* nas_ctx, nas_ue_identity_t* identity) {
    if (nas_ctx == NULL || identity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&nas_ctx->nas_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    *identity = nas_ctx->identity;
    
    pthread_mutex_unlock(&nas_ctx->nas_mutex);
    
    return UESIM_SUCCESS;
}

bool nas_is_ue_registered(nas_ue_context_t* nas_ctx) {
    if (nas_ctx == NULL) {
        return false;
    }
    
    return (nas_ctx->mm_state == NAS_5GMM_REGISTERED);
}

uesim_error_t nas_get_statistics(nas_ue_context_t* nas_ctx, nas_stats_t* stats) {
    if (nas_ctx == NULL || stats == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&nas_ctx->nas_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    *stats = nas_ctx->stats;
    
    pthread_mutex_unlock(&nas_ctx->nas_mutex);
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_update_statistics(nas_ue_context_t* nas_ctx) {
    if (nas_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Update NAS statistics
    if (pthread_mutex_lock(&nas_ctx->nas_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    // In a real implementation, this would update various statistics
    // For now, we'll just log that statistics were updated
    printf("NAS: Updated statistics for UE context %u\n", nas_ctx->ue_id);
    
    pthread_mutex_unlock(&nas_ctx->nas_mutex);
    
    return UESIM_SUCCESS;
}

// NAS Configuration Functions
uesim_error_t nas_set_default_config(nas_ue_context_t* nas_ctx) {
    if (nas_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Set default values
    nas_ctx->t3412_timer = NAS_DEFAULT_T3412;
    nas_ctx->t3422_timer = NAS_DEFAULT_T3422;
    nas_ctx->t3450_timer = NAS_DEFAULT_T3450;
    nas_ctx->t3412_running = false;
    nas_ctx->t3422_running = false;
    nas_ctx->t3450_running = false;
    nas_ctx->num_active_sessions = 0;
    
    // Set default security configuration
    nas_ctx->security_context.ciphering_alg = NAS_CIPHERING_ALG_NEA0;
    nas_ctx->security_context.integrity_alg = NAS_INTEGRITY_ALG_NIA0;
    nas_ctx->security_context.security_context_valid = false;
    nas_ctx->security_context.ksi = 0x07; // No key available
    
    // Set default identity
    strcpy(nas_ctx->identity.suci, "suci-0-0-0-0-0");
    strcpy(nas_ctx->identity.guti, "");
    strcpy(nas_ctx->identity.imsi, "000000000000000");
    strcpy(nas_ctx->identity.imei, "000000000000000");
    strcpy(nas_ctx->identity.msisdn, "000000000000000");
    
    // Initialize PDU sessions
    for (int i = 0; i < NAS_MAX_PDU_SESSIONS; i++) {
        nas_ctx->pdu_sessions[i].pdu_session_id = i;
        nas_ctx->pdu_sessions[i].session_type = NAS_PDU_SESSION_TYPE_IPV4;
        nas_ctx->pdu_sessions[i].ssc_mode = NAS_SSC_MODE_1;
        nas_ctx->pdu_sessions[i].state = NAS_5GSM_PDU_SESSION_INACTIVE;
        nas_ctx->pdu_sessions[i].active = false;
    }
    
    // Initialize statistics
    memset(&nas_ctx->stats, 0, sizeof(nas_stats_t));
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_set_registration_config(nas_ue_context_t* nas_ctx, 
                                         nas_registration_type_t reg_type) {
    if (nas_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Configuration would be used in registration procedures
    printf("NAS: Set registration configuration, type=%d\n", reg_type);
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_set_security_config(nas_ue_context_t* nas_ctx, 
                                     nas_ciphering_algorithm_t cipher_alg,
                                     nas_integrity_algorithm_t integrity_alg) {
    if (nas_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&nas_ctx->security_context.security_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    nas_ctx->security_context.ciphering_alg = cipher_alg;
    nas_ctx->security_context.integrity_alg = integrity_alg;
    
    pthread_mutex_unlock(&nas_ctx->security_context.security_mutex);
    
    printf("NAS: Set security configuration, ciphering=%d, integrity=%d\n", 
           cipher_alg, integrity_alg);
    
    return UESIM_SUCCESS;
}

uesim_error_t nas_set_pdu_session_config(nas_ue_context_t* nas_ctx, uint8_t pdu_session_id,
                                        nas_pdu_session_type_t session_type,
                                        nas_ssc_mode_t ssc_mode) {
    if (nas_ctx == NULL || pdu_session_id >= NAS_MAX_PDU_SESSIONS) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    nas_ctx->pdu_sessions[pdu_session_id].session_type = session_type;
    nas_ctx->pdu_sessions[pdu_session_id].ssc_mode = ssc_mode;
    
    pthread_mutex_unlock(&nas_ctx->pdu_sessions[pdu_session_id].session_mutex);
    
    printf("NAS: Set PDU session configuration, session ID=%u, type=%d, SSC mode=%d\n", 
           pdu_session_id, session_type, ssc_mode);
    
    return UESIM_SUCCESS;
}
