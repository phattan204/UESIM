/*
 * 5G UE Simulation Application
 * E1AP Message Implementation
 * 3GPP TS 38.463
 */

#include "e1ap_messages.h"
#include "asn1_per.h"
#include "../core/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============== Message Type Strings ============== */

static const char* e1ap_message_type_strings[E1AP_MSG_MAX] = {
    "E1SetupRequest",
    "E1SetupResponse",
    "E1SetupFailure",
    "E1ResetRequest",
    "E1ResetResponse",
    "ErrorIndication",
    "E1RemovalRequest",
    "E1RemovalResponse",
    "GNBCUUPConfigUpdate",
    "GNBCUUPConfigUpdateAck",
    "GNBCUUPConfigUpdateFailure",
    "GNBCUCPConfigUpdate",
    "GNBCUCPConfigUpdateAck",
    "GNBCUCPConfigUpdateFailure",
    "BearerContextSetupRequest",
    "BearerContextSetupResponse",
    "BearerContextSetupFailure",
    "BearerContextReleaseCommand",
    "BearerContextReleaseComplete",
    "BearerContextModificationRequest",
    "BearerContextModificationResponse",
    "BearerContextModificationFailure",
    "BearerContextReleaseRequest",
    "PDUSessionResourceSetupRequest",
    "PDUSessionResourceSetupResponse",
    "PDUSessionResourceModificationRequest",
    "PDUSessionResourceModificationResponse",
    "PDUSessionResourceReleaseCommand",
    "PDUSessionResourceReleaseComplete"
};

static const char* e1ap_cause_type_strings[] = {
    "RadioNetwork",
    "Transport",
    "Protocol",
    "Misc",
    "NGAndOrXn"
};

/* ============== Utility Functions ============== */

const char* e1ap_message_type_to_string(e1ap_message_type_t type) {
    if (type >= E1AP_MSG_MAX) return "Unknown";
    return e1ap_message_type_strings[type];
}

const char* e1ap_cause_to_string(const e1ap_cause_t* cause) {
    static char buf[64];
    if (!cause) return "NULL";
    
    const char* type_str = "Unknown";
    if (cause->cause_type < 5) {
        type_str = e1ap_cause_type_strings[cause->cause_type];
    }
    
    snprintf(buf, sizeof(buf), "%s:%u", type_str, cause->cause_value);
    return buf;
}

/* ============== Cause Helpers ============== */

void e1ap_set_cause_radio(e1ap_cause_t* cause, e1ap_cause_radio_value_t value) {
    if (cause) {
        cause->cause_type = E1AP_CAUSE_RADIO_NETWORK;
        cause->cause_value = (uint8_t)value;
    }
}

void e1ap_set_cause_transport(e1ap_cause_t* cause, e1ap_cause_transport_value_t value) {
    if (cause) {
        cause->cause_type = E1AP_CAUSE_TRANSPORT;
        cause->cause_value = (uint8_t)value;
    }
}

void e1ap_set_cause_protocol(e1ap_cause_t* cause, e1ap_cause_protocol_value_t value) {
    if (cause) {
        cause->cause_type = E1AP_CAUSE_PROTOCOL;
        cause->cause_value = (uint8_t)value;
    }
}

void e1ap_set_cause_misc(e1ap_cause_t* cause, e1ap_cause_misc_value_t value) {
    if (cause) {
        cause->cause_type = E1AP_CAUSE_MISC;
        cause->cause_value = (uint8_t)value;
    }
}

/* ============== Message Initialization Helpers ============== */

void e1ap_init_e1_setup_request(e1ap_message_t* msg) {
    if (!msg) return;
    memset(msg, 0, sizeof(e1ap_message_t));
    msg->message_type = E1AP_MSG_E1_SETUP_REQUEST;
    msg->procedure_code = E1AP_PROC_E1_SETUP;
    msg->criticality = 0;
}

