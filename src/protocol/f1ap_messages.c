/*
 * 5G UE Simulation Application
 * F1AP Message Implementation
 * 3GPP TS 38.473
 */

#include "f1ap_messages.h"
#include "asn1_per.h"
#include "../core/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============== Message Type Strings ============== */

static const char* f1ap_message_type_strings[F1AP_MSG_MAX] = {
    "F1SetupRequest",
    "F1SetupResponse",
    "F1SetupFailure",
    "F1ResetRequest",
    "F1ResetResponse",
    "ErrorIndication",
    "F1RemovalRequest",
    "F1RemovalResponse",
    "GNBDUConfigUpdate",
    "GNBDUConfigUpdateAcknowledge",
    "GNBDUConfigUpdateFailure",
    "GNBCUConfigUpdate",
    "GNBCUConfigUpdateAcknowledge",
    "GNBCUConfigUpdateFailure",
    "UEContextSetupRequest",
    "UEContextSetupResponse",
    "UEContextSetupFailure",
    "UEContextReleaseCommand",
    "UEContextReleaseComplete",
    "UEContextModificationRequest",
    "UEContextModificationResponse",
    "UEContextModificationFailure",
    "UEContextReleaseRequest",
    "DLRRCMessageTransfer",
    "ULRRCMessageTransfer",
    "Notify",
    "SystemInformationDeliveryCommand",
    "AccessAndMobilityIndication"
};

static const char* f1ap_cause_type_strings[] = {
    "RadioNetwork",
    "Transport",
    "Protocol",
    "Misc"
};

/* ============== Utility Functions ============== */

const char* f1ap_message_type_to_string(f1ap_message_type_t type) {
    if (type >= F1AP_MSG_MAX) return "Unknown";
    return f1ap_message_type_strings[type];
}

const char* f1ap_cause_to_string(const f1ap_cause_t* cause) {
    static char buf[64];
    if (cause == NULL) return "NULL";
    
    const char* type_str = "Unknown";
    if (cause->cause_type < 4) {
        type_str = f1ap_cause_type_strings[cause->cause_type];
    }
    
    snprintf(buf, sizeof(buf), "%s:%u", type_str, cause->cause_value);
    return buf;
}

/* ============== Cause Helpers ============== */

void f1ap_set_cause_radio(f1ap_cause_t* cause, f1ap_cause_radio_value_t value) {
    if (cause) {
        cause->cause_type = F1AP_CAUSE_RADIO_NETWORK;
        cause->cause_value = (uint8_t)value;
    }
}

void f1ap_set_cause_transport(f1ap_cause_t* cause, f1ap_cause_transport_value_t value) {
    if (cause) {
        cause->cause_type = F1AP_CAUSE_TRANSPORT;
        cause->cause_value = (uint8_t)value;
    }
}

void f1ap_set_cause_protocol(f1ap_cause_t* cause, f1ap_cause_protocol_value_t value) {
    if (cause) {
        cause->cause_type = F1AP_CAUSE_PROTOCOL;
        cause->cause_value = (uint8_t)value;
    }
}

void f1ap_set_cause_misc(f1ap_cause_t* cause, f1ap_cause_misc_value_t value) {
    if (cause) {
        cause->cause_type = F1AP_CAUSE_MISC;
        cause->cause_value = (uint8_t)value;
    }
}

/* ============== Message Initialization Helpers ============== */

void f1ap_init_f1_setup_request(f1ap_message_t* msg) {
    if (!msg) return;
    memset(msg, 0, sizeof(f1ap_message_t));
    msg->message_type = F1AP_MSG_F1_SETUP_REQUEST;
    msg->procedure_code = F1AP_PROC_F1_SETUP;
    msg->criticality = 0; /* reject */
    msg->payload.f1_setup_request.gnb_du_rrc_version[0] = 15; /* v15.0.0 */
}

void f1ap_init_f1_setup_response(f1ap_message_t* msg) {
    if (!msg) return;
    memset(msg, 0, sizeof(f1ap_message_t));
    msg->message_type = F1AP_MSG_F1_SETUP_RESPONSE;
    msg->procedure_code = F1AP_PROC_F1_SETUP;
    msg->criticality = 0;
    msg->payload.f1_setup_response.gnb_cu_rrc_version[0] = 15;
}

void f1ap_init_ue_context_setup_request(f1ap_message_t* msg) {
    if (!msg) return;
    memset(msg, 0, sizeof(f1ap_message_t));
    msg->message_type = F1AP_MSG_UE_CONTEXT_SETUP_REQUEST;
    msg->procedure_code = F1AP_PROC_UE_CONTEXT_SETUP;
    msg->criticality = 0;
}

void f1ap_init_ue_context_release_request(f1ap_message_t* msg) {
    if (!msg) return;
    memset(msg, 0, sizeof(f1ap_message_t));
    msg->message_type = F1AP_MSG_UE_CONTEXT_RELEASE_REQUEST;
    msg->procedure_code = F1AP_PROC_UE_CONTEXT_RELEASE_REQUEST;
    msg->criticality = 0;
}

