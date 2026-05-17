/*
 * 5G UE Simulation Application
 * Mock CU-UP (Central Unit - User Plane) Entity
 * 3GPP TS 38.463 - E1AP Interface (CU-CP to CU-UP)
 */

#include "mock_core.h"
#include "../protocol/e1ap_messages.h"
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

#define CU_UP_MAX_CU_CP_CONNECTIONS  4
#define CU_UP_MAX_BEARER_CONTEXTS    1024
#define CU_UP_E1AP_PORT              38462
#define CU_UP_BUFFER_SIZE            65536
#define CU_UP_MAX_DRB_PER_UE         8

/* ============== CU-UP States ============== */

typedef enum {
    CU_UP_STATE_IDLE = 0,
    CU_UP_STATE_E1_SETUP_PENDING,
    CU_UP_STATE_ACTIVE,
    CU_UP_STATE_RESETTING,
    CU_UP_STATE_MAX
} cu_up_state_t;

/* ============== CU-CP Connection Context ============== */

typedef struct {
    uint64_t gnb_cu_cp_id;
    char gnb_cu_cp_name[64];
    int e1ap_socket;
    cu_up_state_t state;
    
    /* Supported PLMNs */
    uint8_t num_supported_plmns;
    uint8_t supported_plmns[12][3];
    
    /* Connection Info */
    struct sockaddr_in cu_cp_addr;
    time_t connect_time;
    time_t last_activity;
    bool active;
} cu_cp_connection_t;

/* ============== CU-UP Bearer Context ============== */

typedef struct {
    uint32_t gnb_cu_cp_ue_e1ap_id;
    uint32_t gnb_cu_up_ue_e1ap_id;
    uint64_t ran_ue_id;
    
    /* PDU Sessions */
    uint8_t num_pdu_sessions;
    e1ap_pdu_session_info_t pdu_sessions[E1AP_MAX_PDU_SESSIONS];
    
    /* DRBs */
    uint8_t num_drbs;
    e1ap_drb_info_t drbs[CU_UP_MAX_DRB_PER_UE];
    
    /* UE AMBR */
    uint64_t ue_ambr_dl;
    uint64_t ue_ambr_ul;
    
    /* CU-CP Reference */
    uint64_t gnb_cu_cp_id;
    
    /* State */
    uint8_t bearer_state;
    time_t setup_time;
    time_t last_activity;
    bool active;
} cu_up_bearer_context_t;

/* ============== CU-UP Configuration ============== */

typedef struct {
    char bind_ip[46];
    uint16_t e1ap_port;
    
    /* gNB-CU-UP Identity */
    uint32_t gnb_cu_up_id;
    char gnb_cu_up_name[64];
    
    /* Supported PLMNs and S-NSSAIs */
    uint8_t num_supported_plmns;
    uint8_t supported_plmns[12][3];
    uint8_t num_supported_slices;
    e1ap_s_nssai_t supported_slices[E1AP_MAX_QOS_FLOWS];
    
    /* Capacity */
    uint32_t capacity;
    
    /* TNL Addresses */
    uint8_t num_tnla;
    e1ap_tnl_info_t tnla[E1AP_MAX_TNL_INFO];
    
    /* Behavior */
    bool auto_respond;
    bool log_messages;
    
    /* PCAP */
    char pcap_file[256];
} cu_up_config_t;

/* ============== CU-UP Server Context ============== */

struct cu_up_server_s {
    cu_up_config_t config;
    
    /* Socket */
    int e1ap_socket;
    
    /* CU-CP Connection */
    cu_cp_connection_t cu_cp_connection;
    
    /* Bearer Contexts */
    cu_up_bearer_context_t bearer_contexts[CU_UP_MAX_BEARER_CONTEXTS];
    uint32_t num_bearer_contexts;
    uint32_t next_cu_up_ue_id;
    
    /* Server State */
#ifdef _WIN32
    volatile LONG running;
#else
    atomic_bool running;
#endif
    pthread_t e1ap_thread;
    pthread_mutex_t bearer_mutex;
    
    /* Statistics */
    uint64_t e1ap_messages_rx;
    uint64_t e1ap_messages_tx;
    uint64_t e1_setup_success;
    uint64_t e1_setup_failure;
    uint64_t bearer_contexts_created;
    uint64_t bearer_contexts_released;
    uint64_t pdu_sessions_setup;
    uint64_t pdu_sessions_released;
};

/* Forward declaration */
typedef struct cu_up_server_s cu_up_server_t;

/* ============== Forward Declarations ============== */

