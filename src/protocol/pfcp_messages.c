/*
 * 5G UE Simulation Application
 * PFCP Message Implementation
 * 3GPP TS 29.244
 */

#include "pfcp_messages.h"
#include "../core/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============== Message Type Strings ============== */

static const char* pfcp_message_type_strings[] = {
    [PFCP_MSG_HEARTBEAT_REQUEST] = "HeartbeatRequest",
    [PFCP_MSG_HEARTBEAT_RESPONSE] = "HeartbeatResponse",
    [PFCP_MSG_ASSOCIATION_SETUP_REQUEST] = "AssociationSetupRequest",
    [PFCP_MSG_ASSOCIATION_SETUP_RESPONSE] = "AssociationSetupResponse",
    [PFCP_MSG_ASSOCIATION_UPDATE_REQUEST] = "AssociationUpdateRequest",
    [PFCP_MSG_ASSOCIATION_UPDATE_RESPONSE] = "AssociationUpdateResponse",
    [PFCP_MSG_ASSOCIATION_RELEASE_REQUEST] = "AssociationReleaseRequest",
    [PFCP_MSG_ASSOCIATION_RELEASE_RESPONSE] = "AssociationReleaseResponse",
    [PFCP_MSG_SESSION_ESTABLISHMENT_REQUEST] = "SessionEstablishmentRequest",
    [PFCP_MSG_SESSION_ESTABLISHMENT_RESPONSE] = "SessionEstablishmentResponse",
    [PFCP_MSG_SESSION_MODIFICATION_REQUEST] = "SessionModificationRequest",
    [PFCP_MSG_SESSION_MODIFICATION_RESPONSE] = "SessionModificationResponse",
    [PFCP_MSG_SESSION_DELETION_REQUEST] = "SessionDeletionRequest",
    [PFCP_MSG_SESSION_DELETION_RESPONSE] = "SessionDeletionResponse",
    [PFCP_MSG_SESSION_REPORT_REQUEST] = "SessionReportRequest",
    [PFCP_MSG_SESSION_REPORT_RESPONSE] = "SessionReportResponse"
};

static const char* pfcp_cause_strings[] = {
    [PFCP_CAUSE_REQUEST_ACCEPTED] = "RequestAccepted",
    [PFCP_CAUSE_REQUEST_REJECTED] = "RequestRejected",
    [PFCP_CAUSE_SESSION_CONTEXT_NOT_FOUND] = "SessionContextNotFound",
    [PFCP_CAUSE_MANDATORY_IE_MISSING] = "MandatoryIEMissing",
    [PFCP_CAUSE_MANDATORY_IE_INCORRECT] = "MandatoryIEIncorrect",
    [PFCP_CAUSE_SYSTEM_FAILURE] = "SystemFailure",
    [PFCP_CAUSE_REQUEST_TIMEOUT] = "RequestTimeout",
    [PFCP_CAUSE_NO_ESTABLISHED_PFCP_ASSOCIATION] = "NoEstablishedPFCPAssociation",
    [PFCP_CAUSE_RULE_CREATION_MODIFICATION_FAILURE] = "RuleCreationModificationFailure",
    [PFCP_CAUSE_NO_RESOURCES_AVAILABLE] = "NoResourcesAvailable"
};

/* ============== Utility Functions ============== */

const char* pfcp_message_type_to_string(pfcp_message_type_t type) {
    if (type <= PFCP_MSG_SESSION_REPORT_RESPONSE) {
        return pfcp_message_type_strings[type];
    }
    return "Unknown";
}

const char* pfcp_cause_to_string(pfcp_cause_t cause) {
    if (cause <= PFCP_CAUSE_NO_RESOURCES_AVAILABLE) {
        return pfcp_cause_strings[cause];
    }
    return "Unknown";
}

/* ============== IE Helpers ============== */

void pfcp_set_f_teid(pfcp_f_teid_t* teid, uint32_t teid_val, uint32_t ipv4) {
    if (teid) {
        teid->teid = teid_val;
        teid->ipv4_address = ipv4;
        teid->v4_present = 1;
        teid->v6_present = 0;
        teid->chid = 0;
    }
}

void pfcp_set_ue_ip(pfcp_ue_ip_address_t* ue_ip, uint32_t ipv4) {
    if (ue_ip) {
        ue_ip->ipv4_address = ipv4;
        ue_ip->v4_present = 1;
        ue_ip->v6_present = 0;
        ue_ip->sd = 0;
    }
}

/* ============== Message Initialization Helpers ============== */

void pfcp_init_association_setup_request(pfcp_message_t* msg) {
    if (!msg) return;
    memset(msg, 0, sizeof(pfcp_message_t));
    msg->message_type = PFCP_MSG_ASSOCIATION_SETUP_REQUEST;
    msg->version = 1;
    msg->s = 0;
    msg->seid = 0;
}

