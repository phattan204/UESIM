/*
 * 5G UE Simulation Application
 * MAC (Medium Access Control) Layer Implementation
 */

#include "mac.h"
#include "../core/memory.h"
#include "../utils/log.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

// Global MAC context
static atomic_uint g_mac_entity_counter = 0;

uesim_error_t mac_init(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Create MAC entity with default configuration */
    mac_entity_t* mac_entity = NULL;
    uesim_error_t result = mac_create_entity(ue_ctx, NULL, &mac_entity);
    if (result != UESIM_SUCCESS) {
        LOG_ERROR(LOG_CAT_NAME_MAC, "Failed to create entity for UE %u, error=%d", ue_ctx->ue_id, result);
        return result;
    }
    
    /* Store MAC entity in UE context */
    result = ue_set_mac_entity(ue_ctx, mac_entity);
    if (result != UESIM_SUCCESS) {
        mac_destroy_entity(ue_ctx, mac_entity);
        LOG_ERROR(LOG_CAT_NAME_MAC, "Failed to store entity for UE %u, error=%d", ue_ctx->ue_id, result);
        return result;
    }
    
    /* Activate the MAC entity */
    result = mac_activate_entity(mac_entity);
    if (result != UESIM_SUCCESS) {
        mac_destroy_entity(ue_ctx, mac_entity);
        ue_set_mac_entity(ue_ctx, NULL);
        return result;
    }
    
    LOG_INFO(LOG_CAT_NAME_MAC, "Initialized for UE %u (entity_id=%u)", ue_ctx->ue_id, mac_entity->entity_id);
    return UESIM_SUCCESS;
}

void mac_cleanup(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return;
    }
    
    mac_entity_t* mac_entity = ue_get_mac_entity(ue_ctx);
    if (mac_entity != NULL) {
        uint32_t entity_id = mac_entity->entity_id;
        mac_destroy_entity(ue_ctx, mac_entity);
        ue_set_mac_entity(ue_ctx, NULL);
        LOG_INFO(LOG_CAT_NAME_MAC, "Cleanup completed for UE %u (entity_id=%u)", ue_ctx->ue_id, entity_id);
    } else {
        LOG_DEBUG(LOG_CAT_NAME_MAC, "No entity to cleanup for UE %u", ue_ctx->ue_id);
    }
}