void e1ap_init_e1_setup_response(e1ap_message_t* msg) {
    if (!msg) return;
    memset(msg, 0, sizeof(e1ap_message_t));
    msg->message_type = E1AP_MSG_E1_SETUP_RESPONSE;
    msg->procedure_code = E1AP_PROC_E1_SETUP;
    msg->criticality = 0;
}

void e1ap_init_bearer_context_setup_request(e1ap_message_t* msg) {
    if (!msg) return;
    memset(msg, 0, sizeof(e1ap_message_t));
    msg->message_type = E1AP_MSG_BEARER_CONTEXT_SETUP_REQUEST;
    msg->procedure_code = E1AP_PROC_BEARER_CONTEXT_SETUP;
    msg->criticality = 0;
}

void e1ap_init_bearer_context_release_request(e1ap_message_t* msg) {
    if (!msg) return;
    memset(msg, 0, sizeof(e1ap_message_t));
    msg->message_type = E1AP_MSG_BEARER_CONTEXT_RELEASE_REQUEST;
    msg->procedure_code = E1AP_PROC_BEARER_CONTEXT_RELEASE_REQUEST;
    msg->criticality = 0;
}

/* ============== ASN.1 PER Encoding Helpers ============== */

static void e1ap_encode_header(asn1_buffer_t* buf, const e1ap_message_t* msg) {
    asn1_encode_bits(buf, msg->procedure_code, 8);
    asn1_encode_bits(buf, msg->criticality, 2);
    asn1_encode_bits(buf, msg->message_type, 8);
}

static void e1ap_encode_cu_up_id(asn1_buffer_t* buf, const e1ap_gnb_cu_up_id_t* id) {
    asn1_encode_bits(buf, id->gnb_cu_up_id, 32);
    uint8_t len = (uint8_t)strlen(id->gnb_cu_up_name);
    asn1_encode_bits(buf, len, 8);
    for (int i = 0; i < len; i++) {
        asn1_encode_bits(buf, (uint8_t)id->gnb_cu_up_name[i], 8);
    }
}

static void e1ap_encode_cu_cp_id(asn1_buffer_t* buf, const e1ap_gnb_cu_cp_id_t* id) {
    asn1_encode_bits(buf, (uint32_t)(id->gnb_cu_cp_id & 0xFFFFFFFF), 32);
    uint8_t len = (uint8_t)strlen(id->gnb_cu_cp_name);
    asn1_encode_bits(buf, len, 8);
    for (int i = 0; i < len; i++) {
        asn1_encode_bits(buf, (uint8_t)id->gnb_cu_cp_name[i], 8);
    }
}

static void e1ap_encode_ue_ids(asn1_buffer_t* buf, const e1ap_ue_ids_t* ids) {
    asn1_encode_bits(buf, ids->gnb_cu_cp_ue_e1ap_id, 32);
    asn1_encode_bits(buf, ids->gnb_cu_up_ue_e1ap_id, 32);
}

static void e1ap_encode_tnl_info(asn1_buffer_t* buf, const e1ap_tnl_info_t* tnl) {
    asn1_encode_bits(buf, tnl->transport_type, 4);
    asn1_encode_bits(buf, tnl->ip_address, 32);
    asn1_encode_bits(buf, tnl->port, 16);
    asn1_encode_bits(buf, tnl->teid, 32);
}

static void e1ap_encode_qos_flow(asn1_buffer_t* buf, const e1ap_qos_flow_t* flow) {
    asn1_encode_bits(buf, flow->qfi, 6);
    asn1_encode_bits(buf, flow->five_qi, 8);
    asn1_encode_bits(buf, (uint32_t)(flow->gfbr_ul >> 32), 32);
    asn1_encode_bits(buf, (uint32_t)(flow->gfbr_ul & 0xFFFFFFFF), 32);
    asn1_encode_bits(buf, (uint32_t)(flow->gfbr_dl >> 32), 32);
    asn1_encode_bits(buf, (uint32_t)(flow->gfbr_dl & 0xFFFFFFFF), 32);
    asn1_encode_bits(buf, flow->priority_level, 8);
}