void pfcp_init_association_setup_response(pfcp_message_t* msg) {
    if (!msg) return;
    memset(msg, 0, sizeof(pfcp_message_t));
    msg->message_type = PFCP_MSG_ASSOCIATION_SETUP_RESPONSE;
    msg->version = 1;
    msg->s = 0;
    msg->seid = 0;
}

void pfcp_init_session_establishment_request(pfcp_message_t* msg) {
    if (!msg) return;
    memset(msg, 0, sizeof(pfcp_message_t));
    msg->message_type = PFCP_MSG_SESSION_ESTABLISHMENT_REQUEST;
    msg->version = 1;
    msg->s = 1;
}

void pfcp_init_session_establishment_response(pfcp_message_t* msg) {
    if (!msg) return;
    memset(msg, 0, sizeof(pfcp_message_t));
    msg->message_type = PFCP_MSG_SESSION_ESTABLISHMENT_RESPONSE;
    msg->version = 1;
    msg->s = 1;
}

void pfcp_init_session_deletion_request(pfcp_message_t* msg) {
    if (!msg) return;
    memset(msg, 0, sizeof(pfcp_message_t));
    msg->message_type = PFCP_MSG_SESSION_DELETION_REQUEST;
    msg->version = 1;
    msg->s = 1;
}

/* ============== Encoding Helpers ============== */

static void pfcp_encode_header(uint8_t* buf, const pfcp_message_t* msg, size_t body_len) {
    buf[0] = (msg->version << 5) | (msg->mp << 4) | (msg->s << 3) | 0;
    buf[1] = msg->message_type;
    buf[2] = (body_len >> 8) & 0xFF;
    buf[3] = body_len & 0xFF;
    
    if (msg->s) {
        buf[4] = (msg->seid >> 56) & 0xFF;
        buf[5] = (msg->seid >> 48) & 0xFF;
        buf[6] = (msg->seid >> 40) & 0xFF;
        buf[7] = (msg->seid >> 32) & 0xFF;
        buf[8] = (msg->seid >> 24) & 0xFF;
        buf[9] = (msg->seid >> 16) & 0xFF;
        buf[10] = (msg->seid >> 8) & 0xFF;
        buf[11] = msg->seid & 0xFF;
        buf[12] = (msg->sequence_number >> 24) & 0xFF;
        buf[13] = (msg->sequence_number >> 16) & 0xFF;
        buf[14] = (msg->sequence_number >> 8) & 0xFF;
    } else {
        buf[4] = (msg->sequence_number >> 24) & 0xFF;
        buf[5] = (msg->sequence_number >> 16) & 0xFF;
        buf[6] = (msg->sequence_number >> 8) & 0xFF;
    }
}

static size_t pfcp_encode_ie_header(uint8_t* buf, uint16_t type, uint16_t len) {
    buf[0] = (type >> 8) & 0xFF;
    buf[1] = type & 0xFF;
    buf[2] = (len >> 8) & 0xFF;
    buf[3] = len & 0xFF;
    return 4;
}

static size_t pfcp_encode_node_id(uint8_t* buf, const pfcp_node_id_t* node_id) {
    size_t offset = 0;
    
    if (node_id->node_id_type == 0) {
        /* IPv4 */
        offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_NODE_ID, 5);
        buf[offset++] = 0;  /* IPv4 flag */
        buf[offset++] = (node_id->ipv4_address >> 24) & 0xFF;
        buf[offset++] = (node_id->ipv4_address >> 16) & 0xFF;
        buf[offset++] = (node_id->ipv4_address >> 8) & 0xFF;
        buf[offset++] = node_id->ipv4_address & 0xFF;
    }
    
    return offset;
}

static size_t pfcp_encode_f_seid(uint8_t* buf, const pfcp_f_seid_t* f_seid) {
    size_t offset = 0;
    uint16_t len = 9;  /* SEID (8) + flags (1) */
    
    offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_F_SEID, len);
    
    buf[offset++] = f_seid->v4_present | (f_seid->v6_present << 1);
    buf[offset++] = (f_seid->seid >> 56) & 0xFF;
    buf[offset++] = (f_seid->seid >> 48) & 0xFF;
    buf[offset++] = (f_seid->seid >> 40) & 0xFF;
    buf[offset++] = (f_seid->seid >> 32) & 0xFF;
    buf[offset++] = (f_seid->seid >> 24) & 0xFF;
    buf[offset++] = (f_seid->seid >> 16) & 0xFF;
    buf[offset++] = (f_seid->seid >> 8) & 0xFF;
    buf[offset++] = f_seid->seid & 0xFF;
    
    if (f_seid->v4_present) {
        buf[offset++] = (f_seid->ipv4_address >> 24) & 0xFF;
        buf[offset++] = (f_seid->ipv4_address >> 16) & 0xFF;
        buf[offset++] = (f_seid->ipv4_address >> 8) & 0xFF;
        buf[offset++] = f_seid->ipv4_address & 0xFF;
        buf[2] = 13;  /* Update length */
    }
    
    return offset;
}