void f1ap_init_dl_rrc_message_transfer(f1ap_message_t* msg) {
    if (!msg) return;
    memset(msg, 0, sizeof(f1ap_message_t));
    msg->message_type = F1AP_MSG_DL_RRC_MESSAGE_TRANSFER;
    msg->procedure_code = F1AP_PROC_DL_RRC_MESSAGE_TRANSFER;
    msg->criticality = 1; /* ignore */
}

void f1ap_init_ul_rrc_message_transfer(f1ap_message_t* msg) {
    if (!msg) return;
    memset(msg, 0, sizeof(f1ap_message_t));
    msg->message_type = F1AP_MSG_UL_RRC_MESSAGE_TRANSFER;
    msg->procedure_code = F1AP_PROC_UL_RRC_MESSAGE_TRANSFER;
    msg->criticality = 1;
}

/* ============== ASN.1 PER Encoding Helpers ============== */

/* Encode procedure code and message type */
static void f1ap_encode_header(asn1_buffer_t* buf, const f1ap_message_t* msg) {
    /* Procedure Code (8 bits) */
    asn1_encode_bits(buf, msg->procedure_code, 8);
    
    /* Criticality (2 bits) */
    asn1_encode_bits(buf, msg->criticality, 2);
    
    /* Message Type (depends on procedure) */
    asn1_encode_bits(buf, msg->message_type, 8);
}

/* Encode gNB-DU ID */
static void f1ap_encode_gnb_du_id(asn1_buffer_t* buf, const f1ap_gnb_du_id_t* id) {
    /* gNB-DU ID (32 bits) */
    asn1_encode_bits(buf, id->gnb_du_id, 32);
    
    /* gNB-DU Name (variable length) */
    uint8_t len = (uint8_t)strlen((const char*)id->gnb_du_name);
    asn1_encode_bits(buf, len, 8);
    for (int i = 0; i < len; i++) {
        asn1_encode_bits(buf, id->gnb_du_name[i], 8);
    }
}

/* Encode gNB-CU ID */
static void f1ap_encode_gnb_cu_id(asn1_buffer_t* buf, const f1ap_gnb_cu_id_t* id) {
    /* gNB-CU ID (22-32 bits, use 32) */
    asn1_encode_bits(buf, (uint32_t)(id->gnb_cu_id & 0xFFFFFFFF), 32);
    
    /* gNB-CU Name */
    uint8_t len = (uint8_t)strlen((const char*)id->gnb_cu_name);
    asn1_encode_bits(buf, len, 8);
    for (int i = 0; i < len; i++) {
        asn1_encode_bits(buf, id->gnb_cu_name[i], 8);
    }
}

/* Encode UE IDs */
static void f1ap_encode_ue_ids(asn1_buffer_t* buf, const f1ap_ue_ids_t* ids) {
    /* gNB-CU-UE-F1AP-ID (32 bits) */
    asn1_encode_bits(buf, ids->gnb_cu_ue_f1ap_id, 32);
    
    /* gNB-DU-UE-F1AP-ID (32 bits) */
    asn1_encode_bits(buf, ids->gnb_du_ue_f1ap_id, 32);
}

/* Encode NR Cell ID */
static void f1ap_encode_nr_cell_id(asn1_buffer_t* buf, const f1ap_nr_cell_id_t* id) {
    /* NR Cell ID (36 bits) */
    asn1_encode_bits(buf, (uint32_t)((id->nr_cell_id >> 32) & 0xF), 4);
    asn1_encode_bits(buf, (uint32_t)(id->nr_cell_id & 0xFFFFFFFF), 32);
}

/* Encode PLMN ID */
static void f1ap_encode_plmn_id(asn1_buffer_t* buf, const f1ap_plmn_id_t* plmn) {
    /* MCC (3 digits encoded as BCD) */
    asn1_encode_bits(buf, plmn->mcc[0] | (plmn->mcc[1] << 4), 8);
    asn1_encode_bits(buf, plmn->mcc[2] | (0xF << 4), 8);
    
    /* MNC (2 or 3 digits) */
    if (plmn->mnc_length == 2) {
        asn1_encode_bits(buf, plmn->mnc[0] | (plmn->mnc[1] << 4), 8);
        asn1_encode_bits(buf, 0xF0, 8);
    } else {
        asn1_encode_bits(buf, plmn->mnc[0] | (plmn->mnc[1] << 4), 8);
        asn1_encode_bits(buf, plmn->mnc[2] | (0xF << 4), 8);
    }
}