static void e1ap_encode_drb_info(asn1_buffer_t* buf, const e1ap_drb_info_t* drb) {
    asn1_encode_bits(buf, drb->drb_id, 5);
    asn1_encode_bits(buf, drb->pdcp_sn_size, 4);
    asn1_encode_bits(buf, drb->rlc_mode, 2);
    
    asn1_encode_bits(buf, drb->num_qos_flows, 6);
    for (int i = 0; i < drb->num_qos_flows && i < E1AP_MAX_QOS_FLOWS; i++) {
        e1ap_encode_qos_flow(buf, &drb->qos_flows[i]);
    }
    
    asn1_encode_bits(buf, drb->num_dl_up_tnl_info, 4);
    for (int i = 0; i < drb->num_dl_up_tnl_info && i < E1AP_MAX_TNL_INFO; i++) {
        e1ap_encode_tnl_info(buf, &drb->dl_up_tnl_info[i]);
    }
}

static void e1ap_encode_pdu_session_info(asn1_buffer_t* buf, const e1ap_pdu_session_info_t* session) {
    asn1_encode_bits(buf, session->pdu_session_id, 8);
    asn1_encode_bits(buf, session->pdu_session_type, 4);
    asn1_encode_bits(buf, session->ue_ip_address, 32);
    
    asn1_encode_bits(buf, session->s_nssai.sst, 8);
    asn1_encode_bits(buf, session->s_nssai.sd, 24);
    
    asn1_encode_bits(buf, session->num_drbs, 4);
    for (int i = 0; i < session->num_drbs && i < E1AP_MAX_DRB_ID; i++) {
        e1ap_encode_drb_info(buf, &session->drbs[i]);
    }
    
    e1ap_encode_tnl_info(buf, &session->upf_ul_tnl_info);
}

static void e1ap_encode_cause(asn1_buffer_t* buf, const e1ap_cause_t* cause) {
    asn1_encode_bits(buf, cause->cause_type, 3);
    asn1_encode_bits(buf, cause->cause_value, 8);
}

/* ============== Message Encoding ============== */