static size_t pfcp_encode_f_teid(uint8_t* buf, const pfcp_f_teid_t* teid) {
    size_t offset = 0;
    uint16_t len = 5;
    
    if (teid->v4_present) len += 4;
    if (teid->chid) len += 1;
    
    offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_F_TEID, len);
    
    uint8_t flags = (teid->v4_present) | (teid->v6_present << 1) | (teid->chid << 2);
    buf[offset++] = flags;
    
    buf[offset++] = (teid->teid >> 24) & 0xFF;
    buf[offset++] = (teid->teid >> 16) & 0xFF;
    buf[offset++] = (teid->teid >> 8) & 0xFF;
    buf[offset++] = teid->teid & 0xFF;
    
    if (teid->v4_present) {
        buf[offset++] = (teid->ipv4_address >> 24) & 0xFF;
        buf[offset++] = (teid->ipv4_address >> 16) & 0xFF;
        buf[offset++] = (teid->ipv4_address >> 8) & 0xFF;
        buf[offset++] = teid->ipv4_address & 0xFF;
    }
    
    return offset;
}

static size_t pfcp_encode_cause(uint8_t* buf, pfcp_cause_t cause) {
    size_t offset = 0;
    offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_CAUSE, 1);
    buf[offset++] = cause;
    return offset;
}

static size_t pfcp_encode_pdi(uint8_t* buf, const pfcp_pdi_t* pdi) {
    size_t offset = 0;
    size_t pdi_content_len = 0;
    
    /* Calculate content length */
    pdi_content_len = 2;  /* Source interface */
    if (pdi->f_teid_present) {
        pdi_content_len += 4 + 9;  /* IE header + F-TEID (IPv4) */
    }
    if (pdi->ue_ip_present) {
        pdi_content_len += 4 + 5;  /* IE header + UE IP */
    }
    
    offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_PDI, pdi_content_len);
    
    /* Source Interface IE */
    offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_SOURCE_INTERFACE, 1);
    buf[offset++] = pdi->source_interface;
    
    /* F-TEID IE */
    if (pdi->f_teid_present) {
        offset += pfcp_encode_f_teid(buf + offset, &pdi->f_teid);
    }
    
    /* UE IP Address IE */
    if (pdi->ue_ip_present) {
        offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_UE_IP_ADDRESS, 5);
        buf[offset++] = pdi->ue_ip.sd | (pdi->ue_ip.v4_present << 2);
        buf[offset++] = (pdi->ue_ip.ipv4_address >> 24) & 0xFF;
        buf[offset++] = (pdi->ue_ip.ipv4_address >> 16) & 0xFF;
        buf[offset++] = (pdi->ue_ip.ipv4_address >> 8) & 0xFF;
        buf[offset++] = pdi->ue_ip.ipv4_address & 0xFF;
    }
    
    return offset;
}

static size_t pfcp_encode_create_pdr(uint8_t* buf, const pfcp_pdr_t* pdr) {
    size_t offset = 0;
    size_t content_len = 0;
    
    /* Calculate content length */
    content_len = 4 + 2;  /* PDR ID IE header + value */
    content_len += 4 + 1;  /* Precedence IE header + value */
    content_len += 4 + 2;  /* PDI IE header minimum */
    
    size_t pdi_start = offset;
    offset += 4;  /* Skip IE header */
    offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_PDR_ID, 2);
    buf[offset++] = (pdr->pdr_id >> 8) & 0xFF;
    buf[offset++] = pdr->pdr_id & 0xFF;
    
    offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_PRIORITY, 1);
    buf[offset++] = pdr->precedence;
    
    /* PDI */
    size_t pdi_len_offset = offset;
    offset += 4;  /* PDI header placeholder */
    offset += pfcp_encode_pdi(buf + offset, &pdr->pdi);
    
    /* FAR ID */
    if (pdr->far_id_present) {
        offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_FAR_ID, 4);
        buf[offset++] = (pdr->far_id >> 24) & 0xFF;
        buf[offset++] = (pdr->far_id >> 16) & 0xFF;
        buf[offset++] = (pdr->far_id >> 8) & 0xFF;
        buf[offset++] = pdr->far_id & 0xFF;
    }
    
    /* Update PDI length */
    size_t pdi_content_len = offset - pdi_len_offset - 4;
    buf[pdi_len_offset] = PFCP_IE_PDI >> 8;
    buf[pdi_len_offset + 1] = PFCP_IE_PDI & 0xFF;
    buf[pdi_len_offset + 2] = (pdi_content_len >> 8) & 0xFF;
    buf[pdi_len_offset + 3] = pdi_content_len & 0xFF;
    
    /* Update Create PDR length */
    content_len = offset - pdi_start - 4;
    buf[pdi_start] = PFCP_IE_CREATE_PDR >> 8;
    buf[pdi_start + 1] = PFCP_IE_CREATE_PDR & 0xFF;
    buf[pdi_start + 2] = (content_len >> 8) & 0xFF;
    buf[pdi_start + 3] = content_len & 0xFF;
    
    return offset;
}

