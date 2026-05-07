/*
 * 5G UE Simulation Application
 * SDAP (Service Data Adaptation Protocol) Layer Implementation
 * 
 * Implements 3GPP TS 37.324 SDAP specification
 */

#include "sdap.h"
#include "../core/memory.h"
#include <string.h>
#include <stdio.h>

/* Global entity counter */
static atomic_uint g_sdap_entity_counter = 0;

/* SDAP Control PDU Types */
#define SDAP_CTRL_END_MARKER        0x00
#define SDAP_CTRL_REFLECTIVE_QOS    0x01

const char* sdap_pdu_type_to_string(sdap_pdu_type_t type) {
    switch (type) {
        case SDAP_PDU_DATA: return "DATA";
        case SDAP_PDU_CONTROL: return "CONTROL";
        default: return "UNKNOWN";
    }
}

uesim_error_t sdap_init(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Create default SDAP entity for default PDU session */
    sdap_entity_t* sdap_ent = NULL;
    uesim_error_t result = sdap_create_entity(ue_ctx, 1, NULL, &sdap_ent);
    if (result != UESIM_SUCCESS) {
        printf("SDAP: Failed to create entity for UE %u, error=%d\n", ue_ctx->ue_id, result);
        return result;
    }
    
    /* Activate the entity */
    result = sdap_activate_entity(sdap_ent);
    if (result != UESIM_SUCCESS) {
        sdap_destroy_entity(ue_ctx, sdap_ent);
        printf("SDAP: Failed to activate entity for UE %u, error=%d\n", ue_ctx->ue_id, result);
        return result;
    }
    
    printf("SDAP: Initialized for UE %u (entity_id=%u)\n", ue_ctx->ue_id, sdap_ent->entity_id);
    return UESIM_SUCCESS;
}

void sdap_cleanup(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return;
    }
    
    /* Note: SDAP entities are typically cleaned up when PDU session is released
     * For now, just log cleanup - in full implementation would track entities */
    printf("SDAP: Cleanup completed for UE %u\n", ue_ctx->ue_id);
}

