/*
 * 5G UE Simulation Application
 * Mock Integration Layer Implementation
 */

#include "mock_integration.h"
#include "../protocol/ngap_messages.h"
#include "../core/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <ws2tcpip.h>
#else
#include <unistd.h>
#include <pthread.h>
#endif

/* ============== Utility Functions ============== */

void mock_integration_get_default_config(mock_integration_config_t* config) {
    if (config == NULL) return;
    
    memset(config, 0, sizeof(mock_integration_config_t));
    
    /* gNB defaults */
    strncpy(config->gnb_bind_ip, "0.0.0.0", sizeof(config->gnb_bind_ip) - 1);
    config->gnb_ngap_port = MOCK_INT_DEFAULT_GNB_NGAP_PORT;
    config->gnb_gtpu_port = MOCK_INT_DEFAULT_GNB_GTPU_PORT;
    config->gnb_id = 1;
    strncpy(config->gnb_name, "UESim-Mock-gNB-1", sizeof(config->gnb_name) - 1);
    config->tac = 1;
    config->pci = 1;
    
    /* AMF defaults */
    strncpy(config->amf_ip, "127.0.0.1", sizeof(config->amf_ip) - 1);
    config->amf_port = MOCK_INT_DEFAULT_AMF_NGAP_PORT;
    
    /* UPF defaults */
    strncpy(config->upf_ip, "127.0.0.1", sizeof(config->upf_ip) - 1);
    config->upf_port = MOCK_INT_DEFAULT_UPF_GTPU_PORT;
    
    /* Behavior */
    config->auto_connect_amf = true;
    config->auto_forward_nas = true;
    config->log_messages = true;
    config->response_delay_ms = 10;
}

const char* mock_integration_error_to_string(mock_integration_error_t error) {
    static const char* error_strings[] = {
        "Success",
        "Invalid parameter",
        "Memory error",
        "Socket error",
        "Connection error",
        "AMF setup failed",
        "Not connected",
        "Protocol error",
        "Timeout"
    };
    
    if (error > 0 || error < -8) return "Unknown error";
    return error_strings[-error];
}

/* ============== Context Management ============== */

mock_integration_ctx_t* mock_integration_create(const mock_integration_config_t* config) {
    mock_integration_ctx_t* ctx = (mock_integration_ctx_t*)uesim_calloc(1, sizeof(mock_integration_ctx_t));
    if (ctx == NULL) {
        return NULL;
    }
    
    if (config) {
        memcpy(&ctx->config, config, sizeof(mock_integration_config_t));
    } else {
        mock_integration_get_default_config(&ctx->config);
    }
    
    ctx->gnb_server = NULL;
    ctx->amf_server = NULL;
    ctx->upf_server = NULL;
    ctx->amf_socket = SCTP_INVALID_SOCKET;
    ctx->amf_connected = false;
    ctx->amf_setup_complete = false;
    
#ifdef _WIN32
    ctx->running = 0;
#else
    atomic_store(&ctx->running, false);
#endif
    
    return ctx;
}

void mock_integration_destroy(mock_integration_ctx_t* ctx) {
    if (ctx == NULL) return;
    
    mock_integration_stop(ctx);
    
    /* gNB server is managed externally or should be destroyed here */
    if (ctx->gnb_server != NULL) {
        /* Note: gNB server may be managed externally */
    }
    
    uesim_free(ctx);
}

/* ============== AMF Connection ============== */