static size_t pfcp_encode_create_far(uint8_t* buf, const pfcp_far_t* far_data) {
    size_t offset = 0;
    size_t content_len = 4 + 4;  /* FAR ID + Apply Action */
    
    if (far_data->forwarding_params_present) {
        content_len += 4 + 1;  /* Destination Interface */
        if (far_data->outer_header_creation_present) {
            content_len += 4 + 9;  /* Outer Header Creation */
        }
    }
    
    offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_CREATE_FAR, content_len);
    
    /* FAR ID */
    offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_FAR_ID, 4);
    buf[offset++] = (far_data->far_id >> 24) & 0xFF;
    buf[offset++] = (far_data->far_id >> 16) & 0xFF;
    buf[offset++] = (far_data->far_id >> 8) & 0xFF;
    buf[offset++] = far_data->far_id & 0xFF;
    
    /* Apply Action */
    offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_GATE_STATUS, 1);
    buf[offset++] = far_data->apply_action;
    
    /* Forwarding Parameters */
    if (far_data->forwarding_params_present) {
        offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_DESTINATION_INTERFACE, 1);
        buf[offset++] = far_data->destination_interface;
        
        if (far_data->outer_header_creation_present) {
            offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_OUTER_HEADER_CREATION, 9);
            buf[offset++] = 0x01;  /* GTP-U/IPv4 */
            buf[offset++] = (far_data->outer_header_creation.teid >> 24) & 0xFF;
            buf[offset++] = (far_data->outer_header_creation.teid >> 16) & 0xFF;
            buf[offset++] = (far_data->outer_header_creation.teid >> 8) & 0xFF;
            buf[offset++] = far_data->outer_header_creation.teid & 0xFF;
            buf[offset++] = (far_data->outer_header_creation.ipv4_address >> 24) & 0xFF;
            buf[offset++] = (far_data->outer_header_creation.ipv4_address >> 16) & 0xFF;
            buf[offset++] = (far_data->outer_header_creation.ipv4_address >> 8) & 0xFF;
            buf[offset++] = far_data->outer_header_creation.ipv4_address & 0xFF;
        }
    }
    
    return offset;
}

/* ============== QER Encoding Functions ============== */

static size_t pfcp_encode_create_qer(uint8_t* buf, const pfcp_qer_t* qer) {
    if (!buf || !qer) return 0;
    
    size_t offset = 0;
    size_t content_len = 4 + 4;  /* QER ID + Gate Status (2x1 byte) */
    
    /* Calculate content length */
    content_len += 4 + 8 + 8;  /* MBR (UL + DL) */
    content_len += 4 + 8 + 8;  /* GBR (UL + DL) */
    if (qer->qfi_present) content_len += 4 + 1;
    if (qer->qer_correlation_id_present) content_len += 4 + 4;
    
    offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_CREATE_QER, content_len);
    
    /* QER ID */
    offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_QER_ID, 4);
    buf[offset++] = (qer->qer_id >> 24) & 0xFF;
    buf[offset++] = (qer->qer_id >> 16) & 0xFF;
    buf[offset++] = (qer->qer_id >> 8) & 0xFF;
    buf[offset++] = qer->qer_id & 0xFF;
    
    /* Gate Status (UL + DL in single byte) */
    offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_GATE_STATUS, 1);
    buf[offset++] = (qer->ul_gate_status << 4) | (qer->dl_gate_status & 0x0F);
    
    /* MBR - Uplink */
    offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_MBR, 8);
    for (int i = 7; i >= 0; i--) {
        buf[offset++] = (qer->ul_mbr >> (i * 8)) & 0xFF;
    }
    
    /* MBR - Downlink */
    offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_MBR, 8);
    for (int i = 7; i >= 0; i--) {
        buf[offset++] = (qer->dl_mbr >> (i * 8)) & 0xFF;
    }
    
    /* GBR - Uplink */
    offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_GBR, 8);
    for (int i = 7; i >= 0; i--) {
        buf[offset++] = (qer->ul_gbr >> (i * 8)) & 0xFF;
    }
    
    /* GBR - Downlink */
    offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_GBR, 8);
    for (int i = 7; i >= 0; i--) {
        buf[offset++] = (qer->dl_gbr >> (i * 8)) & 0xFF;
    }
    
    /* QFI (QoS Flow Identifier) */
    if (qer->qfi_present) {
        offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_QER_CORRELATION_ID, 1);
        buf[offset++] = qer->qfi;
    }
    
    /* QER Correlation ID */
    if (qer->qer_correlation_id_present) {
        offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_QER_CORRELATION_ID, 4);
        buf[offset++] = (qer->qer_correlation_id >> 24) & 0xFF;
        buf[offset++] = (qer->qer_correlation_id >> 16) & 0xFF;
        buf[offset++] = (qer->qer_correlation_id >> 8) & 0xFF;
        buf[offset++] = qer->qer_correlation_id & 0xFF;
    }
    
    return offset;
}

