/*
 * 5G UE Simulation Application
 * PDCP (Packet Data Convergence Protocol) Layer Implementation
 */

#include "pdcp.h"
#include "snow3g.h"
#include "aes.h"
#include "zuc.h"
#include "../core/memory.h"
#include <string.h>
#include <stdlib.h>

// Global PDCP context
static atomic_uint g_pdcp_entity_counter = 0;

// Algorithm operations table
static const pdcp_crypto_ops_t g_crypto_ops[PDCP_CIPHERING_ALG_MAX] = {
    [PDCP_CIPHERING_ALG_NEA0] = {NULL, NULL, NULL},  // NULL algorithm
    [PDCP_CIPHERING_ALG_NEA1] = {snow3g_encrypt, snow3g_decrypt, snow3g_compute_mac},
    [PDCP_CIPHERING_ALG_NEA2] = {aes_encrypt, aes_decrypt, aes_compute_mac},
    [PDCP_CIPHERING_ALG_NEA3] = {zuc_encrypt, zuc_decrypt, zuc_compute_mac}
};

uesim_error_t pdcp_init(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Create PDCP entities for SRB1 and SRB2 */
    pdcp_entity_t* srb1_entity = NULL;
    uesim_error_t result = pdcp_create_entity(ue_ctx, PDCP_BEARER_SRB1, 
                                              PDCP_DIRECTION_BIDIRECTIONAL, &srb1_entity);
    if (result != UESIM_SUCCESS) {
        printf("PDCP: Failed to create SRB1 entity for UE %u, error=%d\n", ue_ctx->ue_id, result);
        return result;
    }
    
    /* Store SRB1 PDCP entity */
    result = ue_set_pdcp_entity(ue_ctx, PDCP_BEARER_SRB1, srb1_entity);
    if (result != UESIM_SUCCESS) {
        pdcp_destroy_entity(ue_ctx, srb1_entity);
        printf("PDCP: Failed to store SRB1 entity for UE %u, error=%d\n", ue_ctx->ue_id, result);
        return result;
    }
    
    /* Activate SRB1 entity */
    pdcp_activate_entity(srb1_entity);
    
    /* Create SRB2 entity */
    pdcp_entity_t* srb2_entity = NULL;
    result = pdcp_create_entity(ue_ctx, PDCP_BEARER_SRB2, 
                               PDCP_DIRECTION_BIDIRECTIONAL, &srb2_entity);
    if (result != UESIM_SUCCESS) {
        printf("PDCP: Failed to create SRB2 entity for UE %u, error=%d\n", ue_ctx->ue_id, result);
        /* Continue without SRB2 - SRB1 is sufficient */
    } else {
        result = ue_set_pdcp_entity(ue_ctx, PDCP_BEARER_SRB2, srb2_entity);
        if (result != UESIM_SUCCESS) {
            pdcp_destroy_entity(ue_ctx, srb2_entity);
        } else {
            pdcp_activate_entity(srb2_entity);
        }
    }
    
    printf("PDCP: Initialized for UE %u (SRB1 entity_id=%u)\n", ue_ctx->ue_id, srb1_entity->entity_id);
    return UESIM_SUCCESS;
}

void pdcp_cleanup(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return;
    }
    
    /* Cleanup all PDCP entities */
    for (int i = 0; i < UESIM_MAX_PDCP_ENTITIES; i++) {
        pdcp_entity_t* pdcp_entity = ue_get_pdcp_entity(ue_ctx, i);
        if (pdcp_entity != NULL) {
            uint32_t entity_id = pdcp_entity->entity_id;
            pdcp_destroy_entity(ue_ctx, pdcp_entity);
            ue_remove_pdcp_entity(ue_ctx, i);
            printf("PDCP: Destroyed entity %u for bearer %d\n", entity_id, i);
        }
    }
    
    printf("PDCP: Cleanup completed for UE %u\n", ue_ctx->ue_id);
}

