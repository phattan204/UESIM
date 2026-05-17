/*
 * 5G UE Simulation Application
 * Mock DU (Distributed Unit) Entity
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

#define DU_MAX_CU_CONNECTIONS    4
#define DU_MAX_UE_CONTEXTS       1024
#define DU_F1AP_PORT             38472
#define DU_BUFFER_SIZE           65536
#define DU_MAX_DRB_PER_UE        8
#define DU_MAX_SRB_PER_UE        3

/* ============== DU States ============== */

typedef enum {
    DU_STATE_IDLE = 0,
    DU_STATE_CONNECTING,
    DU_STATE_F1_SETUP_PENDING,
    DU_STATE_ACTIVE,
    DU_STATE_RESETTING,
    DU_STATE_MAX
} du_state_t;

/* ============== CU Connection Context ============== */

typedef struct {
    uint64_t gnb_cu_id;
    char gnb_cu_name[64];
    int f1ap_socket;
    du_state_t state;
    
    /* Cells to Activate */
    uint8_t num_cells_to_activate;
    f1ap_served_cell_info_t cells_to_activate[F1AP_MAX_CELL_COUNT];
    
    /* Connection Info */
    struct sockaddr_in cu_addr;
    time_t connect_time;
    time_t last_activity;
    bool active;
} cu_connection_t;

/* ============== DU UE Context ============== */

typedef struct {
    uint32_t gnb_cu_ue_f1ap_id;
    uint32_t gnb_du_ue_f1ap_id;
    uint64_t ran_ue_id;
    
    /* Cell Info */
    uint64_t nr_cell_id;
    uint16_t pci;
    uint32_t tac;
    
    /* DRB/SRB */
    uint8_t num_drbs;
    f1ap_drb_info_t drbs[DU_MAX_DRB_PER_UE];
    uint8_t num_srbs;
    uint8_t srbs[DU_MAX_SRB_PER_UE];
    
    /* RLC/MAC State */
    uint8_t rlc_state[DU_MAX_DRB_PER_UE];
    uint16_t c_rnti;
    
    /* CU Reference */
    uint64_t gnb_cu_id;
    
    /* State */
    uint8_t ue_state;
    time_t setup_time;
    time_t last_activity;
    bool active;
} du_ue_context_t;

/* ============== DU Configuration ============== */

typedef struct {
    char cu_cp_ip[46];
    uint16_t cu_cp_port;
    char bind_ip[46];
    
    /* gNB-DU Identity */
    uint32_t gnb_du_id;
    char gnb_du_name[64];
    
    /* Served Cells */
    uint8_t num_served_cells;
    f1ap_served_cell_info_t served_cells[F1AP_MAX_CELL_COUNT];
    
    /* RRC Version */
    uint8_t rrc_version[4];
    
    /* RANAC */
    uint8_t ranac;
    
    /* Behavior */
    bool auto_respond;
    bool log_messages;
    uint32_t response_delay_ms;
    
    /* PCAP */
    char pcap_file[256];
} du_config_t;

/* ============== DU Server Context ============== */

struct du_server_s {
    du_config_t config;
    
    /* Socket */
    int f1ap_socket;
    
    /* CU Connection */
    cu_connection_t cu_connection;
    
    /* UE Contexts */
    du_ue_context_t ue_contexts[DU_MAX_UE_CONTEXTS];
    uint32_t num_ue_contexts;
    uint32_t next_du_ue_id;
    
    /* Server State */
#ifdef _WIN32
    volatile LONG running;
#else
    atomic_bool running;
#endif
    pthread_t f1ap_thread;
    pthread_mutex_t ue_mutex;
    
    /* Statistics */
    uint64_t f1ap_messages_rx;
    uint64_t f1ap_messages_tx;
    uint64_t f1_setup_success;
    uint64_t f1_setup_failure;
    uint64_t ue_contexts_created;
    uint64_t ue_contexts_released;
    uint64_t ul_rrc_messages;
    uint64_t dl_rrc_messages;
};

/* ============== Forward Declarations ============== */

static void* f1ap_listener_thread(void* arg);

/* ============== DU API Functions ============== */

