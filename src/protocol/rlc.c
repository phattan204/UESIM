/*
 * 5G UE Simulation Application
 * RLC (Radio Link Control) Layer Implementation
 */

#include "rlc.h"
#include "../core/memory.h"
#include "../utils/log.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

// Global RLC context
static atomic_uint g_rlc_entity_counter = 0;

uesim_error_t rlc_init(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Create default RLC entities for SRB1 and SRB2 (AM mode for signaling) */
    rlc_entity_t* srb1_entity = NULL;
    uesim_error_t result = rlc_create_entity(ue_ctx, RLC_BEARER_SRB1, RLC_DIRECTION_BIDIRECTIONAL,
                                             RLC_MODE_AM, NULL, &srb1_entity);
    if (result != UESIM_SUCCESS) {
        LOG_ERROR(LOG_CAT_NAME_RLC, "Failed to create SRB1 entity for UE %u, error=%d", ue_ctx->ue_id, result);
        return result;
    }
    
    /* Store SRB1 RLC entity */
    result = ue_set_rlc_entity(ue_ctx, RLC_BEARER_SRB1, srb1_entity);
    if (result != UESIM_SUCCESS) {
        rlc_destroy_entity(ue_ctx, srb1_entity);
        LOG_ERROR(LOG_CAT_NAME_RLC, "Failed to store SRB1 entity for UE %u, error=%d", ue_ctx->ue_id, result);
        return result;
    }
    
    /* Create SRB2 entity */
    rlc_entity_t* srb2_entity = NULL;
    result = rlc_create_entity(ue_ctx, RLC_BEARER_SRB2, RLC_DIRECTION_BIDIRECTIONAL,
                               RLC_MODE_AM, NULL, &srb2_entity);
    if (result != UESIM_SUCCESS) {
        LOG_WARN(LOG_CAT_NAME_RLC, "Failed to create SRB2 entity for UE %u, error=%d", ue_ctx->ue_id, result);
        /* Continue without SRB2 - SRB1 is sufficient for basic operation */
    } else {
        result = ue_set_rlc_entity(ue_ctx, RLC_BEARER_SRB2, srb2_entity);
        if (result != UESIM_SUCCESS) {
            rlc_destroy_entity(ue_ctx, srb2_entity);
        }
    }
    
    LOG_INFO(LOG_CAT_NAME_RLC, "Initialized for UE %u (SRB1 entity_id=%u)", ue_ctx->ue_id, srb1_entity->entity_id);
    return UESIM_SUCCESS;
}

void rlc_cleanup(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return;
    }
    
    /* Cleanup all RLC entities */
    for (int i = 0; i < UESIM_MAX_RLC_ENTITIES; i++) {
        rlc_entity_t* rlc_entity = ue_get_rlc_entity(ue_ctx, i);
        if (rlc_entity != NULL) {
            uint32_t entity_id = rlc_entity->entity_id;
            rlc_destroy_entity(ue_ctx, rlc_entity);
            ue_remove_rlc_entity(ue_ctx, i);
            LOG_DEBUG(LOG_CAT_NAME_RLC, "Destroyed entity %u for bearer %d", entity_id, i);
        }
    }
    
    LOG_INFO(LOG_CAT_NAME_RLC, "Cleanup completed for UE %u", ue_ctx->ue_id);
}

