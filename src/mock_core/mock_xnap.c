/*
 * 5G UE Simulation Application
 * Mock XnAP (Xn Application Protocol) Interface
 * 3GPP TS 38.423 - gNB to gNB Interface
 */

#include "mock_core.h"
#include "../protocol/xnap_messages.h"
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

#define XNAP_MAX_NEIGHBOR_GNBS      32
#define XNAP_MAX_UE_CONTEXTS        1024
#define XNAP_DEFAULT_PORT           38422
#define XNAP_BUFFER_SIZE            65536

/* ============== XnAP States ============== */

typedef enum {
    XNAP_STATE_IDLE = 0,
    XNAP_STATE_XN_SETUP_PENDING,
    XNAP_STATE_ACTIVE,
    XNAP_STATE_RESETTING,
    XNAP_STATE_MAX
} xnap_state_t;

/* ============== Neighbor gNB Context ============== */

typedef struct {
    uint64_t gnb_id;
    char gnb_name[64];
    int xnap_socket;
    xnap_state_t state;
    
    /* Served Cells */
    uint8_t num_served_cells;
    xnap_served_cell_info_t served_cells[XNAP_MAX_SERVED_CELL_COUNT];
    
    /* Connection Info */
    struct sockaddr_in gnb_addr;
    time_t connect_time;
    time_t last_activity;
    bool active;
} xnap_neighbor_gnb_t;

/* ============== XnAP UE Context ============== */

typedef struct {
    uint32_t source_gnb_ue_xnap_id;
    uint32_t target_gnb_ue_xnap_id;
    uint64_t amf_ue_ngap_id;
    
    /* Handover Info */
    uint64_t target_gnb_id;
    uint64_t source_gnb_id;
    uint16_t target_pci;
    uint64_t target_cell_id;
    
    /* DRB/SRB */
    uint8_t num_drbs;
    xnap_drb_info_t drbs[XNAP_MAX_DRB_COUNT];
    
    /* State */
    uint8_t ho_state;  /* 0=idle, 1=preparing, 2=prepared, 3=executing, 4=completed */
    time_t setup_time;
    bool active;
} xnap_ue_context_t;

/* ============== XnAP Configuration ============== */

typedef struct {
    char bind_ip[46];
    uint16_t xnap_port;
    
    /* gNB Identity */
    uint64_t gnb_id;
    char gnb_name[64];
    
    /* Served Cells */
    uint8_t num_served_cells;
    xnap_served_cell_info_t served_cells[XNAP_MAX_SERVED_CELL_COUNT];
    
    /* Behavior */
    bool auto_respond;
    bool log_messages;
    
    /* PCAP */
    char pcap_file[256];
} xnap_config_t;

/* ============== XnAP Server Context ============== */

struct xnap_server_s {
    xnap_config_t config;
    
    /* Listening Socket */
    int listen_socket;
    
    /* Neighbor gNBs */
    xnap_neighbor_gnb_t neighbors[XNAP_MAX_NEIGHBOR_GNBS];
    uint32_t num_neighbors;
    
    /* UE Contexts (for handover) */
    xnap_ue_context_t ue_contexts[XNAP_MAX_UE_CONTEXTS];
    uint32_t num_ue_contexts;
    uint32_t next_ue_xnap_id;
    
    /* Server State */
#ifdef _WIN32
    volatile LONG running;
#else
    atomic_bool running;
#endif
    pthread_t xnap_thread;
    pthread_mutex_t neighbor_mutex;
    pthread_mutex_t ue_mutex;
    
    /* Statistics */
    uint64_t xnap_messages_rx;
    uint64_t xnap_messages_tx;
    uint64_t xn_setup_success;
    uint64_t xn_setup_failure;
    uint64_t handovers_initiated;
    uint64_t handovers_success;
    uint64_t handovers_failure;
};

/* Forward declaration */
typedef struct xnap_server_s xnap_server_t;

