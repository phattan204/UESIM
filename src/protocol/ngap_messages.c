/*
 * 5G UE Simulation Application
 * NGAP Message Implementation
 * 3GPP TS 38.413
 */

#include "ngap_messages.h"
#include "asn1_per.h"
#include "../core/memory.h"
#include <string.h>
#include <stdio.h>

/* ============== Utility Functions ============== */

const char* ngap_message_type_to_string(ngap_message_type_t type) {
    static const char* type_strings[] = {
        "NGSetupRequest", "NGSetupResponse", "NGSetupFailure",
        "InitialUEMessage", "InitialContextSetupRequest", "InitialContextSetupResponse", "InitialContextSetupFailure",
        "UEContextReleaseRequest", "UEContextReleaseCommand", "UEContextReleaseComplete",
        "UEContextModificationRequest", "UEContextModificationResponse", "UEContextModificationFailure",
        "UplinkNASTransport", "DownlinkNASTransport", "NASNonDeliveryIndication",
        "PDUSessionSetupRequest", "PDUSessionSetupResponse", "PDUSessionSetupFailure",
        "PDUSessionModificationRequest", "PDUSessionModificationResponse", "PDUSessionModificationFailure",
        "PDUSessionReleaseCommand", "PDUSessionReleaseResponse",
        "PDUSessionResourceNotify", "PDUSessionResourceNotifyAck",
        "HandoverRequired", "HandoverCommand", "HandoverPreparationFailure",
        "HandoverRequest", "HandoverRequestAcknowledge", "HandoverFailure",
        "HandoverNotify", "HandoverCancel", "HandoverCancelAcknowledge",
        "PathSwitchRequest", "PathSwitchRequestAcknowledge", "PathSwitchRequestFailure",
        "ErrorIndication", "NGReset", "NGResetAcknowledge", "AMFStatusIndication"
    };
    
    if (type >= NGAP_MSG_MAX) return "Unknown";
    return type_strings[type];
}

const char* ngap_cause_to_string(ngap_cause_t cause) {
    static char buf[64];
    const char* type_str;
    const char* value_str;
    
    switch (cause.cause_type) {
        case NGAP_CAUSE_RADIO_NETWORK:
            type_str = "RadioNetwork";
            switch (cause.cause_value) {
                case NGAP_CAUSE_RADIO_UNSPECIFIED: value_str = "Unspecified"; break;
                case NGAP_CAUSE_RADIO_TX2RELOVERALL_EXPIRY: value_str = "Tx2RELOverallExpiry"; break;
                case NGAP_CAUSE_RADIO_SUCCESSFUL_HANDOVER: value_str = "SuccessfulHandover"; break;
                case NGAP_CAUSE_RADIO_RELEASE_DUE_TO_5GC_GENERATED: value_str = "ReleaseDueTo5GCGenerated"; break;
                default: value_str = "Other"; break;
            }
            break;
        case NGAP_CAUSE_TRANSPORT:
            type_str = "Transport";
            value_str = cause.cause_value == 0 ? "ResourceUnavailable" : "Unspecified";
            break;
        case NGAP_CAUSE_NAS:
            type_str = "NAS";
            value_str = cause.cause_value == 0 ? "NormalRelease" : "Other";
            break;
        case NGAP_CAUSE_PROTOCOL:
            type_str = "Protocol";
            value_str = cause.cause_value == 0 ? "TransferSyntaxError" : "Other";
            break;
        case NGAP_CAUSE_MISC:
            type_str = "Misc";
            value_str = cause.cause_value == 0 ? "ControlProcessingOverload" : "Other";
            break;
        default:
            type_str = "Unknown";
            value_str = "Unknown";
    }
    
    snprintf(buf, sizeof(buf), "%s:%s", type_str, value_str);
    return buf;
}

uint32_t ngap_encode_plmn_id(uint16_t mcc, uint16_t mnc, uint8_t mnc_len) {
    /* PLMN ID encoding per 3GPP TS 23.003
     * BCD encoded: MCC digit 1, MCC digit 2, MNC digit 3, MCC digit 3, MNC digit 1, MNC digit 2
     * For 2-digit MNC: MNC digit 3 = 0xF
     */
    uint8_t plmn[3];
    uint8_t mcc_d1 = mcc / 100;
    uint8_t mcc_d2 = (mcc / 10) % 10;
    uint8_t mcc_d3 = mcc % 10;
    uint8_t mnc_d1, mnc_d2, mnc_d3;
    
    if (mnc_len == 2) {
        mnc_d1 = mnc / 10;
        mnc_d2 = mnc % 10;
        mnc_d3 = 0xF;
    } else {
        mnc_d1 = mnc / 100;
        mnc_d2 = (mnc / 10) % 10;
        mnc_d3 = mnc % 10;
    }
    
    plmn[0] = (mcc_d2 << 4) | mcc_d1;
    plmn[1] = (mnc_d3 << 4) | mcc_d3;
    plmn[2] = (mnc_d2 << 4) | mnc_d1;
    
    return (plmn[0] << 16) | (plmn[1] << 8) | plmn[2];
}

