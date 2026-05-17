/*
 * 5G UE Simulation Application
 * Mock CU-CP (Central Unit - Control Plane) Entity
 * 3GPP TS 38.401 - F1AP Interface
 */

#include "mock_core.h"
#include "../protocol/f1ap_messages.h"
#include "../protocol/asn1_per.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

/* ============== Constants ============== */

/* Note: CU_CP_MAX_DU_CONNECTIONS, CU_CP_MAX_UE_CONTEXTS, CU_CP_F1AP_PORT, 
 * CU_CP_BUFFER_SIZE, cu_cp_state_t, and cu_cp_config_t are defined in mock_core.h */

/* ============== DU Connection Context ============== */

typedef struct {
    uint32_t gnb_du_id;
    char gnb_du_name[64];
    int f1ap_socket;
    cu_cp_state_t state;
    
    /* Served Cells */
    uint8_t num_served_cells;
    f1ap_served_cell_info_t served_cells[F1AP_MAX_CELL_COUNT];
    
    /* Connection Info */
    struct sockaddr_in du_addr;
    time_t connect_time;
    time_t last_activity;
    bool active;
} du_connection_t;

/* ============== CU-CP UE Context ============== */

typedef struct {
    uint32_t gnb_cu_ue_f1ap_id;
    uint32_t gnb_du_ue_f1ap_id;
    uint64_t ran_ue_id;
    
    /* DRB/SRB */
    uint8_t num_drbs;
    f1ap_drb_info_t drbs[F1AP_MAX_DRB_COUNT];
    uint8_t num_srbs;
    uint8_t srbs[F1AP_MAX_SRB_ID];
    
    /* Cell Info */
    uint64_t nr_cell_id;
    uint16_t pci;
    
    /* DU Reference */
    uint32_t gnb_du_id;
    
    /* State */
    uint8_t ue_state;
    time_t setup_time;
    bool active;
} cu_cp_ue_context_t;

/* Note: cu_cp_config_t is defined in mock_core.h */

/* ============== CU-CP Server Context ============== */

struct cu_cp_server_s {
    cu_cp_config_t config;
    
    /* Socket */
    int f1ap_socket;
    
    /* DU Connections */
    du_connection_t du_connections[CU_CP_MAX_DU_CONNECTIONS];
    uint32_t num_du_connections;
    
    /* UE Contexts */
    cu_cp_ue_context_t ue_contexts[CU_CP_MAX_UE_CONTEXTS];
    uint32_t num_ue_contexts;
    uint32_t next_cu_ue_id;
    
    /* Server State */
#ifdef _WIN32
    volatile LONG running;
#else
    atomic_bool running;
#endif
    pthread_t f1ap_thread;
    pthread_mutex_t du_mutex;
    
    /* Statistics */
    uint64_t f1ap_messages_rx;
    uint64_t f1ap_messages_tx;
    uint64_t f1_setup_success;
    uint64_t f1_setup_failure;
    uint64_t ue_contexts_created;
    uint64_t ue_contexts_released;
};

/* ============== Forward Declarations ============== */

static void* f1ap_listener_thread(void* arg);

/* ============== CU-CP API Functions ============== */

void cu_cp_get_default_config(cu_cp_config_t* config) {
    if (!config) return;
    memset(config, 0, sizeof(cu_cp_config_t));
    
    strncpy(config->bind_ip, "0.0.0.0", sizeof(config->bind_ip) - 1);
    config->f1ap_port = CU_CP_F1AP_PORT;
    config->gnb_cu_id = 0x1234567890;  /* Example CU ID */
    strncpy(config->gnb_cu_name, "UESim-CU-CP", sizeof(config->gnb_cu_name) - 1);
    
    /* Default PLMN: 001/01 */
    config->plmn_mcc[0] = 0;
    config->plmn_mcc[1] = 0;
    config->plmn_mcc[2] = 1;
    config->plmn_mnc[0] = 0;
    config->plmn_mnc[1] = 1;
    config->mnc_length = 2;
    config->tac = 1;
    
    config->rrc_version[0] = 15;  /* v15.0.0 */
    config->auto_respond = true;
    config->log_messages = true;
}