static size_t pfcp_encode_update_qer(uint8_t* buf, const pfcp_qer_t* qer) {
    /* Update QER has same structure as Create QER */
    return pfcp_encode_create_qer(buf, qer);
}

static size_t pfcp_encode_remove_qer(uint8_t* buf, uint32_t qer_id) {
    if (!buf) return 0;
    
    size_t offset = 0;
    offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_REMOVE_QER, 4);
    buf[offset++] = (qer_id >> 24) & 0xFF;
    buf[offset++] = (qer_id >> 16) & 0xFF;
    buf[offset++] = (qer_id >> 8) & 0xFF;
    buf[offset++] = qer_id & 0xFF;
    
    return offset;
}

/* ============== URR Encoding Functions ============== */

static size_t pfcp_encode_create_urr(uint8_t* buf, const pfcp_urr_t* urr) {
    if (!buf || !urr) return 0;
    
    size_t offset = 0;
    size_t content_len = 4 + 4;  /* URR ID + Measurement Method */
    
    /* Calculate content length */
    content_len += 4 + 1;  /* Reporting Triggers */
    if (urr->volume_threshold_present) content_len += 4 + 24;  /* Total + UL + DL */
    if (urr->time_threshold_present) content_len += 4 + 4;
    if (urr->volume_quota_present) content_len += 4 + 8;
    if (urr->time_quota_present) content_len += 4 + 4;
    if (urr->quota_validity_time_present) content_len += 4 + 4;
    
    offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_CREATE_URR, content_len);
    
    /* URR ID */
    offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_URR_ID, 4);
    buf[offset++] = (urr->urr_id >> 24) & 0xFF;
    buf[offset++] = (urr->urr_id >> 16) & 0xFF;
    buf[offset++] = (urr->urr_id >> 8) & 0xFF;
    buf[offset++] = urr->urr_id & 0xFF;
    
    /* Measurement Method */
    offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_MEASUREMENT_METHOD, 1);
    buf[offset++] = urr->measurement_method;
    
    /* Reporting Triggers */
    offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_REPORTING_TRIGGERS, 1);
    buf[offset++] = urr->reporting_triggers;
    
    /* Volume Threshold */
    if (urr->volume_threshold_present) {
        offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_VOLUME_THRESHOLD, 24);
        /* Total Volume */
        for (int i = 7; i >= 0; i--) {
            buf[offset++] = (urr->total_volume_threshold >> (i * 8)) & 0xFF;
        }
        /* Uplink Volume */
        for (int i = 7; i >= 0; i--) {
            buf[offset++] = (urr->uplink_volume_threshold >> (i * 8)) & 0xFF;
        }
        /* Downlink Volume */
        for (int i = 7; i >= 0; i--) {
            buf[offset++] = (urr->downlink_volume_threshold >> (i * 8)) & 0xFF;
        }
    }
    
    /* Time Threshold */
    if (urr->time_threshold_present) {
        offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_TIME_THRESHOLD, 4);
        buf[offset++] = (urr->time_threshold >> 24) & 0xFF;
        buf[offset++] = (urr->time_threshold >> 16) & 0xFF;
        buf[offset++] = (urr->time_threshold >> 8) & 0xFF;
        buf[offset++] = urr->time_threshold & 0xFF;
    }
    
    /* Volume Quota */
    if (urr->volume_quota_present) {
        offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_VOLUME_QUOTA, 8);
        for (int i = 7; i >= 0; i--) {
            buf[offset++] = (urr->total_volume_quota >> (i * 8)) & 0xFF;
        }
    }
    
    /* Time Quota */
    if (urr->time_quota_present) {
        offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_TIME_QUOTA, 4);
        buf[offset++] = (urr->time_quota >> 24) & 0xFF;
        buf[offset++] = (urr->time_quota >> 16) & 0xFF;
        buf[offset++] = (urr->time_quota >> 8) & 0xFF;
        buf[offset++] = urr->time_quota & 0xFF;
    }
    
    /* Quota Validity Time */
    if (urr->quota_validity_time_present) {
        offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_QUOTA_VALIDITY_TIME, 4);
        buf[offset++] = (urr->quota_validity_time >> 24) & 0xFF;
        buf[offset++] = (urr->quota_validity_time >> 16) & 0xFF;
        buf[offset++] = (urr->quota_validity_time >> 8) & 0xFF;
        buf[offset++] = urr->quota_validity_time & 0xFF;
    }
    
    return offset;
}

static size_t pfcp_encode_update_urr(uint8_t* buf, const pfcp_urr_t* urr) {
    /* Update URR has same structure as Create URR */
    return pfcp_encode_create_urr(buf, urr);
}

