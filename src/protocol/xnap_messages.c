/*
 * 5G UE Simulation Application
 * XnAP Message Implementation
 * 3GPP TS 38.423
 */

#include "xnap_messages.h"
#include "asn1_per.h"
#include "../core/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============== Message Type Strings ============== */

static const char* xnap_message_type_strings[XNAP_MSG_MAX] = {
    "XnSetupRequest",
    "XnSetupResponse",
    "XnSetupFailure",
    "XnResetRequest",
    "XnResetResponse",
    "ErrorIndication",
    "XnRemovalRequest",
    "XnRemovalResponse",
    "Paging",
    "HandoverRequest",
    "HandoverRequestAcknowledge",
    "HandoverPreparationFailure",
    "HandoverCommand",
    "HandoverCancel",
    "HandoverCancelAcknowledge",
    "HandoverNotify",
    "RetrieveUEContextRequest",
    "RetrieveUEContextResponse",
    "RetrieveUEContextFailure",
    "UEContextTransfer",
    "UEContextTransferResponse",
    "SgNBAdditionRequest",
    "SgNBAdditionRequestAcknowledge",
    "SgNBAdditionRequestReject",
    "SgNBModificationRequest",
    "SgNBModificationRequestAcknowledge",
    "SgNBModificationRequestReject",
    "SgNBReleaseRequest",
    "SgNBReleaseRequestAcknowledge",
    "SIBTransferRequest",
    "SIBTransferResponse",
    "AccessAndMobilityIndication",
    "NeighborCellInformationRequest",
    "NeighborCellInformationResponse"
};

static const char* xnap_cause_type_strings[] = {
    "RadioNetwork",
    "Transport",
    "Protocol",
    "Misc"
};

/* ============== Utility Functions ============== */

const char* xnap_message_type_to_string(xnap_message_type_t type) {
    if (type >= XNAP_MSG_MAX) return "Unknown";
    return xnap_message_type_strings[type];
}

const char* xnap_cause_to_string(const xnap_cause_t* cause) {
    static char buf[64];
    if (!cause) return "NULL";
    
    const char* type_str = "Unknown";
    if (cause->cause_type < 4) {
        type_str = xnap_cause_type_strings[cause->cause_type];
    }
    
    snprintf(buf, sizeof(buf), "%s:%u", type_str, cause->cause_value);
    return buf;
}

/* ============== Cause Helpers ============== */

void xnap_set_cause_radio(xnap_cause_t* cause, xnap_cause_radio_value_t value) {
    if (cause) {
        cause->cause_type = XNAP_CAUSE_RADIO_NETWORK;
        cause->cause_value = (uint8_t)value;
    }
}

void xnap_set_cause_transport(xnap_cause_t* cause, xnap_cause_transport_value_t value) {
    if (cause) {
        cause->cause_type = XNAP_CAUSE_TRANSPORT;
        cause->cause_value = (uint8_t)value;
    }
}

void xnap_set_cause_protocol(xnap_cause_t* cause, xnap_cause_protocol_value_t value) {
    if (cause) {
        cause->cause_type = XNAP_CAUSE_PROTOCOL;
        cause->cause_value = (uint8_t)value;
    }
}

void xnap_set_cause_misc(xnap_cause_t* cause, xnap_cause_misc_value_t value) {
    if (cause) {
        cause->cause_type = XNAP_CAUSE_MISC;
        cause->cause_value = (uint8_t)value;
    }
}

/* ============== Message Initialization Helpers ============== */

void xnap_init_xn_setup_request(xnap_message_t* msg) {
    if (!msg) return;
    memset(msg, 0, sizeof(xnap_message_t));
    msg->message_type = XNAP_MSG_XN_SETUP_REQUEST;
    msg->procedure_code = XNAP_PROC_XN_SETUP;
    msg->criticality = 0;
}

void xnap_init_xn_setup_response(xnap_message_t* msg) {
    if (!msg) return;
    memset(msg, 0, sizeof(xnap_message_t));
    msg->message_type = XNAP_MSG_XN_SETUP_RESPONSE;
    msg->procedure_code = XNAP_PROC_XN_SETUP;
    msg->criticality = 0;
}

void xnap_init_handover_request(xnap_message_t* msg) {
    if (!msg) return;
    memset(msg, 0, sizeof(xnap_message_t));
    msg->message_type = XNAP_MSG_HANDOVER_REQUEST;
    msg->procedure_code = XNAP_PROC_HANDOVER_PREPARATION;
    msg->criticality = 0;
}

void xnap_init_handover_command(xnap_message_t* msg) {
    if (!msg) return;
    memset(msg, 0, sizeof(xnap_message_t));
    msg->message_type = XNAP_MSG_HANDOVER_COMMAND;
    msg->procedure_code = XNAP_PROC_HANDOVER_PREPARATION;
    msg->criticality = 0;
}

void xnap_init_paging(xnap_message_t* msg) {
    if (!msg) return;
    memset(msg, 0, sizeof(xnap_message_t));
    msg->message_type = XNAP_MSG_PAGING;
    msg->procedure_code = XNAP_PROC_PAGING;
    msg->criticality = 0;
}