/* Encode served cell info */
static void f1ap_encode_served_cell_info(asn1_buffer_t* buf, const f1ap_served_cell_info_t* cell) {
    /* NR Cell ID */
    f1ap_encode_nr_cell_id(buf, &cell->nr_cell_id);
    
    /* NR PCI (10 bits) */
    asn1_encode_bits(buf, cell->pci.pci, 10);
    
    /* TAC (24 bits) */
    asn1_encode_bits(buf, cell->tac.tac, 24);
    
    /* Number of PLMNs */
    asn1_encode_bits(buf, cell->num_plmns, 8);
    
    /* PLMN List */
    for (int i = 0; i < cell->num_plmns && i < F1AP_MAX_PLMN_COUNT; i++) {
        f1ap_encode_plmn_id(buf, &cell->plmns[i]);
    }
    
    /* Number of Slices */
    asn1_encode_bits(buf, cell->num_slices, 8);
    
    /* S-NSSAI List */
    for (int i = 0; i < cell->num_slices && i < F1AP_MAX_SLICE_COUNT; i++) {
        /* SST (8 bits) */
        asn1_encode_bits(buf, cell->sst[i], 8);
        /* SD (24 bits) */
        asn1_encode_bits(buf, cell->sd[i], 24);
    }
    
    /* Duplex Mode (4 bits) */
    asn1_encode_bits(buf, cell->ngran_duplex_mode, 4);
}

/* Encode RRC Container */
static void f1ap_encode_rrc_container(asn1_buffer_t* buf, const f1ap_rrc_container_t* container) {
    /* Length (16 bits) */
    asn1_encode_bits(buf, (uint16_t)container->length, 16);
    
    /* RRC Data */
    for (size_t i = 0; i < container->length; i++) {
        asn1_encode_bits(buf, container->rrc_container[i], 8);
    }
}

/* Encode DRB Info */
static void f1ap_encode_drb_info(asn1_buffer_t* buf, const f1ap_drb_info_t* drb) {
    /* DRB ID (5 bits) */
    asn1_encode_bits(buf, drb->drb_id, 5);
    
    /* SDT Mode (1 bit) */
    asn1_encode_bits(buf, drb->sdt_mode, 1);
    
    /* RLC Mode (2 bits) */
    asn1_encode_bits(buf, drb->rlc_mode, 2);
    
    /* Number of UL UP TNL Info (4 bits) */
    asn1_encode_bits(buf, drb->num_ul_up_tnl_info, 4);
    
    /* UL UP TNL Info List */
    for (int i = 0; i < drb->num_ul_up_tnl_info && i < F1AP_MAX_DRB_COUNT; i++) {
        /* IP Address (32 bits) */
        asn1_encode_bits(buf, drb->ul_up_tnl_ip[i], 32);
        /* Port (16 bits) */
        asn1_encode_bits(buf, drb->ul_up_tnl_port[i], 16);
        /* TEID (32 bits) */
        asn1_encode_bits(buf, drb->ul_up_tnl_teid[i], 32);
    }
    
    /* Number of QoS Flows (6 bits) */
    asn1_encode_bits(buf, drb->num_qos_flows, 6);
    
    /* QoS Flow List */
    for (int i = 0; i < drb->num_qos_flows && i < F1AP_MAX_QOS_FLOWS; i++) {
        /* QFI (6 bits) */
        asn1_encode_bits(buf, drb->qos_flows[i].qfi, 6);
        /* 5QI (8 bits) */
        asn1_encode_bits(buf, drb->five_qi[i], 8);
    }
}

/* Encode SRB Info */
static void f1ap_encode_srb_info(asn1_buffer_t* buf, const f1ap_srb_info_t* srb) {
    /* SRB ID (2 bits) */
    asn1_encode_bits(buf, srb->srb_id, 2);
    
    /* RLC Mode (2 bits) */
    asn1_encode_bits(buf, srb->rlc_mode, 2);
}

/* Encode Cause */
static void f1ap_encode_cause(asn1_buffer_t* buf, const f1ap_cause_t* cause) {
    /* Cause Type (2 bits) */
    asn1_encode_bits(buf, cause->cause_type, 2);
    
    /* Cause Value (depends on type, use 8 bits max) */
    asn1_encode_bits(buf, cause->cause_value, 8);
}

/* ============== Message Encoding ============== */