static size_t pfcp_encode_remove_urr(uint8_t* buf, uint32_t urr_id) {
    if (!buf) return 0;
    
    size_t offset = 0;
    offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_REMOVE_URR, 4);
    buf[offset++] = (urr_id >> 24) & 0xFF;
    buf[offset++] = (urr_id >> 16) & 0xFF;
    buf[offset++] = (urr_id >> 8) & 0xFF;
    buf[offset++] = urr_id & 0xFF;
    
    return offset;
}

static size_t pfcp_encode_volume_measurement(uint8_t* buf, const pfcp_volume_measurement_t* vol) {
    if (!buf || !vol) return 0;
    
    size_t offset = 0;
    size_t content_len = 0;
    
    if (vol->total_present) content_len += 8;
    if (vol->uplink_present) content_len += 8;
    if (vol->downlink_present) content_len += 8;
    
    if (content_len == 0) return 0;
    
    offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_VOLUME_MEASUREMENT, content_len);
    
    if (vol->total_present) {
        for (int i = 7; i >= 0; i--) {
            buf[offset++] = (vol->total_volume >> (i * 8)) & 0xFF;
        }
    }
    if (vol->uplink_present) {
        for (int i = 7; i >= 0; i--) {
            buf[offset++] = (vol->uplink_volume >> (i * 8)) & 0xFF;
        }
    }
    if (vol->downlink_present) {
        for (int i = 7; i >= 0; i--) {
            buf[offset++] = (vol->downlink_volume >> (i * 8)) & 0xFF;
        }
    }
    
    return offset;
}

static size_t pfcp_encode_duration_measurement(uint8_t* buf, uint32_t duration) {
    if (!buf) return 0;
    
    size_t offset = 0;
    offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_DURATION_MEASUREMENT, 4);
    buf[offset++] = (duration >> 24) & 0xFF;
    buf[offset++] = (duration >> 16) & 0xFF;
    buf[offset++] = (duration >> 8) & 0xFF;
    buf[offset++] = duration & 0xFF;
    
    return offset;
}

/* ============== Message Encoding ============== */