/* ============== ASN.1 PER Encoding Helpers ============== */

static uint32_t xnap_decode_bits(asn1_buffer_t* buf, uint8_t num_bits) {
    uint32_t value;
    asn1_decode_bits(buf->data, &buf->bit_offset, &value, num_bits);
    return value;
}

static void xnap_encode_header(asn1_buffer_t* buf, const xnap_message_t* msg) {
    asn1_encode_bits(buf, msg->procedure_code, 8);
    asn1_encode_bits(buf, msg->criticality, 2);
    asn1_encode_bits(buf, msg->message_type, 8);
}

static void xnap_encode_global_gnb_id(asn1_buffer_t* buf, const xnap_global_gnb_id_t* id) {
    asn1_encode_bits(buf, id->mcc[0], 8);
    asn1_encode_bits(buf, id->mcc[1], 8);
    asn1_encode_bits(buf, id->mcc[2], 8);
    asn1_encode_bits(buf, id->mnc[0], 8);
    asn1_encode_bits(buf, id->mnc[1], 8);
    asn1_encode_bits(buf, id->mnc[2], 8);
    asn1_encode_bits(buf, id->mnc_length, 3);
    asn1_encode_bits(buf, id->gnb_id_length, 6);
    asn1_encode_bits(buf, (uint32_t)(id->gnb_id >> 32), 32);
    asn1_encode_bits(buf, (uint32_t)(id->gnb_id & 0xFFFFFFFF), 32);
}

static void xnap_encode_gnb_name(asn1_buffer_t* buf, const xnap_gnb_name_t* name) {
    uint8_t len = (uint8_t)strlen(name->name);
    asn1_encode_bits(buf, len, 8);
    for (int i = 0; i < len; i++) {
        asn1_encode_bits(buf, (uint8_t)name->name[i], 8);
    }
}

static void xnap_encode_ue_ids(asn1_buffer_t* buf, const xnap_ue_ids_t* ids) {
    asn1_encode_bits(buf, ids->source_gnb_ue_xnap_id, 32);
    asn1_encode_bits(buf, ids->target_gnb_ue_xnap_id, 32);
}

static void xnap_encode_plmn_id(asn1_buffer_t* buf, const xnap_plmn_id_t* plmn) {
    asn1_encode_bits(buf, plmn->mcc[0], 8);
    asn1_encode_bits(buf, plmn->mcc[1], 8);
    asn1_encode_bits(buf, plmn->mcc[2], 8);
    asn1_encode_bits(buf, plmn->mnc[0], 8);
    asn1_encode_bits(buf, plmn->mnc[1], 8);
    asn1_encode_bits(buf, plmn->mnc[2], 8);
    asn1_encode_bits(buf, plmn->mnc_length, 3);
}

static void xnap_encode_served_cell_info(asn1_buffer_t* buf, const xnap_served_cell_info_t* cell) {
    asn1_encode_bits(buf, (uint32_t)(cell->nr_cell_id.nr_cell_id >> 32), 32);
    asn1_encode_bits(buf, (uint32_t)(cell->nr_cell_id.nr_cell_id & 0xFFFFFFFF), 32);
    asn1_encode_bits(buf, cell->pci.pci, 10);
    asn1_encode_bits(buf, cell->tac.tac, 24);
    
    asn1_encode_bits(buf, cell->num_plmns, 4);
    for (int i = 0; i < cell->num_plmns && i < XNAP_MAX_PLMN_COUNT; i++) {
        xnap_encode_plmn_id(buf, &cell->plmns[i]);
    }
    
    asn1_encode_bits(buf, cell->num_slices, 4);
    for (int i = 0; i < cell->num_slices && i < XNAP_MAX_SLICE_COUNT; i++) {
        asn1_encode_bits(buf, cell->sst[i], 8);
        asn1_encode_bits(buf, cell->sd[i], 24);
    }
    
    asn1_encode_bits(buf, cell->ngran_duplex_mode, 4);
}

static void xnap_encode_tnl_info(asn1_buffer_t* buf, const xnap_tnl_info_t* tnl) {
    asn1_encode_bits(buf, tnl->transport_type, 4);
    asn1_encode_bits(buf, tnl->ip_address, 32);
    asn1_encode_bits(buf, tnl->port, 16);
    asn1_encode_bits(buf, tnl->teid, 32);
}

static void xnap_encode_qos_flow(asn1_buffer_t* buf, const xnap_qos_flow_t* flow) {
    asn1_encode_bits(buf, flow->qfi, 6);
    asn1_encode_bits(buf, flow->five_qi, 8);
    asn1_encode_bits(buf, (uint32_t)(flow->gfbr_ul >> 32), 32);
    asn1_encode_bits(buf, (uint32_t)(flow->gfbr_ul & 0xFFFFFFFF), 32);
    asn1_encode_bits(buf, (uint32_t)(flow->gfbr_dl >> 32), 32);
    asn1_encode_bits(buf, (uint32_t)(flow->gfbr_dl & 0xFFFFFFFF), 32);
}

