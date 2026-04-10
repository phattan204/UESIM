/*
 * 5G UE Simulation Application
 * MAC (Medium Access Control) Layer Implementation
 */

#include "mac.h"
#include "../core/memory.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

// Global MAC context
static atomic_uint g_mac_entity_counter = 0;

uesim_error_t mac_init(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("MAC initialized for UE %u\n", ue_ctx->ue_id);
    return UESIM_SUCCESS;
}

void mac_cleanup(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return;
    }
    
    printf("MAC cleanup completed for UE %u\n", ue_ctx->ue_id);
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
    
    mac_entity->status = MAC_STATUS_CONFIGURED;
    *entity = mac_entity;
    
    printf("MAC entity created: ID=%u\n", mac_entity->entity_id);
    
    return UESIM_SUCCESS;
}

uesim_error_t mac_destroy_entity(ue_context_t* ue_ctx, mac_entity_t* entity) {
    if (ue_ctx == NULL || entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Destroy HARQ processes
    for (int i = 0; i < MAC_MAX_HARQ_PROCESSES; i++) {
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
    
    // In a real implementation, this would queue data to the appropriate logical channel
    // For now, we'll just log the operation
    printf("MAC: Queued data to logical channel %d, length=%zu\n", lch_id, length);
    
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
    
    // Process RACH request
    printf("MAC: Processing RACH request with preamble %u\n", preamble_index);
    
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
    
    // Send RACH preamble
    printf("MAC: Sending RACH preamble %u\n", preamble_index);
    
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
    
    // Process RACH response
    printf("MAC: Received RACH response, length=%zu\n", response_length);
    
    // Update statistics
    if (pthread_mutex_lock(&entity->mac_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    entity->stats.rach_success++;
    
    pthread_mutex_unlock(&entity->mac_mutex);
    
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