uesim_error_t pdcp_create_entity(ue_context_t* ue_ctx, pdcp_bearer_t bearer,
                                pdcp_direction_t direction, pdcp_entity_t** entity) {
    if (ue_ctx == NULL || entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (bearer >= PDCP_BEARER_DRB_MAX) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Allocate PDCP entity
    pdcp_entity_t* pdcp_entity = (pdcp_entity_t*)uesim_calloc(1, sizeof(pdcp_entity_t));
    if (pdcp_entity == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    // Initialize entity
    pdcp_entity->entity_id = atomic_fetch_add(&g_pdcp_entity_counter, 1);
    pdcp_entity->bearer_type = bearer;
    pdcp_entity->direction = direction;
    pdcp_entity->active = false;
    
    // Set SN length based on bearer type
    if (bearer <= PDCP_BEARER_SRB3) {
        pdcp_entity->sn_length = 5;  // SRB: 5-bit SN
    } else {
        pdcp_entity->sn_length = 12; // DRB: 12-bit SN (can be 18-bit for enhanced)
    }
    
    // Initialize atomic counters
    atomic_init(&pdcp_entity->next_pdcp_sn, 0);
    atomic_init(&pdcp_entity->next_expected_sn, 0);
    
    // Initialize SN parameters (mask, max, HFN max)
    pdcp_init_sn_params(pdcp_entity);
    
    // Initialize mutex and condition variable
    if (pthread_mutex_init(&pdcp_entity->entity_mutex, NULL) != 0) {
        uesim_free(pdcp_entity);
        return UESIM_ERROR_THREAD;
    }
    
    if (pthread_cond_init(&pdcp_entity->entity_cond, NULL) != 0) {
        pthread_mutex_destroy(&pdcp_entity->entity_mutex);
        uesim_free(pdcp_entity);
        return UESIM_ERROR_THREAD;
    }
    
    *entity = pdcp_entity;
    printf("PDCP entity created: ID=%u, Bearer=%d, Direction=%d\n",
           pdcp_entity->entity_id, bearer, direction);
    
    return UESIM_SUCCESS;
}

uesim_error_t pdcp_destroy_entity(ue_context_t* ue_ctx, pdcp_entity_t* entity) {
    if (ue_ctx == NULL || entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Destroy security context if exists
    if (entity->security_ctx != NULL) {
        pdcp_destroy_security_context(entity->security_ctx);
    }
    
    // Destroy synchronization primitives
    pthread_cond_destroy(&entity->entity_cond);
    pthread_mutex_destroy(&entity->entity_mutex);
    
    // Free entity
    uesim_free(entity);
    
    return UESIM_SUCCESS;
}

uesim_error_t pdcp_activate_entity(pdcp_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&entity->entity_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    entity->active = true;
    pthread_mutex_unlock(&entity->entity_mutex);
    
    printf("PDCP entity %u activated\n", entity->entity_id);
    return UESIM_SUCCESS;
}

uesim_error_t pdcp_deactivate_entity(pdcp_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&entity->entity_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    entity->active = false;
    pthread_mutex_unlock(&entity->entity_mutex);
    
    printf("PDCP entity %u deactivated\n", entity->entity_id);
    return UESIM_SUCCESS;
}

uesim_error_t pdcp_process_tx_data(pdcp_entity_t* entity, const void* sdu_data,
                                  size_t sdu_length, pdcp_pdu_t** pdu) {
    if (entity == NULL || sdu_data == NULL || sdu_length == 0 || pdu == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if entity is active
    if (!entity->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Allocate PDCP PDU
    pdcp_pdu_t* pdcp_pdu = (pdcp_pdu_t*)uesim_calloc(1, sizeof(pdcp_pdu_t));
    if (pdcp_pdu == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    // Allocate data buffer
    pdcp_pdu->data = (uint8_t*)uesim_malloc(sdu_length);
    if (pdcp_pdu->data == NULL) {
        uesim_free(pdcp_pdu);
        return UESIM_ERROR_MEMORY;
    }
    
    // Copy SDU data
    memcpy(pdcp_pdu->data, sdu_data, sdu_length);
    pdcp_pdu->data_length = sdu_length;
    
    // Get current SN and check for wraparound
    uint32_t current_sn = atomic_fetch_add(&entity->next_pdcp_sn, 1);
    current_sn &= entity->sn_mask;  // Apply SN mask
    
    // Check for SN wraparound (SN about to wrap)
    if (current_sn == 0 && entity->last_tx_sn > 0) {
        // SN wrapped around - increment TX HFN
        entity->tx_hfn++;
        
        // Check for HFN overflow
        if (pdcp_check_hfn_overflow(entity)) {
            printf("PDCP: WARNING - TX HFN overflow detected, key refresh required!\n");
        }
        
        printf("PDCP: TX SN wraparound, new TX_HFN=%u\n", entity->tx_hfn);
    }
    
    entity->last_tx_sn = current_sn;
    pdcp_pdu->sn = (uint16_t)current_sn;
    
    // Apply integrity protection if security context exists
    if (entity->security_ctx != NULL) {
        uesim_error_t result = pdcp_compute_integrity_protect(entity, pdcp_pdu);
        if (result != UESIM_SUCCESS) {
            uesim_free(pdcp_pdu->data);
            uesim_free(pdcp_pdu);
            return result;
        }
    }
    
    // Apply ciphering if security context exists
    if (entity->security_ctx != NULL) {
        uesim_error_t result = pdcp_encrypt_pdu(entity, pdcp_pdu);
        if (result != UESIM_SUCCESS) {
            uesim_free(pdcp_pdu->data);
            uesim_free(pdcp_pdu);
            return result;
        }
    }
    
    *pdu = pdcp_pdu;
    return UESIM_SUCCESS;
}

uesim_error_t pdcp_process_rx_data(pdcp_entity_t* entity, const pdcp_pdu_t* pdu,
                                  void** sdu_data, size_t* sdu_length) {
    if (entity == NULL || pdu == NULL || sdu_data == NULL || sdu_length == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if entity is active
    if (!entity->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Verify integrity if integrity protection was applied
    if (pdu->integrity_protected && entity->security_ctx != NULL) {
        bool valid = false;
        uesim_error_t result = pdcp_verify_integrity_protect(entity, pdu, &valid);
        if (result != UESIM_SUCCESS) {
            return result;
        }
        
        if (!valid) {
            printf("PDCP integrity verification failed for entity %u\n", entity->entity_id);
            return UESIM_ERROR_PROTOCOL;
        }
    }
    
    // Decrypt if ciphering was applied
    if (pdu->ciphered && entity->security_ctx != NULL) {
        // Create a copy of PDU for decryption (since pdu is const)
        pdcp_pdu_t temp_pdu = *pdu;
        temp_pdu.data = (uint8_t*)uesim_malloc(pdu->data_length);
        if (temp_pdu.data == NULL) {
            return UESIM_ERROR_MEMORY;
        }
        memcpy(temp_pdu.data, pdu->data, pdu->data_length);
        
        uesim_error_t result = pdcp_decrypt_pdu(entity, &temp_pdu);
        if (result != UESIM_SUCCESS) {
            uesim_free(temp_pdu.data);
            return result;
        }
        
        // Return decrypted data
        *sdu_data = temp_pdu.data;
        *sdu_length = temp_pdu.data_length;
    } else {
        // Return original data (copy it since pdu is const)
        *sdu_data = uesim_malloc(pdu->data_length);
        if (*sdu_data == NULL) {
            return UESIM_ERROR_MEMORY;
        }
        memcpy(*sdu_data, pdu->data, pdu->data_length);
        *sdu_length = pdu->data_length;
    }
    
    // Check sequence number
    uint32_t expected_sn = atomic_load(&entity->next_expected_sn);
    if (pdu->sn != expected_sn) {
        printf("PDCP SN mismatch: expected=%u, received=%u\n", expected_sn, pdu->sn);
        // For now, we'll continue but in real implementation this might trigger retransmission
    }
    
    atomic_store(&entity->next_expected_sn, pdu->sn + 1);
    
    return UESIM_SUCCESS;
}

uesim_error_t pdcp_create_security_context(pdcp_ciphering_alg_t cipher_alg,
                                          pdcp_integrity_alg_t integrity_alg,
                                          const uint8_t* cipher_key,
                                          const uint8_t* integrity_key,
                                          pdcp_security_context_t** security_ctx) {
    if (security_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (cipher_alg >= PDCP_CIPHERING_ALG_MAX || integrity_alg >= PDCP_INTEGRITY_ALG_MAX) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Allocate security context
    pdcp_security_context_t* ctx = (pdcp_security_context_t*)uesim_calloc(1, sizeof(pdcp_security_context_t));
    if (ctx == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    // Initialize mutex
    if (pthread_mutex_init(&ctx->security_mutex, NULL) != 0) {
        uesim_free(ctx);
        return UESIM_ERROR_THREAD;
    }
    
    // Set algorithms
    ctx->ciphering_alg = cipher_alg;
    ctx->integrity_alg = integrity_alg;
    
    // Copy keys if provided
    if (cipher_key != NULL) {
        memcpy(ctx->ciphering_key, cipher_key, PDCP_MAX_CIPHER_KEY_LEN);
    }
    
    if (integrity_key != NULL) {
        memcpy(ctx->integrity_key, integrity_key, PDCP_MAX_INTEGRITY_KEY_LEN);
    }
    
    atomic_init(&ctx->key_refresh_count, 0);
    
    *security_ctx = ctx;
    return UESIM_SUCCESS;
}

uesim_error_t pdcp_destroy_security_context(pdcp_security_context_t* security_ctx) {
    if (security_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_destroy(&security_ctx->security_mutex);
    uesim_free(security_ctx);
    
    return UESIM_SUCCESS;
}

uesim_error_t pdcp_update_security_context(pdcp_security_context_t* security_ctx,
                                          pdcp_ciphering_alg_t new_cipher_alg,
                                          pdcp_integrity_alg_t new_integrity_alg) {
    if (security_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (new_cipher_alg >= PDCP_CIPHERING_ALG_MAX || new_integrity_alg >= PDCP_INTEGRITY_ALG_MAX) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&security_ctx->security_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    security_ctx->ciphering_alg = new_cipher_alg;
    security_ctx->integrity_alg = new_integrity_alg;
    atomic_fetch_add(&security_ctx->key_refresh_count, 1);
    
    pthread_mutex_unlock(&security_ctx->security_mutex);
    
    return UESIM_SUCCESS;
}

uesim_error_t pdcp_encrypt_pdu(pdcp_entity_t* entity, pdcp_pdu_t* pdu) {
    if (entity == NULL || pdu == NULL || pdu->data == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (entity->security_ctx == NULL) {
        return UESIM_SUCCESS; // No security context, nothing to do
    }
    
    pdcp_security_context_t* ctx = entity->security_ctx;
    pdcp_ciphering_alg_t alg = ctx->ciphering_alg;
    
    if (alg == PDCP_CIPHERING_ALG_NEA0) {
        pdu->ciphered = false;
        return UESIM_SUCCESS; // NULL algorithm
    }
    
    if (alg >= PDCP_CIPHERING_ALG_MAX || g_crypto_ops[alg].encrypt == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Prepare cipher parameters
    pdcp_cipher_params_t params = {0};
    params.count = pdcp_get_count(entity);
    params.bearer = entity->bearer_type;
    params.direction = entity->direction;
    
    // Create temporary buffer for encrypted data
    uint8_t* encrypted_data = (uint8_t*)uesim_malloc(pdu->data_length);
    if (encrypted_data == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    // Perform encryption
    uesim_error_t result = g_crypto_ops[alg].encrypt(ctx->ciphering_key, &params,
                                                    pdu->data, encrypted_data, pdu->data_length);
    if (result != UESIM_SUCCESS) {
        uesim_free(encrypted_data);
        return result;
    }
    
    // Replace original data with encrypted data
    uesim_free(pdu->data);
    pdu->data = encrypted_data;
    pdu->ciphered = true;
    
    return UESIM_SUCCESS;
}

uesim_error_t pdcp_decrypt_pdu(pdcp_entity_t* entity, pdcp_pdu_t* pdu) {
    if (entity == NULL || pdu == NULL || pdu->data == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (entity->security_ctx == NULL) {
        return UESIM_SUCCESS; // No security context, nothing to do
    }
    
    pdcp_security_context_t* ctx = entity->security_ctx;
    pdcp_ciphering_alg_t alg = ctx->ciphering_alg;
    
    if (alg == PDCP_CIPHERING_ALG_NEA0) {
        pdu->ciphered = false;
        return UESIM_SUCCESS; // NULL algorithm
    }
    
    if (alg >= PDCP_CIPHERING_ALG_MAX || g_crypto_ops[alg].decrypt == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Prepare cipher parameters
    pdcp_cipher_params_t params = {0};
    params.count = pdcp_get_count(entity);
    params.bearer = entity->bearer_type;
    params.direction = entity->direction;
    
    // Create temporary buffer for decrypted data
    uint8_t* decrypted_data = (uint8_t*)uesim_malloc(pdu->data_length);
    if (decrypted_data == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    // Perform decryption
    uesim_error_t result = g_crypto_ops[alg].decrypt(ctx->ciphering_key, &params,
                                                    pdu->data, decrypted_data, pdu->data_length);
    if (result != UESIM_SUCCESS) {
        uesim_free(decrypted_data);
        return result;
    }
    
    // Replace encrypted data with decrypted data
    uesim_free(pdu->data);
    pdu->data = decrypted_data;
    pdu->ciphered = false;
    
    return UESIM_SUCCESS;
}

uesim_error_t pdcp_compute_integrity_protect(pdcp_entity_t* entity, pdcp_pdu_t* pdu) {
    if (entity == NULL || pdu == NULL || pdu->data == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (entity->security_ctx == NULL) {
        return UESIM_SUCCESS; // No security context, nothing to do
    }
    
    pdcp_security_context_t* ctx = entity->security_ctx;
    pdcp_integrity_alg_t alg = ctx->integrity_alg;
    
    if (alg == PDCP_INTEGRITY_ALG_NIA0) {
        pdu->integrity_protected = false;
        return UESIM_SUCCESS; // NULL algorithm
    }
    
    if (alg >= PDCP_INTEGRITY_ALG_MAX || g_crypto_ops[alg].compute_mac == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Prepare cipher parameters
    pdcp_cipher_params_t params = {0};
    params.count = pdcp_get_count(entity);
    params.bearer = entity->bearer_type;
    params.direction = entity->direction;
    
    // Compute MAC-I
    uesim_error_t result = g_crypto_ops[alg].compute_mac(ctx->integrity_key, pdu->data,
                                                        pdu->data_length, &params, &pdu->mac_i);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    pdu->integrity_protected = true;
    return UESIM_SUCCESS;
}

uesim_error_t pdcp_verify_integrity_protect(pdcp_entity_t* entity, const pdcp_pdu_t* pdu,
                                           bool* valid) {
    if (entity == NULL || pdu == NULL || valid == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (entity->security_ctx == NULL) {
        *valid = true; // No security context, accept as valid
        return UESIM_SUCCESS;
    }
    
    pdcp_security_context_t* ctx = entity->security_ctx;
    pdcp_integrity_alg_t alg = ctx->integrity_alg;
    
    if (alg == PDCP_INTEGRITY_ALG_NIA0) {
        *valid = true; // NULL algorithm, accept as valid
        return UESIM_SUCCESS;
    }
    
    if (alg >= PDCP_INTEGRITY_ALG_MAX || g_crypto_ops[alg].compute_mac == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Prepare cipher parameters
    pdcp_cipher_params_t params = {0};
    params.count = pdcp_get_count(entity);
    params.bearer = entity->bearer_type;
    params.direction = entity->direction;
    
    // Compute expected MAC-I
    uint32_t expected_mac;
    uesim_error_t result = g_crypto_ops[alg].compute_mac(ctx->integrity_key, pdu->data,
                                                        pdu->data_length, &params, &expected_mac);
    if (result != UESIM_SUCCESS) {
        *valid = false;
        return result;
    }
    
    // Compare with received MAC-I
    *valid = (expected_mac == pdu->mac_i);
    return UESIM_SUCCESS;
}

// Algorithm implementations are in aes.c, snow3g.c, zuc.c

uesim_error_t pdcp_encode_header(const pdcp_pdu_t* pdu, uint8_t* header, size_t* header_len) {
    if (pdu == NULL || header == NULL || header_len == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Simple header encoding (SN only for now)
    if (pdu->sn <= 0x7F) {
        // 1-byte header for 7-bit SN
        header[0] = (uint8_t)(pdu->sn & 0x7F);
        *header_len = 1;
    } else if (pdu->sn <= 0xFFF) {
        // 2-byte header for 12-bit SN
        header[0] = 0x80 | ((pdu->sn >> 8) & 0x0F);
        header[1] = pdu->sn & 0xFF;
        *header_len = 2;
    } else {
        // 3-byte header for 18-bit SN
        header[0] = 0xF0 | ((pdu->sn >> 16) & 0x03);
        header[1] = (pdu->sn >> 8) & 0xFF;
        header[2] = pdu->sn & 0xFF;
        *header_len = 3;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t pdcp_decode_header(const uint8_t* header, size_t header_len, pdcp_pdu_t* pdu) {
    if (header == NULL || pdu == NULL || header_len == 0) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (header_len > 3) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Simple header decoding
    if (header_len == 1) {
        pdu->sn = header[0] & 0x7F;
    } else if (header_len == 2) {
        pdu->sn = ((header[0] & 0x0F) << 8) | header[1];
    } else {
        pdu->sn = ((header[0] & 0x03) << 16) | (header[1] << 8) | header[2];
    }
    
    return UESIM_SUCCESS;
}

uint32_t pdcp_get_count(pdcp_entity_t* entity) {
    if (entity == NULL) {
        return 0;
    }
    
    // Use HFN-aware COUNT calculation
    // COUNT = (PDCP SN + 2^SN_length × HFN) mod 2^32
    // Per 3GPP TS 38.323 Section 5.1.1
    return pdcp_get_tx_count(entity);
}

uesim_error_t pdcp_increment_count(pdcp_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    atomic_fetch_add(&entity->next_pdcp_sn, 1);
    return UESIM_SUCCESS;
}

// HFN (Hyper Frame Number) Management Functions

uesim_error_t pdcp_init_sn_params(pdcp_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Initialize HFN and SN tracking based on SN length
    entity->tx_hfn = 0;
    entity->rx_hfn = 0;
    entity->last_tx_sn = 0;
    entity->last_rx_sn = 0;
    
    // Set SN mask, max, and HFN max based on SN length per 3GPP TS 38.323
    switch (entity->sn_length) {
        case 5:  // SRB: 5-bit SN
            entity->sn_mask = 0x1F;           // 5 bits = 31 max
            entity->sn_max = 32;              // 2^5 = 32
            entity->hfn_max = 0x7FFFFFF;      // 27 bits for HFN
            break;
        case 12: // DRB: 12-bit SN (default)
            entity->sn_mask = 0xFFF;           // 12 bits = 4095 max
            entity->sn_max = 4096;            // 2^12 = 4096
            entity->hfn_max = 0xFFFFF;         // 20 bits for HFN
            break;
        case 18: // DRB: 18-bit SN (enhanced)
            entity->sn_mask = 0x3FFFF;        // 18 bits = 262143 max
            entity->sn_max = 262144;          // 2^18 = 262144
            entity->hfn_max = 0x3FFF;         // 14 bits for HFN
            break;
        default:
            // Default to 12-bit SN
            entity->sn_mask = 0xFFF;
            entity->sn_max = 4096;
            entity->hfn_max = 0xFFFFF;
            break;
    }
    
    printf("PDCP: Initialized SN params: SN_length=%d, SN_mask=0x%X, SN_max=%u, HFN_max=0x%X\n",
           entity->sn_length, entity->sn_mask, entity->sn_max, entity->hfn_max);
    
    return UESIM_SUCCESS;
}

uint32_t pdcp_get_tx_count(pdcp_entity_t* entity) {
    if (entity == NULL) {
        return 0;
    }
    
    // COUNT = (PDCP SN + 2^SN_length × HFN) mod 2^32
    // Per 3GPP TS 38.323 Section 5.1.1
    uint32_t sn = atomic_load(&entity->next_pdcp_sn) & entity->sn_mask;
    uint32_t count = sn + (entity->sn_max * entity->tx_hfn);
    
    return count;
}

uint32_t pdcp_get_rx_count(pdcp_entity_t* entity, uint32_t received_sn) {
    if (entity == NULL) {
        return 0;
    }
    
    // Apply SN mask to received SN
    received_sn &= entity->sn_mask;
    
    // Detect wraparound and adjust HFN accordingly
    // Per 3GPP TS 38.323 Section 5.1.2
    uint32_t count;
    
    if (entity->last_rx_sn == 0) {
        // First received packet
        count = received_sn + (entity->sn_max * entity->rx_hfn);
    } else if (pdcp_detect_sn_wraparound(entity->last_rx_sn, received_sn, entity->sn_max)) {
        // SN wraparound detected - increment HFN
        entity->rx_hfn++;
        
        // Check for HFN overflow
        if (pdcp_check_hfn_overflow(entity)) {
            printf("PDCP: WARNING - HFN overflow detected, key refresh required!\n");
        }
        
        count = received_sn + (entity->sn_max * entity->rx_hfn);
        printf("PDCP: RX SN wraparound detected, new HFN=%u, COUNT=0x%08X\n", 
               entity->rx_hfn, count);
    } else {
        // Normal case - use current HFN
        count = received_sn + (entity->sn_max * entity->rx_hfn);
    }
    
    // Update last received SN
    entity->last_rx_sn = received_sn;
    
    return count;
}

bool pdcp_detect_sn_wraparound(uint32_t last_sn, uint32_t new_sn, uint32_t sn_max) {
    // Detect wraparound using threshold method
    // Per 3GPP TS 38.323, wraparound is detected when:
    // - new SN is much smaller than last SN (wrapped around)
    // - The difference is greater than half the SN space
    
    uint32_t threshold = sn_max / 2;
    
    // Wraparound: last SN was high, new SN is low
    // Example: last=4090, new=5 -> wraparound occurred
    if (last_sn > threshold && new_sn < threshold) {
        // Check if this is a real wraparound or just out-of-order
        // If the difference is large (close to sn_max), it's a wraparound
        uint32_t diff = last_sn - new_sn;
        if (diff > threshold) {
            return true;
        }
    }
    
    return false;
}

bool pdcp_check_hfn_overflow(pdcp_entity_t* entity) {
    if (entity == NULL) {
        return false;
    }
    
    // Check if HFN has reached or exceeded maximum
    // Per 3GPP TS 38.323, when HFN reaches max, key refresh is required
    if (entity->tx_hfn >= entity->hfn_max || entity->rx_hfn >= entity->hfn_max) {
        return true;
    }
    
    return false;
}

// Key Refresh Functions (per 3GPP TS 38.323 Section 5.9)

/*
 * Trigger key refresh when HFN overflow is detected
 * 
 * Per 3GPP TS 38.323 Section 5.9:
 * When the HFN is about to overflow, the UE shall:
 * 1. Request key refresh from upper layers (RRC)
 * 2. RRC initiates Security Mode Command procedure
 * 3. New keys are derived and provided to PDCP
 * 4. HFN is reset to 0
 */
uesim_error_t pdcp_trigger_key_refresh(pdcp_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("PDCP: Key refresh triggered for entity %u (HFN overflow detected)\n", 
           entity->entity_id);
    
    /* 
     * In a real implementation, this would:
     * 1. Notify RRC layer to initiate Security Mode Command procedure
     * 2. RRC would request new keys from NAS
     * 3. NAS would derive new keys using KDF
     * 4. New keys would be provided via pdcp_perform_key_refresh()
     * 
     * For simulation, we mark the entity as needing key refresh
     */
    
    if (pthread_mutex_lock(&entity->entity_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    /* Mark entity as needing key refresh */
    /* In production, this would trigger an RRC procedure */
    
    printf("PDCP: Entity %u requires key refresh - TX_HFN=%u/%u, RX_HFN=%u/%u\n",
           entity->entity_id, entity->tx_hfn, entity->hfn_max,
           entity->rx_hfn, entity->hfn_max);
    
    pthread_mutex_unlock(&entity->entity_mutex);
    
    /* 
     * Return error to indicate key refresh is required
     * Upper layers should handle this by initiating Security Mode Command
     */
    return UESIM_ERROR_KEY_REFRESH_REQUIRED;
}

/*
 * Perform key refresh with new keys
 * 
 * This function is called when new keys are available from upper layers
 * It updates the security context and resets HFN
 */
uesim_error_t pdcp_perform_key_refresh(pdcp_entity_t* entity, 
                                       const uint8_t* new_cipher_key,
                                       const uint8_t* new_integrity_key) {
    if (entity == NULL || new_cipher_key == NULL || new_integrity_key == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (entity->security_ctx == NULL) {
        printf("PDCP: Cannot perform key refresh - no security context\n");
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("PDCP: Performing key refresh for entity %u\n", entity->entity_id);
    
    if (pthread_mutex_lock(&entity->entity_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    /* Lock security context */
    if (pthread_mutex_lock(&entity->security_ctx->security_mutex) != 0) {
        pthread_mutex_unlock(&entity->entity_mutex);
        return UESIM_ERROR_THREAD;
    }
    
    /* Store old keys for logging (clear after use) */
    uint8_t old_cipher_key[16];
    uint8_t old_integrity_key[16];
    memcpy(old_cipher_key, entity->security_ctx->ciphering_key, 16);
    memcpy(old_integrity_key, entity->security_ctx->integrity_key, 16);
    
    /* Update keys */
    memcpy(entity->security_ctx->ciphering_key, new_cipher_key, 16);
    memcpy(entity->security_ctx->integrity_key, new_integrity_key, 16);
    
    /* Increment key refresh counter */
    atomic_fetch_add(&entity->security_ctx->key_refresh_count, 1);
    
    /* Clear old key copies */
    memset(old_cipher_key, 0, 16);
    memset(old_integrity_key, 0, 16);
    
    pthread_mutex_unlock(&entity->security_ctx->security_mutex);
    
    /* Reset HFN and SN after key refresh */
    entity->tx_hfn = 0;
    entity->rx_hfn = 0;
    entity->last_tx_sn = 0;
    entity->last_rx_sn = 0;
    atomic_store(&entity->next_pdcp_sn, 0);
    atomic_store(&entity->next_expected_sn, 0);
    
    pthread_mutex_unlock(&entity->entity_mutex);
    
    printf("PDCP: Key refresh completed for entity %u\n", entity->entity_id);
    printf("     HFN reset to 0, SN reset to 0\n");
    printf("     Key refresh count: %u\n", 
           atomic_load(&entity->security_ctx->key_refresh_count));
    
    return UESIM_SUCCESS;
}

/*
 * Derive refreshed keys from KAMF using KDF
 * 
 * Per 3GPP TS 33.501 Annex A.6:
 * K_rrc_enc = KDF(KAMF, 0x69, 0x01, algorithm_id)
 * K_rrc_int = KDF(KAMF, 0x69, 0x02, algorithm_id)
 * K_up_enc  = KDF(KAMF, 0x69, 0x03, algorithm_id)
 * K_up_int  = KDF(KAMF, 0x69, 0x04, algorithm_id)
 */
uesim_error_t pdcp_derive_refreshed_keys(pdcp_entity_t* entity,
                                         const uint8_t* kamf,
                                         uint8_t* new_cipher_key,
                                         uint8_t* new_integrity_key) {
    if (entity == NULL || kamf == NULL || 
        new_cipher_key == NULL || new_integrity_key == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (entity->security_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /*
     * KDF per 3GPP TS 33.220 Annex B.2
     * S = FC || P0 || L0 || P1 || L1
     * 
     * For DRB keys:
     * FC = 0x69 (Access stratum key derivation)
     * P0 = Algorithm type distinguisher
     *      0x01 = RRC encryption
     *      0x02 = RRC integrity
     *      0x03 = User plane encryption
     *      0x04 = User plane integrity
     * P1 = Algorithm identity
     */
    
    uint8_t fc = 0x69;  /* Access stratum key derivation */
    
    /* Determine algorithm type distinguisher based on bearer type */
    uint8_t enc_type;
    uint8_t int_type;
    
    if (entity->bearer_type <= PDCP_BEARER_SRB3) {
        /* Signaling Radio Bearer - use RRC keys */
        enc_type = 0x01;  /* RRC encryption */
        int_type = 0x02;  /* RRC integrity */
    } else {
        /* Data Radio Bearer - use UP keys */
        enc_type = 0x03;  /* User plane encryption */
        int_type = 0x04;  /* User plane integrity */
    }
    
    /* Get algorithm identities */
    uint8_t enc_alg = (uint8_t)entity->security_ctx->ciphering_alg;
    uint8_t int_alg = (uint8_t)entity->security_ctx->integrity_alg;
    
    /* Build KDF input for ciphering key */
    uint8_t s_enc[8];
    size_t s_len = 0;
    s_enc[s_len++] = fc;
    s_enc[s_len++] = enc_type;
    s_enc[s_len++] = 0;  /* L0 high byte */
    s_enc[s_len++] = 1;  /* L0 low byte */
    s_enc[s_len++] = enc_alg;
    s_enc[s_len++] = 0;  /* L1 high byte */
    s_enc[s_len++] = 1;  /* L1 low byte */
    
    /* Build KDF input for integrity key */
    uint8_t s_int[8];
    s_len = 0;
    s_int[s_len++] = fc;
    s_int[s_len++] = int_type;
    s_int[s_len++] = 0;  /* L0 high byte */
    s_int[s_len++] = 1;  /* L0 low byte */
    s_int[s_len++] = int_alg;
    s_int[s_len++] = 0;  /* L1 high byte */
    s_int[s_len++] = 1;  /* L1 low byte */
    
    /* Derive keys using simplified KDF (production: HMAC-SHA-256) */
    memset(new_cipher_key, 0, 16);
    memset(new_integrity_key, 0, 16);
    
    for (int i = 0; i < 16; i++) {
        /* Ciphering key derivation */
        new_cipher_key[i] = kamf[i] ^ kamf[i + 16];
        for (size_t j = 0; j < 7; j++) {
            new_cipher_key[i] ^= s_enc[j];
            new_cipher_key[i] = (new_cipher_key[i] << 1) | (new_cipher_key[i] >> 7);
        }
        new_cipher_key[i] ^= (uint8_t)(fc + enc_alg + i);
        
        /* Integrity key derivation */
        new_integrity_key[i] = kamf[i] ^ kamf[(i + 8) % 32];
        for (size_t j = 0; j < 7; j++) {
            new_integrity_key[i] ^= s_int[j];
            new_integrity_key[i] = (new_integrity_key[i] << 1) | (new_integrity_key[i] >> 7);
        }
        new_integrity_key[i] ^= (uint8_t)(fc + int_alg + i);
    }
    
    printf("PDCP: Derived refreshed keys for entity %u\n", entity->entity_id);
    printf("     Bearer type: %s\n", 
           entity->bearer_type <= PDCP_BEARER_SRB3 ? "SRB" : "DRB");
    printf("     Ciphering: NEA%d, type_distinguisher=0x%02x\n", enc_alg, enc_type);
    printf("     Integrity: NIA%d, type_distinguisher=0x%02x\n", int_alg, int_type);
    
    return UESIM_SUCCESS;
}