static void xnap_encode_drb_info(asn1_buffer_t* buf, const xnap_drb_info_t* drb) {
    asn1_encode_bits(buf, drb->drb_id, 5);
    asn1_encode_bits(buf, drb->pdcp_sn_size, 4);
    asn1_encode_bits(buf, drb->rlc_mode, 2);
    
    asn1_encode_bits(buf, drb->num_qos_flows, 6);
    for (int i = 0; i < drb->num_qos_flows && i < XNAP_MAX_QOS_FLOWS; i++) {
        xnap_encode_qos_flow(buf, &drb->qos_flows[i]);
    }
    
    asn1_encode_bits(buf, drb->num_dl_up_tnl_info, 4);
    for (int i = 0; i < drb->num_dl_up_tnl_info && i < XNAP_MAX_DRB_COUNT; i++) {
        xnap_encode_tnl_info(buf, &drb->dl_up_tnl_info[i]);
    }
}

static void xnap_encode_cause(asn1_buffer_t* buf, const xnap_cause_t* cause) {
    asn1_encode_bits(buf, cause->cause_type, 3);
    asn1_encode_bits(buf, cause->cause_value, 8);
}

static void xnap_encode_rrc_container(asn1_buffer_t* buf, const xnap_rrc_container_t* container) {
    asn1_encode_bits(buf, (uint32_t)container->length, 16);
    for (size_t i = 0; i < container->length; i++) {
        asn1_encode_bits(buf, container->rrc_container[i], 8);
    }
}

/* ============== Message Encoding ============== */