cu_cp_server_t* cu_cp_create(const cu_cp_config_t* config) {
    cu_cp_server_t* cu_cp = (cu_cp_server_t*)calloc(1, sizeof(cu_cp_server_t));
    if (!cu_cp) return NULL;
    
    if (config) {
        memcpy(&cu_cp->config, config, sizeof(cu_cp_config_t));
    } else {
        cu_cp_get_default_config(&cu_cp->config);
    }
    
    cu_cp->f1ap_socket = -1;
    cu_cp->next_cu_ue_id = 1;
    
    return cu_cp;
}

void cu_cp_destroy(cu_cp_server_t* cu_cp) {
    if (!cu_cp) return;
    
    cu_cp_stop(cu_cp);
    
    /* Close all DU connections */
    for (int i = 0; i < CU_CP_MAX_DU_CONNECTIONS; i++) {
        if (cu_cp->du_connections[i].active && 
            cu_cp->du_connections[i].f1ap_socket >= 0) {
#ifdef _WIN32
            closesocket(cu_cp->du_connections[i].f1ap_socket);
#else
            close(cu_cp->du_connections[i].f1ap_socket);
#endif
        }
    }
    
    pthread_mutex_destroy(&cu_cp->du_mutex);
    free(cu_cp);
}

/* ============== F1AP Listener Thread ============== */