void du_get_default_config(du_config_t* config) {
    if (!config) return;
    memset(config, 0, sizeof(du_config_t));
    
    strncpy(config->cu_cp_ip, "127.0.0.1", sizeof(config->cu_cp_ip) - 1);
    config->cu_cp_port = DU_F1AP_PORT;
    strncpy(config->bind_ip, "0.0.0.0", sizeof(config->bind_ip) - 1);
    
    config->gnb_du_id = 0x00000001;
    strncpy(config->gnb_du_name, "UESim-DU-01", sizeof(config->gnb_du_name) - 1);
    
    /* Default served cell */
    config->num_served_cells = 1;
    config->served_cells[0].nr_cell_id.nr_cell_id = 0x123456789ABULL;
    config->served_cells[0].pci.pci = 1;
    config->served_cells[0].tac.tac = 1;
    config->served_cells[0].num_plmns = 1;
    config->served_cells[0].plmns[0].mcc[0] = 0;
    config->served_cells[0].plmns[0].mcc[1] = 0;
    config->served_cells[0].plmns[0].mcc[2] = 1;
    config->served_cells[0].plmns[0].mnc[0] = 0;
    config->served_cells[0].plmns[0].mnc[1] = 1;
    config->served_cells[0].plmns[0].mnc_length = 2;
    config->served_cells[0].ngran_duplex_mode = 1;  /* TDD */
    
    config->rrc_version[0] = 15;  /* v15.0.0 */
    config->ranac = 1;
    config->auto_respond = true;
    config->log_messages = true;
}

du_server_t* du_create(const du_config_t* config) {
    du_server_t* du = (du_server_t*)calloc(1, sizeof(du_server_t));
    if (!du) return NULL;
    
    if (config) {
        memcpy(&du->config, config, sizeof(du_config_t));
    } else {
        du_get_default_config(&du->config);
    }
    
    du->f1ap_socket = -1;
    du->next_du_ue_id = 1;
    
    return du;
}

void du_destroy(du_server_t* du) {
    if (!du) return;
    
    du_stop(du);
    
    /* Close CU connection */
    if (du->cu_connection.active && du->cu_connection.f1ap_socket >= 0) {
#ifdef _WIN32
        closesocket(du->cu_connection.f1ap_socket);
#else
        close(du->cu_connection.f1ap_socket);
#endif
    }
    
    pthread_mutex_destroy(&du->ue_mutex);
    free(du);
}

/* ============== F1AP Listener Thread ============== */