int f1ap_encode_message(const f1ap_message_t* msg, uint8_t** buffer, size_t* length) {
    if (!msg || !buffer || !length) {
        return -1;
    }
    
    asn1_buffer_t buf;
    uesim_error_t result = asn1_buffer_alloc(&buf, F1AP_MAX_MESSAGE_SIZE);
    if (result != UESIM_SUCCESS) {
        return -1;
    }
    
    /* Encode F1AP header */
    f1ap_encode_header(&buf, msg);
    
    /* Encode message payload based on type */
    switch (msg->message_type) {
        case F1AP_MSG_F1_SETUP_REQUEST: {
            const f1ap_f1_setup_request_t* req = &msg->payload.f1_setup_request;
            
            /* gNB-DU ID */
            f1ap_encode_gnb_du_id(&buf, &req->gnb_du_id);
            
            /* Number of served cells */
            asn1_encode_bits(&buf, req->served_cells.num_cells, 8);
            
            /* Served Cells List */
            for (int i = 0; i < req->served_cells.num_cells && i < F1AP_MAX_CELL_COUNT; i++) {
                f1ap_encode_served_cell_info(&buf, &req->served_cells.cells[i]);
            }
            
            /* RANAC (8 bits) */
            asn1_encode_bits(&buf, req->ranac, 8);
            
            /* gNB-DU RRC Version */
            for (int i = 0; i < 4; i++) {
                asn1_encode_bits(&buf, req->gnb_du_rrc_version[i], 8);
            }
            break;
        }
        
        case F1AP_MSG_F1_SETUP_RESPONSE: {
            const f1ap_f1_setup_response_t* resp = &msg->payload.f1_setup_response;
            
            /* gNB-CU ID */
            f1ap_encode_gnb_cu_id(&buf, &resp->gnb_cu_id);
            
            /* Number of cells to activate */
            asn1_encode_bits(&buf, resp->num_cells_to_activate, 8);
            
            /* Cells to Activate */
            for (int i = 0; i < resp->num_cells_to_activate && i < F1AP_MAX_CELL_COUNT; i++) {
                f1ap_encode_served_cell_info(&buf, &resp->cells_to_activate[i]);
            }
            
            /* Transport Layer Address */
            asn1_encode_bits(&buf, resp->transport_layer_address, 32);
            
            /* gNB-CU RRC Version */
            for (int i = 0; i < 4; i++) {
                asn1_encode_bits(&buf, resp->gnb_cu_rrc_version[i], 8);
            }
            break;
        }
        
        case F1AP_MSG_F1_SETUP_FAILURE: {
            const f1ap_f1_setup_failure_t* fail = &msg->payload.f1_setup_failure;
            f1ap_encode_cause(&buf, &fail->cause);
            asn1_encode_bits(&buf, fail->time_to_wait, 32);
            break;
        }
        
        case F1AP_MSG_UE_CONTEXT_SETUP_REQUEST: {
            const f1ap_ue_context_setup_request_t* req = &msg->payload.ue_context_setup_request;
            
            /* UE IDs */
            f1ap_encode_ue_ids(&buf, &req->ue_ids);
            
            /* RAN UE ID (40 bits) */
            asn1_encode_bits(&buf, (uint32_t)((req->ran_ue_id.ran_ue_id >> 32) & 0xFF), 8);
            asn1_encode_bits(&buf, (uint32_t)(req->ran_ue_id.ran_ue_id & 0xFFFFFFFF), 32);
            
            /* PLMN */
            f1ap_encode_plmn_id(&buf, &req->plmn);
            
            /* Number of DRBs */
            asn1_encode_bits(&buf, req->num_drbs_to_setup, 4);
            
            /* DRBs to Setup */
            for (int i = 0; i < req->num_drbs_to_setup && i < F1AP_MAX_DRB_COUNT; i++) {
                f1ap_encode_drb_info(&buf, &req->drbs_to_setup[i]);
            }
            
            /* Number of SRBs */
            asn1_encode_bits(&buf, req->num_srbs_to_setup, 2);
            
            /* SRBs to Setup */
            for (int i = 0; i < req->num_srbs_to_setup && i < F1AP_MAX_SRB_ID; i++) {
                f1ap_encode_srb_info(&buf, &req->srbs_to_setup[i]);
            }
            
            /* UE AMBR */
            asn1_encode_bits(&buf, (uint32_t)(req->ue_ambr_dl >> 32), 32);
            asn1_encode_bits(&buf, (uint32_t)(req->ue_ambr_dl & 0xFFFFFFFF), 32);
            asn1_encode_bits(&buf, (uint32_t)(req->ue_ambr_ul >> 32), 32);
            asn1_encode_bits(&buf, (uint32_t)(req->ue_ambr_ul & 0xFFFFFFFF), 32);
            
            /* NR Cell ID */
            f1ap_encode_nr_cell_id(&buf, &req->nr_cell_id);
            
            /* Serving Cell Index */
            asn1_encode_bits(&buf, req->serv_cell_idx, 8);
            
            /* Slice Info */
            asn1_encode_bits(&buf, req->sst, 8);
            asn1_encode_bits(&buf, req->sd, 24);
            
            /* RRC Container */
            f1ap_encode_rrc_container(&buf, &req->rrc_container);
            break;
        }
        
        case F1AP_MSG_UE_CONTEXT_SETUP_RESPONSE: {
            const f1ap_ue_context_setup_response_t* resp = &msg->payload.ue_context_setup_response;
            
            /* UE IDs */
            f1ap_encode_ue_ids(&buf, &resp->ue_ids);
            
            /* DRBs Setup */
            asn1_encode_bits(&buf, resp->num_drbs_setup, 4);
            for (int i = 0; i < resp->num_drbs_setup && i < F1AP_MAX_DRB_COUNT; i++) {
                f1ap_encode_drb_info(&buf, &resp->drbs_setup[i]);
            }
            
            /* DRBs Failed */
            asn1_encode_bits(&buf, resp->num_drbs_failed, 4);
            for (int i = 0; i < resp->num_drbs_failed && i < F1AP_MAX_DRB_COUNT; i++) {
                asn1_encode_bits(&buf, resp->drbs_failed[i], 5);
                f1ap_encode_cause(&buf, &resp->drb_fail_causes[i]);
            }
            
            /* SRBs Setup */
            asn1_encode_bits(&buf, resp->num_srbs_setup, 2);
            for (int i = 0; i < resp->num_srbs_setup && i < F1AP_MAX_SRB_ID; i++) {
                asn1_encode_bits(&buf, resp->srbs_setup[i], 2);
            }
            
            /* DU to CU RRC Container */
            f1ap_encode_rrc_container(&buf, &resp->du_to_cu_rrc_container);
            break;
        }
        
        case F1AP_MSG_UE_CONTEXT_RELEASE_REQUEST: {
            const f1ap_ue_context_release_request_t* req = &msg->payload.ue_context_release_request;
            f1ap_encode_ue_ids(&buf, &req->ue_ids);
            f1ap_encode_cause(&buf, &req->cause);
            break;
        }
        
        case F1AP_MSG_UE_CONTEXT_RELEASE_COMMAND: {
            const f1ap_ue_context_release_command_t* cmd = &msg->payload.ue_context_release_command;
            f1ap_encode_ue_ids(&buf, &cmd->ue_ids);
            f1ap_encode_cause(&buf, &cmd->cause);
            f1ap_encode_rrc_container(&buf, &cmd->rrc_container);
            break;
        }
        
        case F1AP_MSG_UE_CONTEXT_RELEASE_COMPLETE: {
            const f1ap_ue_context_release_complete_t* comp = &msg->payload.ue_context_release_complete;
            f1ap_encode_ue_ids(&buf, &comp->ue_ids);
            f1ap_encode_rrc_container(&buf, &comp->rrc_container);
            break;
        }
        
        case F1AP_MSG_DL_RRC_MESSAGE_TRANSFER: {
            const f1ap_dl_rrc_message_transfer_t* transfer = &msg->payload.dl_rrc_message_transfer;
            f1ap_encode_ue_ids(&buf, &transfer->ue_ids);
            asn1_encode_bits(&buf, transfer->srb_id, 2);
            f1ap_encode_rrc_container(&buf, &transfer->rrc_container);
            if (transfer->old_gnb_du_ue_f1ap_id_present) {
                asn1_encode_bits(&buf, 1, 1); /* presence flag */
                asn1_encode_bits(&buf, transfer->old_gnb_du_ue_f1ap_id, 32);
            } else {
                asn1_encode_bits(&buf, 0, 1);
            }
            break;
        }
        
        case F1AP_MSG_UL_RRC_MESSAGE_TRANSFER: {
            const f1ap_ul_rrc_message_transfer_t* transfer = &msg->payload.ul_rrc_message_transfer;
            f1ap_encode_ue_ids(&buf, &transfer->ue_ids);
            asn1_encode_bits(&buf, transfer->srb_id, 2);
            f1ap_encode_rrc_container(&buf, &transfer->rrc_container);
            break;
        }
        
        case F1AP_MSG_NOTIFY: {
            const f1ap_notify_t* notify = &msg->payload.notify;
            f1ap_encode_ue_ids(&buf, &notify->ue_ids);
            asn1_encode_bits(&buf, notify->notification_type, 4);
            f1ap_encode_cause(&buf, &notify->cause);
            break;
        }
        
        case F1AP_MSG_ERROR_INDICATION: {
            const f1ap_error_indication_t* err = &msg->payload.error_indication;
            asn1_encode_bits(&buf, err->ue_ids_present, 1);
            if (err->ue_ids_present) {
                f1ap_encode_ue_ids(&buf, &err->ue_ids);
            }
            asn1_encode_bits(&buf, err->cause_present, 1);
            if (err->cause_present) {
                f1ap_encode_cause(&buf, &err->cause);
            }
            break;
        }
        
        default:
            /* Unknown message type - just encode header */
            break;
    }
    
    *buffer = buf.data;
    *length = (buf.bit_offset + 7) / 8;  /* Convert bits to bytes */
    
    return 0;
}