static void* e1ap_listener_thread(void* arg);

/* ============== CU-UP API Functions ============== */

void cu_up_get_default_config(cu_up_config_t* config) {
    if (!config) return;
    memset(config, 0, sizeof(cu_up_config_t));
    
    strncpy(config->bind_ip, "0.0.0.0", sizeof(config->bind_ip) - 1);
    config->e1ap_port = CU_UP_E1AP_PORT;
    config->gnb_cu_up_id = 0x00000001;
    strncpy(config->gnb_cu_up_name, "UESim-CU-UP", sizeof(config->gnb_cu_up_name) - 1);
    
    /* Default PLMN: 001/01 */
    config->num_supported_plmns = 1;
    config->supported_plmns[0][0] = 0x00;
    config->supported_plmns[0][1] = 0x01;
    config->supported_plmns[0][2] = 0xF1;  /* MCC=001, MNC=01 */
    
    /* Default slice */
    config->num_supported_slices = 1;
    config->supported_slices[0].sst = 1;
    config->supported_slices[0].sd = 0;
    config->supported_slices[0].sd_present = false;
    
    config->capacity = 100;
    config->auto_respond = true;
    config->log_messages = true;
}

cu_up_server_t* cu_up_create(const cu_up_config_t* config) {
    cu_up_server_t* cu_up = (cu_up_server_t*)calloc(1, sizeof(cu_up_server_t));
    if (!cu_up) return NULL;
    
    if (config) {
        memcpy(&cu_up->config, config, sizeof(cu_up_config_t));
    } else {
        cu_up_get_default_config(&cu_up->config);
    }
    
    cu_up->e1ap_socket = -1;
    cu_up->next_cu_up_ue_id = 1;
    
    return cu_up;
}

void cu_up_destroy(cu_up_server_t* cu_up) {
    if (!cu_up) return;
    
    cu_up_stop(cu_up);
    
    /* Close CU-CP connection */
    if (cu_up->cu_cp_connection.active && cu_up->cu_cp_connection.e1ap_socket >= 0) {
#ifdef _WIN32
        closesocket(cu_up->cu_cp_connection.e1ap_socket);
#else
        close(cu_up->cu_cp_connection.e1ap_socket);
#endif
    }
    
    pthread_mutex_destroy(&cu_up->bearer_mutex);
    free(cu_up);
}

/* ============== E1AP Listener Thread ============== */