static void* f1ap_listener_thread(void* arg) {
    du_server_t* du = (du_server_t*)arg;
    uint8_t buffer[DU_BUFFER_SIZE];
    ssize_t bytes_recv;
    fd_set read_fds;
    struct timeval tv;
    
    printf("[DU] F1AP listener thread started\n");
    
    while (atomic_load(&du->running)) {
        FD_ZERO(&read_fds);
        FD_SET(du->cu_connection.f1ap_socket, &read_fds);
        
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int ret = select(du->cu_connection.f1ap_socket + 1, &read_fds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (ret == 0) continue;  /* Timeout */
        
        bytes_recv = recv(du->cu_connection.f1ap_socket, (char*)buffer, sizeof(buffer), 0);
        if (bytes_recv > 0) {
            du_process_f1ap_message(du, buffer, (size_t)bytes_recv);
        } else if (bytes_recv <= 0) {
            /* Connection closed or error */
            printf("[DU] CU-CP connection closed\n");
            break;
        }
    }
    
    printf("[DU] F1AP listener thread stopped\n");
    return NULL;
}

mock_core_error_t du_start(du_server_t* du) {
    if (!du) return MOCK_CORE_ERROR_INVALID_PARAM;
    
    /* Initialize mutex */
    if (pthread_mutex_init(&du->ue_mutex, NULL) != 0) {
        return MOCK_CORE_ERROR_THREAD;
    }
    
    atomic_store(&du->running, 1);
    
    printf("[DU] Server started\n");
    return MOCK_CORE_SUCCESS;
}

void du_stop(du_server_t* du) {
    if (!du) return;
    
    atomic_store(&du->running, 0);
    
    if (du->cu_connection.f1ap_socket >= 0) {
#ifdef _WIN32
        closesocket(du->cu_connection.f1ap_socket);
#else
        close(du->cu_connection.f1ap_socket);
#endif
        du->cu_connection.f1ap_socket = -1;
    }
    
    if (du->f1ap_socket >= 0) {
#ifdef _WIN32
        closesocket(du->f1ap_socket);
#else
        close(du->f1ap_socket);
#endif
        du->f1ap_socket = -1;
    }
    
    /* Wait for thread to terminate if running */
    if (du->f1ap_thread) {
#ifdef _WIN32
        WaitForSingleObject(du->f1ap_thread, 5000);
#else
        pthread_join(du->f1ap_thread, NULL);
#endif
    }
    
    printf("[DU] Server stopped\n");
}

mock_core_error_t du_connect_cu(du_server_t* du, const char* cu_ip, uint16_t port) {
    if (!du || !cu_ip) return MOCK_CORE_ERROR_INVALID_PARAM;
    
    struct sockaddr_in cu_addr;
    
    /* Update config */
    strncpy(du->config.cu_cp_ip, cu_ip, sizeof(du->config.cu_cp_ip) - 1);
    if (port > 0) {
        du->config.cu_cp_port = port;
    }
    
    /* Create socket */
    du->cu_connection.f1ap_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (du->cu_connection.f1ap_socket < 0) {
        return MOCK_CORE_ERROR_SOCKET;
    }
    
    /* Connect to CU-CP */
    memset(&cu_addr, 0, sizeof(cu_addr));
    cu_addr.sin_family = AF_INET;
    cu_addr.sin_port = htons(du->config.cu_cp_port);
    inet_pton(AF_INET, du->config.cu_cp_ip, &cu_addr.sin_addr);
    
    du->cu_connection.state = DU_STATE_CONNECTING;
    
    if (connect(du->cu_connection.f1ap_socket, (struct sockaddr*)&cu_addr, sizeof(cu_addr)) < 0) {
#ifdef _WIN32
        closesocket(du->cu_connection.f1ap_socket);
#else
        close(du->cu_connection.f1ap_socket);
#endif
        du->cu_connection.f1ap_socket = -1;
        du->cu_connection.state = DU_STATE_IDLE;
        return MOCK_CORE_ERROR_SOCKET;
    }
    
    memcpy(&du->cu_connection.cu_addr, &cu_addr, sizeof(cu_addr));
    du->cu_connection.connect_time = time(NULL);
    
    printf("[DU] Connected to CU-CP at %s:%u\n", du->config.cu_cp_ip, du->config.cu_cp_port);
    
    /* Start listener thread if running */
    if (atomic_load(&du->running)) {
        if (pthread_create(&du->f1ap_thread, NULL, f1ap_listener_thread, du) != 0) {
            return MOCK_CORE_ERROR_THREAD;
        }
    }
    
    return MOCK_CORE_SUCCESS;
}

/* ============== F1AP Message Handlers ============== */

static int du_handle_f1_setup_response(du_server_t* du,
                                        const f1ap_message_t* resp) {
    if (!du || !resp) return -1;
    
    const f1ap_f1_setup_response_t* setup_resp = &resp->payload.f1_setup_response;
    
    printf("[DU] F1 Setup Response received:\n");
    printf("[DU]   gNB-CU ID: 0x%llX\n", 
           (unsigned long long)setup_resp->gnb_cu_id.gnb_cu_id);
    printf("[DU]   gNB-CU Name: %s\n", setup_resp->gnb_cu_id.gnb_cu_name);
    printf("[DU]   Cells to Activate: %d\n", setup_resp->num_cells_to_activate);
    
    /* Store CU connection info */
    cu_connection_t* cu = &du->cu_connection;
    cu->gnb_cu_id = setup_resp->gnb_cu_id.gnb_cu_id;
    strncpy(cu->gnb_cu_name, (const char*)setup_resp->gnb_cu_id.gnb_cu_name,
            sizeof(cu->gnb_cu_name) - 1);
    cu->num_cells_to_activate = setup_resp->num_cells_to_activate;
    
    for (int i = 0; i < setup_resp->num_cells_to_activate && i < F1AP_MAX_CELL_COUNT; i++) {
        memcpy(&cu->cells_to_activate[i], &setup_resp->cells_to_activate[i],
               sizeof(f1ap_served_cell_info_t));
    }
    
    cu->state = DU_STATE_ACTIVE;
    cu->active = true;
    du->f1_setup_success++;
    
    return 0;
}

static int du_handle_f1_setup_failure(du_server_t* du,
                                       const f1ap_message_t* fail) {
    if (!du || !fail) return -1;
    
    const f1ap_f1_setup_failure_t* setup_fail = &fail->payload.f1_setup_failure;
    
    printf("[DU] F1 Setup Failure received:\n");
    printf("[DU]   Cause: %s\n", f1ap_cause_to_string(&setup_fail->cause));
    printf("[DU]   Time to Wait: %u seconds\n", setup_fail->time_to_wait);
    
    du->cu_connection.state = DU_STATE_IDLE;
    du->f1_setup_failure++;
    
    return 0;
}

static du_ue_context_t* du_create_ue_context(du_server_t* du) {
    if (!du) return NULL;
    
    for (int i = 0; i < DU_MAX_UE_CONTEXTS; i++) {
        if (!du->ue_contexts[i].active) {
            du_ue_context_t* ue = &du->ue_contexts[i];
            memset(ue, 0, sizeof(du_ue_context_t));
            ue->gnb_du_ue_f1ap_id = du->next_du_ue_id++;
            ue->active = true;
            ue->setup_time = time(NULL);
            ue->c_rnti = (uint16_t)(rand() % 65535) + 1;
            du->num_ue_contexts++;
            du->ue_contexts_created++;
            return ue;
        }
    }
    
    return NULL;
}

static du_ue_context_t* du_find_ue_by_cu_id(du_server_t* du, uint32_t cu_ue_id) {
    if (!du) return NULL;
    
    for (int i = 0; i < DU_MAX_UE_CONTEXTS; i++) {
        if (du->ue_contexts[i].active &&
            du->ue_contexts[i].gnb_cu_ue_f1ap_id == cu_ue_id) {
            return &du->ue_contexts[i];
        }
    }
    
    return NULL;
}

static du_ue_context_t* du_find_ue_by_du_id(du_server_t* du, uint32_t du_ue_id) {
    if (!du) return NULL;
    
    for (int i = 0; i < DU_MAX_UE_CONTEXTS; i++) {
        if (du->ue_contexts[i].active &&
            du->ue_contexts[i].gnb_du_ue_f1ap_id == du_ue_id) {
            return &du->ue_contexts[i];
        }
    }
    
    return NULL;
}

static int du_handle_ue_context_setup_request(du_server_t* du,
                                               const f1ap_message_t* req,
                                               f1ap_message_t* response) {
    if (!du || !req || !response) return -1;
    
    const f1ap_ue_context_setup_request_t* setup_req = &req->payload.ue_context_setup_request;
    
    printf("[DU] UE Context Setup Request:\n");
    printf("[DU]   gNB-CU-UE-F1AP-ID: %u\n", setup_req->ue_ids.gnb_cu_ue_f1ap_id);
    printf("[DU]   RAN UE ID: 0x%llX\n", 
           (unsigned long long)setup_req->ran_ue_id.ran_ue_id);
    printf("[DU]   DRBs to Setup: %d\n", setup_req->num_drbs_to_setup);
    printf("[DU]   SRBs to Setup: %d\n", setup_req->num_srbs_to_setup);
    
    /* Create UE context */
    du_ue_context_t* ue = du_create_ue_context(du);
    if (!ue) {
        /* Send failure */
        response->message_type = F1AP_MSG_UE_CONTEXT_SETUP_FAILURE;
        response->procedure_code = F1AP_PROC_UE_CONTEXT_SETUP;
        f1ap_set_cause_misc(&response->payload.ue_context_setup_failure.cause,
                            F1AP_CAUSE_MISC_CONTROL_PROCESSING_OVERLOAD);
        return 0;
    }
    
    /* Store UE info */
    ue->gnb_cu_ue_f1ap_id = setup_req->ue_ids.gnb_cu_ue_f1ap_id;
    ue->ran_ue_id = setup_req->ran_ue_id.ran_ue_id;
    ue->nr_cell_id = setup_req->nr_cell_id.nr_cell_id;
    ue->gnb_cu_id = du->cu_connection.gnb_cu_id;
    
    /* Setup DRBs */
    ue->num_drbs = setup_req->num_drbs_to_setup;
    for (int i = 0; i < setup_req->num_drbs_to_setup && i < DU_MAX_DRB_PER_UE; i++) {
        memcpy(&ue->drbs[i], &setup_req->drbs_to_setup[i], sizeof(f1ap_drb_info_t));
        ue->rlc_state[i] = 1;  /* RLC established */
    }
    
    /* Setup SRBs */
    ue->num_srbs = setup_req->num_srbs_to_setup;
    for (int i = 0; i < setup_req->num_srbs_to_setup && i < DU_MAX_SRB_PER_UE; i++) {
        ue->srbs[i] = setup_req->srbs_to_setup[i].srb_id;
    }
    
    ue->ue_state = 1;  /* Setup in progress */
    
    /* Build response */
    response->message_type = F1AP_MSG_UE_CONTEXT_SETUP_RESPONSE;
    response->procedure_code = F1AP_PROC_UE_CONTEXT_SETUP;
    response->criticality = 0;
    
    f1ap_ue_context_setup_response_t* resp = &response->payload.ue_context_setup_response;
    resp->ue_ids.gnb_cu_ue_f1ap_id = ue->gnb_cu_ue_f1ap_id;
    resp->ue_ids.gnb_du_ue_f1ap_id = ue->gnb_du_ue_f1ap_id;
    
    /* DRBs Setup */
    resp->num_drbs_setup = ue->num_drbs;
    for (int i = 0; i < ue->num_drbs && i < F1AP_MAX_DRB_COUNT; i++) {
        memcpy(&resp->drbs_setup[i], &ue->drbs[i], sizeof(f1ap_drb_info_t));
        /* Set DL UP TNL info */
        resp->drbs_setup[i].num_ul_up_tnl_info = 1;
        resp->drbs_setup[i].ul_up_tnl_ip[0] = inet_addr(du->config.bind_ip);
        resp->drbs_setup[i].ul_up_tnl_port[0] = 2152;  /* GTP-U port */
        resp->drbs_setup[i].ul_up_tnl_teid[0] = (uint32_t)(ue->gnb_du_ue_f1ap_id << 8 | ue->drbs[i].drb_id);
    }
    
    /* SRBs Setup */
    resp->num_srbs_setup = ue->num_srbs;
    for (int i = 0; i < ue->num_srbs && i < F1AP_MAX_SRB_ID; i++) {
        resp->srbs_setup[i] = ue->srbs[i];
    }
    
    /* DU to CU RRC Container (would contain RRC Reconfiguration Complete) */
    resp->du_to_cu_rrc_container.length = 0;  /* No RRC container in basic implementation */
    
    ue->ue_state = 2;  /* Active */
    
    return 0;
}

static int du_handle_ue_context_release_command(du_server_t* du,
                                                 const f1ap_message_t* cmd,
                                                 f1ap_message_t* response) {
    if (!du || !cmd || !response) return -1;
    
    const f1ap_ue_context_release_command_t* release_cmd = &cmd->payload.ue_context_release_command;
    
    printf("[DU] UE Context Release Command:\n");
    printf("[DU]   gNB-CU-UE-F1AP-ID: %u\n", release_cmd->ue_ids.gnb_cu_ue_f1ap_id);
    printf("[DU]   gNB-DU-UE-F1AP-ID: %u\n", release_cmd->ue_ids.gnb_du_ue_f1ap_id);
    printf("[DU]   Cause: %s\n", f1ap_cause_to_string(&release_cmd->cause));
    
    /* Find UE context */
    du_ue_context_t* ue = du_find_ue_by_cu_id(du, release_cmd->ue_ids.gnb_cu_ue_f1ap_id);
    
    /* Build response */
    response->message_type = F1AP_MSG_UE_CONTEXT_RELEASE_COMPLETE;
    response->procedure_code = F1AP_PROC_UE_CONTEXT_RELEASE;
    response->criticality = 0;
    
    f1ap_ue_context_release_complete_t* resp = &response->payload.ue_context_release_complete;
    resp->ue_ids.gnb_cu_ue_f1ap_id = release_cmd->ue_ids.gnb_cu_ue_f1ap_id;
    resp->ue_ids.gnb_du_ue_f1ap_id = release_cmd->ue_ids.gnb_du_ue_f1ap_id;
    resp->rrc_container.length = 0;
    
    /* Release UE context */
    if (ue) {
        ue->active = false;
        du->num_ue_contexts--;
        du->ue_contexts_released++;
    }
    
    return 0;
}

static int du_handle_dl_rrc_message(du_server_t* du,
                                     const f1ap_message_t* msg,
                                     f1ap_message_t* response) {
    if (!du || !msg) return -1;
    
    const f1ap_dl_rrc_message_transfer_t* transfer = &msg->payload.dl_rrc_message_transfer;
    
    printf("[DU] DL RRC Message Transfer:\n");
    printf("[DU]   gNB-CU-UE-F1AP-ID: %u\n", transfer->ue_ids.gnb_cu_ue_f1ap_id);
    printf("[DU]   gNB-DU-UE-F1AP-ID: %u\n", transfer->ue_ids.gnb_du_ue_f1ap_id);
    printf("[DU]   SRB ID: %u\n", transfer->srb_id);
    printf("[DU]   RRC Container Length: %zu\n", transfer->rrc_container.length);
    
    du->dl_rrc_messages++;
    
    /* Find UE */
    du_ue_context_t* ue = du_find_ue_by_cu_id(du, transfer->ue_ids.gnb_cu_ue_f1ap_id);
    if (!ue) {
        printf("[DU]   UE not found!\n");
        return -1;
    }
    
    ue->last_activity = time(NULL);
    
    /* In real implementation, would process RRC message and forward to UE */
    
    return 0;
}

/* ============== DU Message Processing ============== */

mock_core_error_t du_process_f1ap_message(du_server_t* du,
                                           const uint8_t* data, size_t len) {
    if (!du || !data || len == 0) return -1;
    
    f1ap_message_t msg;
    if (f1ap_decode_message(data, len, &msg) != 0) {
        printf("[DU] Failed to decode F1AP message\n");
        return -1;
    }
    
    du->f1ap_messages_rx++;
    
    if (du->config.log_messages) {
        printf("[DU] Received F1AP Message: %s (Procedure: %d)\n",
               f1ap_message_type_to_string(msg.message_type),
               msg.procedure_code);
    }
    
    f1ap_message_t response;
    memset(&response, 0, sizeof(response));
    
    switch (msg.message_type) {
        case F1AP_MSG_F1_SETUP_RESPONSE:
            du_handle_f1_setup_response(du, &msg);
            break;
            
        case F1AP_MSG_F1_SETUP_FAILURE:
            du_handle_f1_setup_failure(du, &msg);
            break;
            
        case F1AP_MSG_UE_CONTEXT_SETUP_REQUEST:
            du_handle_ue_context_setup_request(du, &msg, &response);
            break;
            
        case F1AP_MSG_UE_CONTEXT_RELEASE_COMMAND:
            du_handle_ue_context_release_command(du, &msg, &response);
            break;
            
        case F1AP_MSG_DL_RRC_MESSAGE_TRANSFER:
            du_handle_dl_rrc_message(du, &msg, &response);
            break;
            
        case F1AP_MSG_F1_RESET_REQUEST: {
            printf("[DU] F1 Reset Request received\n");
            response.message_type = F1AP_MSG_F1_RESET_RESPONSE;
            response.procedure_code = F1AP_PROC_F1_RESET;
            /* Clear all UE contexts */
            for (int i = 0; i < DU_MAX_UE_CONTEXTS; i++) {
                du->ue_contexts[i].active = false;
            }
            du->num_ue_contexts = 0;
            break;
        }
            
        case F1AP_MSG_ERROR_INDICATION: {
            const f1ap_error_indication_t* err = &msg.payload.error_indication;
            printf("[DU] Error Indication: Cause=%s\n",
                   err->cause_present ? f1ap_cause_to_string(&err->cause) : "N/A");
            break;
        }
            
        default:
            printf("[DU] Unhandled message type: %d\n", msg.message_type);
            break;
    }
    
    /* Send response if needed */
    if (response.message_type != F1AP_MSG_MAX && du->config.auto_respond) {
        uint8_t* resp_buffer = NULL;
        size_t resp_len = 0;
        
        if (f1ap_encode_message(&response, &resp_buffer, &resp_len) == 0) {
            printf("[DU] Sending response: %s\n",
                   f1ap_message_type_to_string(response.message_type));
            /* send(du->cu_connection.f1ap_socket, resp_buffer, resp_len, 0); */
            free(resp_buffer);
            du->f1ap_messages_tx++;
        }
    }
    
    f1ap_free_message(&msg);
    return 0;
}

/* ============== DU F1 Setup ============== */

mock_core_error_t du_send_f1_setup_request(du_server_t* du) {
    if (!du) return -1;
    
    f1ap_message_t msg;
    f1ap_init_f1_setup_request(&msg);
    
    f1ap_f1_setup_request_t* req = &msg.payload.f1_setup_request;
    req->gnb_du_id.gnb_du_id = du->config.gnb_du_id;
    strncpy((char*)req->gnb_du_id.gnb_du_name, du->config.gnb_du_name,
            sizeof(req->gnb_du_id.gnb_du_name) - 1);
    
    req->served_cells.num_cells = du->config.num_served_cells;
    for (int i = 0; i < du->config.num_served_cells && i < F1AP_MAX_CELL_COUNT; i++) {
        memcpy(&req->served_cells.cells[i], &du->config.served_cells[i],
               sizeof(f1ap_served_cell_info_t));
    }
    
    req->ranac = du->config.ranac;
    memcpy(req->gnb_du_rrc_version, du->config.rrc_version, 4);
    
    uint8_t* buffer = NULL;
    size_t length = 0;
    
    if (f1ap_encode_message(&msg, &buffer, &length) == 0) {
        printf("[DU] F1 Setup Request encoded: %zu bytes\n", length);
        printf("[DU]   gNB-DU ID: 0x%08X\n", req->gnb_du_id.gnb_du_id);
        printf("[DU]   gNB-DU Name: %s\n", req->gnb_du_id.gnb_du_name);
        printf("[DU]   Served Cells: %d\n", req->served_cells.num_cells);
        
        /* send(du->cu_connection.f1ap_socket, buffer, length, 0); */
        free(buffer);
        du->f1ap_messages_tx++;
        du->cu_connection.state = DU_STATE_F1_SETUP_PENDING;
    }
    
    return 0;
}

/* ============== UL RRC Message Transfer ============== */

static int du_send_ul_rrc_message(du_server_t* du,
                                   du_ue_context_t* ue,
                                   uint8_t srb_id,
                                   const uint8_t* rrc_data,
                                   size_t rrc_len) {
    if (!du || !ue || !rrc_data || rrc_len == 0) return -1;
    
    f1ap_message_t msg;
    f1ap_init_ul_rrc_message_transfer(&msg);
    
    f1ap_ul_rrc_message_transfer_t* transfer = &msg.payload.ul_rrc_message_transfer;
    transfer->ue_ids.gnb_cu_ue_f1ap_id = ue->gnb_cu_ue_f1ap_id;
    transfer->ue_ids.gnb_du_ue_f1ap_id = ue->gnb_du_ue_f1ap_id;
    transfer->srb_id = srb_id;
    
    if (rrc_len > F1AP_MAX_RRC_CONTAINER_SIZE) {
        rrc_len = F1AP_MAX_RRC_CONTAINER_SIZE;
    }
    memcpy(transfer->rrc_container.rrc_container, rrc_data, rrc_len);
    transfer->rrc_container.length = rrc_len;
    
    uint8_t* buffer = NULL;
    size_t length = 0;
    
    if (f1ap_encode_message(&msg, &buffer, &length) == 0) {
        printf("[DU] UL RRC Message Transfer: UE=%u, SRB=%u, Len=%zu\n",
               ue->gnb_du_ue_f1ap_id, srb_id, rrc_len);
        /* send(du->cu_connection.f1ap_socket, buffer, length, 0); */
        free(buffer);
        du->f1ap_messages_tx++;
        du->ul_rrc_messages++;
    }
    
    return 0;
}

/* ============== DU Statistics ============== */

void du_print_statistics(const du_server_t* du) {
    if (!du) return;
    
    printf("\n[DU] Statistics:\n");
    printf("  F1AP Messages RX: %llu\n",
           (unsigned long long)du->f1ap_messages_rx);
    printf("  F1AP Messages TX: %llu\n",
           (unsigned long long)du->f1ap_messages_tx);
    printf("  F1 Setup Success: %llu\n",
           (unsigned long long)du->f1_setup_success);
    printf("  F1 Setup Failure: %llu\n",
           (unsigned long long)du->f1_setup_failure);
    printf("  UE Contexts Created: %llu\n",
           (unsigned long long)du->ue_contexts_created);
    printf("  UE Contexts Released: %llu\n",
           (unsigned long long)du->ue_contexts_released);
    printf("  Active UE Contexts: %u\n", du->num_ue_contexts);
    printf("  UL RRC Messages: %llu\n",
           (unsigned long long)du->ul_rrc_messages);
    printf("  DL RRC Messages: %llu\n",
           (unsigned long long)du->dl_rrc_messages);
}

/* ============== Test Interface ============== */

int mock_du_test(void) {
    printf("\n=== DU Mock Test ===\n\n");
    
    /* Create DU server */
    du_config_t config;
    du_get_default_config(&config);
    config.log_messages = true;
    
    du_server_t* du = du_create(&config);
    if (!du) {
        printf("[DU] Failed to create server\n");
        return -1;
    }
    
    printf("[DU] Server created successfully\n");
    printf("[DU]   gNB-DU ID: 0x%08X\n", du->config.gnb_du_id);
    printf("[DU]   gNB-DU Name: %s\n", du->config.gnb_du_name);
    printf("[DU]   Served Cells: %d\n", du->config.num_served_cells);
    
    /* Test F1 Setup Request generation */
    printf("\n[DU] Testing F1 Setup Request...\n");
    du_send_f1_setup_request(du);
    
    /* Simulate F1 Setup Response from CU-CP */
    printf("\n[DU] Simulating F1 Setup Response...\n");
    
    f1ap_message_t setup_resp;
    f1ap_init_f1_setup_response(&setup_resp);
    
    f1ap_f1_setup_response_t* resp = &setup_resp.payload.f1_setup_response;
    resp->gnb_cu_id.gnb_cu_id = 0x1234567890ULL;
    strncpy((char*)resp->gnb_cu_id.gnb_cu_name, "UESim-CU-CP",
            sizeof(resp->gnb_cu_id.gnb_cu_name) - 1);
    resp->num_cells_to_activate = du->config.num_served_cells;
    for (int i = 0; i < du->config.num_served_cells && i < F1AP_MAX_CELL_COUNT; i++) {
        memcpy(&resp->cells_to_activate[i], &du->config.served_cells[i],
               sizeof(f1ap_served_cell_info_t));
    }
    resp->gnb_cu_rrc_version[0] = 15;
    resp->transport_layer_address = inet_addr("127.0.0.1");
    
    uint8_t* resp_buffer = NULL;
    size_t resp_len = 0;
    if (f1ap_encode_message(&setup_resp, &resp_buffer, &resp_len) == 0) {
        du_process_f1ap_message(du, resp_buffer, resp_len);
        free(resp_buffer);
    }
    
    /* Simulate UE Context Setup Request */
    printf("\n[DU] Simulating UE Context Setup Request...\n");
    
    f1ap_message_t ue_setup_req;
    memset(&ue_setup_req, 0, sizeof(ue_setup_req));
    ue_setup_req.message_type = F1AP_MSG_UE_CONTEXT_SETUP_REQUEST;
    ue_setup_req.procedure_code = F1AP_PROC_UE_CONTEXT_SETUP;
    ue_setup_req.criticality = 0;
    
    f1ap_ue_context_setup_request_t* ue_req = &ue_setup_req.payload.ue_context_setup_request;
    ue_req->ue_ids.gnb_cu_ue_f1ap_id = 1;
    ue_req->ran_ue_id.ran_ue_id = 0x123456789AULL;
    ue_req->num_drbs_to_setup = 1;
    ue_req->drbs_to_setup[0].drb_id = 1;
    ue_req->drbs_to_setup[0].rlc_mode = 1;
    ue_req->num_srbs_to_setup = 1;
    ue_req->srbs_to_setup[0].srb_id = 1;
    ue_req->nr_cell_id.nr_cell_id = du->config.served_cells[0].nr_cell_id.nr_cell_id;
    
    uint8_t* ue_req_buffer = NULL;
    size_t ue_req_len = 0;
    if (f1ap_encode_message(&ue_setup_req, &ue_req_buffer, &ue_req_len) == 0) {
        du_process_f1ap_message(du, ue_req_buffer, ue_req_len);
        free(ue_req_buffer);
    }
    
    /* Print statistics */
    du_print_statistics(du);
    
    /* Cleanup */
    du_destroy(du);
    printf("\n[DU] Server destroyed\n");
    
    printf("\n=== DU Mock Test Complete ===\n");
    return 0;
}

#ifdef BUILD_DU_STANDALONE
int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    return mock_du_test();
}
#endif