/* ============== Message Decoding ============== */

/* Helper: decode bits from buffer, advances bit_offset */
static uint32_t f1ap_decode_bits(asn1_buffer_t* buf, uint8_t num_bits) {
    uint32_t value;
    asn1_decode_bits(buf->data, &buf->bit_offset, &value, num_bits);
    return value;
}

/* Decode gNB-DU ID */
static void f1ap_decode_gnb_du_id(asn1_buffer_t* buf, f1ap_gnb_du_id_t* id) {
    id->gnb_du_id = f1ap_decode_bits(buf, 32);
    
    uint8_t len = f1ap_decode_bits(buf, 8);
    for (int i = 0; i < len && i < 63; i++) {
        id->gnb_du_name[i] = f1ap_decode_bits(buf, 8);
    }
    id->gnb_du_name[len < 64 ? len : 63] = '\0';
}

/* Decode gNB-CU ID */
static void f1ap_decode_gnb_cu_id(asn1_buffer_t* buf, f1ap_gnb_cu_id_t* id) {
    id->gnb_cu_id = f1ap_decode_bits(buf, 32);
    
    uint8_t len = f1ap_decode_bits(buf, 8);
    for (int i = 0; i < len && i < 63; i++) {
        id->gnb_cu_name[i] = f1ap_decode_bits(buf, 8);
    }
    id->gnb_cu_name[len < 64 ? len : 63] = '\0';
}