uesim_error_t mac_create_entity(ue_context_t* ue_ctx, const mac_config_t* config, 
                               mac_entity_t** entity) {
    if (ue_ctx == NULL || entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Allocate MAC entity
    mac_entity_t* mac_entity = (mac_entity_t*)uesim_calloc(1, sizeof(mac_entity_t));
    if (mac_entity == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    // Initialize entity
    mac_entity->entity_id = atomic_fetch_add(&g_mac_entity_counter, 1);
    mac_entity->status = MAC_STATUS_IDLE;
    mac_entity->active = false;
    
    // Initialize atomic counters
    atomic_init(&mac_entity->tb_counter, 0);
    atomic_init(&mac_entity->ce_counter, 0);
    
    // Initialize mutex and condition variable
    if (pthread_mutex_init(&mac_entity->mac_mutex, NULL) != 0) {
        uesim_free(mac_entity);
        return UESIM_ERROR_THREAD;
    }
    
    if (pthread_cond_init(&mac_entity->mac_cond, NULL) != 0) {
        pthread_mutex_destroy(&mac_entity->mac_mutex);
        uesim_free(mac_entity);
        return UESIM_ERROR_THREAD;
    }
    
    // Copy configuration if provided
    if (config != NULL) {
        mac_entity->config = *config;
    } else {
        // Set default configuration
        mac_set_default_config(&mac_entity->config);
    }
    
    // Initialize HARQ processes
    for (int i = 0; i < MAC_MAX_HARQ_PROCESSES; i++) {
        uesim_error_t result = mac_harq_init_process(&mac_entity->ul_harq[i], i);
        if (result != UESIM_SUCCESS) {
            // Cleanup already initialized processes
            for (int j = 0; j < i; j++) {
                pthread_mutex_destroy(&mac_entity->ul_harq[j].harq_mutex);
            }
            pthread_cond_destroy(&mac_entity->mac_cond);
            pthread_mutex_destroy(&mac_entity->mac_mutex);
            uesim_free(mac_entity);
            return result;
        }
        
        result = mac_harq_init_process(&mac_entity->dl_harq[i], i);
        if (result != UESIM_SUCCESS) {
            // Cleanup already initialized processes
            for (int j = 0; j <= i; j++) {
                pthread_mutex_destroy(&mac_entity->ul_harq[j].harq_mutex);
            }
            for (int j = 0; j < i; j++) {
                pthread_mutex_destroy(&mac_entity->dl_harq[j].harq_mutex);
            }
            pthread_cond_destroy(&mac_entity->mac_cond);
            pthread_mutex_destroy(&mac_entity->mac_mutex);
            uesim_free(mac_entity);
            return result;
        }
    }
    
    // Initialize logical channel buffers
    for (int i = 0; i < MAC_LCH_MAX; i++) {
        uesim_error_t result = mac_lch_buffer_init(&mac_entity->lch_buffers[i], 65536);  // 64KB default
        if (result != UESIM_SUCCESS) {
            // Cleanup already initialized buffers
            for (int j = 0; j < i; j++) {
                mac_lch_buffer_cleanup(&mac_entity->lch_buffers[j]);
            }
            // Cleanup HARQ processes
            for (int j = 0; j < MAC_MAX_HARQ_PROCESSES; j++) {
                pthread_mutex_destroy(&mac_entity->ul_harq[j].harq_mutex);
                pthread_mutex_destroy(&mac_entity->dl_harq[j].harq_mutex);
            }
            pthread_cond_destroy(&mac_entity->mac_cond);
            pthread_mutex_destroy(&mac_entity->mac_mutex);
            uesim_free(mac_entity);
            return result;
        }
    }
    
    mac_entity->status = MAC_STATUS_CONFIGURED;
    *entity = mac_entity;
    
    printf("MAC entity created: ID=%u\n", mac_entity->entity_id);
    
    return UESIM_SUCCESS;
}

uesim_error_t mac_destroy_entity(ue_context_t* ue_ctx, mac_entity_t* entity) {
    if (ue_ctx == NULL || entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Cleanup logical channel buffers
    for (int i = 0; i < MAC_LCH_MAX; i++) {
        mac_lch_buffer_cleanup(&entity->lch_buffers[i]);
    }
    
    // Destroy HARQ processes
    for (int i = 0; i < MAC_MAX_HARQ_PROCESSES; i++) {
        if (entity->ul_harq[i].tb_data != NULL) {
            uesim_free(entity->ul_harq[i].tb_data);
        }
        if (entity->dl_harq[i].tb_data != NULL) {
            uesim_free(entity->dl_harq[i].tb_data);
        }
        pthread_mutex_destroy(&entity->ul_harq[i].harq_mutex);
        pthread_mutex_destroy(&entity->dl_harq[i].harq_mutex);
    }
    
    // Destroy synchronization primitives
    pthread_cond_destroy(&entity->mac_cond);
    pthread_mutex_destroy(&entity->mac_mutex);
    
    // Free entity
    uesim_free(entity);
    
    return UESIM_SUCCESS;
}

uesim_error_t mac_configure_entity(mac_entity_t* entity, const mac_config_t* config) {
    if (entity == NULL || config == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&entity->mac_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    entity->config = *config;
    
    pthread_mutex_unlock(&entity->mac_mutex);
    
    printf("MAC entity %u configured\n", entity->entity_id);
    return UESIM_SUCCESS;
}

uesim_error_t mac_activate_entity(mac_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&entity->mac_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    entity->active = true;
    entity->status = MAC_STATUS_ACTIVE;
    
    pthread_mutex_unlock(&entity->mac_mutex);
    
    printf("MAC entity %u activated\n", entity->entity_id);
    return UESIM_SUCCESS;
}

uesim_error_t mac_deactivate_entity(mac_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&entity->mac_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    entity->active = false;
    entity->status = MAC_STATUS_CONFIGURED;
    
    pthread_mutex_unlock(&entity->mac_mutex);
    
    printf("MAC entity %u deactivated\n", entity->entity_id);
    return UESIM_SUCCESS;
}

uesim_error_t mac_suspend_entity(mac_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&entity->mac_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    entity->active = false;
    entity->status = MAC_STATUS_SUSPENDED;
    
    pthread_mutex_unlock(&entity->mac_mutex);
    
    printf("MAC entity %u suspended\n", entity->entity_id);
    return UESIM_SUCCESS;
}

uesim_error_t mac_resume_entity(mac_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (entity->status != MAC_STATUS_SUSPENDED) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&entity->mac_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    entity->active = true;
    entity->status = MAC_STATUS_ACTIVE;
    
    pthread_mutex_unlock(&entity->mac_mutex);
    
    printf("MAC entity %u resumed\n", entity->entity_id);
    return UESIM_SUCCESS;
}

uesim_error_t mac_process_tx_data(mac_entity_t* entity, const void* data, 
                                 size_t data_length, mac_tb_t** tb) {
    if (entity == NULL || data == NULL || data_length == 0 || tb == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if entity is active
    if (!entity->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Create transport block
    uesim_error_t result = mac_create_transport_block(entity, data_length, tb);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    // Copy data to transport block
    memcpy((*tb)->data, data, data_length);
    (*tb)->data_length = data_length;
    
    // Assign HARQ process (round-robin)
    static atomic_uint harq_process_counter = 0;
    uint8_t harq_process_id = atomic_fetch_add(&harq_process_counter, 1) % MAC_MAX_HARQ_PROCESSES;
    (*tb)->harq_process_id = harq_process_id;
    
    // Set RNTI
    (*tb)->rnti = entity->config.c_rnti;
    
    // Set direction
    (*tb)->direction = MAC_DIRECTION_UPLINK;
    
    // Queue transport block
    result = mac_queue_transport_block(entity, *tb);
    if (result != UESIM_SUCCESS) {
        mac_destroy_transport_block(entity, *tb);
        *tb = NULL;
        return result;
    }
    
    printf("MAC: Processed TX data to TB, length=%zu, HARQ process=%u\n", 
           data_length, harq_process_id);
    
    return UESIM_SUCCESS;
}

uesim_error_t mac_process_rx_data(mac_entity_t* entity, const mac_tb_t* tb, 
                                 void** data, size_t* data_length) {
    if (entity == NULL || tb == NULL || data == NULL || data_length == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if entity is active
    if (!entity->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Allocate data buffer
    *data = uesim_malloc(tb->data_length);
    if (*data == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    // Copy TB data
    memcpy(*data, tb->data, tb->data_length);
    *data_length = tb->data_length;
    
    printf("MAC: Processed RX TB to data, length=%zu\n", tb->data_length);
    
    return UESIM_SUCCESS;
}

uesim_error_t mac_transmit_tb(mac_entity_t* entity, mac_tb_t* tb) {
    if (entity == NULL || tb == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if entity is active
    if (!entity->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Get HARQ process
    if (tb->harq_process_id >= MAC_MAX_HARQ_PROCESSES) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    mac_harq_process_t* harq_process = &entity->ul_harq[tb->harq_process_id];
    
    // Start HARQ transmission
    uesim_error_t result = mac_harq_start_transmission(harq_process, tb->data, tb->data_length);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    // Update statistics
    if (pthread_mutex_lock(&entity->mac_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    entity->stats.tx_tb_count++;
    entity->stats.tx_bytes += tb->data_length;
    
    pthread_mutex_unlock(&entity->mac_mutex);
    
    printf("MAC: Transmitted TB, length=%zu, HARQ process=%u\n", 
           tb->data_length, tb->harq_process_id);
    
    return UESIM_SUCCESS;
}

uesim_error_t mac_receive_tb(mac_entity_t* entity, const mac_tb_t* tb) {
    if (entity == NULL || tb == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if entity is active
    if (!entity->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Update statistics
    if (pthread_mutex_lock(&entity->mac_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    entity->stats.rx_tb_count++;
    entity->stats.rx_bytes += tb->data_length;
    
    pthread_mutex_unlock(&entity->mac_mutex);
    
    printf("MAC: Received TB, length=%zu\n", tb->data_length);
    
    return UESIM_SUCCESS;
}

// MAC HARQ Functions
uesim_error_t mac_harq_init_process(mac_harq_process_t* harq_process, uint8_t process_id) {
    if (harq_process == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    memset(harq_process, 0, sizeof(mac_harq_process_t));
    harq_process->process_id = process_id;
    harq_process->state = MAC_HARQ_IDLE;
    harq_process->max_retransmissions = 4; // Default max retransmissions
    
    // Initialize mutex
    if (pthread_mutex_init(&harq_process->harq_mutex, NULL) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t mac_harq_start_transmission(mac_harq_process_t* harq_process, 
                                         const uint8_t* data, size_t length) {
    if (harq_process == NULL || data == NULL || length == 0) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&harq_process->harq_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    // Free existing data if any
    if (harq_process->tb_data != NULL) {
        uesim_free(harq_process->tb_data);
    }
    
    // Allocate new data buffer
    harq_process->tb_data = (uint8_t*)uesim_malloc(length);
    if (harq_process->tb_data == NULL) {
        pthread_mutex_unlock(&harq_process->harq_mutex);
        return UESIM_ERROR_MEMORY;
    }
    
    // Copy data
    memcpy(harq_process->tb_data, data, length);
    harq_process->tb_length = length;
    harq_process->tb_size = (uint16_t)length;
    harq_process->state = MAC_HARQ_ACTIVE;
    harq_process->transmission_time = (uint32_t)time(NULL);
    harq_process->num_retransmissions = 0;
    harq_process->ndi = 1; // New data
    harq_process->rvidx = 0; // First transmission
    
    pthread_mutex_unlock(&harq_process->harq_mutex);
    
    printf("MAC HARQ: Started transmission, process=%u, length=%zu\n", 
           harq_process->process_id, length);
    
    return UESIM_SUCCESS;
}

uesim_error_t mac_harq_process_ack(mac_harq_process_t* harq_process, uint8_t ack) {
    if (harq_process == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&harq_process->harq_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    harq_process->harq_ack = ack;
    
    if (ack == 1) {
        // ACK received - transmission successful
        harq_process->state = MAC_HARQ_IDLE;
        printf("MAC HARQ: ACK received, process=%u\n", harq_process->process_id);
    } else {
        // NACK received - retransmission needed or max retries reached
        harq_process->num_retransmissions++;
        if (harq_process->num_retransmissions < harq_process->max_retransmissions) {
            harq_process->state = MAC_HARQ_PENDING;
            printf("MAC HARQ: NACK received, process=%u, retransmission=%u\n", 
                   harq_process->process_id, harq_process->num_retransmissions);
        } else {
            // Max retransmissions reached - transmission failed
            harq_process->state = MAC_HARQ_IDLE;
            printf("MAC HARQ: Max retransmissions reached, process=%u\n", 
                   harq_process->process_id);
        }
    }
    
    pthread_mutex_unlock(&harq_process->harq_mutex);
    
    return UESIM_SUCCESS;
}

uesim_error_t mac_harq_retransmit(mac_harq_process_t* harq_process) {
    if (harq_process == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&harq_process->harq_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    if (harq_process->state != MAC_HARQ_PENDING) {
        pthread_mutex_unlock(&harq_process->harq_mutex);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (harq_process->tb_data == NULL) {
        pthread_mutex_unlock(&harq_process->harq_mutex);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Update redundancy version
    harq_process->rvidx = (harq_process->rvidx + 1) % 4;
    harq_process->ndi = 0; // Not new data
    harq_process->state = MAC_HARQ_ACTIVE;
    harq_process->transmission_time = (uint32_t)time(NULL);
    
    pthread_mutex_unlock(&harq_process->harq_mutex);
    
    printf("MAC HARQ: Retransmitting, process=%u, RV=%u\n", 
           harq_process->process_id, harq_process->rvidx);
    
    return UESIM_SUCCESS;
}

uesim_error_t mac_harq_reset_process(mac_harq_process_t* harq_process) {
    if (harq_process == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&harq_process->harq_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    // Free data buffer
    if (harq_process->tb_data != NULL) {
        uesim_free(harq_process->tb_data);
        harq_process->tb_data = NULL;
    }
    
    // Reset process state
    harq_process->tb_length = 0;
    harq_process->tb_size = 0;
    harq_process->rvidx = 0;
    harq_process->ndi = 0;
    harq_process->harq_ack = 0;
    harq_process->transmission_time = 0;
    harq_process->num_retransmissions = 0;
    harq_process->state = MAC_HARQ_IDLE;
    
    pthread_mutex_unlock(&harq_process->harq_mutex);
    
    printf("MAC HARQ: Reset process %u\n", harq_process->process_id);
    
    return UESIM_SUCCESS;
}

// MAC Scheduling Functions
uesim_error_t mac_schedule_uplink(mac_entity_t* entity, mac_ul_grant_t* grant) {
    if (entity == NULL || grant == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if entity is active
    if (!entity->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Generate uplink grant
    grant->rnti = entity->config.c_rnti;
    grant->tb_size = 1024; // Default TB size
    grant->mcs = 10; // Default MCS
    grant->ndi = 1; // New data
    grant->rv = 0; // First transmission
    grant->frequency_hopping = 0; // No frequency hopping
    grant->hopping_id = 0;
    grant->grant_time = (uint32_t)time(NULL);
    grant->valid = true;
    
    printf("MAC: Generated UL grant, TB size=%u, MCS=%u\n", grant->tb_size, grant->mcs);
    
    return UESIM_SUCCESS;
}

uesim_error_t mac_schedule_downlink(mac_entity_t* entity, mac_dl_grant_t* grant) {
    if (entity == NULL || grant == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if entity is active
    if (!entity->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Generate downlink grant
    grant->rnti = entity->config.c_rnti;
    grant->tb_size = 2048; // Default TB size
    grant->mcs = 15; // Default MCS
    grant->ndi = 1; // New data
    grant->rv = 0; // First transmission
    grant->grant_time = (uint32_t)time(NULL);
    grant->valid = true;
    
    printf("MAC: Generated DL grant, TB size=%u, MCS=%u\n", grant->tb_size, grant->mcs);
    
    return UESIM_SUCCESS;
}

uesim_error_t mac_process_grant(mac_entity_t* entity, const mac_ul_grant_t* grant) {
    if (entity == NULL || grant == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if entity is active
    if (!entity->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Process the grant
    if (grant->valid) {
        printf("MAC: Processing UL grant, TB size=%u, MCS=%u\n", grant->tb_size, grant->mcs);
    } else {
        printf("MAC: Invalid UL grant received\n");
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t mac_generate_ul_grant(mac_entity_t* entity, uint16_t tb_size, 
                                   mac_ul_grant_t* grant) {
    if (entity == NULL || grant == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if entity is active
    if (!entity->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Generate UL grant with specified TB size
    grant->rnti = entity->config.c_rnti;
    grant->tb_size = tb_size;
    grant->mcs = 10; // Default MCS (would be calculated based on channel quality)
    grant->ndi = 1; // New data
    grant->rv = 0; // First transmission
    grant->frequency_hopping = 0; // No frequency hopping
    grant->hopping_id = 0;
    grant->grant_time = (uint32_t)time(NULL);
    grant->valid = true;
    
    printf("MAC: Generated UL grant with TB size=%u\n", tb_size);
    
    return UESIM_SUCCESS;
}

// MAC Logical Channel Functions
uesim_error_t mac_configure_logical_channel(mac_entity_t* entity, 
                                           const mac_lch_info_t* lch_info) {
    if (entity == NULL || lch_info == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (lch_info->lch_id >= MAC_LCH_MAX) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&entity->mac_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    // Add logical channel configuration
    if (entity->config.num_lch_configured < MAC_MAX_LOGICAL_CHANNELS) {
        entity->config.lch_config[entity->config.num_lch_configured] = *lch_info;
        entity->config.num_lch_configured++;
        printf("MAC: Configured logical channel %d\n", lch_info->lch_id);
    } else {
        pthread_mutex_unlock(&entity->mac_mutex);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_unlock(&entity->mac_mutex);
    
    return UESIM_SUCCESS;
}

uesim_error_t mac_prioritize_logical_channels(mac_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&entity->mac_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    // Sort logical channels by priority (bubble sort for simplicity)
    for (int i = 0; i < entity->config.num_lch_configured - 1; i++) {
        for (int j = 0; j < entity->config.num_lch_configured - i - 1; j++) {
            if (entity->config.lch_config[j].priority > entity->config.lch_config[j + 1].priority) {
                mac_lch_info_t temp = entity->config.lch_config[j];
                entity->config.lch_config[j] = entity->config.lch_config[j + 1];
                entity->config.lch_config[j + 1] = temp;
            }
        }
    }
    
    pthread_mutex_unlock(&entity->mac_mutex);
    
    printf("MAC: Prioritized logical channels\n");
    
    return UESIM_SUCCESS;
}

// MAC Control Element Functions
uesim_error_t mac_create_control_element(mac_entity_t* entity, uint8_t ce_type, 
                                        const void* ce_data, size_t ce_length, 
                                        mac_ce_t** ce) {
    if (entity == NULL || ce == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Allocate control element
    mac_ce_t* new_ce = (mac_ce_t*)uesim_calloc(1, sizeof(mac_ce_t));
    if (new_ce == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    // Allocate data buffer if needed
    if (ce_data != NULL && ce_length > 0) {
        new_ce->ce_data = (uint8_t*)uesim_malloc(ce_length);
        if (new_ce->ce_data == NULL) {
            uesim_free(new_ce);
            return UESIM_ERROR_MEMORY;
        }
        
        memcpy(new_ce->ce_data, ce_data, ce_length);
        new_ce->ce_length = ce_length;
    }
    
    new_ce->ce_type = ce_type;
    new_ce->rnti = entity->config.c_rnti;
    
    *ce = new_ce;
    
    printf("MAC: Created control element, type=%u, length=%zu\n", ce_type, ce_length);
    
    return UESIM_SUCCESS;
}

uesim_error_t mac_process_control_element(mac_entity_t* entity, const mac_ce_t* ce) {
    if (entity == NULL || ce == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if entity is active
    if (!entity->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Process control element based on type
    switch (ce->ce_type) {
        case 0: // Buffer Status Report
            printf("MAC: Processing Buffer Status Report CE\n");
            break;
        case 1: // Power Headroom Report
            printf("MAC: Processing Power Headroom Report CE\n");
            break;
        case 2: // Timing Advance Command
            printf("MAC: Processing Timing Advance Command CE\n");
            break;
        case 3: // DRX Command
            printf("MAC: Processing DRX Command CE\n");
            break;
        default:
            printf("MAC: Processing unknown CE type %u\n", ce->ce_type);
            break;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t mac_destroy_control_element(mac_entity_t* entity, mac_ce_t* ce) {
    if (ce == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (ce->ce_data != NULL) {
        uesim_free(ce->ce_data);
    }
    
    uesim_free(ce);
    
    return UESIM_SUCCESS;
}

// MAC Random Access Functions
uesim_error_t mac_initiate_random_access(mac_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if entity is active
    if (!entity->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Initiate random access procedure
    uint8_t preamble_index = 1; // Default preamble
    
    uesim_error_t result = mac_send_rach_preamble(entity, preamble_index);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    // Update statistics
    if (pthread_mutex_lock(&entity->mac_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    entity->stats.rach_attempts++;
    
    pthread_mutex_unlock(&entity->mac_mutex);
    
    printf("MAC: Initiated random access procedure\n");
    
    return UESIM_SUCCESS;
}

uesim_error_t mac_process_rach_request(mac_entity_t* entity, uint8_t preamble_index) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if entity is active
    if (!entity->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate preamble index (0-63 for long format, 0-127 for short format)
    if (preamble_index >= MAC_MAX_RACH_PREAMBLES) {
        fprintf(stderr, "MAC: Invalid preamble index %u\n", preamble_index);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if RACH procedure is already in progress
    if (entity->rach_state == MAC_RACH_ACTIVE) {
        fprintf(stderr, "MAC: RACH procedure already in progress\n");
        return UESIM_ERROR_PROTOCOL;
    }
    
    // Initialize RACH procedure
    entity->rach_state = MAC_RACH_ACTIVE;
    entity->rach_preamble_index = preamble_index;
    entity->rach_attempt = 1;
    entity->rach_power = entity->config.rach_config.preamble_initial_rx_target_power;
    entity->rach_backoff = 0;
    
    // Generate RA-RNTI (based on time and frequency resources)
    // RA-RNTI = 1 + s_id + 14 × t_id + 14 × 80 × f_id + 14 × 80 × 8 × ul_carrier_id
    uint8_t s_id = 0;  // OFDM symbol index
    uint8_t t_id = (uint8_t)(time(NULL) % 80);  // Slot index
    uint8_t f_id = 0;  // Frequency index
    uint8_t ul_carrier_id = 0;  // UL carrier index
    entity->config.rach_config.ra_rnti = 1 + s_id + 14 * t_id + 14 * 80 * f_id + 14 * 80 * 8 * ul_carrier_id;
    
    // Update statistics
    if (pthread_mutex_lock(&entity->mac_mutex) == 0) {
        entity->stats.rach_attempts++;
        pthread_mutex_unlock(&entity->mac_mutex);
    }
    
    printf("MAC: RACH request processed, preamble=%u, RA-RNTI=0x%04X, attempt=%u, power=%d dBm\n",
           preamble_index, entity->config.rach_config.ra_rnti, entity->rach_attempt, entity->rach_power);
    
    return UESIM_SUCCESS;
}

uesim_error_t mac_send_rach_preamble(mac_entity_t* entity, uint8_t preamble_index) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if entity is active
    if (!entity->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (preamble_index >= MAC_MAX_RACH_PREAMBLES) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if RACH procedure is already in progress
    if (entity->rach_state == MAC_RACH_ACTIVE) {
        fprintf(stderr, "MAC: RACH procedure already in progress\n");
        return UESIM_ERROR_PROTOCOL;
    }
    
    // Initialize RACH state machine
    entity->rach_state = MAC_RACH_ACTIVE;
    entity->rach_preamble_index = preamble_index;
    
    // Power ramping: increase power if this is a retransmission
    if (entity->rach_attempt > 0) {
        entity->rach_power += entity->config.rach_config.power_ramping_step / 10;  // dB
    } else {
        entity->rach_power = entity->config.rach_config.preamble_initial_rx_target_power;
    }
    
    // Generate RA-RNTI based on PRACH occasion
    // RA-RNTI = 1 + s_id + 14 × t_id + 14 × 80 × f_id + 14 × 80 × 8 × ul_carrier_id
    uint8_t s_id = 0;   // Starting symbol (0-13)
    uint8_t t_id = (uint8_t)(time(NULL) % 80);  // Slot index within frame (0-79)
    uint8_t f_id = 0;   // Frequency index (0-7)
    uint8_t ul_carrier_id = 0;  // UL carrier (NUL=0, SUL=1)
    uint16_t ra_rnti = 1 + s_id + 14 * t_id + 14 * 80 * f_id + 14 * 80 * 8 * ul_carrier_id;
    
    entity->config.rach_config.ra_rnti = ra_rnti;
    
    // Create PRACH preamble (simplified - actual preamble is Zadoff-Chu sequence)
    // Format: [Preamble Index (6 bits) | Format info | Timing]
    uint8_t prach_preamble[6];
    prach_preamble[0] = preamble_index & 0x3F;
    prach_preamble[1] = (preamble_index >> 6) & 0x01;  // Extended preamble for short format
    prach_preamble[2] = s_id;
    prach_preamble[3] = t_id;
    prach_preamble[4] = f_id;
    prach_preamble[5] = ul_carrier_id;
    
    // Start RA-Contention Resolution Timer
    entity->rach_contention_timer = entity->config.rach_config.ra_contention_resolution_timer;
    
    // Update statistics
    if (pthread_mutex_lock(&entity->mac_mutex) == 0) {
        entity->stats.rach_attempts++;
        pthread_mutex_unlock(&entity->mac_mutex);
    }
    
    printf("MAC: Sent RACH preamble, index=%u, RA-RNTI=0x%04X, attempt=%u, power=%d dBm, contention_timer=%u\n",
           preamble_index, ra_rnti, entity->rach_attempt + 1, entity->rach_power, entity->rach_contention_timer);
    
    return UESIM_SUCCESS;
}

uesim_error_t mac_receive_rach_response(mac_entity_t* entity, const uint8_t* response_data, 
                                       size_t response_length) {
    if (entity == NULL || response_data == NULL || response_length == 0) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if entity is active
    if (!entity->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if RACH procedure is active
    if (entity->rach_state != MAC_RACH_ACTIVE) {
        fprintf(stderr, "MAC: No RACH procedure in progress\n");
        return UESIM_ERROR_PROTOCOL;
    }
    
    // Minimum RAR size: 1 byte E/T/RAPID + 4 bytes RAR = 5 bytes
    // For multiple RARs, minimum is 8 bytes (2 entries)
    if (response_length < 5) {
        fprintf(stderr, "MAC: RAR too short, length=%zu\n", response_length);
        return UESIM_ERROR_PROTOCOL;
    }
    
    // Parse MAC RAR PDU (3GPP TS 38.321 Section 6.1.5)
    // RAR subheader format: E (1 bit) | T (1 bit) | RAPID (6 bits)
    // RAR format: R (1 bit) | TA command (12 bits) | UL Grant (20 bits) | TC-RNTI (16 bits)
    
    size_t offset = 0;
    bool found_our_rar = false;
    uint16_t tc_rnti = 0;
    uint16_t ta_command = 0;
    uint16_t ul_grant = 0;
    
    while (offset < response_length) {
        // Parse subheader
        uint8_t subheader = response_data[offset];
        bool e_bit = (subheader >> 7) & 0x01;  // Extension bit
        bool t_bit = (subheader >> 6) & 0x01;  // Type bit (1 = RAPID present)
        uint8_t rapid = subheader & 0x3F;      // Random Access Preamble ID
        
        offset++;
        
        // Check if this RAR is for our preamble
        if (t_bit == 1 && rapid == entity->rach_preamble_index) {
            found_our_rar = true;
            
            // Parse RAR payload (4 bytes)
            if (offset + 4 > response_length) {
                fprintf(stderr, "MAC: RAR payload truncated\n");
                return UESIM_ERROR_PROTOCOL;
            }
            
            // Byte 0: R (1 bit) | TA command MSB (7 bits)
            uint8_t r_bit = (response_data[offset] >> 7) & 0x01;
            ta_command = (response_data[offset] & 0x7F) << 5;
            
            // Byte 1: TA command LSB (5 bits) | UL Grant MSB (3 bits)
            ta_command |= (response_data[offset + 1] >> 3) & 0x1F;
            ul_grant = (response_data[offset + 1] & 0x07) << 17;
            
            // Bytes 2-3: UL Grant remaining (13 bits) | TC-RNTI (16 bits)
            ul_grant |= (response_data[offset + 2] << 9) | ((response_data[offset + 3] >> 7) & 0x01) << 8;
            ul_grant |= response_data[offset + 3] & 0xFF;
            
            // Bytes 4-5: TC-RNTI
            if (offset + 5 < response_length) {
                tc_rnti = (response_data[offset + 4] << 8) | response_data[offset + 5];
            }
            
            printf("MAC: RAR matched - RAPID=%u, TA=%u, UL_Grant=0x%05X, TC-RNTI=0x%04X\n",
                   rapid, ta_command, ul_grant, tc_rnti);
            
            offset += 4;  // Move past RAR payload
            break;
        }
        
        // Skip RAR payload if not our preamble
        if (t_bit == 1) {
            offset += 4;  // Skip 4-byte RAR
        }
        
        // Check if more subheaders follow
        if (e_bit == 0) {
            break;
        }
    }
    
    if (!found_our_rar) {
        fprintf(stderr, "MAC: No RAR found for our preamble %u\n", entity->rach_preamble_index);
        
        // Increment attempt counter and apply backoff
        entity->rach_attempt++;
        
        // Check if max attempts reached
        if (entity->rach_attempt >= entity->config.rach_config.preamble_trans_max) {
            fprintf(stderr, "MAC: RACH max attempts reached (%u)\n", entity->rach_attempt);
            entity->rach_state = MAC_RACH_FAILED;
            return UESIM_ERROR_MAX_RETRIES;
        }
        
        // Apply backoff (simplified - would use backoff indicator from RAR)
        entity->rach_backoff = (rand() % 10) + 1;  // 1-10 ms backoff
        
        printf("MAC: RACH retry scheduled, attempt=%u, backoff=%u ms\n",
               entity->rach_attempt, entity->rach_backoff);
        
        return UESIM_ERROR_RETRY;
    }
    
    // RACH successful - update state
    entity->rach_state = MAC_RACH_SUCCESS;
    
    // Apply Timing Advance command
    // TA = N_TA * Tc, where N_TA = TA_command * 16 * 64 / 2^mu
    // For SCS 30kHz (mu=1): N_TA = TA_command * 512
    entity->timing_advance = ta_command * 512;
    
    // Update C-RNTI (use TC-RNTI as temporary C-RNTI)
    if (tc_rnti != 0) {
        entity->config.c_rnti = tc_rnti;
        printf("MAC: C-RNTI updated to 0x%04X\n", tc_rnti);
    }
    
    // Store UL grant for first uplink transmission
    entity->rach_ul_grant = ul_grant;
    
    // Stop contention resolution timer
    entity->rach_contention_timer = 0;
    
    // Update statistics
    if (pthread_mutex_lock(&entity->mac_mutex) == 0) {
        entity->stats.rach_success++;
        pthread_mutex_unlock(&entity->mac_mutex);
    }
    
    printf("MAC: RACH completed successfully - TA=%u, TC-RNTI=0x%04X, UL_Grant=0x%05X\n",
           ta_command, tc_rnti, ul_grant);
    
    return UESIM_SUCCESS;
}

// MAC Scheduling Request Functions
uesim_error_t mac_trigger_scheduling_request(mac_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if entity is active
    if (!entity->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Trigger scheduling request
    if (pthread_mutex_lock(&entity->mac_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    entity->config.sr_config.sr_pending = true;
    entity->stats.scheduling_requests++;
    
    pthread_mutex_unlock(&entity->mac_mutex);
    
    printf("MAC: Triggered scheduling request\n");
    
    return UESIM_SUCCESS;
}

uesim_error_t mac_process_scheduling_response(mac_entity_t* entity, 
                                             const uint8_t* response_data, 
                                             size_t response_length) {
    if (entity == NULL || response_data == NULL || response_length == 0) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if entity is active
    if (!entity->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Process scheduling response
    printf("MAC: Processing scheduling response, length=%zu\n", response_length);
    
    // Clear SR pending flag
    if (pthread_mutex_lock(&entity->mac_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    entity->config.sr_config.sr_pending = false;
    
    pthread_mutex_unlock(&entity->mac_mutex);
    
    return UESIM_SUCCESS;
}

// MAC Transport Block Functions
uesim_error_t mac_create_transport_block(mac_entity_t* entity, size_t data_length, 
                                        mac_tb_t** tb) {
    if (entity == NULL || tb == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Allocate transport block
    mac_tb_t* new_tb = (mac_tb_t*)uesim_calloc(1, sizeof(mac_tb_t));
    if (new_tb == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    // Allocate data buffer
    if (data_length > 0) {
        new_tb->data = (uint8_t*)uesim_malloc(data_length);
        if (new_tb->data == NULL) {
            uesim_free(new_tb);
            return UESIM_ERROR_MEMORY;
        }
        new_tb->data_length = data_length;
    }
    
    // Set TB ID
    new_tb->tb_id = atomic_fetch_add(&entity->tb_counter, 1);
    
    // Set creation time
    new_tb->creation_time = (uint32_t)time(NULL);
    
    *tb = new_tb;
    
    return UESIM_SUCCESS;
}

uesim_error_t mac_destroy_transport_block(mac_entity_t* entity, mac_tb_t* tb) {
    if (tb == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (tb->data != NULL) {
        uesim_free(tb->data);
    }
    
    uesim_free(tb);
    
    return UESIM_SUCCESS;
}

uesim_error_t mac_queue_transport_block(mac_entity_t* entity, mac_tb_t* tb) {
    if (entity == NULL || tb == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if entity is active
    if (!entity->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Queue transport block (simplified implementation)
    if (pthread_mutex_lock(&entity->mac_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    // In a real implementation, this would add the TB to a proper queue
    // For now, we'll just maintain a simple linked list or queue structure
    if (entity->tb_queue == NULL) {
        entity->tb_queue = tb;
    } else {
        // Add to end of queue (simplified)
        mac_tb_t* current = entity->tb_queue;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = tb;
    }
    
    pthread_mutex_unlock(&entity->mac_mutex);
    
    printf("MAC: Queued transport block, ID=%u\n", tb->tb_id);
    
    return UESIM_SUCCESS;
}

uesim_error_t mac_dequeue_transport_block(mac_entity_t* entity, mac_tb_t** tb) {
    if (entity == NULL || tb == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if entity is active
    if (!entity->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Dequeue transport block (simplified implementation)
    if (pthread_mutex_lock(&entity->mac_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    if (entity->tb_queue == NULL) {
        pthread_mutex_unlock(&entity->mac_mutex);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    *tb = entity->tb_queue;
    entity->tb_queue = (*tb)->next;
    (*tb)->next = NULL;
    
    pthread_mutex_unlock(&entity->mac_mutex);
    
    printf("MAC: Dequeued transport block, ID=%u\n", (*tb)->tb_id);
    
    return UESIM_SUCCESS;
}

// MAC Utility Functions
uint16_t mac_calculate_tb_size(mac_entity_t* entity, uint16_t mcs, uint16_t num_prbs) {
    if (entity == NULL) {
        return 0;
    }
    
    // Simplified TB size calculation based on MCS and PRBs
    // In a real implementation, this would use 3GPP tables
    uint16_t tb_size = mcs * num_prbs * 100; // Simplified calculation
    
    return tb_size;
}

uesim_error_t mac_update_statistics(mac_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Update MAC statistics
    if (pthread_mutex_lock(&entity->mac_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    // In a real implementation, this would update various statistics
    // For now, we'll just log that statistics were updated
    printf("MAC: Updated statistics for entity %u\n", entity->entity_id);
    
    pthread_mutex_unlock(&entity->mac_mutex);
    
    return UESIM_SUCCESS;
}

bool mac_is_entity_active(mac_entity_t* entity) {
    if (entity == NULL) {
        return false;
    }
    
    return entity->active;
}

uesim_error_t mac_get_entity_stats(mac_entity_t* entity, mac_stats_t* stats) {
    if (entity == NULL || stats == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&entity->mac_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    *stats = entity->stats;
    
    pthread_mutex_unlock(&entity->mac_mutex);
    
    return UESIM_SUCCESS;
}

// MAC Configuration Functions
uesim_error_t mac_set_default_config(mac_config_t* config) {
    if (config == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    memset(config, 0, sizeof(mac_config_t));
    
    // Set default values
    config->c_rnti = 0x1234; // Default C-RNTI
    config->harq_processes = 8; // Default HARQ processes
    config->tti_length_ms = MAC_DEFAULT_TTI_MS;
    config->max_harq_retx = 4; // Default max HARQ retransmissions
    
    // Set default RACH configuration
    config->rach_config.preamble_index = 1;
    config->rach_config.ra_rnti = 0x1234;
    config->rach_config.ra_response_window = 10;
    config->rach_config.preamble_trans_max = 7;
    config->rach_config.power_ramping_step = 100; // 1 dB in 0.1 dB steps
    config->rach_config.ra_contention_resolution_timer = 64;
    
    // Set default SR configuration
    config->sr_config.sr_id = 1;
    config->sr_config.sr_prohibit_timer = 20; // ms
    config->sr_config.sr_trans_max = 20;
    config->sr_config.sr_pending = false;
    
    config->num_lch_configured = 0;
    
    return UESIM_SUCCESS;
}

uesim_error_t mac_set_rach_config(mac_config_t* config, const mac_rach_config_t* rach_config) {
    if (config == NULL || rach_config == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    config->rach_config = *rach_config;
    
    return UESIM_SUCCESS;
}

uesim_error_t mac_set_sr_config(mac_config_t* config, const mac_sr_config_t* sr_config) {
    if (config == NULL || sr_config == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    config->sr_config = *sr_config;
    
    return UESIM_SUCCESS;
}

uesim_error_t mac_add_logical_channel(mac_config_t* config, const mac_lch_info_t* lch_info) {
    if (config == NULL || lch_info == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (config->num_lch_configured >= MAC_MAX_LOGICAL_CHANNELS) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    config->lch_config[config->num_lch_configured] = *lch_info;
    config->num_lch_configured++;
    
    return UESIM_SUCCESS;
}

// MAC Logical Channel Buffer Management Functions

uesim_error_t mac_lch_buffer_init(mac_lch_buffer_t* buffer, uint32_t max_bytes) {
    if (buffer == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    memset(buffer, 0, sizeof(mac_lch_buffer_t));
    buffer->max_bytes = max_bytes;
    
    // Initialize mutex
    if (pthread_mutex_init(&buffer->buffer_mutex, NULL) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    return UESIM_SUCCESS;
}

void mac_lch_buffer_cleanup(mac_lch_buffer_t* buffer) {
    if (buffer == NULL) {
        return;
    }
    
    if (pthread_mutex_lock(&buffer->buffer_mutex) != 0) {
        return;
    }
    
    // Free all entries
    mac_lch_buffer_entry_t* entry = buffer->head;
    while (entry != NULL) {
        mac_lch_buffer_entry_t* next = entry->next;
        if (entry->data != NULL) {
            uesim_free(entry->data);
        }
        uesim_free(entry);
        entry = next;
    }
    
    buffer->head = NULL;
    buffer->tail = NULL;
    buffer->entry_count = 0;
    buffer->total_bytes = 0;
    
    pthread_mutex_unlock(&buffer->buffer_mutex);
    pthread_mutex_destroy(&buffer->buffer_mutex);
}

uesim_error_t mac_lch_buffer_enqueue(mac_lch_buffer_t* buffer, const void* data, size_t length) {
    if (buffer == NULL || data == NULL || length == 0) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&buffer->buffer_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    // Check buffer capacity
    if (buffer->total_bytes + length > buffer->max_bytes) {
        pthread_mutex_unlock(&buffer->buffer_mutex);
        printf("MAC LCH Buffer: Buffer full, cannot enqueue %zu bytes (current=%u, max=%u)\n",
               length, buffer->total_bytes, buffer->max_bytes);
        return UESIM_ERROR_MEMORY;
    }
    
    // Create new entry
    mac_lch_buffer_entry_t* entry = (mac_lch_buffer_entry_t*)uesim_calloc(1, sizeof(mac_lch_buffer_entry_t));
    if (entry == NULL) {
        pthread_mutex_unlock(&buffer->buffer_mutex);
        return UESIM_ERROR_MEMORY;
    }
    
    // Allocate data buffer
    entry->data = (uint8_t*)uesim_malloc(length);
    if (entry->data == NULL) {
        uesim_free(entry);
        pthread_mutex_unlock(&buffer->buffer_mutex);
        return UESIM_ERROR_MEMORY;
    }
    
    // Copy data
    memcpy(entry->data, data, length);
    entry->data_length = length;
    entry->arrival_time = (uint32_t)time(NULL);
    entry->next = NULL;
    
    // Add to queue
    if (buffer->tail == NULL) {
        buffer->head = entry;
        buffer->tail = entry;
    } else {
        buffer->tail->next = entry;
        buffer->tail = entry;
    }
    
    buffer->entry_count++;
    buffer->total_bytes += (uint32_t)length;
    
    pthread_mutex_unlock(&buffer->buffer_mutex);
    
    return UESIM_SUCCESS;
}

uesim_error_t mac_lch_buffer_dequeue(mac_lch_buffer_t* buffer, void** data, size_t* length) {
    if (buffer == NULL || data == NULL || length == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    *data = NULL;
    *length = 0;
    
    if (pthread_mutex_lock(&buffer->buffer_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    if (buffer->head == NULL) {
        pthread_mutex_unlock(&buffer->buffer_mutex);
        return UESIM_ERROR_INVALID_PARAM;  // Buffer empty
    }
    
    // Get head entry
    mac_lch_buffer_entry_t* entry = buffer->head;
    
    // Return data to caller (caller takes ownership)
    *data = entry->data;
    *length = entry->data_length;
    
    // Update queue
    buffer->head = entry->next;
    if (buffer->head == NULL) {
        buffer->tail = NULL;
    }
    
    buffer->entry_count--;
    buffer->total_bytes -= (uint32_t)entry->data_length;
    
    // Free entry structure (but not data - caller owns it now)
    uesim_free(entry);
    
    pthread_mutex_unlock(&buffer->buffer_mutex);
    
    return UESIM_SUCCESS;
}

uesim_error_t mac_lch_buffer_peek(mac_lch_buffer_t* buffer, void** data, size_t* length) {
    if (buffer == NULL || data == NULL || length == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    *data = NULL;
    *length = 0;
    
    if (pthread_mutex_lock(&buffer->buffer_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    if (buffer->head == NULL) {
        pthread_mutex_unlock(&buffer->buffer_mutex);
        return UESIM_ERROR_INVALID_PARAM;  // Buffer empty
    }
    
    // Return pointer to data (caller must not free)
    *data = buffer->head->data;
    *length = buffer->head->data_length;
    
    pthread_mutex_unlock(&buffer->buffer_mutex);
    
    return UESIM_SUCCESS;
}

uint32_t mac_lch_buffer_get_bytes(mac_lch_buffer_t* buffer) {
    if (buffer == NULL) {
        return 0;
    }
    
    uint32_t bytes;
    if (pthread_mutex_lock(&buffer->buffer_mutex) != 0) {
        return 0;
    }
    bytes = buffer->total_bytes;
    pthread_mutex_unlock(&buffer->buffer_mutex);
    
    return bytes;
}

uint32_t mac_lch_buffer_get_entries(mac_lch_buffer_t* buffer) {
    if (buffer == NULL) {
        return 0;
    }
    
    uint32_t entries;
    if (pthread_mutex_lock(&buffer->buffer_mutex) != 0) {
        return 0;
    }
    entries = buffer->entry_count;
    pthread_mutex_unlock(&buffer->buffer_mutex);
    
    return entries;
}

bool mac_lch_buffer_is_empty(mac_lch_buffer_t* buffer) {
    if (buffer == NULL) {
        return true;
    }
    
    bool empty;
    if (pthread_mutex_lock(&buffer->buffer_mutex) != 0) {
        return true;
    }
    empty = (buffer->head == NULL);
    pthread_mutex_unlock(&buffer->buffer_mutex);
    
    return empty;
}

// Enhanced MAC Logical Channel Functions

uesim_error_t mac_queue_logical_channel_data(mac_entity_t* entity, 
                                            mac_logical_channel_t lch_id, 
                                            const void* data, size_t length) {
    if (entity == NULL || data == NULL || length == 0) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (lch_id >= MAC_LCH_MAX) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if entity is active
    if (!entity->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Get buffer for this logical channel
    mac_lch_buffer_t* buffer = &entity->lch_buffers[lch_id];
    
    // Enqueue data to the buffer
    uesim_error_t result = mac_lch_buffer_enqueue(buffer, data, length);
    if (result != UESIM_SUCCESS) {
        printf("MAC: Failed to queue data to LCH %d, length=%zu, error=%d\n", 
               lch_id, length, result);
        return result;
    }
    
    // Update LCH info queued bytes
    if (pthread_mutex_lock(&entity->mac_mutex) == 0) {
        // Find LCH config and update
        for (int i = 0; i < entity->config.num_lch_configured; i++) {
            if (entity->config.lch_config[i].lch_id == lch_id) {
                entity->config.lch_config[i].queued_bytes += (uint32_t)length;
                break;
            }
        }
        pthread_mutex_unlock(&entity->mac_mutex);
    }
    
    // Trigger scheduling request if SR is not already pending
    if (!entity->config.sr_config.sr_pending) {
        mac_trigger_scheduling_request(entity);
    }
    
    printf("MAC: Queued data to LCH %d, length=%zu, buffer_entries=%u, buffer_bytes=%u\n", 
           lch_id, length, mac_lch_buffer_get_entries(buffer), mac_lch_buffer_get_bytes(buffer));
    
    return UESIM_SUCCESS;
}

uesim_error_t mac_dequeue_logical_channel_data(mac_entity_t* entity, 
                                              mac_logical_channel_t lch_id,
                                              void** data, size_t* length) {
    if (entity == NULL || data == NULL || length == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (lch_id >= MAC_LCH_MAX) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Check if entity is active
    if (!entity->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    *data = NULL;
    *length = 0;
    
    // Get buffer for this logical channel
    mac_lch_buffer_t* buffer = &entity->lch_buffers[lch_id];
    
    // Dequeue data from the buffer
    uesim_error_t result = mac_lch_buffer_dequeue(buffer, data, length);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    // Update LCH info served bytes
    if (pthread_mutex_lock(&entity->mac_mutex) == 0) {
        // Find LCH config and update
        for (int i = 0; i < entity->config.num_lch_configured; i++) {
            if (entity->config.lch_config[i].lch_id == lch_id) {
                entity->config.lch_config[i].served_bytes += (uint32_t)*length;
                if (entity->config.lch_config[i].queued_bytes >= (uint32_t)*length) {
                    entity->config.lch_config[i].queued_bytes -= (uint32_t)*length;
                } else {
                    entity->config.lch_config[i].queued_bytes = 0;
                }
                break;
            }
        }
        pthread_mutex_unlock(&entity->mac_mutex);
    }
    
    printf("MAC: Dequeued data from LCH %d, length=%zu\n", lch_id, *length);
    
    return UESIM_SUCCESS;
}

uesim_error_t mac_get_lch_buffer_status(mac_entity_t* entity, 
                                       mac_logical_channel_t lch_id,
                                       uint32_t* queued_bytes) {
    if (entity == NULL || queued_bytes == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (lch_id >= MAC_LCH_MAX) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    *queued_bytes = mac_lch_buffer_get_bytes(&entity->lch_buffers[lch_id]);
    
    return UESIM_SUCCESS;
}

uesim_error_t mac_get_total_buffer_status(mac_entity_t* entity, uint32_t* total_bytes) {
    if (entity == NULL || total_bytes == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    *total_bytes = 0;
    
    for (int i = 0; i < MAC_LCH_MAX; i++) {
        *total_bytes += mac_lch_buffer_get_bytes(&entity->lch_buffers[i]);
    }
    
    return UESIM_SUCCESS;
}

/* ============== BSR (Buffer Status Report) Functions ============== */

uesim_error_t mac_bsr_config(mac_entity_t* entity, uint16_t periodic_timer, uint16_t retx_timer) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    entity->config.bsr_periodic_timer = periodic_timer;
    entity->config.bsr_retx_timer = retx_timer;
    
    printf("MAC: BSR configured, periodic=%u ms, retx=%u ms\n", periodic_timer, retx_timer);
    return UESIM_SUCCESS;
}

uesim_error_t mac_bsr_update_lcg(mac_entity_t* entity, uint8_t lcg_id, uint32_t buffer_size) {
    if (entity == NULL || lcg_id >= 8) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    mac_lcg_status_t lcg;
    lcg.lcg_id = lcg_id;
    lcg.buffer_size = buffer_size;
    lcg.has_data = (buffer_size > 0);
    
    /* Trigger regular BSR if this is new data */
    if (buffer_size > 0) {
        mac_bsr_trigger_regular(entity, lcg_id);
    }
    
    printf("MAC: LCG %u buffer updated, size=%u bytes\n", lcg_id, buffer_size);
    return UESIM_SUCCESS;
}

uesim_error_t mac_bsr_construct(mac_entity_t* entity, mac_bsr_type_t type, mac_bsr_ce_t* bsr) {
    if (entity == NULL || bsr == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    bsr->bsr_type = type;
    bsr->lcg_num = 0;
    
    for (int i = 0; i < 8; i++) {
        uint32_t bytes = mac_lch_buffer_get_bytes(&entity->lch_buffers[i]);
        bsr->buffer_size[i] = mac_bsr_buffer_size_to_index(bytes);
        if (bytes > 0) bsr->lcg_num++;
    }
    
    printf("MAC: BSR constructed, type=%d, LCGs=%u\n", type, bsr->lcg_num);
    return UESIM_SUCCESS;
}

uesim_error_t mac_bsr_trigger_regular(mac_entity_t* entity, uint8_t lcg_id) {
    if (entity == NULL || lcg_id >= 8) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Regular BSR triggered when new data becomes available */
    printf("MAC: Regular BSR triggered for LCG %u\n", lcg_id);
    return UESIM_SUCCESS;
}

uesim_error_t mac_bsr_trigger_periodic(mac_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Periodic BSR timer expired */
    printf("MAC: Periodic BSR triggered\n");
    return UESIM_SUCCESS;
}

uesim_error_t mac_bsr_cancel(mac_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Cancel all pending BSRs */
    printf("MAC: BSR cancelled\n");
    return UESIM_SUCCESS;
}

uesim_error_t mac_bsr_get_highest_priority_lcg(mac_entity_t* entity, uint8_t* lcg_id) {
    if (entity == NULL || lcg_id == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    uint32_t max_bytes = 0;
    *lcg_id = 0;
    
    for (int i = 0; i < 8; i++) {
        uint32_t bytes = mac_lch_buffer_get_bytes(&entity->lch_buffers[i]);
        if (bytes > max_bytes) {
            max_bytes = bytes;
            *lcg_id = i;
        }
    }
    
    return UESIM_SUCCESS;
}

/* BSR buffer size table (3GPP TS 38.321) - simplified */
uint8_t mac_bsr_buffer_size_to_index(uint32_t buffer_size) {
    if (buffer_size == 0) return 0;
    if (buffer_size <= 10) return 1;
    if (buffer_size <= 20) return 2;
    if (buffer_size <= 40) return 3;
    if (buffer_size <= 80) return 4;
    if (buffer_size <= 160) return 5;
    if (buffer_size <= 320) return 6;
    if (buffer_size <= 640) return 7;
    if (buffer_size <= 1280) return 8;
    if (buffer_size <= 2560) return 9;
    if (buffer_size <= 5120) return 10;
    if (buffer_size <= 10240) return 11;
    if (buffer_size <= 20480) return 12;
    if (buffer_size <= 40960) return 13;
    if (buffer_size <= 81920) return 14;
    return 15; /* > 81920 bytes */
}

uint32_t mac_bsr_index_to_buffer_size(uint8_t index) {
    static const uint32_t sizes[16] = {0, 10, 20, 40, 80, 160, 320, 640,
                                        1280, 2560, 5120, 10240, 20480, 40960, 81920, 150000};
    return (index < 16) ? sizes[index] : 0;
}

/* ============== PHR (Power Headroom Report) Functions ============== */

uesim_error_t mac_phr_config(mac_entity_t* entity, uint8_t periodic_timer, uint8_t prohibit_timer) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    entity->config.phr_config.phr_periodic_timer = periodic_timer;
    entity->config.phr_config.phr_prohibit_timer = prohibit_timer;
    entity->config.phr_config.phr_triggered = false;
    
    printf("MAC: PHR configured, periodic=%u, prohibit=%u\n", periodic_timer, prohibit_timer);
    return UESIM_SUCCESS;
}

uesim_error_t mac_phr_trigger(mac_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    entity->config.phr_config.phr_triggered = true;
    
    printf("MAC: PHR triggered\n");
    return UESIM_SUCCESS;
}

uesim_error_t mac_phr_construct(mac_entity_t* entity, mac_phr_ce_t* phr) {
    if (entity == NULL || phr == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    *phr = entity->config.phr_config;
    phr->phr_triggered = false; /* Clear trigger after construction */
    
    printf("MAC: PHR constructed, type=%d, entries=%u\n", phr->phr_type, phr->num_entries);
    return UESIM_SUCCESS;
}

uesim_error_t mac_phr_update_entry(mac_entity_t* entity, uint8_t cell_id, int8_t ph, int8_t p_cmax) {
    if (entity == NULL || cell_id >= 8) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    mac_phr_entry_t* entry = &entity->config.phr_config.entries[cell_id];
    entry->serving_cell_id = cell_id;
    entry->ph = ph;
    entry->p_cmax = p_cmax;
    entry->pcmax_set = true;
    
    if (cell_id >= entity->config.phr_config.num_entries) {
        entity->config.phr_config.num_entries = cell_id + 1;
    }
    
    printf("MAC: PHR entry updated, cell=%u, PH=%d dB, P_CMAX=%d dBm\n", cell_id, ph, p_cmax);
    return UESIM_SUCCESS;
}

bool mac_phr_is_triggered(mac_entity_t* entity) {
    if (entity == NULL) return false;
    return entity->config.phr_config.phr_triggered;
}

/* ============== DRX (Discontinuous Reception) Functions ============== */

uesim_error_t mac_drx_config(mac_entity_t* entity, const mac_drx_config_t* config) {
    if (entity == NULL || config == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    entity->config.drx_config = *config;
    entity->config.drx_config.drx_active = false;
    entity->config.drx_config.on_duration = false;
    
    printf("MAC: DRX configured, on_duration=%u ms, cycle=%u ms\n",
           config->on_duration_timer, config->long_drx_cycle);
    return UESIM_SUCCESS;
}

uesim_error_t mac_drx_start(mac_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    entity->config.drx_config.drx_active = true;
    entity->config.drx_config.current_cycle = 0;
    
    printf("MAC: DRX started\n");
    return UESIM_SUCCESS;
}

uesim_error_t mac_drx_stop(mac_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    entity->config.drx_config.drx_active = false;
    entity->config.drx_config.on_duration = false;
    
    printf("MAC: DRX stopped\n");
    return UESIM_SUCCESS;
}

uesim_error_t mac_drx_on_duration_start(mac_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    entity->config.drx_config.on_duration = true;
    
    printf("MAC: DRX on-duration started\n");
    return UESIM_SUCCESS;
}

uesim_error_t mac_drx_on_duration_end(mac_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    entity->config.drx_config.on_duration = false;
    
    printf("MAC: DRX on-duration ended\n");
    return UESIM_SUCCESS;
}

uesim_error_t mac_drx_inactivity_timer_start(mac_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("MAC: DRX inactivity timer started\n");
    return UESIM_SUCCESS;
}

uesim_error_t mac_drx_inactivity_timer_stop(mac_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("MAC: DRX inactivity timer stopped\n");
    return UESIM_SUCCESS;
}

uesim_error_t mac_drx_retransmission_timer_start(mac_entity_t* entity, uint8_t harq_id) {
    if (entity == NULL || harq_id >= MAC_MAX_HARQ_PROCESSES) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    entity->config.drx_config.harq_active[harq_id] = true;
    
    printf("MAC: DRX retransmission timer started for HARQ %u\n", harq_id);
    return UESIM_SUCCESS;
}

uesim_error_t mac_drx_retransmission_timer_stop(mac_entity_t* entity, uint8_t harq_id) {
    if (entity == NULL || harq_id >= MAC_MAX_HARQ_PROCESSES) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    entity->config.drx_config.harq_active[harq_id] = false;
    
    printf("MAC: DRX retransmission timer stopped for HARQ %u\n", harq_id);
    return UESIM_SUCCESS;
}

bool mac_drx_is_active_time(mac_entity_t* entity) {
    if (entity == NULL) return true; /* Default to active if no DRX */
    
    if (!entity->config.drx_config.drx_active) return true;
    
    /* Active during on-duration */
    if (entity->config.drx_config.on_duration) return true;
    
    /* Active if any HARQ retransmission timer running */
    for (int i = 0; i < MAC_MAX_HARQ_PROCESSES; i++) {
        if (entity->config.drx_config.harq_active[i]) return true;
    }
    
    return false;
}

bool mac_drx_is_on_duration(mac_entity_t* entity) {
    if (entity == NULL) return false;
    return entity->config.drx_config.on_duration;
}

uesim_error_t mac_drx_update_timers(mac_entity_t* entity, uint32_t current_time_ms) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    mac_drx_config_t* drx = &entity->config.drx_config;
    
    if (!drx->drx_active) return UESIM_SUCCESS;
    
    /* Check if it's time for on-duration */
    uint16_t cycle = drx->use_short_cycle ? drx->short_drx_cycle : drx->long_drx_cycle;
    uint32_t cycle_position = (current_time_ms + drx->drx_start_offset) % cycle;
    
    if (cycle_position < drx->on_duration_timer) {
        if (!drx->on_duration) {
            mac_drx_on_duration_start(entity);
        }
    } else {
        if (drx->on_duration) {
            mac_drx_on_duration_end(entity);
        }
    }
    
    drx->last_activity_time = current_time_ms;
    
    return UESIM_SUCCESS;
}

/* ============== SR (Scheduling Request) Enhanced Functions ============== */

uesim_error_t mac_sr_config(mac_entity_t* entity, uint8_t sr_id, uint8_t pucch_resource,
                           uint16_t period, uint8_t trans_max) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    mac_sr_config_t* sr = &entity->config.sr_config;
    sr->sr_id = sr_id;
    sr->pucch_resource = pucch_resource;
    sr->period = period;
    sr->sr_trans_max = trans_max;
    sr->sr_counter = 0;
    sr->sr_pending = false;
    sr->sr_prohibited = false;
    
    printf("MAC: SR configured, id=%u, period=%u slots, max=%u\n", sr_id, period, trans_max);
    return UESIM_SUCCESS;
}

uesim_error_t mac_sr_trigger(mac_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    mac_sr_config_t* sr = &entity->config.sr_config;
    
    if (sr->sr_prohibited) {
        printf("MAC: SR is prohibited\n");
        return UESIM_SUCCESS;
    }
    
    sr->sr_pending = true;
    sr->sr_counter = 0;
    
    printf("MAC: SR triggered\n");
    return UESIM_SUCCESS;
}

uesim_error_t mac_sr_cancel(mac_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    entity->config.sr_config.sr_pending = false;
    entity->config.sr_config.sr_counter = 0;
    
    printf("MAC: SR cancelled\n");
    return UESIM_SUCCESS;
}

uesim_error_t mac_sr_transmit(mac_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    mac_sr_config_t* sr = &entity->config.sr_config;
    
    if (!sr->sr_pending) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    sr->sr_counter++;
    entity->stats.scheduling_requests++;
    
    if (sr->sr_counter >= sr->sr_trans_max) {
        /* Max SR transmissions reached - trigger RACH */
        printf("MAC: SR max transmissions reached, triggering RACH\n");
        sr->sr_pending = false;
        sr->sr_counter = 0;
        return mac_initiate_random_access(entity);
    }
    
    printf("MAC: SR transmitted, counter=%u/%u\n", sr->sr_counter, sr->sr_trans_max);
    return UESIM_SUCCESS;
}

bool mac_sr_is_pending(mac_entity_t* entity) {
    if (entity == NULL) return false;
    return entity->config.sr_config.sr_pending;
}

bool mac_sr_is_prohibited(mac_entity_t* entity) {
    if (entity == NULL) return false;
    return entity->config.sr_config.sr_prohibited;
}