int pfcp_encode_message(const pfcp_message_t* msg, uint8_t** buffer, size_t* length) {
    if (!msg || !buffer || !length) {
        return -1;
    }
    
    uint8_t* buf = malloc(PFCP_MAX_MESSAGE_SIZE);
    if (!buf) {
        return -1;
    }
    
    size_t offset = 0;
    size_t header_len = msg->s ? 16 : 8;
    offset = header_len;
    
    switch (msg->message_type) {
        case PFCP_MSG_HEARTBEAT_REQUEST:
        case PFCP_MSG_HEARTBEAT_RESPONSE: {
            offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_RECOVERY_TIME_STAMP, 4);
            uint64_t ts = msg->payload.heartbeat.recovery_time_stamp;
            buf[offset++] = (ts >> 24) & 0xFF;
            buf[offset++] = (ts >> 16) & 0xFF;
            buf[offset++] = (ts >> 8) & 0xFF;
            buf[offset++] = ts & 0xFF;
            break;
        }
        
        case PFCP_MSG_ASSOCIATION_SETUP_REQUEST: {
            const pfcp_association_setup_request_t* req = &msg->payload.association_setup_request;
            offset += pfcp_encode_node_id(buf + offset, &req->node_id);
            
            /* Recovery Time Stamp */
            offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_RECOVERY_TIME_STAMP, 4);
            buf[offset++] = (req->recovery_time_stamp >> 24) & 0xFF;
            buf[offset++] = (req->recovery_time_stamp >> 16) & 0xFF;
            buf[offset++] = (req->recovery_time_stamp >> 8) & 0xFF;
            buf[offset++] = req->recovery_time_stamp & 0xFF;
            
            /* CP Function Features */
            offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_CP_FUNCTION_FEATURES, 1);
            buf[offset++] = req->cp_function_features;
            break;
        }
        
        case PFCP_MSG_ASSOCIATION_SETUP_RESPONSE: {
            const pfcp_association_setup_response_t* resp = &msg->payload.association_setup_response;
            offset += pfcp_encode_node_id(buf + offset, &resp->node_id);
            offset += pfcp_encode_cause(buf + offset, resp->cause);
            
            /* Recovery Time Stamp */
            offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_RECOVERY_TIME_STAMP, 4);
            buf[offset++] = (resp->recovery_time_stamp >> 24) & 0xFF;
            buf[offset++] = (resp->recovery_time_stamp >> 16) & 0xFF;
            buf[offset++] = (resp->recovery_time_stamp >> 8) & 0xFF;
            buf[offset++] = resp->recovery_time_stamp & 0xFF;
            
            if (resp->up_function_features_present) {
                offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_UP_FUNCTION_FEATURES, 1);
                buf[offset++] = resp->up_function_features;
            }
            break;
        }
        
        case PFCP_MSG_SESSION_ESTABLISHMENT_REQUEST: {
            const pfcp_session_establishment_request_t* req = &msg->payload.session_est_request;
            offset += pfcp_encode_node_id(buf + offset, &req->cp_node_id);
            offset += pfcp_encode_f_seid(buf + offset, &req->cp_f_seid);
            
            /* Create PDR */
            for (int i = 0; i < req->num_create_pdr && i < PFCP_MAX_PDR_PER_SESSION; i++) {
                offset += pfcp_encode_create_pdr(buf + offset, &req->create_pdr[i]);
            }
            
            /* Create FAR */
            for (int i = 0; i < req->num_create_far && i < PFCP_MAX_FAR_PER_SESSION; i++) {
                offset += pfcp_encode_create_far(buf + offset, &req->create_far[i]);
            }
            
            /* Create QER */
            for (int i = 0; i < req->num_create_qer && i < PFCP_MAX_QER_PER_SESSION; i++) {
                offset += pfcp_encode_create_qer(buf + offset, &req->create_qer[i]);
            }
            
            /* Create URR */
            for (int i = 0; i < req->num_create_urr && i < PFCP_MAX_URR_PER_SESSION; i++) {
                offset += pfcp_encode_create_urr(buf + offset, &req->create_urr[i]);
            }
            
            /* APN/DNN */
            if (req->apn_dnn_len > 0) {
                offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_APN_DNN, req->apn_dnn_len);
                memcpy(buf + offset, req->apn_dnn, req->apn_dnn_len);
                offset += req->apn_dnn_len;
            }
            
            /* S-NSSAI (Network Slicing) */
            if (req->snssai_present) {
                /* Encode S-NSSAI: SST (1 byte) + SD (3 bytes if present) */
                uint8_t s_nssai_len = 1;
                if (req->sd != 0) s_nssai_len = 4;
                offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_SDF_FILTER, s_nssai_len);
                buf[offset++] = req->sst;
                if (s_nssai_len == 4) {
                    buf[offset++] = (req->sd >> 16) & 0xFF;
                    buf[offset++] = (req->sd >> 8) & 0xFF;
                    buf[offset++] = req->sd & 0xFF;
                }
            }
            break;
        }
        
        case PFCP_MSG_SESSION_ESTABLISHMENT_RESPONSE: {
            const pfcp_session_establishment_response_t* resp = &msg->payload.session_est_response;
            offset += pfcp_encode_node_id(buf + offset, &resp->up_node_id);
            offset += pfcp_encode_cause(buf + offset, resp->cause);
            
            if (resp->up_f_seid_present) {
                offset += pfcp_encode_f_seid(buf + offset, &resp->up_f_seid);
            }
            
            /* Created PDR */
            for (int i = 0; i < resp->num_created_pdr && i < PFCP_MAX_PDR_PER_SESSION; i++) {
                offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_CREATED_PDR, 7);
                buf[offset++] = (resp->created_pdr_ids[i] >> 8) & 0xFF;
                buf[offset++] = resp->created_pdr_ids[i] & 0xFF;
                offset += pfcp_encode_f_teid(buf + offset, &resp->created_pdr_teids[i]);
            }
            break;
        }
        
        case PFCP_MSG_SESSION_DELETION_REQUEST: {
            /* Only SEID in header */
            break;
        }
        
        case PFCP_MSG_SESSION_DELETION_RESPONSE: {
            const pfcp_session_deletion_response_t* resp = &msg->payload.session_del_response;
            offset += pfcp_encode_cause(buf + offset, resp->cause);
            break;
        }
        
        case PFCP_MSG_SESSION_REPORT_REQUEST: {
            const pfcp_session_report_request_t* req = &msg->payload.session_report_request;
            offset += pfcp_encode_f_seid(buf + offset, &req->up_f_seid);
            
            /* Usage Report */
            for (int i = 0; i < req->num_usage_reports && i < PFCP_MAX_URR_PER_SESSION; i++) {
                const pfcp_usage_report_t* ur = &req->usage_reports[i];
                size_t report_len = 4 + 4 + 4 + 4;  /* URR ID + start time + end time + duration */
                offset += pfcp_encode_ie_header(buf + offset, 73, report_len);  /* Usage Report IE */
                
                /* URR ID */
                offset += pfcp_encode_ie_header(buf + offset, PFCP_IE_URR_ID, 4);
                buf[offset++] = (ur->urr_id >> 24) & 0xFF;
                buf[offset++] = (ur->urr_id >> 16) & 0xFF;
                buf[offset++] = (ur->urr_id >> 8) & 0xFF;
                buf[offset++] = ur->urr_id & 0xFF;
            }
            break;
        }
        
        case PFCP_MSG_SESSION_REPORT_RESPONSE: {
            const pfcp_session_report_response_t* resp = &msg->payload.session_report_response;
            offset += pfcp_encode_cause(buf + offset, resp->cause);
            break;
        }
        
        default:
            free(buf);
            return -1;
    }
    
    /* Encode header with correct length */
    size_t body_len = offset - header_len;
    pfcp_encode_header(buf, msg, body_len);
    
    *buffer = buf;
    *length = offset;
    
    return 0;
}