/* Decode UE IDs */
static void f1ap_decode_ue_ids(asn1_buffer_t* buf, f1ap_ue_ids_t* ids) {
    ids->gnb_cu_ue_f1ap_id = f1ap_decode_bits(buf, 32);
    ids->gnb_du_ue_f1ap_id = f1ap_decode_bits(buf, 32);
}

/* Decode NR Cell ID */
static void f1ap_decode_nr_cell_id(asn1_buffer_t* buf, f1ap_nr_cell_id_t* id) {
    uint32_t high = f1ap_decode_bits(buf, 4);
    uint32_t low = f1ap_decode_bits(buf, 32);
    id->nr_cell_id = ((uint64_t)high << 32) | low;
}

/* Decode PLMN ID */
static void f1ap_decode_plmn_id(asn1_buffer_t* buf, f1ap_plmn_id_t* plmn) {
    uint8_t b1 = f1ap_decode_bits(buf, 8);
    uint8_t b2 = f1ap_decode_bits(buf, 8);
    uint8_t b3 = f1ap_decode_bits(buf, 8);
    uint8_t b4 = f1ap_decode_bits(buf, 8);
    
    plmn->mcc[0] = b1 & 0x0F;
    plmn->mcc[1] = (b1 >> 4) & 0x0F;
    plmn->mcc[2] = b2 & 0x0F;
    
    plmn->mnc[0] = b3 & 0x0F;
    plmn->mnc[1] = (b3 >> 4) & 0x0F;
    
    if ((b4 >> 4) == 0x0F) {
        plmn->mnc_length = 2;
    } else {
        plmn->mnc[2] = b4 & 0x0F;
        plmn->mnc_length = 3;
    }
}

/* Decode served cell info */
static void f1ap_decode_served_cell_info(asn1_buffer_t* buf, f1ap_served_cell_info_t* cell) {
    f1ap_decode_nr_cell_id(buf, &cell->nr_cell_id);
    cell->pci.pci = f1ap_decode_bits(buf, 10);
    cell->tac.tac = f1ap_decode_bits(buf, 24);
    
    cell->num_plmns = f1ap_decode_bits(buf, 8);
    for (int i = 0; i < cell->num_plmns && i < F1AP_MAX_PLMN_COUNT; i++) {
        f1ap_decode_plmn_id(buf, &cell->plmns[i]);
    }
    
    cell->num_slices = f1ap_decode_bits(buf, 8);
    for (int i = 0; i < cell->num_slices && i < F1AP_MAX_SLICE_COUNT; i++) {
        cell->sst[i] = f1ap_decode_bits(buf, 8);
        cell->sd[i] = f1ap_decode_bits(buf, 24);
    }
    
    cell->ngran_duplex_mode = f1ap_decode_bits(buf, 4);
}

/* Decode RRC Container */
static void f1ap_decode_rrc_container(asn1_buffer_t* buf, f1ap_rrc_container_t* container) {
    container->length = f1ap_decode_bits(buf, 16);
    for (size_t i = 0; i < container->length && i < F1AP_MAX_RRC_CONTAINER_SIZE; i++) {
        container->rrc_container[i] = f1ap_decode_bits(buf, 8);
    }
}

/* Decode Cause */
static void f1ap_decode_cause(asn1_buffer_t* buf, f1ap_cause_t* cause) {
    cause->cause_type = f1ap_decode_bits(buf, 2);
    cause->cause_value = f1ap_decode_bits(buf, 8);
}