static mock_integration_error_t connect_to_amf(mock_integration_ctx_t* ctx) {
    if (ctx == NULL) return MOCK_INT_ERROR_INVALID_PARAM;
    
    /* Initialize SCTP */
    sctp_error_t sctp_err = sctp_init();
    if (sctp_err != SCTP_SUCCESS) {
        fprintf(stderr, "[MockInt] SCTP init failed: %s\n", sctp_error_to_string(sctp_err));
        return MOCK_INT_ERROR_SOCKET;
    }
    
    /* Create SCTP socket */
    sctp_config_t sctp_config;
    sctp_get_default_config(&sctp_config);
    sctp_config.num_in_streams = 2;
    sctp_config.num_out_streams = 2;
    
    sctp_err = sctp_create_socket(&sctp_config, &ctx->amf_socket);
    if (sctp_err != SCTP_SUCCESS) {
        fprintf(stderr, "[MockInt] Failed to create SCTP socket: %s\n", sctp_error_to_string(sctp_err));
        return MOCK_INT_ERROR_SOCKET;
    }
    
    /* Connect to AMF */
    if (ctx->config.log_messages) {
        printf("[MockInt] Connecting to AMF at %s:%u\n", 
               ctx->config.amf_ip, ctx->config.amf_port);
    }
    
    sctp_err = sctp_connect(ctx->amf_socket, ctx->config.amf_ip, ctx->config.amf_port);
    if (sctp_err != SCTP_SUCCESS) {
        fprintf(stderr, "[MockInt] Failed to connect to AMF: %s\n", sctp_error_to_string(sctp_err));
        sctp_close(ctx->amf_socket);
        ctx->amf_socket = SCTP_INVALID_SOCKET;
        return MOCK_INT_ERROR_CONNECT;
    }
    
    ctx->amf_connected = true;
    
    if (ctx->config.log_messages) {
        printf("[MockInt] Connected to AMF (mode: %s)\n", sctp_get_implementation());
    }
    
    return MOCK_INT_SUCCESS;
}

static void disconnect_from_amf(mock_integration_ctx_t* ctx) {
    if (ctx == NULL) return;
    
    if (sctp_socket_is_valid(ctx->amf_socket)) {
        sctp_close(ctx->amf_socket);
        ctx->amf_socket = SCTP_INVALID_SOCKET;
    }
    
    ctx->amf_connected = false;
    ctx->amf_setup_complete = false;
    
    if (ctx->config.log_messages) {
        printf("[MockInt] Disconnected from AMF\n");
    }
}

/* ============== NG Setup ============== */

mock_integration_error_t mock_integration_ng_setup(mock_integration_ctx_t* ctx) {
    if (ctx == NULL) return MOCK_INT_ERROR_INVALID_PARAM;
    if (!ctx->amf_connected) return MOCK_INT_ERROR_NOT_CONNECTED;
    
    /* Build NG Setup Request */
    ngap_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.message_type = NGAP_MSG_NG_SETUP_REQUEST;
    msg.procedure_code = NGAP_PROC_NG_SETUP;
    
    ngap_ng_setup_request_t* req = &msg.payload.ng_setup_request;
    req->global_gnb_id.gnb_id = ctx->config.gnb_id;
    strncpy(req->global_gnb_id.gnb_name, ctx->config.gnb_name, sizeof(req->global_gnb_id.gnb_name) - 1);
    req->num_tai = 1;
    req->tai_list[0].plmn_id[0] = 0x00;
    req->tai_list[0].plmn_id[1] = 0x01;
    req->tai_list[0].plmn_id[2] = 0xF1;
    req->tai_list[0].tac = ctx->config.tac;
    
    /* Encode message */
    uint8_t* buffer = NULL;
    size_t length = 0;
    
    uesim_error_t err = ngap_encode_message(&msg, &buffer, &length);
    if (err != UESIM_SUCCESS || buffer == NULL) {
        fprintf(stderr, "[MockInt] Failed to encode NG Setup Request\n");
        return MOCK_INT_ERROR_PROTOCOL;
    }
    
    /* Send to AMF */
    sctp_error_t sctp_err = sctp_send(ctx->amf_socket, buffer, length, SCTP_PPID_NGAP);
    free(buffer);
    
    if (sctp_err != SCTP_SUCCESS) {
        fprintf(stderr, "[MockInt] Failed to send NG Setup Request: %s\n", 
                sctp_error_to_string(sctp_err));
        return MOCK_INT_ERROR_SOCKET;
    }
    
    if (ctx->config.log_messages) {
        printf("[MockInt] Sent NG Setup Request to AMF\n");
    }
    
    /* Wait for NG Setup Response */
    uint8_t recv_buffer[MOCK_GNB_BUFFER_SIZE];
    size_t bytes_received = 0;
    uint32_t ppid = 0;
    
    sctp_err = sctp_recv(ctx->amf_socket, recv_buffer, sizeof(recv_buffer), 
                         &bytes_received, &ppid);
    
    if (sctp_err != SCTP_SUCCESS) {
        fprintf(stderr, "[MockInt] Failed to receive NG Setup Response: %s\n",
                sctp_error_to_string(sctp_err));
        return MOCK_INT_ERROR_TIMEOUT;
    }
    
    if (ppid != SCTP_PPID_NGAP) {
        fprintf(stderr, "[MockInt] Unexpected PPID: %u (expected NGAP)\n", ppid);
        return MOCK_INT_ERROR_PROTOCOL;
    }
    
    /* Decode response */
    ngap_message_t response;
    memset(&response, 0, sizeof(response));
    
    err = ngap_decode_message(recv_buffer, bytes_received, &response);
    if (err != UESIM_SUCCESS) {
        fprintf(stderr, "[MockInt] Failed to decode NG Setup Response\n");
        return MOCK_INT_ERROR_PROTOCOL;
    }
    
    if (response.message_type == NGAP_MSG_NG_SETUP_RESPONSE) {
        ctx->amf_setup_complete = true;
        
        if (ctx->config.log_messages) {
            printf("[MockInt] NG Setup complete with AMF\n");
        }
        
        ngap_free_message(&response);
        return MOCK_INT_SUCCESS;
    } else if (response.message_type == NGAP_MSG_NG_SETUP_FAILURE) {
        fprintf(stderr, "[MockInt] NG Setup failed\n");
        ngap_free_message(&response);
        return MOCK_INT_ERROR_AMF_SETUP;
    }
    
    ngap_free_message(&response);
    return MOCK_INT_ERROR_PROTOCOL;
}