static void* e1ap_listener_thread(void* arg) {
    cu_up_server_t* cu_up = (cu_up_server_t*)arg;
    uint8_t buffer[CU_UP_BUFFER_SIZE];
    ssize_t bytes_recv;
    fd_set read_fds;
    struct timeval tv;
    
    printf("[CU-UP] E1AP listener thread started\n");
    
    while (atomic_load(&cu_up->running)) {
        FD_ZERO(&read_fds);
        FD_SET(cu_up->cu_cp_connection.e1ap_socket, &read_fds);
        
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int ret = select(cu_up->cu_cp_connection.e1ap_socket + 1, &read_fds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (ret == 0) continue;  /* Timeout */
        
        bytes_recv = recv(cu_up->cu_cp_connection.e1ap_socket, (char*)buffer, sizeof(buffer), 0);
        if (bytes_recv > 0) {
            cu_up_process_e1ap_message(cu_up, buffer, (size_t)bytes_recv);
        } else if (bytes_recv <= 0) {
            printf("[CU-UP] CU-CP connection closed\n");
            break;
        }
    }
    
    printf("[CU-UP] E1AP listener thread stopped\n");
    return NULL;
}

mock_core_error_t cu_up_start(cu_up_server_t* cu_up) {
    if (!cu_up) return MOCK_CORE_ERROR_INVALID_PARAM;
    
    /* Initialize mutex */
    if (pthread_mutex_init(&cu_up->bearer_mutex, NULL) != 0) {
        return MOCK_CORE_ERROR_THREAD;
    }
    
    atomic_store(&cu_up->running, 1);
    
    printf("[CU-UP] Server started\n");
    return MOCK_CORE_SUCCESS;
}

void cu_up_stop(cu_up_server_t* cu_up) {
    if (!cu_up) return;
    
    atomic_store(&cu_up->running, 0);
    
    if (cu_up->cu_cp_connection.e1ap_socket >= 0) {
#ifdef _WIN32
        closesocket(cu_up->cu_cp_connection.e1ap_socket);
#else
        close(cu_up->cu_cp_connection.e1ap_socket);
#endif
        cu_up->cu_cp_connection.e1ap_socket = -1;
    }
    
    if (cu_up->e1ap_socket >= 0) {
#ifdef _WIN32
        closesocket(cu_up->e1ap_socket);
#else
        close(cu_up->e1ap_socket);
#endif
        cu_up->e1ap_socket = -1;
    }
    
    /* Wait for thread to terminate if running */
    if (cu_up->e1ap_thread) {
#ifdef _WIN32
        WaitForSingleObject(cu_up->e1ap_thread, 5000);
#else
        pthread_join(cu_up->e1ap_thread, NULL);
#endif
    }
    
    printf("[CU-UP] Server stopped\n");
}

mock_core_error_t cu_up_connect_cu_cp(cu_up_server_t* cu_up, const char* cu_cp_ip, uint16_t port) {
    if (!cu_up || !cu_cp_ip) return MOCK_CORE_ERROR_INVALID_PARAM;
    
    struct sockaddr_in cu_cp_addr;
    
    /* Create socket */
    cu_up->cu_cp_connection.e1ap_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (cu_up->cu_cp_connection.e1ap_socket < 0) {
        return MOCK_CORE_ERROR_SOCKET;
    }
    
    /* Connect to CU-CP */
    memset(&cu_cp_addr, 0, sizeof(cu_cp_addr));
    cu_cp_addr.sin_family = AF_INET;
    cu_cp_addr.sin_port = htons(port > 0 ? port : CU_UP_E1AP_PORT);
    inet_pton(AF_INET, cu_cp_ip, &cu_cp_addr.sin_addr);
    
    cu_up->cu_cp_connection.state = CU_UP_STATE_CONNECTING;
    
    if (connect(cu_up->cu_cp_connection.e1ap_socket, (struct sockaddr*)&cu_cp_addr, sizeof(cu_cp_addr)) < 0) {
#ifdef _WIN32
        closesocket(cu_up->cu_cp_connection.e1ap_socket);
#else
        close(cu_up->cu_cp_connection.e1ap_socket);
#endif
        cu_up->cu_cp_connection.e1ap_socket = -1;
        cu_up->cu_cp_connection.state = CU_UP_STATE_IDLE;
        return MOCK_CORE_ERROR_SOCKET;
    }
    
    memcpy(&cu_up->cu_cp_connection.cu_cp_addr, &cu_cp_addr, sizeof(cu_cp_addr));
    cu_up->cu_cp_connection.connect_time = time(NULL);
    
    printf("[CU-UP] Connected to CU-CP at %s:%u\n", cu_cp_ip, port > 0 ? port : CU_UP_E1AP_PORT);
    
    /* Start listener thread if running */
    if (atomic_load(&cu_up->running)) {
        if (pthread_create(&cu_up->e1ap_thread, NULL, e1ap_listener_thread, cu_up) != 0) {
            return MOCK_CORE_ERROR_THREAD;
        }
    }
    
    return MOCK_CORE_SUCCESS;
}

/* ============== E1AP Message Handlers ============== */

static int cu_up_handle_e1_setup_request(cu_up_server_t* cu_up,
                                          const e1ap_message_t* req,
                                          e1ap_message_t* response) {
    if (!cu_up || !req || !response) return -1;
    
    const e1ap_e1_setup_request_t* setup_req = &req->payload.e1_setup_request;
    
    printf("[CU-UP] E1 Setup Request from gNB-CU-UP ID: 0x%08X\n",
           setup_req->gnb_cu_up_id.gnb_cu_up_id);
    printf("[CU-UP]   gNB-CU-UP Name: %s\n", setup_req->gnb_cu_up_id.gnb_cu_up_name);
    printf("[CU-UP]   Supported PLMNs: %d\n", setup_req->num_supported_plmns);
    
    /* Store CU-CP connection info */
    cu_up->cu_cp_connection.active = true;
    cu_up->cu_cp_connection.state = CU_UP_STATE_ACTIVE;
    cu_up->e1_setup_success++;
    
    /* Build E1 Setup Response */
    response->message_type = E1AP_MSG_E1_SETUP_RESPONSE;
    response->procedure_code = E1AP_PROC_E1_SETUP;
    response->criticality = 0;
    
    e1ap_e1_setup_response_t* resp = &response->payload.e1_setup_response;
    resp->gnb_cu_cp_id.gnb_cu_cp_id = cu_up->config.gnb_cu_up_id;  /* Using our ID */
    strncpy(resp->gnb_cu_cp_id.gnb_cu_cp_name, cu_up->config.gnb_cu_up_name,
            sizeof(resp->gnb_cu_cp_id.gnb_cu_cp_name) - 1);
    resp->num_supported_plmns = cu_up->config.num_supported_plmns;
    memcpy(resp->supported_plmns, cu_up->config.supported_plmns,
           sizeof(cu_up->config.supported_plmns));
    
    return 0;
}

static cu_up_bearer_context_t* cu_up_create_bearer_context(cu_up_server_t* cu_up) {
    if (!cu_up) return NULL;
    
    for (int i = 0; i < CU_UP_MAX_BEARER_CONTEXTS; i++) {
        if (!cu_up->bearer_contexts[i].active) {
            cu_up_bearer_context_t* ctx = &cu_up->bearer_contexts[i];
            memset(ctx, 0, sizeof(cu_up_bearer_context_t));
            ctx->gnb_cu_up_ue_e1ap_id = cu_up->next_cu_up_ue_id++;
            ctx->active = true;
            ctx->setup_time = time(NULL);
            cu_up->num_bearer_contexts++;
            cu_up->bearer_contexts_created++;
            return ctx;
        }
    }
    
    return NULL;
}

static int cu_up_handle_bearer_context_setup_request(cu_up_server_t* cu_up,
                                                      const e1ap_message_t* req,
                                                      e1ap_message_t* response) {
    if (!cu_up || !req || !response) return -1;
    
    const e1ap_bearer_context_setup_request_t* setup_req = &req->payload.bearer_context_setup_request;
    
    printf("[CU-UP] Bearer Context Setup Request:\n");
    printf("[CU-UP]   RAN UE ID: 0x%llX\n", (unsigned long long)setup_req->ran_ue_id.ran_ue_id);
    printf("[CU-UP]   PDU Sessions: %d\n", setup_req->num_pdu_sessions);
    
    /* Create bearer context */
    cu_up_bearer_context_t* ctx = cu_up_create_bearer_context(cu_up);
    if (!ctx) {
        response->message_type = E1AP_MSG_BEARER_CONTEXT_SETUP_FAILURE;
        response->procedure_code = E1AP_PROC_BEARER_CONTEXT_SETUP;
        e1ap_set_cause_misc(&response->payload.bearer_context_setup_failure.cause,
                            E1AP_CAUSE_MISC_CONTROL_PROCESSING_OVERLOAD);
        return 0;
    }
    
    ctx->gnb_cu_cp_ue_e1ap_id = setup_req->ue_ids.gnb_cu_cp_ue_e1ap_id;
    ctx->ran_ue_id = setup_req->ran_ue_id.ran_ue_id;
    ctx->ue_ambr_dl = setup_req->ue_ambr_dl;
    ctx->ue_ambr_ul = setup_req->ue_ambr_ul;
    
    /* Copy PDU sessions */
    ctx->num_pdu_sessions = setup_req->num_pdu_sessions;
    for (int i = 0; i < setup_req->num_pdu_sessions && i < E1AP_MAX_PDU_SESSIONS; i++) {
        memcpy(&ctx->pdu_sessions[i], &setup_req->pdu_sessions[i], sizeof(e1ap_pdu_session_info_t));
        cu_up->pdu_sessions_setup++;
    }
    
    /* Build response */
    response->message_type = E1AP_MSG_BEARER_CONTEXT_SETUP_RESPONSE;
    response->procedure_code = E1AP_PROC_BEARER_CONTEXT_SETUP;
    response->criticality = 0;
    
    e1ap_bearer_context_setup_response_t* resp = &response->payload.bearer_context_setup_response;
    resp->ue_ids.gnb_cu_cp_ue_e1ap_id = ctx->gnb_cu_cp_ue_e1ap_id;
    resp->ue_ids.gnb_cu_up_ue_e1ap_id = ctx->gnb_cu_up_ue_e1ap_id;
    resp->num_pdu_sessions_setup = ctx->num_pdu_sessions;
    
    for (int i = 0; i < ctx->num_pdu_sessions && i < E1AP_MAX_PDU_SESSIONS; i++) {
        resp->pdu_session_ids[i] = ctx->pdu_sessions[i].pdu_session_id;
    }
    
    ctx->bearer_state = 2;  /* Active */
    
    return 0;
}

/* ============== CU-UP Message Processing ============== */

mock_core_error_t cu_up_process_e1ap_message(cu_up_server_t* cu_up,
                                              const uint8_t* data, size_t len) {
    if (!cu_up || !data || len == 0) return MOCK_CORE_ERROR_INVALID_PARAM;
    
    e1ap_message_t msg;
    if (e1ap_decode_message(data, len, &msg) != 0) {
        printf("[CU-UP] Failed to decode E1AP message\n");
        return MOCK_CORE_ERROR_PROTOCOL;
    }
    
    cu_up->e1ap_messages_rx++;
    
    if (cu_up->config.log_messages) {
        printf("[CU-UP] Received E1AP Message: %s (Procedure: %d)\n",
               e1ap_message_type_to_string(msg.message_type),
               msg.procedure_code);
    }
    
    e1ap_message_t response;
    memset(&response, 0, sizeof(response));
    
    switch (msg.message_type) {
        case E1AP_MSG_E1_SETUP_REQUEST:
            cu_up_handle_e1_setup_request(cu_up, &msg, &response);
            break;
            
        case E1AP_MSG_BEARER_CONTEXT_SETUP_REQUEST:
            cu_up_handle_bearer_context_setup_request(cu_up, &msg, &response);
            break;
            
        case E1AP_MSG_BEARER_CONTEXT_RELEASE_COMMAND: {
            const e1ap_bearer_context_release_command_t* cmd = &msg.payload.bearer_context_release_command;
            printf("[CU-UP] Bearer Context Release Command: UE ID=%u\n",
                   cmd->ue_ids.gnb_cu_cp_ue_e1ap_id);
            
            /* Find and release bearer context */
            for (int i = 0; i < CU_UP_MAX_BEARER_CONTEXTS; i++) {
                if (cu_up->bearer_contexts[i].active &&
                    cu_up->bearer_contexts[i].gnb_cu_cp_ue_e1ap_id == cmd->ue_ids.gnb_cu_cp_ue_e1ap_id) {
                    cu_up->bearer_contexts[i].active = false;
                    cu_up->num_bearer_contexts--;
                    cu_up->bearer_contexts_released++;
                    break;
                }
            }
            
            response.message_type = E1AP_MSG_BEARER_CONTEXT_RELEASE_COMPLETE;
            response.procedure_code = E1AP_PROC_BEARER_CONTEXT_RELEASE;
            response.payload.bearer_context_release_complete.ue_ids = cmd->ue_ids;
            break;
        }
            
        case E1AP_MSG_ERROR_INDICATION: {
            const e1ap_error_indication_t* err = &msg.payload.error_indication;
            printf("[CU-UP] Error Indication: Cause=%s\n",
                   err->cause_present ? e1ap_cause_to_string(&err->cause) : "N/A");
            break;
        }
            
        default:
            printf("[CU-UP] Unhandled message type: %d\n", msg.message_type);
            break;
    }
    
    /* Send response if needed */
    if (response.message_type != E1AP_MSG_MAX && cu_up->config.auto_respond) {
        uint8_t* resp_buffer = NULL;
        size_t resp_len = 0;
        
        if (e1ap_encode_message(&response, &resp_buffer, &resp_len) == 0) {
            printf("[CU-UP] Sending response: %s\n",
                   e1ap_message_type_to_string(response.message_type));
            /* send(cu_up->cu_cp_connection.e1ap_socket, resp_buffer, resp_len, 0); */
            free(resp_buffer);
            cu_up->e1ap_messages_tx++;
        }
    }
    
    e1ap_free_message(&msg);
    return MOCK_CORE_SUCCESS;
}

/* ============== CU-UP Statistics ============== */

void cu_up_print_statistics(const cu_up_server_t* cu_up) {
    if (!cu_up) return;
    
    printf("\n[CU-UP] Statistics:\n");
    printf("  E1AP Messages RX: %llu\n", (unsigned long long)cu_up->e1ap_messages_rx);
    printf("  E1AP Messages TX: %llu\n", (unsigned long long)cu_up->e1ap_messages_tx);
    printf("  E1 Setup Success: %llu\n", (unsigned long long)cu_up->e1_setup_success);
    printf("  E1 Setup Failure: %llu\n", (unsigned long long)cu_up->e1_setup_failure);
    printf("  Bearer Contexts Created: %llu\n", (unsigned long long)cu_up->bearer_contexts_created);
    printf("  Bearer Contexts Released: %llu\n", (unsigned long long)cu_up->bearer_contexts_released);
    printf("  Active Bearer Contexts: %u\n", cu_up->num_bearer_contexts);
    printf("  PDU Sessions Setup: %llu\n", (unsigned long long)cu_up->pdu_sessions_setup);
    printf("  PDU Sessions Released: %llu\n", (unsigned long long)cu_up->pdu_sessions_released);
}