int xnap_encode_message(const xnap_message_t* msg, uint8_t** buffer, size_t* length) {
    if (!msg || !buffer || !length) {
        return -1;
    }
    
    asn1_buffer_t buf;
    uesim_error_t result = asn1_buffer_alloc(&buf, XNAP_MAX_MESSAGE_SIZE);
    if (result != UESIM_SUCCESS) {
        return -1;
    }
    
    xnap_encode_header(&buf, msg);
    
    switch (msg->message_type) {
        case XNAP_MSG_XN_SETUP_REQUEST: {
            const xnap_xn_setup_request_t* req = &msg->payload.xn_setup_request;
            xnap_encode_global_gnb_id(&buf, &req->global_gnb_id);
            xnap_encode_gnb_name(&buf, &req->gnb_name);
            
            asn1_encode_bits(&buf, req->num_served_cells, 8);
            for (int i = 0; i < req->num_served_cells && i < XNAP_MAX_SERVED_CELL_COUNT; i++) {
                xnap_encode_served_cell_info(&buf, &req->served_cells[i]);
            }
            
            asn1_encode_bits(&buf, req->gnb_cu_id_present, 1);
            if (req->gnb_cu_id_present) {
                asn1_encode_bits(&buf, (uint32_t)(req->gnb_cu_id >> 32), 32);
                asn1_encode_bits(&buf, (uint32_t)(req->gnb_cu_id & 0xFFFFFFFF), 32);
            }
            
            asn1_encode_bits(&buf, req->gnb_du_id_present, 1);
            if (req->gnb_du_id_present) {
                asn1_encode_bits(&buf, req->gnb_du_id, 32);
            }
            break;
        }
        
        case XNAP_MSG_XN_SETUP_RESPONSE: {
            const xnap_xn_setup_response_t* resp = &msg->payload.xn_setup_response;
            xnap_encode_global_gnb_id(&buf, &resp->global_gnb_id);
            xnap_encode_gnb_name(&buf, &resp->gnb_name);
            
            asn1_encode_bits(&buf, resp->num_served_cells, 8);
            for (int i = 0; i < resp->num_served_cells && i < XNAP_MAX_SERVED_CELL_COUNT; i++) {
                xnap_encode_served_cell_info(&buf, &resp->served_cells[i]);
            }
            
            asn1_encode_bits(&buf, resp->gnb_cu_id_present, 1);
            if (resp->gnb_cu_id_present) {
                asn1_encode_bits(&buf, (uint32_t)(resp->gnb_cu_id >> 32), 32);
                asn1_encode_bits(&buf, (uint32_t)(resp->gnb_cu_id & 0xFFFFFFFF), 32);
            }
            break;
        }
        
        case XNAP_MSG_XN_SETUP_FAILURE: {
            const xnap_xn_setup_failure_t* fail = &msg->payload.xn_setup_failure;
            xnap_encode_cause(&buf, &fail->cause);
            asn1_encode_bits(&buf, fail->time_to_wait, 32);
            break;
        }
        
        case XNAP_MSG_HANDOVER_REQUEST: {
            const xnap_handover_request_t* req = &msg->payload.handover_request;
            xnap_encode_ue_ids(&buf, &req->ue_ids);
            
            asn1_encode_bits(&buf, (uint32_t)(req->amf_ue_id.amf_ue_id >> 32), 8);
            asn1_encode_bits(&buf, (uint32_t)(req->amf_ue_id.amf_ue_id & 0xFFFFFFFF), 32);
            asn1_encode_bits(&buf, (uint32_t)(req->ran_ue_id.ran_ue_id >> 32), 8);
            asn1_encode_bits(&buf, (uint32_t)(req->ran_ue_id.ran_ue_id & 0xFFFFFFFF), 32);
            
            xnap_encode_global_gnb_id(&buf, &req->target_gnb_id);
            asn1_encode_bits(&buf, (uint32_t)(req->target_cell_id.nr_cell_id >> 32), 32);
            asn1_encode_bits(&buf, (uint32_t)(req->target_cell_id.nr_cell_id & 0xFFFFFFFF), 32);
            
            xnap_encode_rrc_container(&buf, &req->source_to_target_container);
            
            asn1_encode_bits(&buf, req->num_drbs_to_setup, 4);
            for (int i = 0; i < req->num_drbs_to_setup && i < XNAP_MAX_DRB_COUNT; i++) {
                xnap_encode_drb_info(&buf, &req->drbs_to_setup[i]);
            }
            
            asn1_encode_bits(&buf, (uint32_t)(req->ue_ambr_dl >> 32), 32);
            asn1_encode_bits(&buf, (uint32_t)(req->ue_ambr_dl & 0xFFFFFFFF), 32);
            asn1_encode_bits(&buf, (uint32_t)(req->ue_ambr_ul >> 32), 32);
            asn1_encode_bits(&buf, (uint32_t)(req->ue_ambr_ul & 0xFFFFFFFF), 32);
            
            asn1_encode_bits(&buf, req->snssai_present, 1);
            if (req->snssai_present) {
                asn1_encode_bits(&buf, req->sst, 8);
                asn1_encode_bits(&buf, req->sd, 24);
            }
            
            asn1_encode_bits(&buf, req->security_key_present, 1);
            if (req->security_key_present) {
                for (int i = 0; i < 32; i++) {
                    asn1_encode_bits(&buf, req->security_key[i], 8);
                }
            }
            
            xnap_encode_plmn_id(&buf, &req->plmn);
            break;
        }
        
        case XNAP_MSG_HANDOVER_REQUEST_ACKNOWLEDGE: {
            const xnap_handover_request_ack_t* ack = &msg->payload.handover_request_ack;
            xnap_encode_ue_ids(&buf, &ack->ue_ids);
            xnap_encode_rrc_container(&buf, &ack->target_to_source_container);
            
            asn1_encode_bits(&buf, ack->num_drbs_setup, 4);
            for (int i = 0; i < ack->num_drbs_setup && i < XNAP_MAX_DRB_COUNT; i++) {
                xnap_encode_drb_info(&buf, &ack->drbs_setup[i]);
            }
            
            asn1_encode_bits(&buf, ack->num_drbs_failed, 4);
            for (int i = 0; i < ack->num_drbs_failed && i < XNAP_MAX_DRB_COUNT; i++) {
                asn1_encode_bits(&buf, ack->drbs_failed[i], 5);
                xnap_encode_cause(&buf, &ack->drb_fail_causes[i]);
            }
            break;
        }
        
        case XNAP_MSG_HANDOVER_PREPARATION_FAILURE: {
            const xnap_handover_preparation_failure_t* fail = &msg->payload.handover_preparation_failure;
            xnap_encode_ue_ids(&buf, &fail->ue_ids);
            xnap_encode_cause(&buf, &fail->cause);
            break;
        }
        
        case XNAP_MSG_HANDOVER_COMMAND: {
            const xnap_handover_command_t* cmd = &msg->payload.handover_command;
            xnap_encode_ue_ids(&buf, &cmd->ue_ids);
            xnap_encode_rrc_container(&buf, &cmd->handover_command);
            break;
        }
        
        case XNAP_MSG_HANDOVER_CANCEL: {
            const xnap_handover_cancel_t* cancel = &msg->payload.handover_cancel;
            xnap_encode_ue_ids(&buf, &cancel->ue_ids);
            xnap_encode_cause(&buf, &cancel->cause);
            break;
        }
        
        case XNAP_MSG_HANDOVER_CANCEL_ACKNOWLEDGE: {
            const xnap_handover_cancel_ack_t* ack = &msg->payload.handover_cancel_ack;
            xnap_encode_ue_ids(&buf, &ack->ue_ids);
            break;
        }
        
        case XNAP_MSG_HANDOVER_NOTIFY: {
            const xnap_handover_notify_t* notify = &msg->payload.handover_notify;
            xnap_encode_ue_ids(&buf, &notify->ue_ids);
            asn1_encode_bits(&buf, (uint32_t)(notify->target_cell_id.nr_cell_id >> 32), 32);
            asn1_encode_bits(&buf, (uint32_t)(notify->target_cell_id.nr_cell_id & 0xFFFFFFFF), 32);
            asn1_encode_bits(&buf, notify->pci.pci, 10);
            break;
        }
        
        case XNAP_MSG_PAGING: {
            const xnap_paging_t* paging = &msg->payload.paging;
            asn1_encode_bits(&buf, (uint32_t)(paging->amf_ue_id.amf_ue_id >> 32), 8);
            asn1_encode_bits(&buf, (uint32_t)(paging->amf_ue_id.amf_ue_id & 0xFFFFFFFF), 32);
            asn1_encode_bits(&buf, paging->ue_identity_index, 16);
            asn1_encode_bits(&buf, paging->paging_identity_type, 1);
            asn1_encode_bits(&buf, (uint32_t)(paging->paging_identity >> 32), 32);
            asn1_encode_bits(&buf, (uint32_t)(paging->paging_identity & 0xFFFFFFFF), 32);
            
            asn1_encode_bits(&buf, paging->num_tais, 4);
            for (int i = 0; i < paging->num_tais && i < XNAP_MAX_TAI_COUNT; i++) {
                asn1_encode_bits(&buf, paging->tai_list[i].tac, 24);
            }
            
            asn1_encode_bits(&buf, paging->paging_cause, 1);
            
            asn1_encode_bits(&buf, paging->assistance_data_present, 1);
            if (paging->assistance_data_present) {
                asn1_encode_bits(&buf, paging->paging_attempt_count, 4);
                asn1_encode_bits(&buf, paging->intended_n_paging_attempts, 4);
            }
            break;
        }
        
        case XNAP_MSG_ERROR_INDICATION: {
            const xnap_error_indication_t* err = &msg->payload.error_indication;
            asn1_encode_bits(&buf, err->ue_ids_present, 1);
            if (err->ue_ids_present) {
                xnap_encode_ue_ids(&buf, &err->ue_ids);
            }
            asn1_encode_bits(&buf, err->cause_present, 1);
            if (err->cause_present) {
                xnap_encode_cause(&buf, &err->cause);
            }
            break;
        }
        
        case XNAP_MSG_SGNB_ADDITION_REQUEST: {
            const xnap_sgnb_addition_request_t* req = &msg->payload.sgnb_addition_request;
            xnap_encode_ue_ids(&buf, &req->ue_ids);
            asn1_encode_bits(&buf, (uint32_t)(req->ran_ue_id.ran_ue_id >> 32), 8);
            asn1_encode_bits(&buf, (uint32_t)(req->ran_ue_id.ran_ue_id & 0xFFFFFFFF), 32);
            asn1_encode_bits(&buf, (uint32_t)(req->target_cell_id.nr_cell_id >> 32), 32);
            asn1_encode_bits(&buf, (uint32_t)(req->target_cell_id.nr_cell_id & 0xFFFFFFFF), 32);
            
            asn1_encode_bits(&buf, req->num_drbs_to_setup, 4);
            for (int i = 0; i < req->num_drbs_to_setup && i < XNAP_MAX_DRB_COUNT; i++) {
                xnap_encode_drb_info(&buf, &req->drbs_to_setup[i]);
            }
            
            asn1_encode_bits(&buf, req->security_key_present, 1);
            if (req->security_key_present) {
                for (int i = 0; i < 32; i++) {
                    asn1_encode_bits(&buf, req->security_key[i], 8);
                }
            }
            break;
        }
        
        case XNAP_MSG_SGNB_ADDITION_REQUEST_ACKNOWLEDGE: {
            const xnap_sgnb_addition_request_ack_t* ack = &msg->payload.sgnb_addition_request_ack;
            xnap_encode_ue_ids(&buf, &ack->ue_ids);
            
            asn1_encode_bits(&buf, ack->num_drbs_setup, 4);
            for (int i = 0; i < ack->num_drbs_setup && i < XNAP_MAX_DRB_COUNT; i++) {
                xnap_encode_drb_info(&buf, &ack->drbs_setup[i]);
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

static void xnap_decode_global_gnb_id(asn1_buffer_t* buf, xnap_global_gnb_id_t* id) {
    id->mcc[0] = xnap_decode_bits(buf, 8);
    id->mcc[1] = xnap_decode_bits(buf, 8);
    id->mcc[2] = xnap_decode_bits(buf, 8);
    id->mnc[0] = xnap_decode_bits(buf, 8);
    id->mnc[1] = xnap_decode_bits(buf, 8);
    id->mnc[2] = xnap_decode_bits(buf, 8);
    id->mnc_length = xnap_decode_bits(buf, 3);
    id->gnb_id_length = xnap_decode_bits(buf, 6);
    id->gnb_id = ((uint64_t)xnap_decode_bits(buf, 32) << 32) | xnap_decode_bits(buf, 32);
}

static void xnap_decode_gnb_name(asn1_buffer_t* buf, xnap_gnb_name_t* name) {
    uint8_t len = xnap_decode_bits(buf, 8);
    for (int i = 0; i < len && i < 63; i++) {
        name->name[i] = (char)xnap_decode_bits(buf, 8);
    }
    name->name[len < 64 ? len : 63] = '\0';
}

static void xnap_decode_ue_ids(asn1_buffer_t* buf, xnap_ue_ids_t* ids) {
    ids->source_gnb_ue_xnap_id = xnap_decode_bits(buf, 32);
    ids->target_gnb_ue_xnap_id = xnap_decode_bits(buf, 32);
}

static void xnap_decode_plmn_id(asn1_buffer_t* buf, xnap_plmn_id_t* plmn) {
    plmn->mcc[0] = xnap_decode_bits(buf, 8);
    plmn->mcc[1] = xnap_decode_bits(buf, 8);
    plmn->mcc[2] = xnap_decode_bits(buf, 8);
    plmn->mnc[0] = xnap_decode_bits(buf, 8);
    plmn->mnc[1] = xnap_decode_bits(buf, 8);
    plmn->mnc[2] = xnap_decode_bits(buf, 8);
    plmn->mnc_length = xnap_decode_bits(buf, 3);
}

static void xnap_decode_served_cell_info(asn1_buffer_t* buf, xnap_served_cell_info_t* cell) {
    cell->nr_cell_id.nr_cell_id = ((uint64_t)xnap_decode_bits(buf, 32) << 32) | xnap_decode_bits(buf, 32);
    cell->pci.pci = xnap_decode_bits(buf, 10);
    cell->tac.tac = xnap_decode_bits(buf, 24);
    
    cell->num_plmns = xnap_decode_bits(buf, 4);
    for (int i = 0; i < cell->num_plmns && i < XNAP_MAX_PLMN_COUNT; i++) {
        xnap_decode_plmn_id(buf, &cell->plmns[i]);
    }
    
    cell->num_slices = xnap_decode_bits(buf, 4);
    for (int i = 0; i < cell->num_slices && i < XNAP_MAX_SLICE_COUNT; i++) {
        cell->sst[i] = xnap_decode_bits(buf, 8);
        cell->sd[i] = xnap_decode_bits(buf, 24);
    }
    
    cell->ngran_duplex_mode = xnap_decode_bits(buf, 4);
}

static void xnap_decode_tnl_info(asn1_buffer_t* buf, xnap_tnl_info_t* tnl) {
    tnl->transport_type = xnap_decode_bits(buf, 4);
    tnl->ip_address = xnap_decode_bits(buf, 32);
    tnl->port = xnap_decode_bits(buf, 16);
    tnl->teid = xnap_decode_bits(buf, 32);
}

static void xnap_decode_qos_flow(asn1_buffer_t* buf, xnap_qos_flow_t* flow) {
    flow->qfi = xnap_decode_bits(buf, 6);
    flow->five_qi = xnap_decode_bits(buf, 8);
    flow->gfbr_ul = ((uint64_t)xnap_decode_bits(buf, 32) << 32) | xnap_decode_bits(buf, 32);
    flow->gfbr_dl = ((uint64_t)xnap_decode_bits(buf, 32) << 32) | xnap_decode_bits(buf, 32);
}

static void xnap_decode_drb_info(asn1_buffer_t* buf, xnap_drb_info_t* drb) {
    drb->drb_id = xnap_decode_bits(buf, 5);
    drb->pdcp_sn_size = xnap_decode_bits(buf, 4);
    drb->rlc_mode = xnap_decode_bits(buf, 2);
    
    drb->num_qos_flows = xnap_decode_bits(buf, 6);
    for (int i = 0; i < drb->num_qos_flows && i < XNAP_MAX_QOS_FLOWS; i++) {
        xnap_decode_qos_flow(buf, &drb->qos_flows[i]);
    }
    
    drb->num_dl_up_tnl_info = xnap_decode_bits(buf, 4);
    for (int i = 0; i < drb->num_dl_up_tnl_info && i < XNAP_MAX_DRB_COUNT; i++) {
        xnap_decode_tnl_info(buf, &drb->dl_up_tnl_info[i]);
    }
}

static void xnap_decode_cause(asn1_buffer_t* buf, xnap_cause_t* cause) {
    cause->cause_type = xnap_decode_bits(buf, 3);
    cause->cause_value = xnap_decode_bits(buf, 8);
}

static void xnap_decode_rrc_container(asn1_buffer_t* buf, xnap_rrc_container_t* container) {
    container->length = xnap_decode_bits(buf, 16);
    for (size_t i = 0; i < container->length && i < XNAP_MAX_RRC_CONTAINER_SIZE; i++) {
        container->rrc_container[i] = xnap_decode_bits(buf, 8);
    }
}

/* ============== Message Decoding ============== */

int xnap_decode_message(const uint8_t* buffer, size_t length, xnap_message_t* msg) {
    if (!buffer || !msg || length == 0) {
        return -1;
    }
    
    asn1_buffer_t buf;
    buf.data = (uint8_t*)buffer;
    buf.size = length;
    buf.bit_offset = 0;
    
    memset(msg, 0, sizeof(xnap_message_t));
    
    msg->procedure_code = xnap_decode_bits(&buf, 8);
    msg->criticality = xnap_decode_bits(&buf, 2);
    msg->message_type = xnap_decode_bits(&buf, 8);
    
    switch (msg->message_type) {
        case XNAP_MSG_XN_SETUP_REQUEST: {
            xnap_xn_setup_request_t* req = &msg->payload.xn_setup_request;
            xnap_decode_global_gnb_id(&buf, &req->global_gnb_id);
            xnap_decode_gnb_name(&buf, &req->gnb_name);
            
            req->num_served_cells = xnap_decode_bits(&buf, 8);
            for (int i = 0; i < req->num_served_cells && i < XNAP_MAX_SERVED_CELL_COUNT; i++) {
                xnap_decode_served_cell_info(&buf, &req->served_cells[i]);
            }
            
            req->gnb_cu_id_present = xnap_decode_bits(&buf, 1);
            if (req->gnb_cu_id_present) {
                req->gnb_cu_id = ((uint64_t)xnap_decode_bits(&buf, 32) << 32) | xnap_decode_bits(&buf, 32);
            }
            
            req->gnb_du_id_present = xnap_decode_bits(&buf, 1);
            if (req->gnb_du_id_present) {
                req->gnb_du_id = xnap_decode_bits(&buf, 32);
            }
            break;
        }
        
        case XNAP_MSG_XN_SETUP_RESPONSE: {
            xnap_xn_setup_response_t* resp = &msg->payload.xn_setup_response;
            xnap_decode_global_gnb_id(&buf, &resp->global_gnb_id);
            xnap_decode_gnb_name(&buf, &resp->gnb_name);
            
            resp->num_served_cells = xnap_decode_bits(&buf, 8);
            for (int i = 0; i < resp->num_served_cells && i < XNAP_MAX_SERVED_CELL_COUNT; i++) {
                xnap_decode_served_cell_info(&buf, &resp->served_cells[i]);
            }
            
            resp->gnb_cu_id_present = xnap_decode_bits(&buf, 1);
            if (resp->gnb_cu_id_present) {
                resp->gnb_cu_id = ((uint64_t)xnap_decode_bits(&buf, 32) << 32) | xnap_decode_bits(&buf, 32);
            }
            break;
        }
        
        case XNAP_MSG_XN_SETUP_FAILURE: {
            xnap_xn_setup_failure_t* fail = &msg->payload.xn_setup_failure;
            xnap_decode_cause(&buf, &fail->cause);
            fail->time_to_wait = xnap_decode_bits(&buf, 32);
            break;
        }
        
        case XNAP_MSG_HANDOVER_REQUEST: {
            xnap_handover_request_t* req = &msg->payload.handover_request;
            xnap_decode_ue_ids(&buf, &req->ue_ids);
            
            req->amf_ue_id.amf_ue_id = ((uint64_t)xnap_decode_bits(&buf, 8) << 32) | xnap_decode_bits(&buf, 32);
            req->ran_ue_id.ran_ue_id = ((uint64_t)xnap_decode_bits(&buf, 8) << 32) | xnap_decode_bits(&buf, 32);
            
            xnap_decode_global_gnb_id(&buf, &req->target_gnb_id);
            req->target_cell_id.nr_cell_id = ((uint64_t)xnap_decode_bits(&buf, 32) << 32) | xnap_decode_bits(&buf, 32);
            
            xnap_decode_rrc_container(&buf, &req->source_to_target_container);
            
            req->num_drbs_to_setup = xnap_decode_bits(&buf, 4);
            for (int i = 0; i < req->num_drbs_to_setup && i < XNAP_MAX_DRB_COUNT; i++) {
                xnap_decode_drb_info(&buf, &req->drbs_to_setup[i]);
            }
            
            req->ue_ambr_dl = ((uint64_t)xnap_decode_bits(&buf, 32) << 32) | xnap_decode_bits(&buf, 32);
            req->ue_ambr_ul = ((uint64_t)xnap_decode_bits(&buf, 32) << 32) | xnap_decode_bits(&buf, 32);
            
            req->snssai_present = xnap_decode_bits(&buf, 1);
            if (req->snssai_present) {
                req->sst = xnap_decode_bits(&buf, 8);
                req->sd = xnap_decode_bits(&buf, 24);
            }
            
            req->security_key_present = xnap_decode_bits(&buf, 1);
            if (req->security_key_present) {
                for (int i = 0; i < 32; i++) {
                    req->security_key[i] = xnap_decode_bits(&buf, 8);
                }
            }
            
            xnap_decode_plmn_id(&buf, &req->plmn);
            break;
        }
        
        case XNAP_MSG_HANDOVER_REQUEST_ACKNOWLEDGE: {
            xnap_handover_request_ack_t* ack = &msg->payload.handover_request_ack;
            xnap_decode_ue_ids(&buf, &ack->ue_ids);
            xnap_decode_rrc_container(&buf, &ack->target_to_source_container);
            
            ack->num_drbs_setup = xnap_decode_bits(&buf, 4);
            for (int i = 0; i < ack->num_drbs_setup && i < XNAP_MAX_DRB_COUNT; i++) {
                xnap_decode_drb_info(&buf, &ack->drbs_setup[i]);
            }
            
            ack->num_drbs_failed = xnap_decode_bits(&buf, 4);
            for (int i = 0; i < ack->num_drbs_failed && i < XNAP_MAX_DRB_COUNT; i++) {
                ack->drbs_failed[i] = xnap_decode_bits(&buf, 5);
                xnap_decode_cause(&buf, &ack->drb_fail_causes[i]);
            }
            break;
        }
        
        case XNAP_MSG_HANDOVER_PREPARATION_FAILURE: {
            xnap_handover_preparation_failure_t* fail = &msg->payload.handover_preparation_failure;
            xnap_decode_ue_ids(&buf, &fail->ue_ids);
            xnap_decode_cause(&buf, &fail->cause);
            break;
        }
        
        case XNAP_MSG_HANDOVER_COMMAND: {
            xnap_handover_command_t* cmd = &msg->payload.handover_command;
            xnap_decode_ue_ids(&buf, &cmd->ue_ids);
            xnap_decode_rrc_container(&buf, &cmd->handover_command);
            break;
        }
        
        case XNAP_MSG_HANDOVER_CANCEL: {
            xnap_handover_cancel_t* cancel = &msg->payload.handover_cancel;
            xnap_decode_ue_ids(&buf, &cancel->ue_ids);
            xnap_decode_cause(&buf, &cancel->cause);
            break;
        }
        
        case XNAP_MSG_HANDOVER_CANCEL_ACKNOWLEDGE: {
            xnap_handover_cancel_ack_t* ack = &msg->payload.handover_cancel_ack;
            xnap_decode_ue_ids(&buf, &ack->ue_ids);
            break;
        }
        
        case XNAP_MSG_HANDOVER_NOTIFY: {
            xnap_handover_notify_t* notify = &msg->payload.handover_notify;
            xnap_decode_ue_ids(&buf, &notify->ue_ids);
            notify->target_cell_id.nr_cell_id = ((uint64_t)xnap_decode_bits(&buf, 32) << 32) | xnap_decode_bits(&buf, 32);
            notify->pci.pci = xnap_decode_bits(&buf, 10);
            break;
        }
        
        case XNAP_MSG_PAGING: {
            xnap_paging_t* paging = &msg->payload.paging;
            paging->amf_ue_id.amf_ue_id = ((uint64_t)xnap_decode_bits(&buf, 8) << 32) | xnap_decode_bits(&buf, 32);
            paging->ue_identity_index = xnap_decode_bits(&buf, 16);
            paging->paging_identity_type = xnap_decode_bits(&buf, 1);
            paging->paging_identity = ((uint64_t)xnap_decode_bits(&buf, 32) << 32) | xnap_decode_bits(&buf, 32);
            
            paging->num_tais = xnap_decode_bits(&buf, 4);
            for (int i = 0; i < paging->num_tais && i < XNAP_MAX_TAI_COUNT; i++) {
                paging->tai_list[i].tac = xnap_decode_bits(&buf, 24);
            }
            
            paging->paging_cause = xnap_decode_bits(&buf, 1);
            
            paging->assistance_data_present = xnap_decode_bits(&buf, 1);
            if (paging->assistance_data_present) {
                paging->paging_attempt_count = xnap_decode_bits(&buf, 4);
                paging->intended_n_paging_attempts = xnap_decode_bits(&buf, 4);
            }
            break;
        }
        
        case XNAP_MSG_ERROR_INDICATION: {
            xnap_error_indication_t* err = &msg->payload.error_indication;
            err->ue_ids_present = xnap_decode_bits(&buf, 1);
            if (err->ue_ids_present) {
                xnap_decode_ue_ids(&buf, &err->ue_ids);
            }
            err->cause_present = xnap_decode_bits(&buf, 1);
            if (err->cause_present) {
                xnap_decode_cause(&buf, &err->cause);
            }
            break;
        }
        
        case XNAP_MSG_SGNB_ADDITION_REQUEST: {
            xnap_sgnb_addition_request_t* req = &msg->payload.sgnb_addition_request;
            xnap_decode_ue_ids(&buf, &req->ue_ids);
            req->ran_ue_id.ran_ue_id = ((uint64_t)xnap_decode_bits(&buf, 8) << 32) | xnap_decode_bits(&buf, 32);
            req->target_cell_id.nr_cell_id = ((uint64_t)xnap_decode_bits(&buf, 32) << 32) | xnap_decode_bits(&buf, 32);
            
            req->num_drbs_to_setup = xnap_decode_bits(&buf, 4);
            for (int i = 0; i < req->num_drbs_to_setup && i < XNAP_MAX_DRB_COUNT; i++) {
                xnap_decode_drb_info(&buf, &req->drbs_to_setup[i]);
            }
            
            req->security_key_present = xnap_decode_bits(&buf, 1);
            if (req->security_key_present) {
                for (int i = 0; i < 32; i++) {
                    req->security_key[i] = xnap_decode_bits(&buf, 8);
                }
            }
            break;
        }
        
        case XNAP_MSG_SGNB_ADDITION_REQUEST_ACKNOWLEDGE: {
            xnap_sgnb_addition_request_ack_t* ack = &msg->payload.sgnb_addition_request_ack;
            xnap_decode_ue_ids(&buf, &ack->ue_ids);
            
            ack->num_drbs_setup = xnap_decode_bits(&buf, 4);
            for (int i = 0; i < ack->num_drbs_setup && i < XNAP_MAX_DRB_COUNT; i++) {
                xnap_decode_drb_info(&buf, &ack->drbs_setup[i]);
            }
            break;
        }
        
        default:
            break;
    }
    
    return 0;
}

void xnap_free_message(xnap_message_t* msg) {
    (void)msg;
}