/* ============== AMF Listener Thread ============== */

static void* amf_listener_thread(void* arg) {
    mock_integration_ctx_t* ctx = (mock_integration_ctx_t*)arg;
    uint8_t buffer[MOCK_GNB_BUFFER_SIZE];
    size_t bytes_received;
    uint32_t ppid;
    
    printf("[MockInt] AMF listener thread started\n");
    
    while (atomic_load(&ctx->running)) {
        /* Receive from AMF */
        sctp_error_t sctp_err = sctp_recv(ctx->amf_socket, buffer, sizeof(buffer),
                                          &bytes_received, &ppid);
        
        if (!atomic_load(&ctx->running)) break;
        
        if (sctp_err != SCTP_SUCCESS) {
            if (sctp_err == SCTP_ERROR_TIMEOUT) continue;
            fprintf(stderr, "[MockInt] AMF receive error: %s\n", sctp_error_to_string(sctp_err));
            continue;
        }
        
        if (ppid == SCTP_PPID_NGAP && bytes_received > 0) {
            ctx->messages_from_amf++;
            
            if (ctx->config.log_messages) {
                printf("[MockInt] Received %zu bytes from AMF (NGAP)\n", bytes_received);
            }
            
            /* Process the message */
            mock_integration_process_amf_message(ctx, buffer, bytes_received);
        }
    }
    
    printf("[MockInt] AMF listener thread stopped\n");
    return NULL;
}

/* ============== GTP-U Data Path ============== */

typedef struct {
    uint32_t teid;
    uint32_t ue_ip;
    uint32_t peer_ip;
    uint16_t peer_port;
    int ue_socket;
    bool uplink;
    bool active;
} gtpu_tunnel_t;

#define MAX_GTPU_TUNNELS 256

static gtpu_tunnel_t gtpu_tunnels[MAX_GTPU_TUNNELS];
static int gtpu_socket = -1;
static pthread_mutex_t gtpu_mutex;

static gtpu_tunnel_t* find_tunnel_by_teid(uint32_t teid) {
    for (int i = 0; i < MAX_GTPU_TUNNELS; i++) {
        if (gtpu_tunnels[i].active && gtpu_tunnels[i].teid == teid) {
            return &gtpu_tunnels[i];
        }
    }
    return NULL;
}

