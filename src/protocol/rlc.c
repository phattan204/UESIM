/*
 * 5G UE Simulation Application
 * RLC (Radio Link Control) Layer Implementation
 */

#include "rlc.h"
#include "../core/memory.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

// Global RLC context
static atomic_uint g_rlc_entity_counter = 0;

uesim_error_t rlc_init(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("RLC initialized for UE %u\n", ue_ctx->ue_id);
    return UESIM_SUCCESS;
}

void rlc_cleanup(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return;
    }
    
    printf("RLC cleanup completed for UE %u\n", ue_ctx->ue_id);
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
    
    printf("RLC entity created: ID=%u, Bearer=%d, Direction=%d, Mode=%d\n",
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
    
    printf("RLC entity %u configured with mode %d\n", entity->entity_id, config->mode);
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
    
    printf("RLC entity %u activated\n", entity->entity_id);
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
    
    printf("RLC entity %u deactivated\n", entity->entity_id);
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
    
    printf("RLC entity %u suspended\n", entity->entity_id);
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
    
    printf("RLC entity %u resumed\n", entity->entity_id);
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
    
    printf("RLC TM: Processed SDU to PDU, length=%zu\n", sdu_length);
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
    
    printf("RLC TM: Processed PDU to SDU, length=%zu\n", pdu_list->data_length);
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
    
    printf("RLC TM: Received PDU queued as SDU, length=%zu\n", pdu->data_length);
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
    
    printf("RLC UM: Processed SDU to %zu PDUs, total length=%zu\n", num_pdus, sdu_length);
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
    
    printf("RLC UM: Processed PDU list to SDU, length=%zu\n", total_length);
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
    
    printf("RLC UM: Received PDU queued, SN=%u, length=%zu\n", pdu->sn, pdu->data_length);
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
    
    printf("RLC AM: Processed SDU to %zu PDUs, total length=%zu\n", num_pdus, sdu_length);
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
    
    printf("RLC AM: Processed PDU list to SDU, length=%zu\n", total_length);
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
    
    printf("RLC AM: Received data PDU, SN=%u, length=%zu\n", pdu->sn, pdu->data_length);
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
        printf("RLC AM: Polling triggered for entity %u\n", entity->entity_id);
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
    
    // Process received status PDU
    printf("RLC AM: Processing status PDU, length=%zu\n", status_length);
    
    // Parse status PDU and handle acknowledgments
    // This would involve updating transmit window and retransmitting unacknowledged PDUs
    
    return UESIM_SUCCESS;
}

uesim_error_t rlc_am_generate_status_pdu(rlc_entity_t* entity, uint8_t** status_data,
                                        size_t* status_length) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (entity->mode != RLC_MODE_AM) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Generate status PDU with current receive state
    printf("RLC AM: Generating status PDU for entity %u\n", entity->entity_id);
    
    // In a real implementation, this would create a proper status PDU
    // with acknowledgment information and bitmap of received PDUs
    
    return UESIM_SUCCESS;
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