/* ============== Forward Declarations ============== */

static void* xnap_listener_thread(void* arg);

/* ============== XnAP API Functions ============== */

void xnap_get_default_config(xnap_config_t* config) {
    if (!config) return;
    memset(config, 0, sizeof(xnap_config_t));
    
    strncpy(config->bind_ip, "0.0.0.0", sizeof(config->bind_ip) - 1);
    config->xnap_port = XNAP_DEFAULT_PORT;
    config->gnb_id = 0x00000001;
    strncpy(config->gnb_name, "UESim-gNB-Xn", sizeof(config->gnb_name) - 1);
    
    /* Default served cell */
    config->num_served_cells = 1;
    config->served_cells[0].nr_cell_id.nr_cell_id = 0x123456789ABULL;
    config->served_cells[0].pci.pci = 1;
    config->served_cells[0].tac.tac = 1;
    
    config->auto_respond = true;
    config->log_messages = true;
}

xnap_server_t* xnap_create(const xnap_config_t* config) {
    xnap_server_t* server = (xnap_server_t*)calloc(1, sizeof(xnap_server_t));
    if (!server) return NULL;
    
    if (config) {
        memcpy(&server->config, config, sizeof(xnap_config_t));
    } else {
        xnap_get_default_config(&server->config);
    }
    
    server->listen_socket = -1;
    server->next_ue_xnap_id = 1;
    
    return server;
}

void xnap_destroy(xnap_server_t* server) {
    if (!server) return;
    
    xnap_stop(server);
    
    /* Close all neighbor connections */
    for (int i = 0; i < XNAP_MAX_NEIGHBOR_GNBS; i++) {
        if (server->neighbors[i].active && server->neighbors[i].xnap_socket >= 0) {
#ifdef _WIN32
            closesocket(server->neighbors[i].xnap_socket);
#else
            close(server->neighbors[i].xnap_socket);
#endif
        }
    }
    
    pthread_mutex_destroy(&server->neighbor_mutex);
    pthread_mutex_destroy(&server->ue_mutex);
    free(server);
}

/* ============== XnAP Listener Thread ============== */