static void* f1ap_listener_thread(void* arg) {
    cu_cp_server_t* cu_cp = (cu_cp_server_t*)arg;
    struct sockaddr_in client_addr;
    socklen_t client_len;
    uint8_t buffer[CU_CP_BUFFER_SIZE];
    ssize_t bytes_recv;
    fd_set read_fds;
    struct timeval tv;
    
    printf("[CU-CP] F1AP listener thread started on port %u\n", cu_cp->config.f1ap_port);
    
    while (atomic_load(&cu_cp->running)) {
        FD_ZERO(&read_fds);
        FD_SET(cu_cp->f1ap_socket, &read_fds);
        
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int ret = select(cu_cp->f1ap_socket + 1, &read_fds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (ret == 0) continue;  /* Timeout */
        
        client_len = sizeof(client_addr);
        int client_sock = accept(cu_cp->f1ap_socket, 
                                  (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) continue;
        
        /* Receive data */
        bytes_recv = recv(client_sock, (char*)buffer, sizeof(buffer), 0);
        if (bytes_recv > 0) {
            cu_cp_process_f1ap_message(cu_cp, buffer, (size_t)bytes_recv, client_sock);
        }
        
#ifdef _WIN32
        closesocket(client_sock);
#else
        close(client_sock);
#endif
    }
    
    printf("[CU-CP] F1AP listener thread stopped\n");
    return NULL;
}

mock_core_error_t cu_cp_start(cu_cp_server_t* cu_cp) {
    if (!cu_cp) return MOCK_CORE_ERROR_INVALID_PARAM;
    
    struct sockaddr_in addr;
    
    /* Create TCP socket for F1AP */
    cu_cp->f1ap_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (cu_cp->f1ap_socket < 0) {
        return MOCK_CORE_ERROR_SOCKET;
    }
    
    /* Set socket options */
    int opt = 1;
    setsockopt(cu_cp->f1ap_socket, SOL_SOCKET, SO_REUSEADDR,
               (const char*)&opt, sizeof(opt));
    
    /* Bind */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(cu_cp->config.f1ap_port);
    inet_pton(AF_INET, cu_cp->config.bind_ip, &addr.sin_addr);
    
    if (bind(cu_cp->f1ap_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
#ifdef _WIN32
        closesocket(cu_cp->f1ap_socket);
#else
        close(cu_cp->f1ap_socket);
#endif
        cu_cp->f1ap_socket = -1;
        return MOCK_CORE_ERROR_SOCKET;
    }
    
    /* Listen */
    if (listen(cu_cp->f1ap_socket, 10) < 0) {
#ifdef _WIN32
        closesocket(cu_cp->f1ap_socket);
#else
        close(cu_cp->f1ap_socket);
#endif
        cu_cp->f1ap_socket = -1;
        return MOCK_CORE_ERROR_SOCKET;
    }
    
    /* Initialize mutex */
    if (pthread_mutex_init(&cu_cp->du_mutex, NULL) != 0) {
#ifdef _WIN32
        closesocket(cu_cp->f1ap_socket);
#else
        close(cu_cp->f1ap_socket);
#endif
        cu_cp->f1ap_socket = -1;
        return MOCK_CORE_ERROR_THREAD;
    }
    
    /* Start F1AP listener thread */
    atomic_store(&cu_cp->running, 1);
    if (pthread_create(&cu_cp->f1ap_thread, NULL, f1ap_listener_thread, cu_cp) != 0) {
        atomic_store(&cu_cp->running, 0);
#ifdef _WIN32
        closesocket(cu_cp->f1ap_socket);
#else
        close(cu_cp->f1ap_socket);
#endif
        cu_cp->f1ap_socket = -1;
        return MOCK_CORE_ERROR_THREAD;
    }
    
    printf("[CU-CP] Server started on %s:%u\n", 
           cu_cp->config.bind_ip, cu_cp->config.f1ap_port);
    return MOCK_CORE_SUCCESS;
}

void cu_cp_stop(cu_cp_server_t* cu_cp) {
    if (!cu_cp) return;
    
    atomic_store(&cu_cp->running, 0);
    
    if (cu_cp->f1ap_socket >= 0) {
#ifdef _WIN32
        closesocket(cu_cp->f1ap_socket);
#else
        close(cu_cp->f1ap_socket);
#endif
        cu_cp->f1ap_socket = -1;
    }
    
    /* Wait for thread to terminate */
#ifdef _WIN32
    WaitForSingleObject(cu_cp->f1ap_thread, 5000);
#else
    pthread_join(cu_cp->f1ap_thread, NULL);
#endif
    
    printf("[CU-CP] Server stopped\n");
}

/* ============== F1AP Message Handlers ============== */

static int cu_cp_handle_f1_setup_request(cu_cp_server_t* cu_cp, 
                                          const f1ap_message_t* req,
                                          int socket,
                                          f1ap_message_t* response) {
    if (!cu_cp || !req || !response) return -1;
    
    const f1ap_f1_setup_request_t* setup_req = &req->payload.f1_setup_request;
    f1ap_f1_setup_response_t* setup_resp = &response->payload.f1_setup_response;
    
    printf("[CU-CP] F1 Setup Request from gNB-DU ID: 0x%08X\n", 
           setup_req->gnb_du_id.gnb_du_id);
    printf("[CU-CP]   gNB-DU Name: %s\n", setup_req->gnb_du_id.gnb_du_name);
    printf("[CU-CP]   Served Cells: %d\n", setup_req->served_cells.num_cells);
    
    /* Find or create DU connection */
    du_connection_t* du_conn = NULL;
    for (int i = 0; i < CU_CP_MAX_DU_CONNECTIONS; i++) {
        if (cu_cp->du_connections[i].active &&
            cu_cp->du_connections[i].gnb_du_id == setup_req->gnb_du_id.gnb_du_id) {
            du_conn = &cu_cp->du_connections[i];
            break;
        }
    }
    
    if (!du_conn) {
        /* Create new DU connection */
        for (int i = 0; i < CU_CP_MAX_DU_CONNECTIONS; i++) {
            if (!cu_cp->du_connections[i].active) {
                du_conn = &cu_cp->du_connections[i];
                du_conn->gnb_du_id = setup_req->gnb_du_id.gnb_du_id;
                strncpy(du_conn->gnb_du_name, 
                        (const char*)setup_req->gnb_du_id.gnb_du_name,
                        sizeof(du_conn->gnb_du_name) - 1);
                du_conn->f1ap_socket = socket;
                du_conn->active = true;
                du_conn->state = CU_CP_STATE_ACTIVE;
                du_conn->connect_time = time(NULL);
                cu_cp->num_du_connections++;
                break;
            }
        }
    }
    
    if (!du_conn) {
        /* No free slots - send failure */
        response->message_type = F1AP_MSG_F1_SETUP_FAILURE;
        response->procedure_code = F1AP_PROC_F1_SETUP;
        f1ap_set_cause_misc(&response->payload.f1_setup_failure.cause,
                            F1AP_CAUSE_MISC_CONTROL_PROCESSING_OVERLOAD);
        cu_cp->f1_setup_failure++;
        return 0;
    }
    
    /* Store served cells */
    du_conn->num_served_cells = setup_req->served_cells.num_cells;
    for (int i = 0; i < du_conn->num_served_cells && i < F1AP_MAX_CELL_COUNT; i++) {
        memcpy(&du_conn->served_cells[i], &setup_req->served_cells.cells[i],
               sizeof(f1ap_served_cell_info_t));
    }
    
    /* Build F1 Setup Response */
    response->message_type = F1AP_MSG_F1_SETUP_RESPONSE;
    response->procedure_code = F1AP_PROC_F1_SETUP;
    response->criticality = 0;
    
    /* gNB-CU ID */
    setup_resp->gnb_cu_id.gnb_cu_id = cu_cp->config.gnb_cu_id;
    strncpy((char*)setup_resp->gnb_cu_id.gnb_cu_name, cu_cp->config.gnb_cu_name,
            sizeof(setup_resp->gnb_cu_id.gnb_cu_name) - 1);
    
    /* Cells to Activate */
    setup_resp->num_cells_to_activate = du_conn->num_served_cells;
    for (int i = 0; i < du_conn->num_served_cells && i < F1AP_MAX_CELL_COUNT; i++) {
        memcpy(&setup_resp->cells_to_activate[i], &du_conn->served_cells[i],
               sizeof(f1ap_served_cell_info_t));
    }
    
    /* RRC Version */
    memcpy(setup_resp->gnb_cu_rrc_version, cu_cp->config.rrc_version, 4);
    
    /* Transport Layer Address (CU-CP IP) */
    setup_resp->transport_layer_address = inet_addr(cu_cp->config.bind_ip);
    
    du_conn->state = CU_CP_STATE_ACTIVE;
    cu_cp->f1_setup_success++;
    
    return 0;
}

static int cu_cp_handle_ue_context_setup_response(cu_cp_server_t* cu_cp,
                                                   const f1ap_message_t* resp,
                                                   f1ap_message_t* response) {
    if (!cu_cp || !resp) return -1;
    
    const f1ap_ue_context_setup_response_t* ue_resp = &resp->payload.ue_context_setup_response;
    
    printf("[CU-CP] UE Context Setup Response:\n");
    printf("[CU-CP]   gNB-CU-UE-F1AP-ID: %u\n", ue_resp->ue_ids.gnb_cu_ue_f1ap_id);
    printf("[CU-CP]   gNB-DU-UE-F1AP-ID: %u\n", ue_resp->ue_ids.gnb_du_ue_f1ap_id);
    printf("[CU-CP]   DRBs Setup: %d, Failed: %d\n", 
           ue_resp->num_drbs_setup, ue_resp->num_drbs_failed);
    
    /* Find UE context */
    for (int i = 0; i < CU_CP_MAX_UE_CONTEXTS; i++) {
        if (cu_cp->ue_contexts[i].active &&
            cu_cp->ue_contexts[i].gnb_cu_ue_f1ap_id == ue_resp->ue_ids.gnb_cu_ue_f1ap_id) {
            cu_cp_ue_context_t* ue = &cu_cp->ue_contexts[i];
            ue->gnb_du_ue_f1ap_id = ue_resp->ue_ids.gnb_du_ue_f1ap_id;
            ue->num_drbs = ue_resp->num_drbs_setup;
            for (int j = 0; j < ue_resp->num_drbs_setup && j < F1AP_MAX_DRB_COUNT; j++) {
                memcpy(&ue->drbs[j], &ue_resp->drbs_setup[j], sizeof(f1ap_drb_info_t));
            }
            ue->num_srbs = ue_resp->num_srbs_setup;
            ue->ue_state = 2;  /* Active */
            break;
        }
    }
    
    return 0;
}

static int cu_cp_handle_ul_rrc_message(cu_cp_server_t* cu_cp,
                                        const f1ap_message_t* msg,
                                        f1ap_message_t* response) {
    if (!cu_cp || !msg) return -1;
    
    const f1ap_ul_rrc_message_transfer_t* transfer = &msg->payload.ul_rrc_message_transfer;
    
    printf("[CU-CP] UL RRC Message Transfer:\n");
    printf("[CU-CP]   gNB-CU-UE-F1AP-ID: %u\n", transfer->ue_ids.gnb_cu_ue_f1ap_id);
    printf("[CU-CP]   gNB-DU-UE-F1AP-ID: %u\n", transfer->ue_ids.gnb_du_ue_f1ap_id);
    printf("[CU-CP]   SRB ID: %u\n", transfer->srb_id);
    printf("[CU-CP]   RRC Container Length: %zu\n", transfer->rrc_container.length);
    
    /* Process RRC message - in real implementation, would parse and handle RRC */
    
    return 0;
}

/* ============== CU-CP UE Context Management ============== */

static cu_cp_ue_context_t* cu_cp_create_ue_context(cu_cp_server_t* cu_cp,
                                                    uint32_t gnb_du_id) {
    if (!cu_cp) return NULL;
    
    for (int i = 0; i < CU_CP_MAX_UE_CONTEXTS; i++) {
        if (!cu_cp->ue_contexts[i].active) {
            cu_cp_ue_context_t* ue = &cu_cp->ue_contexts[i];
            memset(ue, 0, sizeof(cu_cp_ue_context_t));
            ue->gnb_cu_ue_f1ap_id = cu_cp->next_cu_ue_id++;
            ue->gnb_du_id = gnb_du_id;
            ue->active = true;
            ue->setup_time = time(NULL);
            cu_cp->num_ue_contexts++;
            cu_cp->ue_contexts_created++;
            return ue;
        }
    }
    
    return NULL;
}

static int cu_cp_send_ue_context_setup_request(cu_cp_server_t* cu_cp,
                                                du_connection_t* du_conn,
                                                uint64_t ran_ue_id,
                                                const f1ap_plmn_id_t* plmn,
                                                uint8_t sst, uint32_t sd) {
    if (!cu_cp || !du_conn) return -1;
    
    /* Create UE context */
    cu_cp_ue_context_t* ue = cu_cp_create_ue_context(cu_cp, du_conn->gnb_du_id);
    if (!ue) return -1;
    
    ue->ran_ue_id = ran_ue_id;
    if (du_conn->num_served_cells > 0) {
        ue->nr_cell_id = du_conn->served_cells[0].nr_cell_id.nr_cell_id;
        ue->pci = du_conn->served_cells[0].pci.pci;
    }
    
    /* Build F1AP UE Context Setup Request */
    f1ap_message_t msg;
    f1ap_init_ue_context_setup_request(&msg);
    
    f1ap_ue_context_setup_request_t* req = &msg.payload.ue_context_setup_request;
    req->ue_ids.gnb_cu_ue_f1ap_id = ue->gnb_cu_ue_f1ap_id;
    req->ue_ids.gnb_du_ue_f1ap_id = 0;  /* To be assigned by DU */
    req->ran_ue_id.ran_ue_id = ran_ue_id;
    
    if (plmn) {
        memcpy(&req->plmn, plmn, sizeof(f1ap_plmn_id_t));
    }
    
    req->nr_cell_id.nr_cell_id = ue->nr_cell_id;
    req->sst = sst;
    req->sd = sd;
    
    /* Setup default DRB */
    req->num_drbs_to_setup = 1;
    req->drbs_to_setup[0].drb_id = 1;
    req->drbs_to_setup[0].rlc_mode = 1;  /* AM */
    req->drbs_to_setup[0].num_qos_flows = 1;
    req->drbs_to_setup[0].qos_flows[0].qfi = 1;
    req->drbs_to_setup[0].five_qi[0] = 9;  /* Default 5QI */
    
    /* Setup SRB1 */
    req->num_srbs_to_setup = 1;
    req->srbs_to_setup[0].srb_id = 1;
    req->srbs_to_setup[0].rlc_mode = 1;  /* AM */
    
    /* UE AMBR */
    req->ue_ambr_dl = 1000000000ULL;  /* 1 Gbps */
    req->ue_ambr_ul = 500000000ULL;   /* 500 Mbps */
    
    /* Encode and send */
    uint8_t* buffer = NULL;
    size_t length = 0;
    
    if (f1ap_encode_message(&msg, &buffer, &length) == 0) {
        printf("[CU-CP] Sending UE Context Setup Request for UE ID: %u\n",
               ue->gnb_cu_ue_f1ap_id);
        /* send(du_conn->f1ap_socket, buffer, length, 0); */
        free(buffer);
        cu_cp->f1ap_messages_tx++;
    }
    
    return 0;
}

/* ============== CU-CP Message Processing ============== */

mock_core_error_t cu_cp_process_f1ap_message(cu_cp_server_t* cu_cp,
                                              const uint8_t* data, size_t len,
                                              int socket) {
    if (!cu_cp || !data || len == 0) return -1;
    
    f1ap_message_t msg;
    if (f1ap_decode_message(data, len, &msg) != 0) {
        printf("[CU-CP] Failed to decode F1AP message\n");
        return -1;
    }
    
    cu_cp->f1ap_messages_rx++;
    
    if (cu_cp->config.log_messages) {
        printf("[CU-CP] Received F1AP Message: %s (Procedure: %d)\n",
               f1ap_message_type_to_string(msg.message_type),
               msg.procedure_code);
    }
    
    f1ap_message_t response;
    memset(&response, 0, sizeof(response));
    
    switch (msg.message_type) {
        case F1AP_MSG_F1_SETUP_REQUEST:
            cu_cp_handle_f1_setup_request(cu_cp, &msg, socket, &response);
            break;
            
        case F1AP_MSG_UE_CONTEXT_SETUP_RESPONSE:
            cu_cp_handle_ue_context_setup_response(cu_cp, &msg, &response);
            break;
            
        case F1AP_MSG_UE_CONTEXT_SETUP_FAILURE: {
            const f1ap_ue_context_setup_failure_t* fail = &msg.payload.ue_context_setup_failure;
            printf("[CU-CP] UE Context Setup Failure: Cause=%s\n",
                   f1ap_cause_to_string(&fail->cause));
            break;
        }
            
        case F1AP_MSG_UE_CONTEXT_RELEASE_REQUEST: {
            const f1ap_ue_context_release_request_t* req = &msg.payload.ue_context_release_request;
            printf("[CU-CP] UE Context Release Request: UE ID=%u, Cause=%s\n",
                   req->ue_ids.gnb_cu_ue_f1ap_id, f1ap_cause_to_string(&req->cause));
            break;
        }
            
        case F1AP_MSG_UE_CONTEXT_RELEASE_COMPLETE: {
            const f1ap_ue_context_release_complete_t* comp = &msg.payload.ue_context_release_complete;
            printf("[CU-CP] UE Context Release Complete: UE ID=%u\n",
                   comp->ue_ids.gnb_cu_ue_f1ap_id);
            cu_cp->ue_contexts_released++;
            break;
        }
            
        case F1AP_MSG_UL_RRC_MESSAGE_TRANSFER:
            cu_cp_handle_ul_rrc_message(cu_cp, &msg, &response);
            break;
            
        case F1AP_MSG_NOTIFY: {
            const f1ap_notify_t* notify = &msg.payload.notify;
            printf("[CU-CP] Notify: UE ID=%u, Type=%u, Cause=%s\n",
                   notify->ue_ids.gnb_cu_ue_f1ap_id, notify->notification_type,
                   f1ap_cause_to_string(&notify->cause));
            break;
        }
            
        case F1AP_MSG_ERROR_INDICATION: {
            const f1ap_error_indication_t* err = &msg.payload.error_indication;
            printf("[CU-CP] Error Indication: Cause=%s\n",
                   err->cause_present ? f1ap_cause_to_string(&err->cause) : "N/A");
            break;
        }
            
        default:
            printf("[CU-CP] Unhandled message type: %d\n", msg.message_type);
            break;
    }
    
    /* Send response if needed */
    if (response.message_type != F1AP_MSG_MAX && cu_cp->config.auto_respond) {
        uint8_t* resp_buffer = NULL;
        size_t resp_len = 0;
        
        if (f1ap_encode_message(&response, &resp_buffer, &resp_len) == 0) {
            /* send(socket, resp_buffer, resp_len, 0); */
            free(resp_buffer);
            cu_cp->f1ap_messages_tx++;
        }
    }
    
    f1ap_free_message(&msg);
    return 0;
}

/* ============== CU-CP Statistics ============== */

void cu_cp_print_statistics(const cu_cp_server_t* cu_cp) {
    if (!cu_cp) return;
    
    printf("\n[CU-CP] Statistics:\n");
    printf("  F1AP Messages RX: %llu\n", 
           (unsigned long long)cu_cp->f1ap_messages_rx);
    printf("  F1AP Messages TX: %llu\n",
           (unsigned long long)cu_cp->f1ap_messages_tx);
    printf("  F1 Setup Success: %llu\n",
           (unsigned long long)cu_cp->f1_setup_success);
    printf("  F1 Setup Failure: %llu\n",
           (unsigned long long)cu_cp->f1_setup_failure);
    printf("  Active DU Connections: %u\n", cu_cp->num_du_connections);
    printf("  UE Contexts Created: %llu\n",
           (unsigned long long)cu_cp->ue_contexts_created);
    printf("  UE Contexts Released: %llu\n",
           (unsigned long long)cu_cp->ue_contexts_released);
    printf("  Active UE Contexts: %u\n", cu_cp->num_ue_contexts);
}

/* ============== Test Interface ============== */

int mock_cu_cp_test(void) {
    printf("\n=== CU-CP Mock Test ===\n\n");
    
    /* Create CU-CP server */
    cu_cp_config_t config;
    cu_cp_get_default_config(&config);
    config.log_messages = true;
    
    cu_cp_server_t* cu_cp = cu_cp_create(&config);
    if (!cu_cp) {
        printf("[CU-CP] Failed to create server\n");
        return -1;
    }
    
    printf("[CU-CP] Server created successfully\n");
    printf("[CU-CP]   gNB-CU ID: 0x%llX\n", 
           (unsigned long long)cu_cp->config.gnb_cu_id);
    printf("[CU-CP]   gNB-CU Name: %s\n", cu_cp->config.gnb_cu_name);
    printf("[CU-CP]   F1AP Port: %u\n", cu_cp->config.f1ap_port);
    
    /* Test F1 Setup Request processing */
    printf("\n[CU-CP] Testing F1 Setup Request...\n");
    
    f1ap_message_t setup_req;
    f1ap_init_f1_setup_request(&setup_req);
    
    f1ap_f1_setup_request_t* req = &setup_req.payload.f1_setup_request;
    req->gnb_du_id.gnb_du_id = 0x00000001;
    strncpy((char*)req->gnb_du_id.gnb_du_name, "UESim-DU-01", 
            sizeof(req->gnb_du_id.gnb_du_name) - 1);
    
    req->served_cells.num_cells = 1;
    req->served_cells.cells[0].nr_cell_id.nr_cell_id = 0x123456789ABULL;
    req->served_cells.cells[0].pci.pci = 1;
    req->served_cells.cells[0].tac.tac = 1;
    req->served_cells.cells[0].num_plmns = 1;
    req->served_cells.cells[0].plmns[0].mcc[0] = 0;
    req->served_cells.cells[0].plmns[0].mcc[1] = 0;
    req->served_cells.cells[0].plmns[0].mcc[2] = 1;
    req->served_cells.cells[0].plmns[0].mnc[0] = 0;
    req->served_cells.cells[0].plmns[0].mnc[1] = 1;
    req->served_cells.cells[0].plmns[0].mnc_length = 2;
    
    /* Encode and decode test */
    uint8_t* buffer = NULL;
    size_t length = 0;
    
    if (f1ap_encode_message(&setup_req, &buffer, &length) == 0) {
        printf("[CU-CP] F1 Setup Request encoded: %zu bytes\n", length);
        
        /* Process through CU-CP */
        cu_cp_process_f1ap_message(cu_cp, buffer, length, -1);
        
        free(buffer);
    }
    
    /* Test UE Context Setup */
    printf("\n[CU-CP] Testing UE Context Setup...\n");
    
    if (cu_cp->num_du_connections > 0) {
        du_connection_t* du = &cu_cp->du_connections[0];
        f1ap_plmn_id_t plmn = {
            .mcc = {0, 0, 1},
            .mnc = {0, 1, 0},
            .mnc_length = 2
        };
        cu_cp_send_ue_context_setup_request(cu_cp, du, 0x123456789AULL, &plmn, 1, 0);
    }
    
    /* Print statistics */
    cu_cp_print_statistics(cu_cp);
    
    /* Cleanup */
    cu_cp_destroy(cu_cp);
    printf("\n[CU-CP] Server destroyed\n");
    
    printf("\n=== CU-CP Mock Test Complete ===\n");
    return 0;
}

#ifdef BUILD_CU_CP_STANDALONE
int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    return mock_cu_cp_test();
}
#endif