/* ============== Decoding Helpers ============== */

static uint16_t pfcp_decode_uint16(const uint8_t* buf) {
    return (buf[0] << 8) | buf[1];
}

static uint32_t pfcp_decode_uint32(const uint8_t* buf) {
    return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

static uint64_t pfcp_decode_uint64(const uint8_t* buf) {
    return ((uint64_t)buf[0] << 56) | ((uint64_t)buf[1] << 48) |
           ((uint64_t)buf[2] << 40) | ((uint64_t)buf[3] << 32) |
           ((uint64_t)buf[4] << 24) | ((uint64_t)buf[5] << 16) |
           ((uint64_t)buf[6] << 8) | buf[7];
}

static void pfcp_decode_node_id(const uint8_t* buf, size_t len, pfcp_node_id_t* node_id) {
    if (len < 1) return;
    
    node_id->node_id_type = buf[0];
    if (node_id->node_id_type == 0 && len >= 5) {
        node_id->ipv4_address = pfcp_decode_uint32(&buf[1]);
    }
}

static void pfcp_decode_f_seid(const uint8_t* buf, size_t len, pfcp_f_seid_t* f_seid) {
    if (len < 9) return;
    
    f_seid->v4_present = buf[0] & 0x01;
    f_seid->v6_present = (buf[0] >> 1) & 0x01;
    f_seid->seid = pfcp_decode_uint64(&buf[1]);
    
    if (f_seid->v4_present && len >= 13) {
        f_seid->ipv4_address = pfcp_decode_uint32(&buf[9]);
    }
}

static void pfcp_decode_f_teid(const uint8_t* buf, size_t len, pfcp_f_teid_t* teid) {
    if (len < 5) return;
    
    teid->v4_present = buf[0] & 0x01;
    teid->v6_present = (buf[0] >> 1) & 0x01;
    teid->chid = (buf[0] >> 2) & 0x01;
    teid->teid = pfcp_decode_uint32(&buf[1]);
    
    if (teid->v4_present && len >= 9) {
        teid->ipv4_address = pfcp_decode_uint32(&buf[5]);
    }
}

/* ============== Message Decoding ============== */

int pfcp_decode_message(const uint8_t* buffer, size_t length, pfcp_message_t* msg) {
    if (!buffer || !msg || length < 4) {
        return -1;
    }
    
    memset(msg, 0, sizeof(pfcp_message_t));
    
    /* Parse header */
    msg->version = (buffer[0] >> 5) & 0x07;
    msg->mp = (buffer[0] >> 4) & 0x01;
    msg->s = (buffer[0] >> 3) & 0x01;
    msg->message_type = buffer[1];
    msg->message_length = (buffer[2] << 8) | buffer[3];
    
    size_t offset = 4;
    
    if (msg->s) {
        if (length < 16) return -1;
        msg->seid = pfcp_decode_uint64(&buffer[4]);
        msg->sequence_number = (buffer[12] << 16) | (buffer[13] << 8) | buffer[14];
        offset = 16;
    } else {
        if (length < 8) return -1;
        msg->sequence_number = (buffer[4] << 16) | (buffer[5] << 8) | buffer[6];
        offset = 8;
    }
    
    /* Parse IEs */
    while (offset + 4 <= length) {
        uint16_t ie_type = pfcp_decode_uint16(&buffer[offset]);
        uint16_t ie_len = pfcp_decode_uint16(&buffer[offset + 2]);
        offset += 4;
        
        if (offset + ie_len > length) break;
        
        switch (ie_type) {
            case PFCP_IE_NODE_ID:
                pfcp_decode_node_id(&buffer[offset], ie_len, 
                    &msg->payload.association_setup_request.node_id);
                break;
                
            case PFCP_IE_F_SEID:
                pfcp_decode_f_seid(&buffer[offset], ie_len,
                    &msg->payload.session_est_request.cp_f_seid);
                break;
                
            case PFCP_IE_CAUSE:
                if (ie_len >= 1) {
                    msg->payload.session_est_response.cause = buffer[offset];
                }
                break;
                
            case PFCP_IE_RECOVERY_TIME_STAMP:
                if (ie_len >= 4) {
                    msg->payload.heartbeat.recovery_time_stamp = pfcp_decode_uint32(&buffer[offset]);
                }
                break;
                
            case PFCP_IE_CP_FUNCTION_FEATURES:
                if (ie_len >= 1) {
                    msg->payload.association_setup_request.cp_function_features = buffer[offset];
                }
                break;
                
            case PFCP_IE_UP_FUNCTION_FEATURES:
                if (ie_len >= 1) {
                    msg->payload.association_setup_response.up_function_features = buffer[offset];
                    msg->payload.association_setup_response.up_function_features_present = 1;
                }
                break;
                
            default:
                break;
        }
        
        offset += ie_len;
    }
    
    return 0;
}

void pfcp_free_message(pfcp_message_t* msg) {
    (void)msg;
}