static gtpu_tunnel_t* find_free_tunnel_slot(void) {
    for (int i = 0; i < MAX_GTPU_TUNNELS; i++) {
        if (!gtpu_tunnels[i].active) {
            return &gtpu_tunnels[i];
        }
    }
    return NULL;
}

mock_integration_error_t mock_integration_create_gtpu_tunnel(
    mock_integration_ctx_t* ctx,
    uint32_t teid,
    uint32_t ue_ip,
    uint32_t upf_ip,
    uint16_t upf_port,
    bool uplink) {
    
    pthread_mutex_lock(&gtpu_mutex);
    
    gtpu_tunnel_t* tunnel = find_free_tunnel_slot();
    if (!tunnel) {
        pthread_mutex_unlock(&gtpu_mutex);
        return MOCK_INT_ERROR_CAPACITY;
    }
    
    tunnel->teid = teid;
    tunnel->ue_ip = ue_ip;
    tunnel->peer_ip = upf_ip;
    tunnel->peer_port = upf_port;
    tunnel->uplink = uplink;
    tunnel->active = true;
    
    pthread_mutex_unlock(&gtpu_mutex);
    
    if (ctx->config.log_messages) {
        printf("[MockInt] Created GTP-U tunnel: TEID=%u, UE-IP=%u.%u.%u.%u, UPF=%u.%u.%u.%u:%u\n",
               teid,
               (ue_ip >> 24) & 0xFF, (ue_ip >> 16) & 0xFF, (ue_ip >> 8) & 0xFF, ue_ip & 0xFF,
               (upf_ip >> 24) & 0xFF, (upf_ip >> 16) & 0xFF, (upf_ip >> 8) & 0xFF, upf_ip & 0xFF,
               upf_port);
    }
    
    return MOCK_INT_SUCCESS;
}

void mock_integration_delete_gtpu_tunnel(uint32_t teid) {
    pthread_mutex_lock(&gtpu_mutex);
    
    gtpu_tunnel_t* tunnel = find_tunnel_by_teid(teid);
    if (tunnel) {
        tunnel->active = false;
    }
    
    pthread_mutex_unlock(&gtpu_mutex);
}

static void* gtpu_listener_thread(void* arg) {
    mock_integration_ctx_t* ctx = (mock_integration_ctx_t*)arg;
    uint8_t buffer[MOCK_GNB_BUFFER_SIZE];
    struct sockaddr_in src_addr;
    socklen_t addr_len;
    ssize_t bytes_received;
    
    /* GTP-U header structure */
    typedef struct {
        uint8_t flags;
        uint8_t message_type;
        uint16_t length;
        uint32_t teid;
    } __attribute__((packed)) gtpu_header_t;
    
    printf("[MockInt] GTP-U listener thread started on port %u\n", ctx->config.gnb_gtpu_port);
    
    while (atomic_load(&ctx->running)) {
        addr_len = sizeof(src_addr);
        bytes_received = recvfrom(gtpu_socket, (char*)buffer, sizeof(buffer), 0,
                                   (struct sockaddr*)&src_addr, &addr_len);
        
        if (!atomic_load(&ctx->running)) break;
        
        if (bytes_received < (ssize_t)sizeof(gtpu_header_t)) {
            continue;
        }
        
        gtpu_header_t* gtpu_hdr = (gtpu_header_t*)buffer;
        
        /* Check GTP-U version (should be 1) */
        if ((gtpu_hdr->flags & 0xE0) != 0x30) {
            continue;  /* Not GTP-U v1 */
        }
        
        /* Extract TEID */
        uint32_t teid = ntohl(gtpu_hdr->teid);
        
        pthread_mutex_lock(&gtpu_mutex);
        gtpu_tunnel_t* tunnel = find_tunnel_by_teid(teid);
        if (tunnel && tunnel->uplink) {
            /* Forward to UE */
            /* In real implementation, would strip GTP-U header and forward to UE */
            if (ctx->config.log_messages) {
                printf("[MockInt] GTP-U DL: TEID=%u, %zd bytes\n", teid, bytes_received);
            }
        }
        pthread_mutex_unlock(&gtpu_mutex);
    }
    
    printf("[MockInt] GTP-U listener thread stopped\n");
    return NULL;
}