uesim_error_t sdap_create_entity(ue_context_t* ue_ctx, uint32_t pdu_session_id,
                                 const sdap_config_t* config, sdap_entity_t** entity) {
    if (ue_ctx == NULL || entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    sdap_entity_t* sdap_ent = (sdap_entity_t*)uesim_calloc(1, sizeof(sdap_entity_t));
    if (sdap_ent == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    sdap_ent->entity_id = atomic_fetch_add(&g_sdap_entity_counter, 1);
    sdap_ent->pdu_session_id = pdu_session_id;
    sdap_ent->active = false;
    
    /* Initialize mutex */
    if (pthread_mutex_init(&sdap_ent->entity_mutex, NULL) != 0) {
        uesim_free(sdap_ent);
        return UESIM_ERROR_THREAD;
    }
    
    /* Copy configuration if provided */
    if (config != NULL) {
        sdap_ent->config = *config;
        memcpy(sdap_ent->qos_flow_map, config->qos_mappings, 
               sizeof(sdap_qos_mapping_t) * SDAP_MAX_QOS_FLOWS);
    } else {
        sdap_ent->config.pdu_session_id = pdu_session_id;
    }
    
    *entity = sdap_ent;
    
    printf("SDAP: Created entity %u for PDU session %u\n", 
           sdap_ent->entity_id, pdu_session_id);
    
    return UESIM_SUCCESS;
}

uesim_error_t sdap_destroy_entity(ue_context_t* ue_ctx, sdap_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_destroy(&entity->entity_mutex);
    uesim_free(entity);
    
    printf("SDAP: Entity destroyed\n");
    return UESIM_SUCCESS;
}

uesim_error_t sdap_configure_entity(sdap_entity_t* entity, const sdap_config_t* config) {
    if (entity == NULL || config == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&entity->entity_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    entity->config = *config;
    memcpy(entity->qos_flow_map, config->qos_mappings,
           sizeof(sdap_qos_mapping_t) * SDAP_MAX_QOS_FLOWS);
    
    pthread_mutex_unlock(&entity->entity_mutex);
    
    printf("SDAP: Entity %u configured with %u QoS flows\n",
           entity->entity_id, config->num_qos_flows);
    
    return UESIM_SUCCESS;
}

uesim_error_t sdap_activate_entity(sdap_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&entity->entity_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    entity->active = true;
    
    pthread_mutex_unlock(&entity->entity_mutex);
    
    printf("SDAP: Entity %u activated\n", entity->entity_id);
    return UESIM_SUCCESS;
}

uesim_error_t sdap_deactivate_entity(sdap_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&entity->entity_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    entity->active = false;
    
    pthread_mutex_unlock(&entity->entity_mutex);
    
    printf("SDAP: Entity %u deactivated\n", entity->entity_id);
    return UESIM_SUCCESS;
}

uesim_error_t sdap_map_qos_flow(sdap_entity_t* entity, uint8_t qfi, uint8_t drb_id,
                                bool reflective_qos) {
    if (entity == NULL || qfi > 63) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&entity->entity_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    /* Find empty slot or existing mapping */
    for (int i = 0; i < SDAP_MAX_QOS_FLOWS; i++) {
        if (!entity->qos_flow_map[i].active) {
            entity->qos_flow_map[i].qfi = qfi;
            entity->qos_flow_map[i].drb_id = drb_id;
            entity->qos_flow_map[i].reflective_qos = reflective_qos;
            entity->qos_flow_map[i].active = true;
            entity->config.num_qos_flows++;
            
            pthread_mutex_unlock(&entity->entity_mutex);
            
            printf("SDAP: Mapped QFI %u to DRB %u (reflective=%s)\n",
                   qfi, drb_id, reflective_qos ? "yes" : "no");
            return UESIM_SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&entity->entity_mutex);
    return UESIM_ERROR_CAPACITY;
}

uesim_error_t sdap_unmap_qos_flow(sdap_entity_t* entity, uint8_t qfi) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&entity->entity_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    for (int i = 0; i < SDAP_MAX_QOS_FLOWS; i++) {
        if (entity->qos_flow_map[i].active && entity->qos_flow_map[i].qfi == qfi) {
            entity->qos_flow_map[i].active = false;
            entity->config.num_qos_flows--;
            
            pthread_mutex_unlock(&entity->entity_mutex);
            
            printf("SDAP: Unmapped QFI %u\n", qfi);
            return UESIM_SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&entity->entity_mutex);
    return UESIM_ERROR_NOT_FOUND;
}

uesim_error_t sdap_update_qos_mapping(sdap_entity_t* entity, uint8_t qfi,
                                      uint8_t new_drb_id) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&entity->entity_mutex) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    for (int i = 0; i < SDAP_MAX_QOS_FLOWS; i++) {
        if (entity->qos_flow_map[i].active && entity->qos_flow_map[i].qfi == qfi) {
            entity->qos_flow_map[i].drb_id = new_drb_id;
            
            pthread_mutex_unlock(&entity->entity_mutex);
            
            printf("SDAP: Updated QFI %u mapping to DRB %u\n", qfi, new_drb_id);
            return UESIM_SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&entity->entity_mutex);
    return UESIM_ERROR_NOT_FOUND;
}

int8_t sdap_get_drb_for_qos(sdap_entity_t* entity, uint8_t qfi) {
    if (entity == NULL) {
        return -1;
    }
    
    for (int i = 0; i < SDAP_MAX_QOS_FLOWS; i++) {
        if (entity->qos_flow_map[i].active && entity->qos_flow_map[i].qfi == qfi) {
            return entity->qos_flow_map[i].drb_id;
        }
    }
    
    /* Return default DRB if configured */
    if (entity->config.default_drb_configured) {
        return entity->config.default_drb_id;
    }
    
    return -1;
}

bool sdap_is_reflective_qos(sdap_entity_t* entity, uint8_t qfi) {
    if (entity == NULL) {
        return false;
    }
    
    for (int i = 0; i < SDAP_MAX_QOS_FLOWS; i++) {
        if (entity->qos_flow_map[i].active && entity->qos_flow_map[i].qfi == qfi) {
            return entity->qos_flow_map[i].reflective_qos;
        }
    }
    
    return false;
}

uesim_error_t sdap_create_pdu(size_t data_length, sdap_pdu_t** pdu) {
    if (pdu == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    sdap_pdu_t* new_pdu = (sdap_pdu_t*)uesim_calloc(1, sizeof(sdap_pdu_t));
    if (new_pdu == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
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

void sdap_destroy_pdu(sdap_pdu_t* pdu) {
    if (pdu != NULL) {
        if (pdu->data != NULL) {
            uesim_free(pdu->data);
        }
        uesim_free(pdu);
    }
}

uesim_error_t sdap_create_sdu(size_t data_length, sdap_sdu_t** sdu) {
    if (sdu == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    sdap_sdu_t* new_sdu = (sdap_sdu_t*)uesim_calloc(1, sizeof(sdap_sdu_t));
    if (new_sdu == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    if (data_length > 0) {
        new_sdu->data = (uint8_t*)uesim_malloc(data_length);
        if (new_sdu->data == NULL) {
            uesim_free(new_sdu);
            return UESIM_ERROR_MEMORY;
        }
        new_sdu->data_length = data_length;
    }
    
    *sdu = new_sdu;
    return UESIM_SUCCESS;
}

void sdap_destroy_sdu(sdap_sdu_t* sdu) {
    if (sdu != NULL) {
        if (sdu->data != NULL) {
            uesim_free(sdu->data);
        }
        uesim_free(sdu);
    }
}

uesim_error_t sdap_build_data_pdu(sdap_entity_t* entity, const uint8_t* sdu_data,
                                  size_t sdu_length, uint8_t qfi, bool rqi,
                                  sdap_pdu_t** pdu) {
    if (entity == NULL || sdu_data == NULL || pdu == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Allocate PDU with header (1 byte) + SDU */
    size_t pdu_length = 1 + sdu_length;
    uesim_error_t result = sdap_create_pdu(pdu_length, pdu);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    /* Build SDAP header */
    /* D/C = 0 (Data PDU), RQI, QFI */
    (*pdu)->data[0] = (qfi & 0x3F) | ((rqi ? 1 : 0) << 6);
    /* DC bit is 0 for data, so no need to set */
    
    /* Copy SDU data after header */
    memcpy((*pdu)->data + 1, sdu_data, sdu_length);
    
    (*pdu)->qfi = qfi;
    (*pdu)->is_control = false;
    
    return UESIM_SUCCESS;
}

uesim_error_t sdap_parse_data_pdu(sdap_entity_t* entity, const uint8_t* pdu_data,
                                  size_t pdu_length, uint8_t* qfi, bool* rqi,
                                  uint8_t** sdu_data, size_t* sdu_length) {
    if (entity == NULL || pdu_data == NULL || pdu_length < 1) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Parse SDAP header */
    *qfi = pdu_data[0] & 0x3F;
    *rqi = (pdu_data[0] >> 6) & 0x01;
    
    /* Extract SDU */
    *sdu_length = pdu_length - 1;
    if (*sdu_length > 0) {
        *sdu_data = (uint8_t*)uesim_malloc(*sdu_length);
        if (*sdu_data == NULL) {
            return UESIM_ERROR_MEMORY;
        }
        memcpy(*sdu_data, pdu_data + 1, *sdu_length);
    } else {
        *sdu_data = NULL;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t sdap_process_ul_sdu(sdap_entity_t* entity, const uint8_t* data,
                                  size_t length, uint8_t qfi, sdap_pdu_t** pdu) {
    if (entity == NULL || data == NULL || pdu == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (!entity->active) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Get DRB for this QFI */
    int8_t drb_id = sdap_get_drb_for_qos(entity, qfi);
    if (drb_id < 0) {
        entity->stats.mapping_errors++;
        printf("SDAP: No DRB mapping for QFI %u\n", qfi);
        return UESIM_ERROR_NOT_FOUND;
    }
    
    /* Check if reflective QoS indication should be set */
    bool rqi = sdap_is_reflective_qos(entity, qfi);
    
    /* Build SDAP data PDU */
    uesim_error_t result = sdap_build_data_pdu(entity, data, length, qfi, rqi, pdu);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    /* Update statistics */
    if (pthread_mutex_lock(&entity->entity_mutex) == 0) {
        entity->stats.tx_pdus++;
        entity->stats.tx_bytes += length;
        pthread_mutex_unlock(&entity->entity_mutex);
    }
    
    printf("SDAP: UL SDU processed, QFI=%u, DRB=%d, len=%zu\n", qfi, drb_id, length);
    
    return UESIM_SUCCESS;
}

uesim_error_t sdap_process_ul_pdu(sdap_entity_t* entity, const sdap_pdu_t* pdu,
                                  uint8_t* drb_id) {
    if (entity == NULL || pdu == NULL || drb_id == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Get DRB for this QFI */
    int8_t drb = sdap_get_drb_for_qos(entity, pdu->qfi);
    if (drb < 0) {
        return UESIM_ERROR_NOT_FOUND;
    }
    
    *drb_id = (uint8_t)drb;
    return UESIM_SUCCESS;
}

uesim_error_t sdap_process_dl_pdu(sdap_entity_t* entity, const uint8_t* pdu_data,
                                  size_t pdu_length, uint8_t drb_id,
                                  sdap_sdu_t** sdu) {
    if (entity == NULL || pdu_data == NULL || sdu == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pdu_length < 1) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    uint8_t qfi;
    bool rqi;
    uint8_t* sdu_data;
    size_t sdu_length;
    
    /* Parse the PDU */
    uesim_error_t result = sdap_parse_data_pdu(entity, pdu_data, pdu_length,
                                               &qfi, &rqi, &sdu_data, &sdu_length);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    /* Create SDU structure */
    result = sdap_create_sdu(sdu_length, sdu);
    if (result != UESIM_SUCCESS) {
        if (sdu_data != NULL) uesim_free(sdu_data);
        return result;
    }
    
    (*sdu)->data = sdu_data;
    (*sdu)->data_length = sdu_length;
    (*sdu)->qfi = qfi;
    (*sdu)->drb_id = drb_id;
    
    /* Handle reflective QoS indication */
    if (rqi) {
        /* Update QoS flow mapping based on reflective QoS */
        sdap_map_qos_flow(entity, qfi, drb_id, true);
    }
    
    /* Update statistics */
    if (pthread_mutex_lock(&entity->entity_mutex) == 0) {
        entity->stats.rx_pdus++;
        entity->stats.rx_bytes += sdu_length;
        pthread_mutex_unlock(&entity->entity_mutex);
    }
    
    printf("SDAP: DL PDU processed, QFI=%u, DRB=%u, len=%zu\n", qfi, drb_id, sdu_length);
    
    return UESIM_SUCCESS;
}

uesim_error_t sdap_process_dl_sdu(sdap_entity_t* entity, const sdap_sdu_t* sdu) {
    if (entity == NULL || sdu == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Validate SDU data */
    if (sdu->data == NULL && sdu->data_length > 0) {
        fprintf(stderr, "SDAP: Invalid SDU - NULL data with non-zero length\n");
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Check entity is active */
    if (!entity->active) {
        fprintf(stderr, "SDAP: Entity not active, cannot process DL SDU\n");
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Validate QFI (0-63 per 3GPP TS 37.324) */
    if (sdu->qfi > 63) {
        fprintf(stderr, "SDAP: Invalid QFI %u (must be 0-63)\n", sdu->qfi);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Get QoS flow mapping for this QFI */
    int8_t drb_id = sdap_get_drb_for_qos(entity, sdu->qfi);
    if (drb_id < 0) {
        fprintf(stderr, "SDAP: No DRB mapping found for QFI %u\n", sdu->qfi);
        entity->stats.mapping_errors++;
        return UESIM_ERROR_NOT_FOUND;
    }
    
    /* Update statistics */
    if (pthread_mutex_lock(&entity->entity_mutex) == 0) {
        entity->stats.rx_pdus++;
        entity->stats.rx_bytes += sdu->data_length;
        pthread_mutex_unlock(&entity->entity_mutex);
    }
    
    /* Process the SDU based on QoS flow configuration */
    /* In a real implementation, this would:
     * 1. Check the QoS profile for this QFI
     * 2. Apply any required QoS marking (DSCP, etc.)
     * 3. Pass to the IP stack via TUN/TAP device or socket
     * 4. Handle reflective QoS if RQI is set
     */
    
    /* Extract IP packet from SDU */
    if (sdu->data_length > 0 && sdu->data != NULL) {
        /* Basic IP packet validation */
        uint8_t ip_version = 0;
        if (sdu->data_length >= 1) {
            ip_version = (sdu->data[0] >> 4) & 0x0F;
        }
        
        /* Validate IP version (4 for IPv4, 6 for IPv6) */
        if (ip_version == 4) {
            /* IPv4 packet - minimum header is 20 bytes */
            if (sdu->data_length < 20) {
                fprintf(stderr, "SDAP: Truncated IPv4 packet, len=%zu\n", sdu->data_length);
                return UESIM_ERROR_PROTOCOL;
            }
            
            /* Extract IPv4 header fields for logging/processing */
            uint8_t ihl = sdu->data[0] & 0x0F;  /* Internet Header Length */
            uint8_t tos = sdu->data[1];          /* Type of Service (DSCP + ECN) */
            uint16_t total_len = (sdu->data[2] << 8) | sdu->data[3];
            uint8_t protocol = sdu->data[9];     /* Protocol (TCP=6, UDP=17, etc.) */
            uint32_t src_ip = (sdu->data[12] << 24) | (sdu->data[13] << 16) | 
                              (sdu->data[14] << 8) | sdu->data[15];
            uint32_t dst_ip = (sdu->data[16] << 24) | (sdu->data[17] << 16) | 
                              (sdu->data[18] << 8) | sdu->data[19];
            
            /* Map QFI to DSCP (simplified) */
            uint8_t dscp = sdu->qfi;  /* Use QFI as DSCP for simplicity */
            
            printf("SDAP: DL IPv4 packet - QFI=%u, DRB=%d, len=%zu, proto=%u, "
                   "src=%u.%u.%u.%u, dst=%u.%u.%u.%u, DSCP=%u\n",
                   sdu->qfi, drb_id, sdu->data_length, protocol,
                   (src_ip >> 24) & 0xFF, (src_ip >> 16) & 0xFF,
                   (src_ip >> 8) & 0xFF, src_ip & 0xFF,
                   (dst_ip >> 24) & 0xFF, (dst_ip >> 16) & 0xFF,
                   (dst_ip >> 8) & 0xFF, dst_ip & 0xFF, dscp);
            
            /* In real implementation: write to TUN device or pass to IP stack */
            
        } else if (ip_version == 6) {
            /* IPv6 packet - minimum header is 40 bytes */
            if (sdu->data_length < 40) {
                fprintf(stderr, "SDAP: Truncated IPv6 packet, len=%zu\n", sdu->data_length);
                return UESIM_ERROR_PROTOCOL;
            }
            
            /* Extract IPv6 header fields */
            uint16_t payload_len = (sdu->data[4] << 8) | sdu->data[5];
            uint8_t next_header = sdu->data[6];
            uint8_t hop_limit = sdu->data[7];
            
            /* Traffic class (includes DSCP) */
            uint32_t flow_label = ((sdu->data[0] & 0x0F) << 16) | 
                                  (sdu->data[1] << 8) | sdu->data[2];
            uint8_t traffic_class = (sdu->data[0] << 4) | ((sdu->data[1] >> 4) & 0x0F);
            
            printf("SDAP: DL IPv6 packet - QFI=%u, DRB=%d, len=%zu, next_hdr=%u, "
                   "hop_limit=%u, flow=0x%05X, traffic_class=0x%02X\n",
                   sdu->qfi, drb_id, sdu->data_length, next_header, 
                   hop_limit, flow_label, traffic_class);
            
            /* In real implementation: write to TUN device or pass to IP stack */
            
        } else {
            fprintf(stderr, "SDAP: Unknown IP version %u, treating as raw data\n", ip_version);
            printf("SDAP: DL raw data - QFI=%u, DRB=%d, len=%zu\n",
                   sdu->qfi, drb_id, sdu->data_length);
        }
    }
    
    /* Handle reflective QoS indication if set */
    for (int i = 0; i < SDAP_MAX_QOS_FLOWS; i++) {
        if (entity->qos_flow_map[i].active && 
            entity->qos_flow_map[i].qfi == sdu->qfi &&
            entity->qos_flow_map[i].reflective_qos) {
            /* Update UL QoS mapping based on DL mapping */
            printf("SDAP: Reflective QoS applied for QFI %u -> DRB %d\n",
                   sdu->qfi, sdu->drb_id);
            break;
        }
    }
    
    printf("SDAP: DL SDU delivered to IP stack, QFI=%u, DRB=%d, len=%zu\n",
           sdu->qfi, drb_id, sdu->data_length);
    
    return UESIM_SUCCESS;
}

uesim_error_t sdap_send_end_marker(sdap_entity_t* entity, uint8_t qfi,
                                   sdap_pdu_t** pdu) {
    if (entity == NULL || pdu == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Create control PDU (2 bytes: type + QFI) */
    uesim_error_t result = sdap_create_pdu(2, pdu);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    /* Build end marker PDU */
    (*pdu)->data[0] = 0x80 | SDAP_CTRL_END_MARKER; /* D/C=1, PDU type */
    (*pdu)->data[1] = qfi;
    (*pdu)->is_control = true;
    
    entity->stats.tx_control_pdus++;
    
    printf("SDAP: End marker sent for QFI %u\n", qfi);
    return UESIM_SUCCESS;
}

uesim_error_t sdap_send_reflective_qos(sdap_entity_t* entity, uint8_t qfi,
                                       uint8_t drb_id, sdap_pdu_t** pdu) {
    if (entity == NULL || pdu == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Create control PDU (3 bytes: type + QFI + DRB ID) */
    uesim_error_t result = sdap_create_pdu(3, pdu);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    /* Build reflective QoS control PDU */
    (*pdu)->data[0] = 0x80 | SDAP_CTRL_REFLECTIVE_QOS;
    (*pdu)->data[1] = qfi;
    (*pdu)->data[2] = drb_id;
    (*pdu)->is_control = true;
    
    entity->stats.tx_control_pdus++;
    
    printf("SDAP: Reflective QoS sent, QFI=%u -> DRB=%u\n", qfi, drb_id);
    return UESIM_SUCCESS;
}

uesim_error_t sdap_handle_control_pdu(sdap_entity_t* entity, const uint8_t* data,
                                      size_t length) {
    if (entity == NULL || data == NULL || length < 2) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    uint8_t pdu_type = data[0] & 0x0F;
    
    entity->stats.rx_control_pdus++;
    
    switch (pdu_type) {
        case SDAP_CTRL_END_MARKER:
            printf("SDAP: Received end marker for QFI %u\n", data[1]);
            /* End marker handling - flush any buffered data for this QFI */
            break;
            
        case SDAP_CTRL_REFLECTIVE_QOS:
            if (length >= 3) {
                uint8_t qfi = data[1];
                uint8_t drb_id = data[2];
                printf("SDAP: Received reflective QoS mapping, QFI=%u -> DRB=%u\n", qfi, drb_id);
                /* Apply reflective QoS mapping */
                sdap_map_qos_flow(entity, qfi, drb_id, true);
            }
            break;
            
        default:
            printf("SDAP: Unknown control PDU type %u\n", pdu_type);
            return UESIM_ERROR_PROTOCOL;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t sdap_get_stats(sdap_entity_t* entity, sdap_stats_t* stats) {
    if (entity == NULL || stats == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&entity->entity_mutex) == 0) {
        *stats = entity->stats;
        pthread_mutex_unlock(&entity->entity_mutex);
        return UESIM_SUCCESS;
    }
    
    return UESIM_ERROR_THREAD;
}

uesim_error_t sdap_reset_stats(sdap_entity_t* entity) {
    if (entity == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(&entity->entity_mutex) == 0) {
        memset(&entity->stats, 0, sizeof(sdap_stats_t));
        pthread_mutex_unlock(&entity->entity_mutex);
        printf("SDAP: Statistics reset\n");
        return UESIM_SUCCESS;
    }
    
    return UESIM_ERROR_THREAD;
}