int e1ap_encode_message(const e1ap_message_t* msg, uint8_t** buffer, size_t* length) {
    if (!msg || !buffer || !length) {
        return -1;
    }
    
    asn1_buffer_t buf;
    uesim_error_t result = asn1_buffer_alloc(&buf, E1AP_MAX_MESSAGE_SIZE);
    if (result != UESIM_SUCCESS) {
        return -1;
    }
    
    e1ap_encode_header(&buf, msg);
    
    switch (msg->message_type) {
        case E1AP_MSG_E1_SETUP_REQUEST: {
            const e1ap_e1_setup_request_t* req = &msg->payload.e1_setup_request;
            e1ap_encode_cu_up_id(&buf, &req->gnb_cu_up_id);
            
            asn1_encode_bits(&buf, req->num_supported_plmns, 8);
            for (int i = 0; i < req->num_supported_plmns && i < 12; i++) {
                for (int j = 0; j < 3; j++) {
                    asn1_encode_bits(&buf, req->supported_plmns[i][j], 8);
                }
            }
            
            asn1_encode_bits(&buf, req->capacity, 32);
            
            asn1_encode_bits(&buf, req->num_tnla, 4);
            for (int i = 0; i < req->num_tnla && i < E1AP_MAX_TNL_INFO; i++) {
                e1ap_encode_tnl_info(&buf, &req->tnla[i]);
            }
            break;
        }
        
        case E1AP_MSG_E1_SETUP_RESPONSE: {
            const e1ap_e1_setup_response_t* resp = &msg->payload.e1_setup_response;
            e1ap_encode_cu_cp_id(&buf, &resp->gnb_cu_cp_id);
            
            asn1_encode_bits(&buf, resp->num_supported_plmns, 8);
            for (int i = 0; i < resp->num_supported_plmns && i < 12; i++) {
                for (int j = 0; j < 3; j++) {
                    asn1_encode_bits(&buf, resp->supported_plmns[i][j], 8);
                }
            }
            break;
        }
        
        case E1AP_MSG_E1_SETUP_FAILURE: {
            const e1ap_e1_setup_failure_t* fail = &msg->payload.e1_setup_failure;
            e1ap_encode_cause(&buf, &fail->cause);
            asn1_encode_bits(&buf, fail->time_to_wait, 32);
            break;
        }
        
        case E1AP_MSG_BEARER_CONTEXT_SETUP_REQUEST: {
            const e1ap_bearer_context_setup_request_t* req = &msg->payload.bearer_context_setup_request;
            e1ap_encode_ue_ids(&buf, &req->ue_ids);
            
            asn1_encode_bits(&buf, (uint32_t)((req->ran_ue_id.ran_ue_id >> 32) & 0xFF), 8);
            asn1_encode_bits(&buf, (uint32_t)(req->ran_ue_id.ran_ue_id & 0xFFFFFFFF), 32);
            
            asn1_encode_bits(&buf, req->num_pdu_sessions, 8);
            for (int i = 0; i < req->num_pdu_sessions && i < E1AP_MAX_PDU_SESSIONS; i++) {
                e1ap_encode_pdu_session_info(&buf, &req->pdu_sessions[i]);
            }
            
            asn1_encode_bits(&buf, (uint32_t)(req->ue_ambr_dl >> 32), 32);
            asn1_encode_bits(&buf, (uint32_t)(req->ue_ambr_dl & 0xFFFFFFFF), 32);
            asn1_encode_bits(&buf, (uint32_t)(req->ue_ambr_ul >> 32), 32);
            asn1_encode_bits(&buf, (uint32_t)(req->ue_ambr_ul & 0xFFFFFFFF), 32);
            
            asn1_encode_bits(&buf, req->security_key_present, 1);
            if (req->security_key_present) {
                for (int i = 0; i < 32; i++) {
                    asn1_encode_bits(&buf, req->security_key[i], 8);
                }
            }
            break;
        }
        
        case E1AP_MSG_BEARER_CONTEXT_SETUP_RESPONSE: {
            const e1ap_bearer_context_setup_response_t* resp = &msg->payload.bearer_context_setup_response;
            e1ap_encode_ue_ids(&buf, &resp->ue_ids);
            
            asn1_encode_bits(&buf, resp->num_pdu_sessions_setup, 8);
            for (int i = 0; i < resp->num_pdu_sessions_setup && i < E1AP_MAX_PDU_SESSIONS; i++) {
                asn1_encode_bits(&buf, resp->pdu_session_ids[i], 8);
            }
            
            asn1_encode_bits(&buf, resp->num_drbs_setup, 4);
            for (int i = 0; i < resp->num_drbs_setup && i < E1AP_MAX_DRB_ID; i++) {
                e1ap_encode_drb_info(&buf, &resp->drbs_setup[i]);
            }
            break;
        }
        
        case E1AP_MSG_BEARER_CONTEXT_SETUP_FAILURE: {
            const e1ap_bearer_context_setup_failure_t* fail = &msg->payload.bearer_context_setup_failure;
            e1ap_encode_ue_ids(&buf, &fail->ue_ids);
            e1ap_encode_cause(&buf, &fail->cause);
            break;
        }
        
        case E1AP_MSG_BEARER_CONTEXT_RELEASE_COMMAND: {
            const e1ap_bearer_context_release_command_t* cmd = &msg->payload.bearer_context_release_command;
            e1ap_encode_ue_ids(&buf, &cmd->ue_ids);
            e1ap_encode_cause(&buf, &cmd->cause);
            break;
        }
        
        case E1AP_MSG_BEARER_CONTEXT_RELEASE_COMPLETE: {
            const e1ap_bearer_context_release_complete_t* comp = &msg->payload.bearer_context_release_complete;
            e1ap_encode_ue_ids(&buf, &comp->ue_ids);
            break;
        }
        
        case E1AP_MSG_BEARER_CONTEXT_RELEASE_REQUEST: {
            const e1ap_bearer_context_release_request_t* req = &msg->payload.bearer_context_release_request;
            e1ap_encode_ue_ids(&buf, &req->ue_ids);
            e1ap_encode_cause(&buf, &req->cause);
            break;
        }
        
        case E1AP_MSG_ERROR_INDICATION: {
            const e1ap_error_indication_t* err = &msg->payload.error_indication;
            asn1_encode_bits(&buf, err->ue_ids_present, 1);
            if (err->ue_ids_present) {
                e1ap_encode_ue_ids(&buf, &err->ue_ids);
            }
            asn1_encode_bits(&buf, err->cause_present, 1);
            if (err->cause_present) {
                e1ap_encode_cause(&buf, &err->cause);
            }
            break;
        }
        
        default:
            break;
    }
    
    *buffer = buf.data;
    *length = (buf.bit_offset + 7) / 8;
    
    return 0;
}