static void* xnap_listener_thread(void* arg) {
    xnap_server_t* server = (xnap_server_t*)arg;
    uint8_t buffer[XNAP_BUFFER_SIZE];
    struct sockaddr_in client_addr;
    socklen_t client_len;
    ssize_t bytes_recv;
    fd_set read_fds;
    struct timeval tv;
    int max_fd;
    
    printf("[XnAP] Listener thread started on port %u\n", server->config.xnap_port);
    
    while (atomic_load(&server->running)) {
        FD_ZERO(&read_fds);
        FD_SET(server->listen_socket, &read_fds);
        max_fd = server->listen_socket;
        
        /* Add all neighbor sockets */
        for (int i = 0; i < XNAP_MAX_NEIGHBOR_GNBS; i++) {
            if (server->neighbors[i].active && server->neighbors[i].xnap_socket >= 0) {
                FD_SET(server->neighbors[i].xnap_socket, &read_fds);
                if (server->neighbors[i].xnap_socket > max_fd) {
                    max_fd = server->neighbors[i].xnap_socket;
                }
            }
        }
        
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int ret = select(max_fd + 1, &read_fds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (ret == 0) continue;  /* Timeout */
        
        /* Check for new connections */
        if (FD_ISSET(server->listen_socket, &read_fds)) {
            client_len = sizeof(client_addr);
            int client_sock = accept(server->listen_socket,
                                      (struct sockaddr*)&client_addr, &client_len);
            if (client_sock >= 0) {
                printf("[XnAP] New connection from %s:%u\n",
                       inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                /* Connection will be associated after Xn Setup */
            }
        }
        
        /* Check for data from neighbors */
        for (int i = 0; i < XNAP_MAX_NEIGHBOR_GNBS; i++) {
            if (server->neighbors[i].active &&
                server->neighbors[i].xnap_socket >= 0 &&
                FD_ISSET(server->neighbors[i].xnap_socket, &read_fds)) {
                
                bytes_recv = recv(server->neighbors[i].xnap_socket, (char*)buffer, sizeof(buffer), 0);
                if (bytes_recv > 0) {
                    xnap_process_message(server, buffer, (size_t)bytes_recv, i);
                } else {
                    printf("[XnAP] Neighbor gNB disconnected\n");
                    server->neighbors[i].active = false;
#ifdef _WIN32
                    closesocket(server->neighbors[i].xnap_socket);
#else
                    close(server->neighbors[i].xnap_socket);
#endif
                    server->neighbors[i].xnap_socket = -1;
                }
            }
        }
    }
    
    printf("[XnAP] Listener thread stopped\n");
    return NULL;
}

mock_core_error_t xnap_start(xnap_server_t* server) {
    if (!server) return MOCK_CORE_ERROR_INVALID_PARAM;
    
    struct sockaddr_in addr;
    
    /* Create TCP socket for XnAP */
    server->listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server->listen_socket < 0) {
        return MOCK_CORE_ERROR_SOCKET;
    }
    
    /* Set socket options */
    int opt = 1;
    setsockopt(server->listen_socket, SOL_SOCKET, SO_REUSEADDR,
               (const char*)&opt, sizeof(opt));
    
    /* Bind */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server->config.xnap_port);
    inet_pton(AF_INET, server->config.bind_ip, &addr.sin_addr);
    
    if (bind(server->listen_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
#ifdef _WIN32
        closesocket(server->listen_socket);
#else
        close(server->listen_socket);
#endif
        server->listen_socket = -1;
        return MOCK_CORE_ERROR_SOCKET;
    }
    
    /* Listen */
    if (listen(server->listen_socket, 10) < 0) {
#ifdef _WIN32
        closesocket(server->listen_socket);
#else
        close(server->listen_socket);
#endif
        server->listen_socket = -1;
        return MOCK_CORE_ERROR_SOCKET;
    }
    
    /* Initialize mutexes */
    if (pthread_mutex_init(&server->neighbor_mutex, NULL) != 0 ||
        pthread_mutex_init(&server->ue_mutex, NULL) != 0) {
#ifdef _WIN32
        closesocket(server->listen_socket);
#else
        close(server->listen_socket);
#endif
        server->listen_socket = -1;
        return MOCK_CORE_ERROR_THREAD;
    }
    
    /* Start listener thread */
    atomic_store(&server->running, 1);
    if (pthread_create(&server->xnap_thread, NULL, xnap_listener_thread, server) != 0) {
        atomic_store(&server->running, 0);
#ifdef _WIN32
        closesocket(server->listen_socket);
#else
        close(server->listen_socket);
#endif
        server->listen_socket = -1;
        return MOCK_CORE_ERROR_THREAD;
    }
    
    printf("[XnAP] Server started on %s:%u\n",
           server->config.bind_ip, server->config.xnap_port);
    return MOCK_CORE_SUCCESS;
}

void xnap_stop(xnap_server_t* server) {
    if (!server) return;
    
    atomic_store(&server->running, 0);
    
    if (server->listen_socket >= 0) {
#ifdef _WIN32
        closesocket(server->listen_socket);
#else
        close(server->listen_socket);
#endif
        server->listen_socket = -1;
    }
    
#ifdef _WIN32
    WaitForSingleObject(server->xnap_thread, 5000);
#else
    pthread_join(server->xnap_thread, NULL);
#endif
    
    printf("[XnAP] Server stopped\n");
}

/* ============== XnAP Message Handlers ============== */

static int xnap_handle_xn_setup_request(xnap_server_t* server,
                                         const xnap_message_t* req,
                                         int neighbor_idx,
                                         xnap_message_t* response) {
    if (!server || !req || !response) return -1;
    
    const xnap_xn_setup_request_t* setup_req = &req->payload.xn_setup_request;
    
    printf("[XnAP] Xn Setup Request from gNB ID: 0x%llX\n",
           (unsigned long long)setup_req->gnb_id.gnb_id);
    printf("[XnAP]   gNB Name: %s\n", setup_req->gnb_id.gnb_name);
    printf("[XnAP]   Served Cells: %d\n", setup_req->num_served_cells);
    
    /* Store neighbor info */
    xnap_neighbor_gnb_t* neighbor = &server->neighbors[neighbor_idx];
    neighbor->gnb_id = setup_req->gnb_id.gnb_id;
    strncpy(neighbor->gnb_name, setup_req->gnb_id.gnb_name, sizeof(neighbor->gnb_name) - 1);
    neighbor->num_served_cells = setup_req->num_served_cells;
    for (int i = 0; i < setup_req->num_served_cells && i < XNAP_MAX_SERVED_CELL_COUNT; i++) {
        memcpy(&neighbor->served_cells[i], &setup_req->served_cells[i],
               sizeof(xnap_served_cell_info_t));
    }
    neighbor->state = XNAP_STATE_ACTIVE;
    neighbor->active = true;
    server->num_neighbors++;
    server->xn_setup_success++;
    
    /* Build Xn Setup Response */
    response->message_type = XNAP_MSG_XN_SETUP_RESPONSE;
    response->procedure_code = XNAP_PROC_XN_SETUP;
    response->criticality = 0;
    
    xnap_xn_setup_response_t* resp = &response->payload.xn_setup_response;
    resp->gnb_id.gnb_id = server->config.gnb_id;
    strncpy(resp->gnb_id.gnb_name, server->config.gnb_name, sizeof(resp->gnb_id.gnb_name) - 1);
    resp->num_served_cells = server->config.num_served_cells;
    for (int i = 0; i < server->config.num_served_cells && i < XNAP_MAX_SERVED_CELL_COUNT; i++) {
        memcpy(&resp->served_cells[i], &server->config.served_cells[i],
               sizeof(xnap_served_cell_info_t));
    }
    
    return 0;
}

static int xnap_handle_handover_request(xnap_server_t* server,
                                         const xnap_message_t* req,
                                         xnap_message_t* response) {
    if (!server || !req || !response) return -1;
    
    const xnap_handover_request_t* ho_req = &req->payload.handover_request;
    
    printf("[XnAP] Handover Request:\n");
    printf("[XnAP]   Source gNB UE XnAP ID: %u\n", ho_req->source_gnb_ue_xnap_id);
    printf("[XnAP]   Target Cell ID: 0x%llX\n",
           (unsigned long long)ho_req->target_cell_id.nr_cell_id);
    printf("[XnAP]   DRBs: %d\n", ho_req->num_drbs);
    
    /* Find or create UE context */
    pthread_mutex_lock(&server->ue_mutex);
    xnap_ue_context_t* ue = NULL;
    for (int i = 0; i < XNAP_MAX_UE_CONTEXTS; i++) {
        if (!server->ue_contexts[i].active) {
            ue = &server->ue_contexts[i];
            memset(ue, 0, sizeof(xnap_ue_context_t));
            ue->target_gnb_ue_xnap_id = server->next_ue_xnap_id++;
            ue->source_gnb_ue_xnap_id = ho_req->source_gnb_ue_xnap_id;
            ue->active = true;
            ue->ho_state = 2;  /* Prepared */
            ue->setup_time = time(NULL);
            server->num_ue_contexts++;
            break;
        }
    }
    pthread_mutex_unlock(&server->ue_mutex);
    
    if (!ue) {
        response->message_type = XNAP_MSG_HANDOVER_PREPARATION_FAILURE;
        response->procedure_code = XNAP_PROC_HANDOVER_PREPARATION;
        xnap_set_cause_misc(&response->payload.handover_preparation_failure.cause,
                            XNAP_CAUSE_MISC_CONTROL_PROCESSING_OVERLOAD);
        server->handovers_failure++;
        return 0;
    }
    
    /* Store DRB info */
    ue->num_drbs = ho_req->num_drbs;
    for (int i = 0; i < ho_req->num_drbs && i < XNAP_MAX_DRB_COUNT; i++) {
        memcpy(&ue->drbs[i], &ho_req->drbs[i], sizeof(xnap_drb_info_t));
    }
    
    /* Build Handover Request Acknowledge */
    response->message_type = XNAP_MSG_HANDOVER_REQUEST_ACKNOWLEDGE;
    response->procedure_code = XNAP_PROC_HANDOVER_PREPARATION;
    response->criticality = 0;
    
    xnap_handover_request_acknowledge_t* ack = &response->payload.handover_request_acknowledge;
    ack->source_gnb_ue_xnap_id = ue->source_gnb_ue_xnap_id;
    ack->target_gnb_ue_xnap_id = ue->target_gnb_ue_xnap_id;
    ack->num_drbs_setup = ue->num_drbs;
    
    /* DL Forwarding TNL (would be allocated) */
    for (int i = 0; i < ue->num_drbs && i < XNAP_MAX_DRB_COUNT; i++) {
        ack->dl_forwarding_tnl[i].ip_address = inet_addr(server->config.bind_ip);
        ack->dl_forwarding_tnl[i].teid = (uint32_t)(ue->target_gnb_ue_xnap_id << 8 | i);
        ack->dl_forwarding_tnl[i].port = 2152;
    }
    
    server->handovers_success++;
    return 0;
}

/* ============== XnAP Message Processing ============== */

mock_core_error_t xnap_process_message(xnap_server_t* server,
                                        const uint8_t* data, size_t len,
                                        int neighbor_idx) {
    if (!server || !data || len == 0) return MOCK_CORE_ERROR_INVALID_PARAM;
    
    xnap_message_t msg;
    if (xnap_decode_message(data, len, &msg) != 0) {
        printf("[XnAP] Failed to decode XnAP message\n");
        return MOCK_CORE_ERROR_PROTOCOL;
    }
    
    server->xnap_messages_rx++;
    
    if (server->config.log_messages) {
        printf("[XnAP] Received XnAP Message: %s (Procedure: %d)\n",
               xnap_message_type_to_string(msg.message_type),
               msg.procedure_code);
    }
    
    xnap_message_t response;
    memset(&response, 0, sizeof(response));
    
    switch (msg.message_type) {
        case XNAP_MSG_XN_SETUP_REQUEST:
            xnap_handle_xn_setup_request(server, &msg, neighbor_idx, &response);
            break;
            
        case XNAP_MSG_HANDOVER_REQUEST:
            xnap_handle_handover_request(server, &msg, &response);
            break;
            
        case XNAP_MSG_HANDOVER_NOTIFY: {
            const xnap_handover_notify_t* notify = &msg.payload.handover_notify;
            printf("[XnAP] Handover Notify: UE ID=%u\n", notify->source_gnb_ue_xnap_id);
            server->handovers_initiated++;
            break;
        }
            
        case XNAP_MSG_HANDOVER_CANCEL: {
            const xnap_handover_cancel_t* cancel = &msg.payload.handover_cancel;
            printf("[XnAP] Handover Cancel: UE ID=%u, Reason=%u\n",
                   cancel->source_gnb_ue_xnap_id, cancel->cause.cause_value);
            break;
        }
            
        case XNAP_MSG_XN_RESET_REQUEST:
            printf("[XnAP] Xn Reset Request received\n");
            response.message_type = XNAP_MSG_XN_RESET_RESPONSE;
            response.procedure_code = XNAP_PROC_XN_RESET;
            break;
            
        case XNAP_MSG_ERROR_INDICATION: {
            const xnap_error_indication_t* err = &msg.payload.error_indication;
            printf("[XnAP] Error Indication: Cause=%s\n",
                   err->cause_present ? xnap_cause_to_string(&err->cause) : "N/A");
            break;
        }
            
        default:
            printf("[XnAP] Unhandled message type: %d\n", msg.message_type);
            break;
    }
    
    /* Send response if needed */
    if (response.message_type != XNAP_MSG_MAX && server->config.auto_respond) {
        uint8_t* resp_buffer = NULL;
        size_t resp_len = 0;
        
        if (xnap_encode_message(&response, &resp_buffer, &resp_len) == 0) {
            printf("[XnAP] Sending response: %s\n",
                   xnap_message_type_to_string(response.message_type));
            /* send to neighbor socket */
            free(resp_buffer);
            server->xnap_messages_tx++;
        }
    }
    
    xnap_free_message(&msg);
    return MOCK_CORE_SUCCESS;
}

/* ============== XnAP Handover Initiation ============== */

mock_core_error_t xnap_initiate_handover(xnap_server_t* server,
                                          uint32_t source_ue_id,
                                          uint64_t target_gnb_id,
                                          uint64_t target_cell_id) {
    if (!server) return MOCK_CORE_ERROR_INVALID_PARAM;
    
    /* Find target gNB */
    xnap_neighbor_gnb_t* target = NULL;
    for (int i = 0; i < XNAP_MAX_NEIGHBOR_GNBS; i++) {
        if (server->neighbors[i].active &&
            server->neighbors[i].gnb_id == target_gnb_id) {
            target = &server->neighbors[i];
            break;
        }
    }
    
    if (!target) {
        printf("[XnAP] Target gNB not found: 0x%llX\n", (unsigned long long)target_gnb_id);
        return MOCK_CORE_ERROR_NOT_FOUND;
    }
    
    /* Build Handover Request */
    xnap_message_t msg;
    xnap_init_handover_request(&msg);
    
    xnap_handover_request_t* req = &msg.payload.handover_request;
    req->source_gnb_ue_xnap_id = source_ue_id;
    req->target_cell_id.nr_cell_id = target_cell_id;
    req->num_drbs = 1;
    req->drbs[0].drb_id = 1;
    req->drbs[0].qos_flow_info.qfi = 1;
    
    uint8_t* buffer = NULL;
    size_t length = 0;
    
    if (xnap_encode_message(&msg, &buffer, &length) == 0) {
        printf("[XnAP] Sending Handover Request to gNB 0x%llX\n",
               (unsigned long long)target_gnb_id);
        /* send(target->xnap_socket, buffer, length, 0); */
        free(buffer);
        server->xnap_messages_tx++;
        server->handovers_initiated++;
    }
    
    return MOCK_CORE_SUCCESS;
}

/* ============== XnAP Statistics ============== */

void xnap_print_statistics(const xnap_server_t* server) {
    if (!server) return;
    
    printf("\n[XnAP] Statistics:\n");
    printf("  XnAP Messages RX: %llu\n", (unsigned long long)server->xnap_messages_rx);
    printf("  XnAP Messages TX: %llu\n", (unsigned long long)server->xnap_messages_tx);
    printf("  Xn Setup Success: %llu\n", (unsigned long long)server->xn_setup_success);
    printf("  Xn Setup Failure: %llu\n", (unsigned long long)server->xn_setup_failure);
    printf("  Active Neighbors: %u\n", server->num_neighbors);
    printf("  Handovers Initiated: %llu\n", (unsigned long long)server->handovers_initiated);
    printf("  Handovers Success: %llu\n", (unsigned long long)server->handovers_success);
    printf("  Handovers Failure: %llu\n", (unsigned long long)server->handovers_failure);
    printf("  Active UE Contexts: %u\n", server->num_ue_contexts);
}