uesim_error_t rlc_create_entity(ue_context_t* ue_ctx, rlc_bearer_t bearer,
                               rlc_direction_t direction, rlc_mode_t mode,
                               const rlc_config_t* config, rlc_entity_t** entity) {
    if (ue_ctx == NULL || entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (bearer >= RLC_BEARER_DRB_MAX) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (mode >= RLC_MODE_MAX) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Allocate RLC entity
    rlc_entity_t* rlc_entity = (rlc_entity_t*)uesim_calloc(1, sizeof(rlc_entity_t));
    if (rlc_entity == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    // Initialize entity
    rlc_entity->entity_id = atomic_fetch_add(&g_rlc_entity_counter, 1);
    rlc_entity->bearer_type = bearer;
    rlc_entity->direction = direction;
    rlc_entity->mode = mode;
    rlc_entity->status = RLC_STATUS_IDLE;
    rlc_entity->active = false;
    
    // Initialize atomic counters
    atomic_init(&rlc_entity->sdu_counter, 0);
    atomic_init(&rlc_entity->pdu_counter, 0);
    
    // Initialize mutex and condition variable
    if (pthread_mutex_init(&rlc_entity->entity_mutex, NULL) != 0) {
        uesim_free(rlc_entity);
        return UESIM_ERROR_THREAD;
    }
    
    if (pthread_cond_init(&rlc_entity->entity_cond, NULL) != 0) {
        pthread_mutex_destroy(&rlc_entity->entity_mutex);
        uesim_free(rlc_entity);
        return UESIM_ERROR_THREAD;
    }
    
    // Copy configuration if provided
    if (config != NULL) {
        rlc_entity->config = *config;
    }
    
    // Initialize mode-specific entity data
    switch (mode) {
        case RLC_MODE_TM:
            if (pthread_mutex_init(&rlc_entity->entity.tm.entity_mutex, NULL) != 0) {
                pthread_cond_destroy(&rlc_entity->entity_cond);
                pthread_mutex_destroy(&rlc_entity->entity_mutex);
                uesim_free(rlc_entity);
                return UESIM_ERROR_THREAD;
            }
            if (pthread_cond_init(&rlc_entity->entity.tm.entity_cond, NULL) != 0) {
                pthread_mutex_destroy(&rlc_entity->entity.tm.entity_mutex);
                pthread_cond_destroy(&rlc_entity->entity_cond);
                pthread_mutex_destroy(&rlc_entity->entity_mutex);
                uesim_free(rlc_entity);
                return UESIM_ERROR_THREAD;
            }
            break;
            
        case RLC_MODE_UM:
            if (pthread_mutex_init(&rlc_entity->entity.um.entity_mutex, NULL) != 0) {
                pthread_cond_destroy(&rlc_entity->entity_cond);
                pthread_mutex_destroy(&rlc_entity->entity_mutex);
                uesim_free(rlc_entity);
                return UESIM_ERROR_THREAD;
            }
            if (pthread_cond_init(&rlc_entity->entity.um.entity_cond, NULL) != 0) {
                pthread_mutex_destroy(&rlc_entity->entity.um.entity_mutex);
                pthread_cond_destroy(&rlc_entity->entity_cond);
                pthread_mutex_destroy(&rlc_entity->entity_mutex);
                uesim_free(rlc_entity);
                return UESIM_ERROR_THREAD;
            }
            break;
            
        case RLC_MODE_AM:
            if (pthread_mutex_init(&rlc_entity->entity.am.entity_mutex, NULL) != 0) {
                pthread_cond_destroy(&rlc_entity->entity_cond);
                pthread_mutex_destroy(&rlc_entity->entity_mutex);
                uesim_free(rlc_entity);
                return UESIM_ERROR_THREAD;
            }
            if (pthread_cond_init(&rlc_entity->entity.am.entity_cond, NULL) != 0) {
                pthread_mutex_destroy(&rlc_entity->entity.am.entity_mutex);
                pthread_cond_destroy(&rlc_entity->entity_cond);
                pthread_mutex_destroy(&rlc_entity->entity_mutex);
                uesim_free(rlc_entity);
                return UESIM_ERROR_THREAD;
            }
            break;
            
        default:
            pthread_cond_destroy(&rlc_entity->entity_cond);
            pthread_mutex_destroy(&rlc_entity->entity_mutex);
            uesim_free(rlc_entity);
            return UESIM_ERROR_INVALID_PARAM;
    }
    
    rlc_entity->status = RLC_STATUS_CONFIGURED;
    *entity = rlc_entity;
    
    LOG_INFO(LOG_CAT_NAME_RLC, "Entity created: ID=%u, Bearer=%d, Direction=%d, Mode=%d",
           rlc_entity->entity_id, bearer, direction, mode);
    
    return UESIM_SUCCESS;
}

uesim_error_t rlc_destroy_entity(ue_context_t* ue_ctx, rlc_entity_t* entity) {
    if (ue_ctx == NULL || entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Destroy mode-specific entity data
    switch (entity->mode) {
        case RLC_MODE_TM:
            pthread_cond_destroy(&entity->entity.tm.entity_cond);
            pthread_mutex_destroy(&entity->entity.tm.entity_mutex);
            break;
            
        case RLC_MODE_UM:
            pthread_cond_destroy(&entity->entity.um.entity_cond);
            pthread_mutex_destroy(&entity->entity.um.entity_mutex);
            break;
            
        case RLC_MODE_AM:
            pthread_cond_destroy(&entity->entity.am.entity_cond);
            pthread_mutex_destroy(&entity->entity.am.entity_mutex);
            break;
            
        default:
            break;
    }
    
    // Destroy synchronization primitives
    pthread_cond_destroy(&entity->entity_cond);
    pthread_mutex_destroy(&entity->entity_mutex);
    
    // Free entity
    uesim_free(entity);
    
    return UESIM_SUCCESS;
}

uesim_error_t rlc_configure_entity(rlc_entity_t* entity, const rlc_config_t* config) {
    if (entity == NULL || config == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (config->mode >= RLC_MODE_MAX) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&entity->entity_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    entity->config = *config;
    entity->mode = config->mode;
    
    pthread_mutex_unlock(&entity->entity_mutex);
    
    LOG_INFO(LOG_CAT_NAME_RLC, "Entity %u configured with mode %d", entity->entity_id, config->mode);
    return UESIM_SUCCESS;
}

uesim_error_t rlc_activate_entity(rlc_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&entity->entity_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    entity->active = true;
    entity->status = RLC_STATUS_ACTIVE;
    
    pthread_mutex_unlock(&entity->entity_mutex);
    
    LOG_INFO(LOG_CAT_NAME_RLC, "Entity %u activated", entity->entity_id);
    return UESIM_SUCCESS;
}

uesim_error_t rlc_deactivate_entity(rlc_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&entity->entity_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    entity->active = false;
    entity->status = RLC_STATUS_CONFIGURED;
    
    pthread_mutex_unlock(&entity->entity_mutex);
    
    LOG_INFO(LOG_CAT_NAME_RLC, "Entity %u deactivated", entity->entity_id);
    return UESIM_SUCCESS;
}

uesim_error_t rlc_suspend_entity(rlc_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&entity->entity_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    entity->active = false;
    entity->status = RLC_STATUS_SUSPENDED;
    
    pthread_mutex_unlock(&entity->entity_mutex);
    
    LOG_INFO(LOG_CAT_NAME_RLC, "Entity %u suspended", entity->entity_id);
    return UESIM_SUCCESS;
}

uesim_error_t rlc_resume_entity(rlc_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (entity->status != RLC_STATUS_SUSPENDED) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&entity->entity_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    entity->active = true;
    entity->status = RLC_STATUS_ACTIVE;
    
    pthread_mutex_unlock(&entity->entity_mutex);
    
    LOG_INFO(LOG_CAT_NAME_RLC, "Entity %u resumed", entity->entity_id);
    return UESIM_SUCCESS;
}

uesim_error_t rlc_process_tx_data(rlc_entity_t* entity, const void* sdu_data,
                                 size_t sdu_length, rlc_pdu_t** pdu_list) {
    if (entity == NULL || sdu_data == NULL || sdu_length == 0 || pdu_list == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if entity is active
    if (!entity->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if entity is configured
    if (entity->status != RLC_STATUS_ACTIVE) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Process data based on RLC mode
    switch (entity->mode) {
        case RLC_MODE_TM:
            return rlc_tm_process_tx_data(entity, sdu_data, sdu_length, pdu_list);
            
        case RLC_MODE_UM:
            return rlc_um_process_tx_data(entity, sdu_data, sdu_length, pdu_list);
            
        case RLC_MODE_AM:
            return rlc_am_process_tx_data(entity, sdu_data, sdu_length, pdu_list);
            
        default:
            return UESIM_ERROR_INVALID_PARAM;
    }
}

uesim_error_t rlc_process_rx_data(rlc_entity_t* entity, const rlc_pdu_t* pdu_list,
                                 void** sdu_data, size_t* sdu_length) {
    if (entity == NULL || pdu_list == NULL || sdu_data == NULL || sdu_length == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if entity is active
    if (!entity->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if entity is configured
    if (entity->status != RLC_STATUS_ACTIVE) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Process data based on RLC mode
    switch (entity->mode) {
        case RLC_MODE_TM:
            return rlc_tm_process_rx_data(entity, pdu_list, sdu_data, sdu_length);
            
        case RLC_MODE_UM:
            return rlc_um_process_rx_data(entity, pdu_list, sdu_data, sdu_length);
            
        case RLC_MODE_AM:
            return rlc_am_process_rx_data(entity, pdu_list, sdu_data, sdu_length);
            
        default:
            return UESIM_ERROR_INVALID_PARAM;
    }
}

uesim_error_t rlc_receive_pdu(rlc_entity_t* entity, const rlc_pdu_t* pdu) {
    if (entity == NULL || pdu == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if entity is active
    if (!entity->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Process PDU based on RLC mode
    switch (entity->mode) {
        case RLC_MODE_TM:
            return rlc_tm_receive_pdu(entity, pdu);
            
        case RLC_MODE_UM:
            return rlc_um_receive_pdu(entity, pdu);
            
        case RLC_MODE_AM:
            return rlc_am_receive_pdu(entity, pdu);
            
        default:
            return UESIM_ERROR_INVALID_PARAM;
    }
}

// RLC TM Mode Implementation
uesim_error_t rlc_tm_process_tx_data(rlc_entity_t* entity, const void* sdu_data,
                                    size_t sdu_length, rlc_pdu_t** pdu_list) {
    if (entity == NULL || sdu_data == NULL || sdu_length == 0 || pdu_list == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (entity->mode != RLC_MODE_TM) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // In TM mode, SDU is directly passed as PDU without any RLC header
    rlc_pdu_t* pdu = NULL;
    uesim_error_t result = rlc_create_pdu(entity, sdu_length, &pdu);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    // Copy SDU data to PDU
    memcpy(pdu->data, sdu_data, sdu_length);
    pdu->data_length = sdu_length;
    
    // Set PDU in list
    *pdu_list = pdu;
    
    LOG_DEBUG(LOG_CAT_NAME_RLC, "TM: Processed SDU to PDU, length=%zu", sdu_length);
    return UESIM_SUCCESS;
}

uesim_error_t rlc_tm_process_rx_data(rlc_entity_t* entity, const rlc_pdu_t* pdu_list,
                                    void** sdu_data, size_t* sdu_length) {
    if (entity == NULL || pdu_list == NULL || sdu_data == NULL || sdu_length == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (entity->mode != RLC_MODE_TM) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // In TM mode, PDU is directly passed as SDU without any processing
    *sdu_data = uesim_malloc(pdu_list->data_length);
    if (*sdu_data == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    memcpy(*sdu_data, pdu_list->data, pdu_list->data_length);
    *sdu_length = pdu_list->data_length;
    
    LOG_DEBUG(LOG_CAT_NAME_RLC, "TM: Processed PDU to SDU, length=%zu", pdu_list->data_length);
    return UESIM_SUCCESS;
}

uesim_error_t rlc_tm_receive_pdu(rlc_entity_t* entity, const rlc_pdu_t* pdu) {
    if (entity == NULL || pdu == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (entity->mode != RLC_MODE_TM) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // In TM mode, received PDU is directly queued to receive buffer
    if (pthread_mutex_lock(&entity->entity.tm.entity_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    // Create SDU from PDU data
    rlc_sdu_t* sdu = NULL;
    uesim_error_t result = rlc_create_sdu(entity, pdu->data_length, &sdu);
    if (result != UESIM_SUCCESS) {
        pthread_mutex_unlock(&entity->entity.tm.entity_mutex);
        return result;
    }
    
    memcpy(sdu->data, pdu->data, pdu->data_length);
    sdu->data_length = pdu->data_length;
    
    // Queue SDU to receive buffer
    if (entity->entity.tm.rx_buffer == NULL) {
        entity->entity.tm.rx_buffer = sdu;
    } else {
        rlc_sdu_t* current = entity->entity.tm.rx_buffer;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = sdu;
    }
    
    pthread_mutex_unlock(&entity->entity.tm.entity_mutex);
    
    LOG_DEBUG(LOG_CAT_NAME_RLC, "TM: Received PDU queued as SDU, length=%zu", pdu->data_length);
    return UESIM_SUCCESS;
}

// RLC UM Mode Implementation
uesim_error_t rlc_um_process_tx_data(rlc_entity_t* entity, const void* sdu_data,
                                    size_t sdu_length, rlc_pdu_t** pdu_list) {
    if (entity == NULL || sdu_data == NULL || sdu_length == 0 || pdu_list == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (entity->mode != RLC_MODE_UM) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // In UM mode, SDU is segmented if necessary and sequence numbers are assigned
    size_t max_pdu_size = RLC_MAX_PDU_SIZE;
    size_t num_pdus = (sdu_length + max_pdu_size - 1) / max_pdu_size;
    
    rlc_pdu_t* head_pdu = NULL;
    rlc_pdu_t* current_pdu = NULL;
    
    for (size_t i = 0; i < num_pdus; i++) {
        size_t segment_size = (i == num_pdus - 1) ? 
                             (sdu_length - i * max_pdu_size) : max_pdu_size;
        
        rlc_pdu_t* pdu = NULL;
        uesim_error_t result = rlc_create_pdu(entity, segment_size, &pdu);
        if (result != UESIM_SUCCESS) {
            // Cleanup already created PDUs
            rlc_destroy_pdu_list(entity, head_pdu);
            return result;
        }
        
        // Copy segment data
        memcpy(pdu->data, (uint8_t*)sdu_data + i * max_pdu_size, segment_size);
        pdu->data_length = segment_size;
        
        // Assign sequence number
        pdu->sn = atomic_fetch_add(&entity->pdu_counter, 1);
        
        // Set framing info
        if (num_pdus == 1) {
            pdu->fi = 0; // Full SDU
        } else if (i == 0) {
            pdu->fi = 1; // First segment
        } else if (i == num_pdus - 1) {
            pdu->fi = 2; // Last segment
        } else {
            pdu->fi = 3; // Middle segment
        }
        
        // Link PDUs
        if (head_pdu == NULL) {
            head_pdu = pdu;
            current_pdu = pdu;
        } else {
            current_pdu->next = pdu;
            current_pdu = pdu;
        }
    }
    
    *pdu_list = head_pdu;
    
    LOG_DEBUG(LOG_CAT_NAME_RLC, "UM: Processed SDU to %zu PDUs, total length=%zu", num_pdus, sdu_length);
    return UESIM_SUCCESS;
}

uesim_error_t rlc_um_process_rx_data(rlc_entity_t* entity, const rlc_pdu_t* pdu_list,
                                    void** sdu_data, size_t* sdu_length) {
    if (entity == NULL || pdu_list == NULL || sdu_data == NULL || sdu_length == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (entity->mode != RLC_MODE_UM) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // In UM mode, PDUs are reassembled into SDU
    size_t total_length = 0;
    const rlc_pdu_t* current = pdu_list;
    
    // Calculate total length
    while (current != NULL) {
        total_length += current->data_length;
        current = current->next;
    }
    
    // Allocate SDU data
    *sdu_data = uesim_malloc(total_length);
    if (*sdu_data == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    // Copy PDU data to SDU
    current = pdu_list;
    size_t offset = 0;
    while (current != NULL) {
        memcpy((uint8_t*)(*sdu_data) + offset, current->data, current->data_length);
        offset += current->data_length;
        current = current->next;
    }
    
    *sdu_length = total_length;
    
    LOG_DEBUG(LOG_CAT_NAME_RLC, "UM: Processed PDU list to SDU, length=%zu", total_length);
    return UESIM_SUCCESS;
}

uesim_error_t rlc_um_receive_pdu(rlc_entity_t* entity, const rlc_pdu_t* pdu) {
    if (entity == NULL || pdu == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (entity->mode != RLC_MODE_UM) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // In UM mode, received PDU is queued for reassembly
    if (pthread_mutex_lock(&entity->entity.um.entity_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    // Create PDU copy for queuing
    rlc_pdu_t* pdu_copy = NULL;
    uesim_error_t result = rlc_create_pdu(entity, pdu->data_length, &pdu_copy);
    if (result != UESIM_SUCCESS) {
        pthread_mutex_unlock(&entity->entity.um.entity_mutex);
        return result;
    }
    
    memcpy(pdu_copy->data, pdu->data, pdu->data_length);
    pdu_copy->data_length = pdu->data_length;
    pdu_copy->sn = pdu->sn;
    pdu_copy->fi = pdu->fi;
    pdu_copy->so = pdu->so;
    pdu_copy->is_segment = pdu->is_segment;
    
    // Queue PDU to receive buffer
    if (entity->entity.um.rx_buffer == NULL) {
        entity->entity.um.rx_buffer = pdu_copy;
    } else {
        rlc_pdu_t* current = entity->entity.um.rx_buffer;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = pdu_copy;
    }
    
    pthread_mutex_unlock(&entity->entity.um.entity_mutex);
    
    LOG_DEBUG(LOG_CAT_NAME_RLC, "UM: Received PDU queued, SN=%u, length=%zu", pdu->sn, pdu->data_length);
    return UESIM_SUCCESS;
}

// RLC AM Mode Implementation
uesim_error_t rlc_am_process_tx_data(rlc_entity_t* entity, const void* sdu_data,
                                    size_t sdu_length, rlc_pdu_t** pdu_list) {
    if (entity == NULL || sdu_data == NULL || sdu_length == 0 || pdu_list == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (entity->mode != RLC_MODE_AM) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // In AM mode, SDU is segmented, sequence numbered, and placed in transmit window
    size_t max_pdu_size = RLC_MAX_PDU_SIZE;
    size_t num_pdus = (sdu_length + max_pdu_size - 1) / max_pdu_size;
    
    rlc_pdu_t* head_pdu = NULL;
    rlc_pdu_t* current_pdu = NULL;
    
    for (size_t i = 0; i < num_pdus; i++) {
        size_t segment_size = (i == num_pdus - 1) ? 
                             (sdu_length - i * max_pdu_size) : max_pdu_size;
        
        rlc_pdu_t* pdu = NULL;
        uesim_error_t result = rlc_create_pdu(entity, segment_size, &pdu);
        if (result != UESIM_SUCCESS) {
            // Cleanup already created PDUs
            rlc_destroy_pdu_list(entity, head_pdu);
            return result;
        }
        
        // Copy segment data
        memcpy(pdu->data, (uint8_t*)sdu_data + i * max_pdu_size, segment_size);
        pdu->data_length = segment_size;
        
        // Assign sequence number
        pdu->sn = atomic_fetch_add(&entity->pdu_counter, 1);
        
        // Set framing info
        if (num_pdus == 1) {
            pdu->fi = 0; // Full SDU
        } else if (i == 0) {
            pdu->fi = 1; // First segment
        } else if (i == num_pdus - 1) {
            pdu->fi = 2; // Last segment
        } else {
            pdu->fi = 3; // Middle segment
        }
        
        // Link PDUs
        if (head_pdu == NULL) {
            head_pdu = pdu;
            current_pdu = pdu;
        } else {
            current_pdu->next = pdu;
            current_pdu = pdu;
        }
    }
    
    *pdu_list = head_pdu;
    
    LOG_DEBUG(LOG_CAT_NAME_RLC, "AM: Processed SDU to %zu PDUs, total length=%zu", num_pdus, sdu_length);
    return UESIM_SUCCESS;
}

uesim_error_t rlc_am_process_rx_data(rlc_entity_t* entity, const rlc_pdu_t* pdu_list,
                                    void** sdu_data, size_t* sdu_length) {
    if (entity == NULL || pdu_list == NULL || sdu_data == NULL || sdu_length == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (entity->mode != RLC_MODE_AM) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // In AM mode, PDUs are reassembled and acknowledged
    size_t total_length = 0;
    const rlc_pdu_t* current = pdu_list;
    
    // Calculate total length
    while (current != NULL) {
        total_length += current->data_length;
        current = current->next;
    }
    
    // Allocate SDU data
    *sdu_data = uesim_malloc(total_length);
    if (*sdu_data == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    // Copy PDU data to SDU
    current = pdu_list;
    size_t offset = 0;
    while (current != NULL) {
        memcpy((uint8_t*)(*sdu_data) + offset, current->data, current->data_length);
        offset += current->data_length;
        current = current->next;
    }
    
    *sdu_length = total_length;
    
    LOG_DEBUG(LOG_CAT_NAME_RLC, "AM: Processed PDU list to SDU, length=%zu", total_length);
    return UESIM_SUCCESS;
}

uesim_error_t rlc_am_receive_pdu(rlc_entity_t* entity, const rlc_pdu_t* pdu) {
    if (entity == NULL || pdu == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (entity->mode != RLC_MODE_AM) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // In AM mode, received PDU is processed for reassembly and acknowledgment
    if (pthread_mutex_lock(&entity->entity.am.entity_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    // Check if this is a status PDU
    if (pdu->data_length >= 2 && pdu->data[0] == 0xC0) { // Status PDU indicator
        uesim_error_t result = rlc_am_process_status_pdu(entity, pdu->data, pdu->data_length);
        pthread_mutex_unlock(&entity->entity.am.entity_mutex);
        return result;
    }
    
    // Regular data PDU - queue for reassembly
    rlc_pdu_t* pdu_copy = NULL;
    uesim_error_t result = rlc_create_pdu(entity, pdu->data_length, &pdu_copy);
    if (result != UESIM_SUCCESS) {
        pthread_mutex_unlock(&entity->entity.am.entity_mutex);
        return result;
    }
    
    memcpy(pdu_copy->data, pdu->data, pdu->data_length);
    pdu_copy->data_length = pdu->data_length;
    pdu_copy->sn = pdu->sn;
    pdu_copy->fi = pdu->fi;
    pdu_copy->so = pdu->so;
    pdu_copy->is_segment = pdu->is_segment;
    
    // Queue PDU to receive buffer
    if (entity->entity.am.rx_buffer == NULL) {
        entity->entity.am.rx_buffer = pdu_copy;
    } else {
        rlc_pdu_t* current = entity->entity.am.rx_buffer;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = pdu_copy;
    }
    
    pthread_mutex_unlock(&entity->entity.am.entity_mutex);
    
    LOG_DEBUG(LOG_CAT_NAME_RLC, "AM: Received data PDU, SN=%u, length=%zu", pdu->sn, pdu->data_length);
    return UESIM_SUCCESS;
}

// RLC PDU Management
uesim_error_t rlc_create_pdu(rlc_entity_t* entity, size_t data_length, rlc_pdu_t** pdu) {
    if (entity == NULL || pdu == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Allocate PDU
    rlc_pdu_t* new_pdu = (rlc_pdu_t*)uesim_calloc(1, sizeof(rlc_pdu_t));
    if (new_pdu == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    // Allocate data buffer
    if (data_length > 0) {
        new_pdu->data = (uint8_t*)uesim_malloc(data_length);
        if (new_pdu->data == NULL) {
            uesim_free(new_pdu);
            return UESIM_ERROR_MEMORY;
        }
        new_pdu->data_length = data_length;
    }
    
    *pdu = new_pdu;
    return UESIM_SUCCESS;
}

uesim_error_t rlc_destroy_pdu(rlc_entity_t* entity, rlc_pdu_t* pdu) {
    if (pdu == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pdu->data != NULL) {
        uesim_free(pdu->data);
    }
    
    uesim_free(pdu);
    return UESIM_SUCCESS;
}

uesim_error_t rlc_destroy_pdu_list(rlc_entity_t* entity, rlc_pdu_t* pdu_list) {
    rlc_pdu_t* current = pdu_list;
    while (current != NULL) {
        rlc_pdu_t* next = current->next;
        rlc_destroy_pdu(entity, current);
        current = next;
    }
    return UESIM_SUCCESS;
}

// RLC SDU Management
uesim_error_t rlc_create_sdu(rlc_entity_t* entity, size_t data_length, rlc_sdu_t** sdu) {
    if (entity == NULL || sdu == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Allocate SDU
    rlc_sdu_t* new_sdu = (rlc_sdu_t*)uesim_calloc(1, sizeof(rlc_sdu_t));
    if (new_sdu == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    // Allocate data buffer
    if (data_length > 0) {
        new_sdu->data = (uint8_t*)uesim_malloc(data_length);
        if (new_sdu->data == NULL) {
            uesim_free(new_sdu);
            return UESIM_ERROR_MEMORY;
        }
        new_sdu->data_length = data_length;
    }
    
    // Set SDU ID
    new_sdu->sdu_id = atomic_fetch_add(&entity->sdu_counter, 1);
    
    // Set creation time
    new_sdu->creation_time = (uint32_t)time(NULL);
    
    *sdu = new_sdu;
    return UESIM_SUCCESS;
}

uesim_error_t rlc_destroy_sdu(rlc_entity_t* entity, rlc_sdu_t* sdu) {
    if (sdu == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (sdu->data != NULL) {
        uesim_free(sdu->data);
    }
    
    uesim_free(sdu);
    return UESIM_SUCCESS;
}

uesim_error_t rlc_queue_sdu(rlc_entity_t* entity, rlc_sdu_t* sdu) {
    if (entity == NULL || sdu == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Queue SDU to transmit buffer based on RLC mode
    switch (entity->mode) {
        case RLC_MODE_TM:
            if (pthread_mutex_lock(&entity->entity.tm.entity_mutex) != 0) {
                return UESIM_ERROR_THREAD;
            }
            if (entity->entity.tm.tx_buffer == NULL) {
                entity->entity.tm.tx_buffer = sdu;
            } else {
                rlc_sdu_t* current = entity->entity.tm.tx_buffer;
                while (current->next != NULL) {
                    current = current->next;
                }
                current->next = sdu;
            }
            pthread_mutex_unlock(&entity->entity.tm.entity_mutex);
            break;
            
        case RLC_MODE_UM:
            // UM mode doesn't typically queue SDUs, processes them directly
            return UESIM_ERROR_INVALID_PARAM;
            
        case RLC_MODE_AM:
            if (pthread_mutex_lock(&entity->entity.am.entity_mutex) != 0) {
                return UESIM_ERROR_THREAD;
            }
            if (entity->entity.am.tx_buffer == NULL) {
                entity->entity.am.tx_buffer = sdu;
            } else {
                rlc_sdu_t* current = entity->entity.am.tx_buffer;
                while (current->next != NULL) {
                    current = current->next;
                }
                current->next = sdu;
            }
            pthread_mutex_unlock(&entity->entity.am.entity_mutex);
            break;
            
        default:
            return UESIM_ERROR_INVALID_PARAM;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t rlc_dequeue_sdu(rlc_entity_t* entity, rlc_sdu_t** sdu) {
    if (entity == NULL || sdu == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Dequeue SDU from receive buffer based on RLC mode
    switch (entity->mode) {
        case RLC_MODE_TM:
            if (pthread_mutex_lock(&entity->entity.tm.entity_mutex) != 0) {
                return UESIM_ERROR_THREAD;
            }
            if (entity->entity.tm.rx_buffer == NULL) {
                pthread_mutex_unlock(&entity->entity.tm.entity_mutex);
                return UESIM_ERROR_INVALID_PARAM;
            }
            *sdu = entity->entity.tm.rx_buffer;
            entity->entity.tm.rx_buffer = (*sdu)->next;
            (*sdu)->next = NULL;
            pthread_mutex_unlock(&entity->entity.tm.entity_mutex);
            break;
            
        case RLC_MODE_UM:
            // UM mode doesn't typically queue SDUs for dequeue
            return UESIM_ERROR_INVALID_PARAM;
            
        case RLC_MODE_AM:
            if (pthread_mutex_lock(&entity->entity.am.entity_mutex) != 0) {
                return UESIM_ERROR_THREAD;
            }
            if (entity->entity.am.rx_buffer == NULL) {
                pthread_mutex_unlock(&entity->entity.am.entity_mutex);
                return UESIM_ERROR_INVALID_PARAM;
            }
            *sdu = entity->entity.am.rx_buffer;
            entity->entity.am.rx_buffer = (*sdu)->next;
            (*sdu)->next = NULL;
            pthread_mutex_unlock(&entity->entity.am.entity_mutex);
            break;
            
        default:
            return UESIM_ERROR_INVALID_PARAM;
    }
    
    return UESIM_SUCCESS;
}

// RLC AM Specific Functions
uesim_error_t rlc_am_poll_entity(rlc_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (entity->mode != RLC_MODE_AM) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if polling conditions are met
    if (pthread_mutex_lock(&entity->entity.am.entity_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    bool should_poll = false;
    
    // Check poll PDU counter
    if (entity->entity.am.poll_pdu_counter >= entity->config.config.am.poll_pdu) {
        should_poll = true;
        entity->entity.am.poll_pdu_counter = 0;
    }
    
    // Check poll byte counter
    if (entity->entity.am.poll_byte_counter >= entity->config.config.am.poll_byte) {
        should_poll = true;
        entity->entity.am.poll_byte_counter = 0;
    }
    
    pthread_mutex_unlock(&entity->entity.am.entity_mutex);
    
    if (should_poll) {
        LOG_DEBUG(LOG_CAT_NAME_RLC, "AM: Polling triggered for entity %u", entity->entity_id);
        // Generate and send status PDU
        return rlc_am_generate_status_pdu(entity, NULL, NULL);
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t rlc_am_process_status_pdu(rlc_entity_t* entity, const uint8_t* status_data,
                                       size_t status_length) {
    if (entity == NULL || status_data == NULL || status_length == 0) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (entity->mode != RLC_MODE_AM) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    rlc_am_entity_t* am = &entity->entity.am;
    
    if (pthread_mutex_lock(&am->tx_window.window_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    // Parse status PDU
    // Format: CPT (3 bits) | ACK_SN (12/18 bits) | E1 (1 bit) | [NACK_SN | SOstart | SOend]...
    size_t bit_offset = 0;
    
    // Read CPT (Control PDU Type) - should be 0 for STATUS
    uint8_t cpt = (status_data[0] >> 5) & 0x07;
    if (cpt != 0) {
        LOG_ERROR(LOG_CAT_NAME_RLC, "AM: Invalid STATUS PDU CPT=%u", cpt);
        pthread_mutex_unlock(&am->tx_window.window_mutex);
        return UESIM_ERROR_PROTOCOL;
    }
    
    // Read ACK_SN (acknowledged sequence number)
    uint16_t ack_sn;
    if (entity->config.config.am.sn_length == 12) {
        ack_sn = ((status_data[0] & 0x1F) << 7) | ((status_data[1] >> 1) & 0x7F);
        bit_offset = 16;
    } else { // 18-bit SN
        ack_sn = ((status_data[0] & 0x1F) << 13) | (status_data[1] << 5) | 
                 ((status_data[2] >> 3) & 0x1F);
        bit_offset = 24;
    }
    
    LOG_DEBUG(LOG_CAT_NAME_RLC, "AM: STATUS PDU received, ACK_SN=%u", ack_sn);
    
    // Update VT(A) - acknowledged pointer
    rlc_am_tx_window_t* tx_win = &am->tx_window;
    uint16_t sn_mask = (entity->config.config.am.sn_length == 12) ? 0xFFF : 0x3FFFF;
    
    // Remove acknowledged PDUs from transmit window
    while (tx_win->vt_a != ack_sn) {
        uint16_t idx = tx_win->vt_a % tx_win->window_size;
        if (tx_win->tx_window[idx] != NULL) {
            rlc_destroy_pdu(entity, tx_win->tx_window[idx]);
            tx_win->tx_window[idx] = NULL;
        }
        tx_win->vt_a = (tx_win->vt_a + 1) & sn_mask;
    }
    
    // Check for E1 flag (NACK list present)
    bool e1 = (status_data[bit_offset / 8] >> (7 - (bit_offset % 8))) & 1;
    bit_offset++;
    
    // Process NACK list
    while (e1 && (bit_offset / 8) < status_length) {
        // Read NACK_SN
        uint16_t nack_sn;
        if (entity->config.config.am.sn_length == 12) {
            nack_sn = ((status_data[bit_offset / 8] >> (7 - (bit_offset % 8))) & 0x1F) << 7;
            bit_offset += 5;
            nack_sn |= (status_data[bit_offset / 8] >> (7 - (bit_offset % 8))) & 0x7F;
            bit_offset += 7;
        } else {
            nack_sn = ((status_data[bit_offset / 8] >> (7 - (bit_offset % 8))) & 0x1F) << 13;
            bit_offset += 5;
            nack_sn |= (status_data[bit_offset / 8] >> (7 - (bit_offset % 8))) << 5;
            bit_offset += 8;
            nack_sn |= (status_data[bit_offset / 8] >> (7 - (bit_offset % 8))) & 0x1F;
            bit_offset += 5;
        }
        
        // Mark PDU for retransmission
        uint16_t idx = nack_sn % tx_win->window_size;
        if (tx_win->tx_window[idx] != NULL) {
            tx_win->tx_window[idx]->is_segment = true; // Mark for retransmission
            LOG_DEBUG(LOG_CAT_NAME_RLC, "AM: NACK received for SN=%u, marking for retransmission", nack_sn);
        }
        
        // Read E1 (more NACKs) and E2 (has SOstart/SOend)
        e1 = (status_data[bit_offset / 8] >> (7 - (bit_offset % 8))) & 1;
        bit_offset++;
        bool e2 = (status_data[bit_offset / 8] >> (7 - (bit_offset % 8))) & 1;
        bit_offset++;
        
        // Skip SOstart/SOend if present
        if (e2) {
            bit_offset += 32; // SOstart (16 bits) + SOend (16 bits)
        }
    }
    
    pthread_mutex_unlock(&am->tx_window.window_mutex);
    LOG_DEBUG(LOG_CAT_NAME_RLC, "AM: STATUS PDU processed, new VT(A)=%u", tx_win->vt_a);
    
    return UESIM_SUCCESS;
}

uesim_error_t rlc_am_generate_status_pdu(rlc_entity_t* entity, uint8_t** status_data,
                                        size_t* status_length) {
    if (entity == NULL || status_data == NULL || status_length == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (entity->mode != RLC_MODE_AM) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    rlc_am_entity_t* am = &entity->entity.am;
    
    if (pthread_mutex_lock(&am->rx_window.window_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    rlc_am_rx_window_t* rx_win = &am->rx_window;
    
    // Allocate status PDU buffer (max 128 bytes for NACK list)
    uint8_t* pdu = (uint8_t*)uesim_malloc(128);
    if (pdu == NULL) {
        pthread_mutex_unlock(&am->rx_window.window_mutex);
        return UESIM_ERROR_MEMORY;
    }
    memset(pdu, 0, 128);
    
    size_t bit_offset = 0;
    uint16_t sn_mask = (entity->config.config.am.sn_length == 12) ? 0xFFF : 0x3FFFF;
    
    // CPT = 0 (STATUS PDU), D/C = 1 (Control PDU)
    pdu[0] = 0x80; // D/C=1, CPT=0
    bit_offset = 3;
    
    // Write ACK_SN (highest successfully received SN + 1)
    uint16_t ack_sn = rx_win->vr_r;
    if (entity->config.config.am.sn_length == 12) {
        pdu[0] |= (ack_sn >> 7) & 0x1F;
        pdu[1] = (ack_sn & 0x7F) << 1;
        bit_offset = 16;
    } else { // 18-bit SN
        pdu[0] |= (ack_sn >> 13) & 0x1F;
        pdu[1] = (ack_sn >> 5) & 0xFF;
        pdu[2] = (ack_sn & 0x1F) << 3;
        bit_offset = 24;
    }
    
    // Build NACK list for missing PDUs
    uint16_t nack_count = 0;
    uint16_t sn = rx_win->vr_r;
    uint16_t end_sn = rx_win->vr_h;
    
    while (sn != end_sn && nack_count < 16) {
        uint16_t idx = sn % rx_win->window_size;
        
        // Check if PDU is missing
        if (rx_win->rx_window[idx] == NULL) {
            // Set E1 flag (more NACKs follow)
            pdu[bit_offset / 8] |= (1 << (7 - (bit_offset % 8)));
            bit_offset++;
            
            // Write NACK_SN
            if (entity->config.config.am.sn_length == 12) {
                pdu[bit_offset / 8] |= (sn >> 7) & 0x1F;
                bit_offset += 5;
                pdu[bit_offset / 8] |= ((sn & 0x7F) << (7 - (bit_offset % 8)));
                bit_offset += 7;
            } else {
                pdu[bit_offset / 8] |= (sn >> 13) & 0x1F;
                bit_offset += 5;
                pdu[bit_offset / 8] |= (sn >> 5) & 0xFF;
                bit_offset += 8;
                pdu[bit_offset / 8] |= ((sn & 0x1F) << (7 - (bit_offset % 8)));
                bit_offset += 5;
            }
            
            // E2 = 0 (no SOstart/SOend for full PDU NACK)
            bit_offset++;
            
            nack_count++;
        }
        
        sn = (sn + 1) & sn_mask;
    }
    
    // Clear E1 flag after last NACK
    pdu[bit_offset / 8] &= ~(1 << (7 - (bit_offset % 8)));
    
    *status_data = pdu;
    *status_length = (bit_offset + 7) / 8;
    
    pthread_mutex_unlock(&am->rx_window.window_mutex);
    
    LOG_DEBUG(LOG_CAT_NAME_RLC, "AM: Generated STATUS PDU, ACK_SN=%u, NACKs=%u, length=%zu",
           ack_sn, nack_count, *status_length);
    
    return UESIM_SUCCESS;
}

// RLC AM Window Management Functions

uesim_error_t rlc_am_init_tx_window(rlc_am_tx_window_t* win, uint16_t window_size) {
    if (win == NULL || window_size == 0) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    win->tx_window = (rlc_pdu_t**)uesim_calloc(window_size, sizeof(rlc_pdu_t*));
    if (win->tx_window == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    win->window_size = window_size;
    win->vt_a = 0;    // VT(A) - acknowledged
    win->vt_ms = window_size;  // VT(MS) - max send
    win->vt_s = 0;    // VT(S) - next to send
    
    if (pthread_mutex_init(&win->window_mutex, NULL) != 0) {
        uesim_free(win->tx_window);
        return UESIM_ERROR_THREAD;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t rlc_am_init_rx_window(rlc_am_rx_window_t* win, uint16_t window_size) {
    if (win == NULL || window_size == 0) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    win->rx_window = (rlc_pdu_t**)uesim_calloc(window_size, sizeof(rlc_pdu_t*));
    if (win->rx_window == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    win->window_size = window_size;
    win->vr_r = 0;   // VR(R) - last received
    win->vr_mr = window_size;  // VR(MR) - max receive
    win->vr_x = 0;   // VR(X) - t-reassembly trigger
    win->vr_ms = 0;  // VR(MS) - max status
    win->vr_h = 0;   // VR(H) - highest received
    
    if (pthread_mutex_init(&win->window_mutex, NULL) != 0) {
        uesim_free(win->rx_window);
        return UESIM_ERROR_THREAD;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t rlc_am_destroy_tx_window(rlc_am_tx_window_t* win) {
    if (win == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (win->tx_window != NULL) {
        for (uint16_t i = 0; i < win->window_size; i++) {
            if (win->tx_window[i] != NULL) {
                rlc_pdu_t* pdu = win->tx_window[i];
                while (pdu != NULL) {
                    rlc_pdu_t* next = pdu->next;
                    if (pdu->data) uesim_free(pdu->data);
                    uesim_free(pdu);
                    pdu = next;
                }
            }
        }
        uesim_free(win->tx_window);
    }
    
    pthread_mutex_destroy(&win->window_mutex);
    return UESIM_SUCCESS;
}

uesim_error_t rlc_am_destroy_rx_window(rlc_am_rx_window_t* win) {
    if (win == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (win->rx_window != NULL) {
        for (uint16_t i = 0; i < win->window_size; i++) {
            if (win->rx_window[i] != NULL) {
                rlc_pdu_t* pdu = win->rx_window[i];
                while (pdu != NULL) {
                    rlc_pdu_t* next = pdu->next;
                    if (pdu->data) uesim_free(pdu->data);
                    uesim_free(pdu);
                    pdu = next;
                }
            }
        }
        uesim_free(win->rx_window);
    }
    
    pthread_mutex_destroy(&win->window_mutex);
    return UESIM_SUCCESS;
}

uesim_error_t rlc_am_tx_window_insert(rlc_am_tx_window_t* win, rlc_pdu_t* pdu, uint8_t sn_length) {
    if (win == NULL || pdu == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&win->window_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    uint16_t sn_mask = (sn_length == 12) ? 0xFFF : 0x3FFFF;
    uint16_t idx = pdu->sn % win->window_size;
    
    // Check if SN is within window
    int16_t diff = (pdu->sn - win->vt_a) & sn_mask;
    if (diff >= win->window_size) {
        pthread_mutex_unlock(&win->window_mutex);
        return UESIM_ERROR_CAPACITY;
    }
    
    // Insert PDU into window
    pdu->next = win->tx_window[idx];
    win->tx_window[idx] = pdu;
    
    // Update VT(S)
    win->vt_s = (pdu->sn + 1) & sn_mask;
    
    pthread_mutex_unlock(&win->window_mutex);
    return UESIM_SUCCESS;
}

rlc_pdu_t* rlc_am_tx_window_get(rlc_am_tx_window_t* win, uint16_t sn) {
    if (win == NULL || win->tx_window == NULL) {
        return NULL;
    }
    
    uint16_t idx = sn % win->window_size;
    return win->tx_window[idx];
}

uesim_error_t rlc_am_rx_window_insert(rlc_am_rx_window_t* win, rlc_pdu_t* pdu, uint8_t sn_length) {
    if (win == NULL || pdu == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&win->window_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    uint16_t sn_mask = (sn_length == 12) ? 0xFFF : 0x3FFFF;
    uint16_t idx = pdu->sn % win->window_size;
    
    // Check if SN is within window
    int16_t diff = (pdu->sn - win->vr_r) & sn_mask;
    if (diff >= win->window_size) {
        pthread_mutex_unlock(&win->window_mutex);
        return UESIM_ERROR_CAPACITY;
    }
    
    // Insert PDU into window
    pdu->next = win->rx_window[idx];
    win->rx_window[idx] = pdu;
    
    // Update VR(H) if this is highest SN received
    diff = (pdu->sn - win->vr_h) & sn_mask;
    if (diff > 0 && diff < win->window_size) {
        win->vr_h = pdu->sn;
    }
    
    // Try to advance VR(R)
    while (win->rx_window[win->vr_r % win->window_size] != NULL) {
        win->vr_r = (win->vr_r + 1) & sn_mask;
        win->vr_mr = (win->vr_r + win->window_size) & sn_mask;
    }
    
    pthread_mutex_unlock(&win->window_mutex);
    return UESIM_SUCCESS;
}

rlc_pdu_t* rlc_am_rx_window_get(rlc_am_rx_window_t* win, uint16_t sn) {
    if (win == NULL || win->rx_window == NULL) {
        return NULL;
    }
    
    uint16_t idx = sn % win->window_size;
    return win->rx_window[idx];
}

// RLC Utility Functions
uint16_t rlc_get_sequence_number(rlc_entity_t* entity) {
    if (entity == NULL) {
        return 0;
    }
    
    return atomic_load(&entity->pdu_counter);
}

uesim_error_t rlc_increment_sequence_number(rlc_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    atomic_fetch_add(&entity->pdu_counter, 1);
    return UESIM_SUCCESS;
}

bool rlc_is_entity_active(rlc_entity_t* entity) {
    if (entity == NULL) {
        return false;
    }
    
    return entity->active;
}

uesim_error_t rlc_get_entity_stats(rlc_entity_t* entity, rlc_stats_t* stats) {
    if (entity == NULL || stats == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Return current entity statistics
    // In a real implementation, this would track actual transmission/reception stats
    
    memset(stats, 0, sizeof(rlc_stats_t));
    return UESIM_SUCCESS;
}

// RLC Configuration Functions
uesim_error_t rlc_set_tm_config(rlc_config_t* config) {
    if (config == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    config->mode = RLC_MODE_TM;
    // TM mode has minimal configuration
    return UESIM_SUCCESS;
}

uesim_error_t rlc_set_um_config(rlc_config_t* config, uint8_t sn_length, uint16_t t_reassembly) {
    if (config == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (sn_length != 6 && sn_length != 12) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    config->mode = RLC_MODE_UM;
    config->config.um.sn_length = sn_length;
    config->config.um.t_reassembly = t_reassembly;
    config->config.um.enable_ciphering = true;
    
    return UESIM_SUCCESS;
}

uesim_error_t rlc_set_am_config(rlc_config_t* config, uint8_t sn_length, uint16_t t_poll_retransmit,
                               uint16_t t_reassembly, uint16_t t_status_prohibit,
                               uint32_t poll_pdu, uint32_t poll_byte, uint16_t max_retx_threshold) {
    if (config == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (sn_length != 12 && sn_length != 18) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    config->mode = RLC_MODE_AM;
    config->config.am.sn_length = sn_length;
    config->config.am.t_poll_retransmit = t_poll_retransmit;
    config->config.am.t_reassembly = t_reassembly;
    config->config.am.t_status_prohibit = t_status_prohibit;
    config->config.am.poll_pdu = poll_pdu;
    config->config.am.poll_byte = poll_byte;
    config->config.am.max_retx_threshold = max_retx_threshold;
    
    return UESIM_SUCCESS;
}