/* ============== Decoding Helpers ============== */

static uint32_t e1ap_decode_bits(asn1_buffer_t* buf, uint8_t num_bits) {
    uint32_t value;
    asn1_decode_bits(buf->data, &buf->bit_offset, &value, num_bits);
    return value;
}

static void e1ap_decode_cu_up_id(asn1_buffer_t* buf, e1ap_gnb_cu_up_id_t* id) {
    id->gnb_cu_up_id = e1ap_decode_bits(buf, 32);
    uint8_t len = e1ap_decode_bits(buf, 8);
    for (int i = 0; i < len && i < 63; i++) {
        id->gnb_cu_up_name[i] = (char)e1ap_decode_bits(buf, 8);
    }
    id->gnb_cu_up_name[len < 64 ? len : 63] = '\0';
}

static void e1ap_decode_cu_cp_id(asn1_buffer_t* buf, e1ap_gnb_cu_cp_id_t* id) {
    id->gnb_cu_cp_id = e1ap_decode_bits(buf, 32);
    uint8_t len = e1ap_decode_bits(buf, 8);
    for (int i = 0; i < len && i < 63; i++) {
        id->gnb_cu_cp_name[i] = (char)e1ap_decode_bits(buf, 8);
    }
    id->gnb_cu_cp_name[len < 64 ? len : 63] = '\0';
}

static void e1ap_decode_ue_ids(asn1_buffer_t* buf, e1ap_ue_ids_t* ids) {
    ids->gnb_cu_cp_ue_e1ap_id = e1ap_decode_bits(buf, 32);
    ids->gnb_cu_up_ue_e1ap_id = e1ap_decode_bits(buf, 32);
}

static void e1ap_decode_tnl_info(asn1_buffer_t* buf, e1ap_tnl_info_t* tnl) {
    tnl->transport_type = e1ap_decode_bits(buf, 4);
    tnl->ip_address = e1ap_decode_bits(buf, 32);
    tnl->port = e1ap_decode_bits(buf, 16);
    tnl->teid = e1ap_decode_bits(buf, 32);
}

static void e1ap_decode_qos_flow(asn1_buffer_t* buf, e1ap_qos_flow_t* flow) {
    flow->qfi = e1ap_decode_bits(buf, 6);
    flow->five_qi = e1ap_decode_bits(buf, 8);
    flow->gfbr_ul = ((uint64_t)e1ap_decode_bits(buf, 32) << 32) | e1ap_decode_bits(buf, 32);
    flow->gfbr_dl = ((uint64_t)e1ap_decode_bits(buf, 32) << 32) | e1ap_decode_bits(buf, 32);
    flow->priority_level = e1ap_decode_bits(buf, 8);
}