/* Decode DRB Info */
static void f1ap_decode_drb_info(asn1_buffer_t* buf, f1ap_drb_info_t* drb) {
    drb->drb_id = f1ap_decode_bits(buf, 5);
    drb->sdt_mode = f1ap_decode_bits(buf, 1);
    drb->rlc_mode = f1ap_decode_bits(buf, 2);
    
    drb->num_ul_up_tnl_info = f1ap_decode_bits(buf, 4);
    for (int i = 0; i < drb->num_ul_up_tnl_info && i < F1AP_MAX_DRB_COUNT; i++) {
        drb->ul_up_tnl_ip[i] = f1ap_decode_bits(buf, 32);
        drb->ul_up_tnl_port[i] = f1ap_decode_bits(buf, 16);
        drb->ul_up_tnl_teid[i] = f1ap_decode_bits(buf, 32);
    }
    
    drb->num_qos_flows = f1ap_decode_bits(buf, 6);
    for (int i = 0; i < drb->num_qos_flows && i < F1AP_MAX_QOS_FLOWS; i++) {
        drb->qos_flows[i].qfi = f1ap_decode_bits(buf, 6);
        drb->five_qi[i] = f1ap_decode_bits(buf, 8);
    }
}

/* ============== Main Decode Function ============== */

int f1ap_decode_message(const uint8_t* buffer, size_t length, f1ap_message_t* msg) {
    if (!buffer || !msg || length == 0) {
        return -1;
    }
    
    asn1_buffer_t buf;
    buf.data = (uint8_t*)buffer;
    buf.size = length;
    buf.bit_offset = 0;
    
    memset(msg, 0, sizeof(f1ap_message_t));
    
    /* Decode header */
    msg->procedure_code = f1ap_decode_bits(&buf, 8);
    msg->criticality = f1ap_decode_bits(&buf, 2);
    msg->message_type = f1ap_decode_bits(&buf, 8);
    
    /* Decode payload based on message type */
    switch (msg->message_type) {
        case F1AP_MSG_F1_SETUP_REQUEST: {
            f1ap_f1_setup_request_t* req = &msg->payload.f1_setup_request;
            f1ap_decode_gnb_du_id(&buf, &req->gnb_du_id);
            
            req->served_cells.num_cells = f1ap_decode_bits(&buf, 8);
            for (int i = 0; i < req->served_cells.num_cells && i < F1AP_MAX_CELL_COUNT; i++) {
                f1ap_decode_served_cell_info(&buf, &req->served_cells.cells[i]);
            }
            
            req->ranac = f1ap_decode_bits(&buf, 8);
            for (int i = 0; i < 4; i++) {
                req->gnb_du_rrc_version[i] = f1ap_decode_bits(&buf, 8);
            }
            break;
        }
        
        case F1AP_MSG_F1_SETUP_RESPONSE: {
            f1ap_f1_setup_response_t* resp = &msg->payload.f1_setup_response;
            f1ap_decode_gnb_cu_id(&buf, &resp->gnb_cu_id);
            
            resp->num_cells_to_activate = f1ap_decode_bits(&buf, 8);
            for (int i = 0; i < resp->num_cells_to_activate && i < F1AP_MAX_CELL_COUNT; i++) {
                f1ap_decode_served_cell_info(&buf, &resp->cells_to_activate[i]);
            }
            
            resp->transport_layer_address = f1ap_decode_bits(&buf, 32);
            for (int i = 0; i < 4; i++) {
                resp->gnb_cu_rrc_version[i] = f1ap_decode_bits(&buf, 8);
            }
            break;
        }
        
        case F1AP_MSG_F1_SETUP_FAILURE: {
            f1ap_f1_setup_failure_t* fail = &msg->payload.f1_setup_failure;
            f1ap_decode_cause(&buf, &fail->cause);
            fail->time_to_wait = f1ap_decode_bits(&buf, 32);
            break;
        }
        
        case F1AP_MSG_UE_CONTEXT_SETUP_REQUEST: {
            f1ap_ue_context_setup_request_t* req = &msg->payload.ue_context_setup_request;
            f1ap_decode_ue_ids(&buf, &req->ue_ids);
            
            uint32_t high = f1ap_decode_bits(&buf, 8);
            uint32_t low = f1ap_decode_bits(&buf, 32);
            req->ran_ue_id.ran_ue_id = ((uint64_t)high << 32) | low;
            
            f1ap_decode_plmn_id(&buf, &req->plmn);
            
            req->num_drbs_to_setup = f1ap_decode_bits(&buf, 4);
            for (int i = 0; i < req->num_drbs_to_setup && i < F1AP_MAX_DRB_COUNT; i++) {
                f1ap_decode_drb_info(&buf, &req->drbs_to_setup[i]);
            }
            
            req->num_srbs_to_setup = f1ap_decode_bits(&buf, 2);
            
            req->ue_ambr_dl = ((uint64_t)f1ap_decode_bits(&buf, 32) << 32) | f1ap_decode_bits(&buf, 32);
            req->ue_ambr_ul = ((uint64_t)f1ap_decode_bits(&buf, 32) << 32) | f1ap_decode_bits(&buf, 32);
            
            f1ap_decode_nr_cell_id(&buf, &req->nr_cell_id);
            req->serv_cell_idx = f1ap_decode_bits(&buf, 8);
            req->sst = f1ap_decode_bits(&buf, 8);
            req->sd = f1ap_decode_bits(&buf, 24);
            f1ap_decode_rrc_container(&buf, &req->rrc_container);
            break;
        }
        
        case F1AP_MSG_UE_CONTEXT_SETUP_RESPONSE: {
            f1ap_ue_context_setup_response_t* resp = &msg->payload.ue_context_setup_response;
            f1ap_decode_ue_ids(&buf, &resp->ue_ids);
            
            resp->num_drbs_setup = f1ap_decode_bits(&buf, 4);
            for (int i = 0; i < resp->num_drbs_setup && i < F1AP_MAX_DRB_COUNT; i++) {
                f1ap_decode_drb_info(&buf, &resp->drbs_setup[i]);
            }
            
            resp->num_drbs_failed = f1ap_decode_bits(&buf, 4);
            for (int i = 0; i < resp->num_drbs_failed && i < F1AP_MAX_DRB_COUNT; i++) {
                resp->drbs_failed[i] = f1ap_decode_bits(&buf, 5);
                f1ap_decode_cause(&buf, &resp->drb_fail_causes[i]);
            }
            
            resp->num_srbs_setup = f1ap_decode_bits(&buf, 2);
            for (int i = 0; i < resp->num_srbs_setup && i < F1AP_MAX_SRB_ID; i++) {
                resp->srbs_setup[i] = f1ap_decode_bits(&buf, 2);
            }
            
            f1ap_decode_rrc_container(&buf, &resp->du_to_cu_rrc_container);
            break;
        }
        
        case F1AP_MSG_UE_CONTEXT_RELEASE_REQUEST: {
            f1ap_ue_context_release_request_t* req = &msg->payload.ue_context_release_request;
            f1ap_decode_ue_ids(&buf, &req->ue_ids);
            f1ap_decode_cause(&buf, &req->cause);
            break;
        }
        
        case F1AP_MSG_UE_CONTEXT_RELEASE_COMMAND: {
            f1ap_ue_context_release_command_t* cmd = &msg->payload.ue_context_release_command;
            f1ap_decode_ue_ids(&buf, &cmd->ue_ids);
            f1ap_decode_cause(&buf, &cmd->cause);
            f1ap_decode_rrc_container(&buf, &cmd->rrc_container);
            break;
        }
        
        case F1AP_MSG_UE_CONTEXT_RELEASE_COMPLETE: {
            f1ap_ue_context_release_complete_t* comp = &msg->payload.ue_context_release_complete;
            f1ap_decode_ue_ids(&buf, &comp->ue_ids);
            f1ap_decode_rrc_container(&buf, &comp->rrc_container);
            break;
        }
        
        case F1AP_MSG_DL_RRC_MESSAGE_TRANSFER: {
            f1ap_dl_rrc_message_transfer_t* transfer = &msg->payload.dl_rrc_message_transfer;
            f1ap_decode_ue_ids(&buf, &transfer->ue_ids);
            transfer->srb_id = f1ap_decode_bits(&buf, 2);
            f1ap_decode_rrc_container(&buf, &transfer->rrc_container);
            transfer->old_gnb_du_ue_f1ap_id_present = f1ap_decode_bits(&buf, 1);
            if (transfer->old_gnb_du_ue_f1ap_id_present) {
                transfer->old_gnb_du_ue_f1ap_id = f1ap_decode_bits(&buf, 32);
            }
            break;
        }
        
        case F1AP_MSG_UL_RRC_MESSAGE_TRANSFER: {
            f1ap_ul_rrc_message_transfer_t* transfer = &msg->payload.ul_rrc_message_transfer;
            f1ap_decode_ue_ids(&buf, &transfer->ue_ids);
            transfer->srb_id = f1ap_decode_bits(&buf, 2);
            f1ap_decode_rrc_container(&buf, &transfer->rrc_container);
            break;
        }
        
        case F1AP_MSG_NOTIFY: {
            f1ap_notify_t* notify = &msg->payload.notify;
            f1ap_decode_ue_ids(&buf, &notify->ue_ids);
            notify->notification_type = f1ap_decode_bits(&buf, 4);
            f1ap_decode_cause(&buf, &notify->cause);
            break;
        }
        
        case F1AP_MSG_ERROR_INDICATION: {
            f1ap_error_indication_t* err = &msg->payload.error_indication;
            err->ue_ids_present = f1ap_decode_bits(&buf, 1);
            if (err->ue_ids_present) {
                f1ap_decode_ue_ids(&buf, &err->ue_ids);
            }
            err->cause_present = f1ap_decode_bits(&buf, 1);
            if (err->cause_present) {
                f1ap_decode_cause(&buf, &err->cause);
            }
            break;
        }
        
        default:
            /* Unknown message type */
            break;
    }
    
    return 0;
}

void f1ap_free_message(f1ap_message_t* msg) {
    /* No dynamic allocations in message structure */
    (void)msg;
}