/* ============== Start/Stop ============== */

mock_integration_error_t mock_integration_start(mock_integration_ctx_t* ctx) {
    if (ctx == NULL) return MOCK_INT_ERROR_INVALID_PARAM;
    
#ifdef _WIN32
    if (ctx->running) return MOCK_INT_SUCCESS;
    ctx->running = 1;
#else
    if (atomic_load(&ctx->running)) return MOCK_INT_SUCCESS;
    atomic_store(&ctx->running, true);
#endif
    
    /* Initialize GTP-U mutex */
    pthread_mutex_init(&gtpu_mutex, NULL);
    
    /* Create GTP-U socket */
    gtpu_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (gtpu_socket >= 0) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(ctx->config.gnb_gtpu_port);
        inet_pton(AF_INET, ctx->config.gnb_bind_ip, &addr.sin_addr);
        
        if (bind(gtpu_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
#ifdef _WIN32
            closesocket(gtpu_socket);
#else
            close(gtpu_socket);
#endif
            gtpu_socket = -1;
        }
    }
    
    /* Connect to AMF if auto-connect is enabled */
    if (ctx->config.auto_connect_amf) {
        mock_integration_error_t err = connect_to_amf(ctx);
        if (err != MOCK_INT_SUCCESS) {
#ifdef _WIN32
            ctx->running = 0;
#else
            atomic_store(&ctx->running, false);
#endif
            return err;
        }
        
        /* Perform NG Setup */
        err = mock_integration_ng_setup(ctx);
        if (err != MOCK_INT_SUCCESS) {
            disconnect_from_amf(ctx);
#ifdef _WIN32
            ctx->running = 0;
#else
            atomic_store(&ctx->running, false);
#endif
            return err;
        }
        
        /* Start AMF listener thread */
        pthread_t amf_thread;
        if (pthread_create(&amf_thread, NULL, amf_listener_thread, ctx) != 0) {
            fprintf(stderr, "[MockInt] Failed to start AMF listener thread\n");
        }
    }
    
    /* Start GTP-U listener thread */
    if (gtpu_socket >= 0) {
        pthread_t gtpu_thread;
        if (pthread_create(&gtpu_thread, NULL, gtpu_listener_thread, ctx) != 0) {
            fprintf(stderr, "[MockInt] Failed to start GTP-U listener thread\n");
        }
    }
    
    if (ctx->config.log_messages) {
        printf("[MockInt] Integration layer started\n");
        printf("  gNB NGAP: %s:%u\n", ctx->config.gnb_bind_ip, ctx->config.gnb_ngap_port);
        printf("  gNB GTP-U: %s:%u\n", ctx->config.gnb_bind_ip, ctx->config.gnb_gtpu_port);
        printf("  AMF: %s:%u (%s)\n", ctx->config.amf_ip, ctx->config.amf_port,
               ctx->amf_connected ? "connected" : "not connected");
    }
    
    return MOCK_INT_SUCCESS;
}

void mock_integration_stop(mock_integration_ctx_t* ctx) {
    if (ctx == NULL) return;
    
#ifdef _WIN32
    if (!ctx->running) return;
    ctx->running = 0;
#else
    if (!atomic_load(&ctx->running)) return;
    atomic_store(&ctx->running, false);
#endif
    
    /* Disconnect from AMF */
    disconnect_from_amf(ctx);
    
    /* Clear UE mappings */
    for (int i = 0; i < MOCK_GNB_MAX_UES; i++) {
        ctx->ue_mapping[i].active = false;
    }
    
    if (ctx->config.log_messages) {
        printf("[MockInt] Integration layer stopped\n");
    }
}