static void e1ap_decode_drb_info(asn1_buffer_t* buf, e1ap_drb_info_t* drb) {
    drb->drb_id = e1ap_decode_bits(buf, 5);
    drb->pdcp_sn_size = e1ap_decode_bits(buf, 4);
    drb->rlc_mode = e1ap_decode_bits(buf, 2);
    
    drb->num_qos_flows = e1ap_decode_bits(buf, 6);
    for (int i = 0; i < drb->num_qos_flows && i < E1AP_MAX_QOS_FLOWS; i++) {
        e1ap_decode_qos_flow(buf, &drb->qos_flows[i]);
    }
    
    drb->num_dl_up_tnl_info = e1ap_decode_bits(buf, 4);
    for (int i = 0; i < drb->num_dl_up_tnl_info && i < E1AP_MAX_TNL_INFO; i++) {
        e1ap_decode_tnl_info(buf, &drb->dl_up_tnl_info[i]);
    }
}

static void e1ap_decode_pdu_session_info(asn1_buffer_t* buf, e1ap_pdu_session_info_t* session) {
    session->pdu_session_id = e1ap_decode_bits(buf, 8);
    session->pdu_session_type = e1ap_decode_bits(buf, 4);
    session->ue_ip_address = e1ap_decode_bits(buf, 32);
    
    session->s_nssai.sst = e1ap_decode_bits(buf, 8);
    session->s_nssai.sd = e1ap_decode_bits(buf, 24);
    
    session->num_drbs = e1ap_decode_bits(buf, 4);
    for (int i = 0; i < session->num_drbs && i < E1AP_MAX_DRB_ID; i++) {
        e1ap_decode_drb_info(buf, &session->drbs[i]);
    }
    
    e1ap_decode_tnl_info(buf, &session->upf_ul_tnl_info);
}

static void e1ap_decode_cause(asn1_buffer_t* buf, e1ap_cause_t* cause) {
    cause->cause_type = e1ap_decode_bits(buf, 3);
    cause->cause_value = e1ap_decode_bits(buf, 8);
}

/* ============== Message Decoding ============== */