void ngap_decode_plmn_id(uint8_t plmn_id[3], uint16_t* mcc, uint16_t* mnc, uint8_t* mnc_len) {
    uint8_t mcc_d1 = plmn_id[0] & 0x0F;
    uint8_t mcc_d2 = (plmn_id[0] >> 4) & 0x0F;
    uint8_t mcc_d3 = plmn_id[1] & 0x0F;
    uint8_t mnc_d3 = (plmn_id[1] >> 4) & 0x0F;
    uint8_t mnc_d1 = plmn_id[2] & 0x0F;
    uint8_t mnc_d2 = (plmn_id[2] >> 4) & 0x0F;
    
    *mcc = mcc_d1 * 100 + mcc_d2 * 10 + mcc_d3;
    
    if (mnc_d3 == 0xF) {
        *mnc = mnc_d1 * 10 + mnc_d2;
        *mnc_len = 2;
    } else {
        *mnc = mnc_d1 * 100 + mnc_d2 * 10 + mnc_d3;
        *mnc_len = 3;
    }
}

/* ============== Message Encoding ============== */

uesim_error_t ngap_encode_message(const ngap_message_t* msg, uint8_t** data, size_t* len) {
    if (msg == NULL || data == NULL || len == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    asn1_buffer_t buf;
    uesim_error_t result = asn1_buffer_alloc(&buf, NGAP_MAX_MESSAGE_SIZE);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    /* Encode NGAP PDU header
     * NGAP uses ASN.1 PER aligned encoding
     * Structure: [InitiatingMessage | SuccessfulOutcome | UnsuccessfulOutcome]
     */
    
    /* Encode message type and procedure code */
    asn1_encode_bits(&buf, msg->procedure_code, 8);
    asn1_encode_bits(&buf, msg->criticality, 2);
    asn1_encode_bits(&buf, 0, 6);  /* Spare */
    
    /* Encode transaction ID */
    asn1_encode_bits(&buf, msg->transaction_id & 0xFF, 8);
    
    /* Encode message-specific content */
    switch (msg->message_type) {
        case NGAP_MSG_NG_SETUP_REQUEST: {
            const ngap_ng_setup_request_t* req = &msg->payload.ng_setup_request;
            
            /* Global gNB ID */
            asn1_encode_octet_string(&buf, req->global_gnb_id.plmn_id, 3);
            asn1_encode_bits(&buf, req->global_gnb_id.gnb_id, 32);
            
            /* TAI list */
            asn1_encode_bits(&buf, req->num_tai, 8);
            for (int i = 0; i < req->num_tai; i++) {
                asn1_encode_octet_string(&buf, req->tai_list[i].plmn_id, 3);
                asn1_encode_bits(&buf, req->tai_list[i].tac, 24);
            }
            
            /* Default paging DRX */
            asn1_encode_bits(&buf, req->default_paging_drx, 4);
            asn1_encode_bits(&buf, 0, 4);  /* Spare */
            
            /* Max UE connections */
            asn1_encode_bits(&buf, req->max_ue_connections, 16);
            break;
        }
        
        case NGAP_MSG_INITIAL_UE_MESSAGE: {
            const ngap_initial_ue_message_t* req = &msg->payload.initial_ue_message;
            
            /* RAN UE NGAP ID */
            asn1_encode_bits(&buf, req->ue_ids.ran_ue_ngap_id, 32);
            
            /* User Location Info */
            asn1_encode_octet_string(&buf, req->user_location.nr_cgi.plmn_id, 3);
            asn1_encode_bits(&buf, req->user_location.nr_cgi.cell_id, 36);
            asn1_encode_bits(&buf, req->user_location.tai, 24);
            
            /* RRC Establishment Cause */
            asn1_encode_bits(&buf, req->rrc_establishment_cause, 4);
            asn1_encode_bits(&buf, 0, 4);  /* Spare */
            
            /* NAS PDU */
            asn1_encode_length(&buf, req->nas_pdu_len);
            if (req->nas_pdu_len > 0) {
                asn1_encode_octet_string(&buf, req->nas_pdu, req->nas_pdu_len);
            }
            break;
        }
        
        case NGAP_MSG_UPLINK_NAS_TRANSPORT: {
            const ngap_uplink_nas_transport_t* req = &msg->payload.uplink_nas_transport;
            
            /* UE IDs */
            asn1_encode_bits(&buf, req->ue_ids.ran_ue_ngap_id, 32);
            asn1_encode_bits(&buf, (uint32_t)(req->ue_ids.amf_ue_ngap_id & 0xFFFFFFFF), 32);
            asn1_encode_bits(&buf, (uint32_t)(req->ue_ids.amf_ue_ngap_id >> 32), 8);
            
            /* User Location Info */
            asn1_encode_octet_string(&buf, req->user_location.nr_cgi.plmn_id, 3);
            asn1_encode_bits(&buf, req->user_location.nr_cgi.cell_id, 36);
            asn1_encode_bits(&buf, req->user_location.tai, 24);
            
            /* NAS PDU */
            asn1_encode_length(&buf, req->nas_pdu_len);
            if (req->nas_pdu_len > 0) {
                asn1_encode_octet_string(&buf, req->nas_pdu, req->nas_pdu_len);
            }
            break;
        }
        
        case NGAP_MSG_UE_CONTEXT_RELEASE_REQUEST: {
            const ngap_ue_context_release_request_t* req = &msg->payload.ue_context_release_request;
            
            /* UE IDs */
            asn1_encode_bits(&buf, req->ue_ids.ran_ue_ngap_id, 32);
            asn1_encode_bits(&buf, (uint32_t)(req->ue_ids.amf_ue_ngap_id & 0xFFFFFFFF), 32);
            asn1_encode_bits(&buf, (uint32_t)(req->ue_ids.amf_ue_ngap_id >> 32), 8);
            
            /* Cause */
            asn1_encode_bits(&buf, req->cause.cause_type, 3);
            asn1_encode_bits(&buf, req->cause.cause_value, 5);
            break;
        }
        
        case NGAP_MSG_UE_CONTEXT_RELEASE_COMPLETE: {
            const ngap_ue_context_release_complete_t* req = &msg->payload.ue_context_release_complete;
            
            /* UE IDs */
            asn1_encode_bits(&buf, req->ue_ids.ran_ue_ngap_id, 32);
            asn1_encode_bits(&buf, (uint32_t)(req->ue_ids.amf_ue_ngap_id & 0xFFFFFFFF), 32);
            asn1_encode_bits(&buf, (uint32_t)(req->ue_ids.amf_ue_ngap_id >> 32), 8);
            
            /* User Location Info */
            asn1_encode_octet_string(&buf, req->user_location.nr_cgi.plmn_id, 3);
            asn1_encode_bits(&buf, req->user_location.nr_cgi.cell_id, 36);
            asn1_encode_bits(&buf, req->user_location.tai, 24);
            break;
        }
        
        default:
            /* For unimplemented messages, encode minimal header */
            asn1_encode_bits(&buf, 0, 8);  /* Placeholder */
            break;
    }
    
    *len = asn1_buffer_length(&buf);
    *data = buf.data;
    buf.own_data = false;
    asn1_buffer_free(&buf);
    
    return UESIM_SUCCESS;
}

/* ============== Message Decoding ============== */

uesim_error_t ngap_decode_message(const uint8_t* data, size_t len, ngap_message_t* msg) {
    if (data == NULL || len < 4 || msg == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    memset(msg, 0, sizeof(ngap_message_t));
    size_t bit_offset = 0;
    
    /* Decode procedure code */
    uint32_t proc_code;
    asn1_decode_bits(data, &bit_offset, &proc_code, 8);
    msg->procedure_code = (ngap_procedure_code_t)proc_code;
    
    /* Decode criticality */
    uint32_t criticality;
    asn1_decode_bits(data, &bit_offset, &criticality, 2);
    msg->criticality = (uint8_t)criticality;
    
    /* Skip spare */
    uint32_t spare;
    asn1_decode_bits(data, &bit_offset, &spare, 6);
    
    /* Decode transaction ID */
    uint32_t trans_id;
    asn1_decode_bits(data, &bit_offset, &trans_id, 8);
    msg->transaction_id = trans_id;
    
    /* Determine message type from procedure code */
    switch (msg->procedure_code) {
        case NGAP_PROC_NG_SETUP:
            msg->message_type = NGAP_MSG_NG_SETUP_REQUEST;
            break;
        case NGAP_PROC_INITIAL_UE:
            msg->message_type = NGAP_MSG_INITIAL_UE_MESSAGE;
            break;
        case NGAP_PROC_UPLINK_NAS_TRANSPORT:
            msg->message_type = NGAP_MSG_UPLINK_NAS_TRANSPORT;
            break;
        case NGAP_PROC_UE_CONTEXT_RELEASE_REQUEST:
            msg->message_type = NGAP_MSG_UE_CONTEXT_RELEASE_REQUEST;
            break;
        case NGAP_PROC_INITIAL_CONTEXT_SETUP:
            msg->message_type = NGAP_MSG_INITIAL_CONTEXT_SETUP_REQUEST;
            break;
        case NGAP_PROC_PDU_SESSION_SETUP:
            msg->message_type = NGAP_MSG_PDU_SESSION_SETUP_REQUEST;
            break;
        case NGAP_PROC_HANDOVER_PREPARATION:
            msg->message_type = NGAP_MSG_HANDOVER_REQUIRED;
            break;
        case NGAP_PROC_PATH_SWITCH_REQUEST:
            msg->message_type = NGAP_MSG_PATH_SWITCH_REQUEST;
            break;
        default:
            msg->message_type = NGAP_MSG_ERROR_INDICATION;
            break;
    }
    
    /* Decode message-specific content */
    asn1_skip_to_byte_boundary(&bit_offset);
    size_t data_offset = bit_offset / 8;
    
    switch (msg->message_type) {
        case NGAP_MSG_INITIAL_UE_MESSAGE: {
            ngap_initial_ue_message_t* req = &msg->payload.initial_ue_message;
            
            /* RAN UE NGAP ID */
            uint32_t ran_ue_id;
            asn1_decode_bits(data, &bit_offset, &ran_ue_id, 32);
            req->ue_ids.ran_ue_ngap_id = ran_ue_id;
            
            /* User Location Info - simplified */
            asn1_skip_to_byte_boundary(&bit_offset);
            data_offset = bit_offset / 8;
            if (data_offset + 10 <= len) {
                memcpy(req->user_location.nr_cgi.plmn_id, data + data_offset, 3);
                data_offset += 3;
                bit_offset = data_offset * 8;
                
                uint32_t cell_id;
                asn1_decode_bits(data, &bit_offset, &cell_id, 36);
                req->user_location.nr_cgi.cell_id = cell_id;
                
                uint32_t tac;
                asn1_decode_bits(data, &bit_offset, &tac, 24);
                req->user_location.tai = tac;
            }
            
            /* RRC Establishment Cause */
            uint32_t cause;
            asn1_decode_bits(data, &bit_offset, &cause, 4);
            req->rrc_establishment_cause = (uint8_t)cause;
            
            /* Skip spare */
            asn1_decode_bits(data, &bit_offset, &spare, 4);
            
            /* NAS PDU */
            asn1_skip_to_byte_boundary(&bit_offset);
            data_offset = bit_offset / 8;
            if (data_offset < len) {
                size_t nas_len = len - data_offset;
                if (nas_len > sizeof(req->nas_pdu)) nas_len = sizeof(req->nas_pdu);
                memcpy(req->nas_pdu, data + data_offset, nas_len);
                req->nas_pdu_len = nas_len;
            }
            break;
        }
        
        case NGAP_MSG_UPLINK_NAS_TRANSPORT: {
            ngap_uplink_nas_transport_t* req = &msg->payload.uplink_nas_transport;
            
            /* UE IDs */
            uint32_t ran_ue_id, amf_ue_id_lo, amf_ue_id_hi;
            asn1_decode_bits(data, &bit_offset, &ran_ue_id, 32);
            asn1_decode_bits(data, &bit_offset, &amf_ue_id_lo, 32);
            asn1_decode_bits(data, &bit_offset, &amf_ue_id_hi, 8);
            req->ue_ids.ran_ue_ngap_id = ran_ue_id;
            req->ue_ids.amf_ue_ngap_id = ((uint64_t)amf_ue_id_hi << 32) | amf_ue_id_lo;
            
            /* Skip to NAS PDU */
            asn1_skip_to_byte_boundary(&bit_offset);
            data_offset = bit_offset / 8 + 10;  /* Skip user location */
            if (data_offset < len) {
                size_t nas_len = len - data_offset;
                if (nas_len > sizeof(req->nas_pdu)) nas_len = sizeof(req->nas_pdu);
                memcpy(req->nas_pdu, data + data_offset, nas_len);
                req->nas_pdu_len = nas_len;
            }
            break;
        }
        
        default:
            /* Copy remaining data as raw payload */
            asn1_skip_to_byte_boundary(&bit_offset);
            data_offset = bit_offset / 8;
            if (data_offset < len) {
                /* Store raw data for later processing */
            }
            break;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t ngap_free_message(ngap_message_t* msg) {
    if (msg == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    /* No dynamic allocation in message structure */
    return UESIM_SUCCESS;
}

/* ============== Message Creation Functions ============== */

uesim_error_t ngap_create_ng_setup_request(const ngap_global_gnb_id_t* gnb_id, 
                                            ngap_message_t* msg) {
    if (gnb_id == NULL || msg == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    memset(msg, 0, sizeof(ngap_message_t));
    msg->message_type = NGAP_MSG_NG_SETUP_REQUEST;
    msg->procedure_code = NGAP_PROC_NG_SETUP;
    msg->criticality = 0;  /* reject */
    msg->transaction_id = 0;
    
    ngap_ng_setup_request_t* req = &msg->payload.ng_setup_request;
    memcpy(&req->global_gnb_id, gnb_id, sizeof(ngap_global_gnb_id_t));
    req->default_paging_drx = 1;  /* 32 paging frames */
    req->max_ue_connections = 1000;
    req->num_tai = 1;
    
    return UESIM_SUCCESS;
}

uesim_error_t ngap_create_initial_ue_message(uint32_t ran_ue_id, 
                                              const uint8_t* nas_pdu, size_t nas_len,
                                              ngap_message_t* msg) {
    if (msg == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    memset(msg, 0, sizeof(ngap_message_t));
    msg->message_type = NGAP_MSG_INITIAL_UE_MESSAGE;
    msg->procedure_code = NGAP_PROC_INITIAL_UE;
    msg->criticality = 0;  /* reject */
    msg->transaction_id = ran_ue_id & 0xFF;
    
    ngap_initial_ue_message_t* req = &msg->payload.initial_ue_message;
    req->ue_ids.ran_ue_ngap_id = ran_ue_id;
    req->rrc_establishment_cause = 2;  /* mo-Signalling */
    req->ue_context_request = 1;
    
    if (nas_pdu != NULL && nas_len > 0) {
        size_t copy_len = nas_len < sizeof(req->nas_pdu) ? nas_len : sizeof(req->nas_pdu);
        memcpy(req->nas_pdu, nas_pdu, copy_len);
        req->nas_pdu_len = copy_len;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t ngap_create_uplink_nas_transport(uint32_t ran_ue_id, uint64_t amf_ue_id,
                                                const uint8_t* nas_pdu, size_t nas_len,
                                                ngap_message_t* msg) {
    if (msg == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    memset(msg, 0, sizeof(ngap_message_t));
    msg->message_type = NGAP_MSG_UPLINK_NAS_TRANSPORT;
    msg->procedure_code = NGAP_PROC_UPLINK_NAS_TRANSPORT;
    msg->criticality = 0;  /* ignore */
    msg->transaction_id = ran_ue_id & 0xFF;
    
    ngap_uplink_nas_transport_t* req = &msg->payload.uplink_nas_transport;
    req->ue_ids.ran_ue_ngap_id = ran_ue_id;
    req->ue_ids.amf_ue_ngap_id = amf_ue_id;
    
    if (nas_pdu != NULL && nas_len > 0) {
        size_t copy_len = nas_len < sizeof(req->nas_pdu) ? nas_len : sizeof(req->nas_pdu);
        memcpy(req->nas_pdu, nas_pdu, copy_len);
        req->nas_pdu_len = copy_len;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t ngap_create_ue_context_release_request(uint32_t ran_ue_id, uint64_t amf_ue_id,
                                                      ngap_cause_t cause, ngap_message_t* msg) {
    if (msg == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    memset(msg, 0, sizeof(ngap_message_t));
    msg->message_type = NGAP_MSG_UE_CONTEXT_RELEASE_REQUEST;
    msg->procedure_code = NGAP_PROC_UE_CONTEXT_RELEASE_REQUEST;
    msg->criticality = 0;  /* reject */
    msg->transaction_id = ran_ue_id & 0xFF;
    
    ngap_ue_context_release_request_t* req = &msg->payload.ue_context_release_request;
    req->ue_ids.ran_ue_ngap_id = ran_ue_id;
    req->ue_ids.amf_ue_ngap_id = amf_ue_id;
    req->cause = cause;
    
    return UESIM_SUCCESS;
}