bool mock_integration_is_running(mock_integration_ctx_t* ctx) {
    if (ctx == NULL) return false;
    
#ifdef _WIN32
    return ctx->running != 0;
#else
    return atomic_load(&ctx->running);
#endif
}

bool mock_integration_amf_connected(mock_integration_ctx_t* ctx) {
    if (ctx == NULL) return false;
    return ctx->amf_connected && ctx->amf_setup_complete;
}

/* ============== UE Mapping ============== */

mock_integration_error_t mock_integration_register_ue(mock_integration_ctx_t* ctx,
                                                       uint32_t ran_ue_ngap_id,
                                                       int ue_socket) {
    if (ctx == NULL) return MOCK_INT_ERROR_INVALID_PARAM;
    
    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < MOCK_GNB_MAX_UES; i++) {
        if (!ctx->ue_mapping[i].active) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        return MOCK_INT_ERROR_MEMORY;
    }
    
    ctx->ue_mapping[slot].ran_ue_ngap_id = ran_ue_ngap_id;
    ctx->ue_mapping[slot].amf_ue_ngap_id = 0;  /* Will be assigned by AMF */
    ctx->ue_mapping[slot].ue_socket = ue_socket;
    ctx->ue_mapping[slot].active = true;
    
    if (ctx->config.log_messages) {
        printf("[MockInt] Registered UE: RAN-UE-ID=%u, slot=%d\n", ran_ue_ngap_id, slot);
    }
    
    return MOCK_INT_SUCCESS;
}

void mock_integration_unregister_ue(mock_integration_ctx_t* ctx, uint32_t ran_ue_ngap_id) {
    if (ctx == NULL) return;
    
    for (int i = 0; i < MOCK_GNB_MAX_UES; i++) {
        if (ctx->ue_mapping[i].active && 
            ctx->ue_mapping[i].ran_ue_ngap_id == ran_ue_ngap_id) {
            ctx->ue_mapping[i].active = false;
            
            if (ctx->config.log_messages) {
                printf("[MockInt] Unregistered UE: RAN-UE-ID=%u\n", ran_ue_ngap_id);
            }
            break;
        }
    }
}

int mock_integration_find_ue_mapping(mock_integration_ctx_t* ctx, uint32_t ran_ue_ngap_id) {
    if (ctx == NULL) return -1;
    
    for (int i = 0; i < MOCK_GNB_MAX_UES; i++) {
        if (ctx->ue_mapping[i].active && 
            ctx->ue_mapping[i].ran_ue_ngap_id == ran_ue_ngap_id) {
            return i;
        }
    }
    
    return -1;
}

/* ============== Message Forwarding ============== */

mock_integration_error_t mock_integration_forward_to_amf(mock_integration_ctx_t* ctx,
                                                          mock_gnb_ue_context_t* ue_ctx,
                                                          const void* data, size_t len) {
    if (ctx == NULL || data == NULL || len == 0) {
        return MOCK_INT_ERROR_INVALID_PARAM;
    }
    
    if (!ctx->amf_connected) {
        return MOCK_INT_ERROR_NOT_CONNECTED;
    }
    
    /* Send to AMF */
    sctp_error_t sctp_err = sctp_send(ctx->amf_socket, data, len, SCTP_PPID_NGAP);
    if (sctp_err != SCTP_SUCCESS) {
        fprintf(stderr, "[MockInt] Failed to forward to AMF: %s\n",
                sctp_error_to_string(sctp_err));
        return MOCK_INT_ERROR_SOCKET;
    }
    
    ctx->messages_to_amf++;
    
    if (ctx->config.log_messages) {
        printf("[MockInt] Forwarded %zu bytes to AMF (UE %u)\n", 
               len, ue_ctx ? ue_ctx->ran_ue_ngap_id : 0);
    }
    
    return MOCK_INT_SUCCESS;
}