int e1ap_decode_message(const uint8_t* buffer, size_t length, e1ap_message_t* msg) {
    if (!buffer || !msg || length == 0) {
        return -1;
    }
    
    asn1_buffer_t buf;
    buf.data = (uint8_t*)buffer;
    buf.size = length;
    buf.bit_offset = 0;
    
    memset(msg, 0, sizeof(e1ap_message_t));
    
    msg->procedure_code = e1ap_decode_bits(&buf, 8);
    msg->criticality = e1ap_decode_bits(&buf, 2);
    msg->message_type = e1ap_decode_bits(&buf, 8);
    
    switch (msg->message_type) {
        case E1AP_MSG_E1_SETUP_REQUEST: {
            e1ap_e1_setup_request_t* req = &msg->payload.e1_setup_request;
            e1ap_decode_cu_up_id(&buf, &req->gnb_cu_up_id);
            
            req->num_supported_plmns = e1ap_decode_bits(&buf, 8);
            for (int i = 0; i < req->num_supported_plmns && i < 12; i++) {
                for (int j = 0; j < 3; j++) {
                    req->supported_plmns[i][j] = e1ap_decode_bits(&buf, 8);
                }
            }
            
            req->capacity = e1ap_decode_bits(&buf, 32);
            
            req->num_tnla = e1ap_decode_bits(&buf, 4);
            for (int i = 0; i < req->num_tnla && i < E1AP_MAX_TNL_INFO; i++) {
                e1ap_decode_tnl_info(&buf, &req->tnla[i]);
            }
            break;
        }
        
        case E1AP_MSG_E1_SETUP_RESPONSE: {
            e1ap_e1_setup_response_t* resp = &msg->payload.e1_setup_response;
            e1ap_decode_cu_cp_id(&buf, &resp->gnb_cu_cp_id);
            
            resp->num_supported_plmns = e1ap_decode_bits(&buf, 8);
            for (int i = 0; i < resp->num_supported_plmns && i < 12; i++) {
                for (int j = 0; j < 3; j++) {
                    resp->supported_plmns[i][j] = e1ap_decode_bits(&buf, 8);
                }
            }
            break;
        }
        
        case E1AP_MSG_E1_SETUP_FAILURE: {
            e1ap_e1_setup_failure_t* fail = &msg->payload.e1_setup_failure;
            e1ap_decode_cause(&buf, &fail->cause);
            fail->time_to_wait = e1ap_decode_bits(&buf, 32);
            break;
        }
        
        case E1AP_MSG_BEARER_CONTEXT_SETUP_REQUEST: {
            e1ap_bearer_context_setup_request_t* req = &msg->payload.bearer_context_setup_request;
            e1ap_decode_ue_ids(&buf, &req->ue_ids);
            
            uint32_t high = e1ap_decode_bits(&buf, 8);
            uint32_t low = e1ap_decode_bits(&buf, 32);
            req->ran_ue_id.ran_ue_id = ((uint64_t)high << 32) | low;
            
            req->num_pdu_sessions = e1ap_decode_bits(&buf, 8);
            for (int i = 0; i < req->num_pdu_sessions && i < E1AP_MAX_PDU_SESSIONS; i++) {
                e1ap_decode_pdu_session_info(&buf, &req->pdu_sessions[i]);
            }
            
            req->ue_ambr_dl = ((uint64_t)e1ap_decode_bits(&buf, 32) << 32) | e1ap_decode_bits(&buf, 32);
            req->ue_ambr_ul = ((uint64_t)e1ap_decode_bits(&buf, 32) << 32) | e1ap_decode_bits(&buf, 32);
            
            req->security_key_present = e1ap_decode_bits(&buf, 1);
            if (req->security_key_present) {
                for (int i = 0; i < 32; i++) {
                    req->security_key[i] = e1ap_decode_bits(&buf, 8);
                }
            }
            break;
        }
        
        case E1AP_MSG_BEARER_CONTEXT_SETUP_RESPONSE: {
            e1ap_bearer_context_setup_response_t* resp = &msg->payload.bearer_context_setup_response;
            e1ap_decode_ue_ids(&buf, &resp->ue_ids);
            
            resp->num_pdu_sessions_setup = e1ap_decode_bits(&buf, 8);
            for (int i = 0; i < resp->num_pdu_sessions_setup && i < E1AP_MAX_PDU_SESSIONS; i++) {
                resp->pdu_session_ids[i] = e1ap_decode_bits(&buf, 8);
            }
            
            resp->num_drbs_setup = e1ap_decode_bits(&buf, 4);
            for (int i = 0; i < resp->num_drbs_setup && i < E1AP_MAX_DRB_ID; i++) {
                e1ap_decode_drb_info(&buf, &resp->drbs_setup[i]);
            }
            break;
        }
        
        case E1AP_MSG_BEARER_CONTEXT_RELEASE_COMMAND: {
            e1ap_bearer_context_release_command_t* cmd = &msg->payload.bearer_context_release_command;
            e1ap_decode_ue_ids(&buf, &cmd->ue_ids);
            e1ap_decode_cause(&buf, &cmd->cause);
            break;
        }
        
        case E1AP_MSG_BEARER_CONTEXT_RELEASE_COMPLETE: {
            e1ap_bearer_context_release_complete_t* comp = &msg->payload.bearer_context_release_complete;
            e1ap_decode_ue_ids(&buf, &comp->ue_ids);
            break;
        }
        
        case E1AP_MSG_ERROR_INDICATION: {
            e1ap_error_indication_t* err = &msg->payload.error_indication;
            err->ue_ids_present = e1ap_decode_bits(&buf, 1);
            if (err->ue_ids_present) {
                e1ap_decode_ue_ids(&buf, &err->ue_ids);
            }
            err->cause_present = e1ap_decode_bits(&buf, 1);
            if (err->cause_present) {
                e1ap_decode_cause(&buf, &err->cause);
            }
            break;
        }
        
        default:
            break;
    }
    
    return 0;
}

void e1ap_free_message(e1ap_message_t* msg) {
    (void)msg;
}