mock_integration_error_t mock_integration_process_amf_message(mock_integration_ctx_t* ctx,
                                                               const void* data, size_t len) {
    if (ctx == NULL || data == NULL || len == 0) {
        return MOCK_INT_ERROR_INVALID_PARAM;
    }
    
    ctx->messages_from_amf++;
    
    if (ctx->config.log_messages) {
        printf("[MockInt] Received %zu bytes from AMF\n", len);
    }
    
    /* Decode and route message */
    ngap_message_t msg;
    memset(&msg, 0, sizeof(msg));
    
    uesim_error_t err = ngap_decode_message(data, len, &msg);
    if (err != UESIM_SUCCESS) {
        fprintf(stderr, "[MockInt] Failed to decode AMF message\n");
        return MOCK_INT_ERROR_PROTOCOL;
    }
    
    /* Route based on message type */
    switch (msg.message_type) {
        case NGAP_MSG_DOWNLINK_NAS_TRANSPORT: {
            /* Find UE and forward */
            const ngap_downlink_nas_transport_t* dl_nas = &msg.payload.downlink_nas_transport;
            
            int slot = mock_integration_find_ue_mapping(ctx, dl_nas->ue_ids.ran_ue_ngap_id);
            if (slot >= 0) {
                /* Update AMF UE ID if not set */
                if (ctx->ue_mapping[slot].amf_ue_ngap_id == 0) {
                    ctx->ue_mapping[slot].amf_ue_ngap_id = dl_nas->ue_ids.amf_ue_ngap_id;
                }
                
                /* Forward to UE via socket */
                int ue_socket = ctx->ue_mapping[slot].ue_socket;
                if (ue_socket >= 0) {
                    /* Send NAS PDU to UE */
                    send(ue_socket, dl_nas->nas_pdu, dl_nas->nas_pdu_len, 0);
                    ctx->messages_to_ue++;
                }
            }
            break;
        }
        
        case NGAP_MSG_PDU_SESSION_SETUP_RESPONSE: {
            /* Handle PDU session response */
            const ngap_pdu_session_setup_response_t* resp = &msg.payload.pdu_session_setup_response;
            
            if (ctx->config.log_messages) {
                printf("[MockInt] PDU Session Setup Response: session=%u, success=%s\n",
                       resp->pdu_session.pdu_session_id, resp->success ? "yes" : "no");
            }
            break;
        }
        
        case NGAP_MSG_UE_CONTEXT_RELEASE_COMMAND: {
            /* Handle UE context release */
            const ngap_ue_context_release_command_t* cmd = &msg.payload.ue_context_release_command;
            
            if (ctx->config.log_messages) {
                printf("[MockInt] UE Context Release Command for UE %u\n",
                       cmd->ue_ids.ran_ue_ngap_id);
            }
            
            /* Unregister UE */
            mock_integration_unregister_ue(ctx, cmd->ue_ids.ran_ue_ngap_id);
            break;
        }
        
        default:
            if (ctx->config.log_messages) {
                printf("[MockInt] Unhandled AMF message type: %d\n", msg.message_type);
            }
            break;
    }
    
    ngap_free_message(&msg);
    return MOCK_INT_SUCCESS;
}

/* ============== Statistics ============== */

void mock_integration_print_stats(const mock_integration_ctx_t* ctx) {
    if (ctx == NULL) return;
    
    printf("\n[MockInt] Statistics:\n");
    printf("  AMF Connected: %s\n", ctx->amf_connected ? "yes" : "no");
    printf("  NG Setup Complete: %s\n", ctx->amf_setup_complete ? "yes" : "no");
    printf("  Messages to AMF: %llu\n", (unsigned long long)ctx->messages_to_amf);
    printf("  Messages from AMF: %llu\n", (unsigned long long)ctx->messages_from_amf);
    printf("  Messages to UE: %llu\n", (unsigned long long)ctx->messages_to_ue);
    printf("  Messages from UE: %llu\n", (unsigned long long)ctx->messages_from_ue);
    
    /* Count active UEs */
    int active_ues = 0;
    for (int i = 0; i < MOCK_GNB_MAX_UES; i++) {
        if (ctx->ue_mapping[i].active) active_ues++;
    }
    printf("  Active UEs: %d\n", active_ues);
}