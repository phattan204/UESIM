/*
 * 5G UE Simulation Application
 * Mock gNB Server Implementation
 */

#include "mock_gnb_server.h"
#include "mock_gnb_response.h"
#include "../protocol/asn1_per.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#include <io.h>
#define close closesocket
#define SHUT_RDWR SD_BOTH

/* Windows doesn't have gettimeofday, provide alternative */
static int gettimeofday(struct timeval* tv, void* tz) {
    (void)tz;
    FILETIME ft;
    ULARGE_INTEGER uli;
    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    /* Convert from 100-nanosecond intervals since Jan 1, 1601 to Unix epoch */
    uli.QuadPart -= 116444736000000000ULL;
    tv->tv_sec = (long)(uli.QuadPart / 10000000ULL);
    tv->tv_usec = (long)((uli.QuadPart % 10000000ULL) / 10);
    return 0;
}

#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#endif

/* ============== Global Server Context ============== */

static mock_gnb_server_t g_server = {0};

/* ============== Forward Declarations ============== */

static void* ngap_listener_thread(void* arg);
static void* gtpu_listener_thread(void* arg);
static mock_gnb_error_t handle_client_connection(int client_socket);
static void process_ngap_message(mock_gnb_ue_context_t* ue_ctx, const void* data, size_t len);
static void send_ngap_response(mock_gnb_ue_context_t* ue_ctx, const void* data, size_t len);

/* ============== Utility Functions ============== */

const char* mock_gnb_ue_state_to_string(mock_gnb_ue_state_t state) {
    static const char* state_strings[] = {
        "IDLE", "CONNECTING", "CONNECTED", "REGISTERED",
        "DEREGISTERING", "HANDOVER_PREP", "REESTABLISHING", "RELEASED"
    };
    if (state >= MOCK_GNB_UE_STATE_MAX) return "UNKNOWN";
    return state_strings[state];
}

const char* mock_ngap_message_type_to_string(mock_ngap_message_type_t type) {
    static const char* type_strings[] = {
        "NGSetupRequest", "NGSetupResponse", "NGSetupFailure",
        "InitialUEMessage", "InitialContextSetupRequest", "InitialContextSetupResponse",
        "InitialContextSetupFailure", "UEContextReleaseRequest", "UEContextReleaseCommand",
        "UEContextReleaseComplete", "PDUSessionSetupRequest", "PDUSessionSetupResponse",
        "PDUSessionReleaseCommand", "PDUSessionReleaseComplete", "HandoverPreparation",
        "HandoverRequest", "HandoverCommand", "HandoverNotify", "PathSwitchRequest",
        "UplinkNASTransport", "DownlinkNASTransport", "ErrorIndication"
    };
    if (type >= MOCK_NGAP_MAX) return "UNKNOWN";
    return type_strings[type];
}

const char* mock_nas_message_type_to_string(mock_nas_message_type_t type) {
    static const char* type_strings[] = {
        "RegistrationRequest", "RegistrationAccept", "RegistrationReject",
        "RegistrationComplete", "AuthenticationRequest", "AuthenticationResponse",
        "AuthenticationReject", "SecurityModeCommand", "SecurityModeComplete",
        "SecurityModeReject", "ULNASTransport", "DLNASTransport",
        "PDUSessionEstablishmentRequest", "PDUSessionEstablishmentAccept",
        "PDUSessionEstablishmentReject", "PDUSessionReleaseRequest",
        "PDUSessionReleaseCommand", "PDUSessionReleaseComplete"
    };
    if (type >= MOCK_NAS_MAX) return "UNKNOWN";
    return type_strings[type];
}

uint64_t mock_gnb_get_time_ms(void) {
#ifdef _WIN32
    return (uint64_t)GetTickCount();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

void mock_gnb_get_default_config(mock_gnb_config_t* config) {
    if (config == NULL) return;
    
    memset(config, 0, sizeof(mock_gnb_config_t));
    strncpy(config->bind_ip, "0.0.0.0", sizeof(config->bind_ip) - 1);
    config->ngap_port = MOCK_GNB_DEFAULT_NGAP_PORT;
    config->gtpu_port = MOCK_GNB_DEFAULT_GTPU_PORT;
    config->max_ues = MOCK_GNB_MAX_UES;
    config->max_connections = MOCK_GNB_MAX_CONNECTIONS;
    config->auto_respond = true;
    config->response_delay_ms = 10;  /* 10ms default delay */
    config->accept_all_connections = true;
    config->log_messages = true;
    config->log_to_console = true;
    
    /* Default cell config */
    config->cell_config.gnb_id = 1;
    strncpy(config->cell_config.gnb_name, "mock-gnb-1", sizeof(config->cell_config.gnb_name) - 1);
    config->cell_config.tac = 1;
    config->cell_config.pci = 1;
    config->cell_config.cell_id = 1;
    config->cell_config.supports_xn_handover = true;
    config->cell_config.supports_n2_handover = true;
    config->cell_config.max_ues_supported = 1000;
}

/* ============== PCAP Functions ============== */

/* PCAP file header */
typedef struct {
    uint32_t magic_number;   /* 0xa1b2c3d4 */
    uint16_t version_major;  /* 2 */
    uint16_t version_minor;  /* 4 */
    int32_t  thiszone;       /* GMT to local correction */
    uint32_t sigfigs;        /* accuracy of timestamps */
    uint32_t snaplen;        /* max length of captured packets */
    uint32_t network;        /* data link type (1 = Ethernet) */
} pcap_hdr_t;

/* PCAP packet header */
typedef struct {
    uint32_t ts_sec;         /* timestamp seconds */
    uint32_t ts_usec;        /* timestamp microseconds */
    uint32_t incl_len;       /* number of octets of packet saved */
    uint32_t orig_len;       /* actual length of packet */
} pcaprec_hdr_t;

mock_gnb_error_t mock_gnb_pcap_init(const char* filename) {
    if (filename == NULL) {
        return MOCK_GNB_ERROR_INVALID_PARAM;
    }
    
    g_server.pcap.pcap_file = fopen(filename, "wb");
    if (g_server.pcap.pcap_file == NULL) {
        fprintf(stderr, "Failed to open PCAP file: %s\n", filename);
        return MOCK_GNB_ERROR_FILE;
    }
    
    /* Write PCAP header */
    pcap_hdr_t hdr = {
        .magic_number = 0xa1b2c3d4,
        .version_major = 2,
        .version_minor = 4,
        .thiszone = 0,
        .sigfigs = 0,
        .snaplen = 65535,
        .network = 101  /* Raw IP */
    };
    
    if (fwrite(&hdr, sizeof(hdr), 1, g_server.pcap.pcap_file) != 1) {
        fclose(g_server.pcap.pcap_file);
        g_server.pcap.pcap_file = NULL;
        return MOCK_GNB_ERROR_FILE;
    }
    
#ifdef _WIN32
    g_server.pcap.pcap_mutex = CreateMutex(NULL, FALSE, NULL);
#else
    pthread_mutex_init(&g_server.pcap.pcap_mutex, NULL);
#endif
    g_server.pcap.enabled = true;
    g_server.pcap.packet_count = 0;
    
    printf("PCAP logging initialized: %s\n", filename);
    return MOCK_GNB_SUCCESS;
}

mock_gnb_error_t mock_gnb_pcap_write_packet(const void* data, size_t len,
                                            const struct sockaddr_in* src_addr,
                                            const struct sockaddr_in* dst_addr,
                                            uint8_t protocol) {
    if (!g_server.pcap.enabled || g_server.pcap.pcap_file == NULL) {
        return MOCK_GNB_SUCCESS;  /* PCAP not enabled, silently succeed */
    }
    
    if (data == NULL || len == 0) {
        return MOCK_GNB_ERROR_INVALID_PARAM;
    }
    
#ifdef _WIN32
    WaitForSingleObject(g_server.pcap.pcap_mutex, INFINITE);
#else
    pthread_mutex_lock(&g_server.pcap.pcap_mutex);
#endif
    
    /* Get current time */
    struct timeval tv;
#ifdef _WIN32
    gettimeofday(&tv, NULL);
#else
    gettimeofday(&tv, NULL);
#endif
    
    /* Build simple IP header + data for raw IP PCAP */
    uint8_t ip_header[20];
    memset(ip_header, 0, sizeof(ip_header));
    ip_header[0] = 0x45;  /* IPv4, 20-byte header */
    ip_header[9] = protocol;
    memcpy(&ip_header[12], &src_addr->sin_addr, 4);
    memcpy(&ip_header[16], &dst_addr->sin_addr, 4);
    
    /* Calculate total length */
    uint16_t total_len = htons((uint16_t)(20 + len));
    memcpy(&ip_header[2], &total_len, 2);
    
    /* Write PCAP record header */
    pcaprec_hdr_t rec_hdr = {
        .ts_sec = (uint32_t)tv.tv_sec,
        .ts_usec = (uint32_t)tv.tv_usec,
        .incl_len = (uint32_t)(20 + len),
        .orig_len = (uint32_t)(20 + len)
    };
    
    fwrite(&rec_hdr, sizeof(rec_hdr), 1, g_server.pcap.pcap_file);
    fwrite(ip_header, sizeof(ip_header), 1, g_server.pcap.pcap_file);
    fwrite(data, 1, len, g_server.pcap.pcap_file);
    fflush(g_server.pcap.pcap_file);
    
    g_server.pcap.packet_count++;
    
#ifdef _WIN32
    ReleaseMutex(g_server.pcap.pcap_mutex);
#else
    pthread_mutex_unlock(&g_server.pcap.pcap_mutex);
#endif
    
    return MOCK_GNB_SUCCESS;
}

void mock_gnb_pcap_close(void) {
    if (g_server.pcap.pcap_file != NULL) {
        fclose(g_server.pcap.pcap_file);
        g_server.pcap.pcap_file = NULL;
    }
    g_server.pcap.enabled = false;
}

/* ============== UE Context Management ============== */

mock_gnb_ue_context_t* mock_gnb_create_ue_context(int socket) {
    mock_gnb_ue_context_t* ue_ctx = (mock_gnb_ue_context_t*)calloc(1, sizeof(mock_gnb_ue_context_t));
    if (ue_ctx == NULL) {
        return NULL;
    }
    
    /* Generate unique IDs */
    static uint32_t ran_ue_id_counter = 0;
    ue_ctx->ran_ue_ngap_id = ++ran_ue_id_counter;
    ue_ctx->amf_ue_ngap_id = (uint64_t)ue_ctx->ran_ue_ngap_id;
    ue_ctx->rnti = (uint16_t)(rand() & 0xFFFF);
    
    ue_ctx->ngap_socket = socket;
    ue_ctx->state = MOCK_GNB_UE_STATE_CONNECTING;
    ue_ctx->connect_time = time(NULL);
    ue_ctx->last_activity = ue_ctx->connect_time;
    
    /* Add to server context */
#ifdef _WIN32
    WaitForSingleObject(g_server.ue_mutex, INFINITE);
#else
    pthread_mutex_lock(&g_server.ue_mutex);
#endif
    
    for (int i = 0; i < MOCK_GNB_MAX_UES; i++) {
        if (g_server.ue_contexts[i] == NULL) {
            g_server.ue_contexts[i] = ue_ctx;
            g_server.num_active_ues++;
            break;
        }
    }
    
#ifdef _WIN32
    ReleaseMutex(g_server.ue_mutex);
#else
    pthread_mutex_unlock(&g_server.ue_mutex);
#endif
    
    printf("[Mock gNB] Created UE context: RAN-UE-NGAP-ID=%u, RNTI=%04x\n",
           ue_ctx->ran_ue_ngap_id, ue_ctx->rnti);
    
    return ue_ctx;
}

mock_gnb_ue_context_t* mock_gnb_find_ue_by_socket(int socket) {
#ifdef _WIN32
    WaitForSingleObject(g_server.ue_mutex, INFINITE);
#else
    pthread_mutex_lock(&g_server.ue_mutex);
#endif
    
    mock_gnb_ue_context_t* ue_ctx = NULL;
    for (int i = 0; i < MOCK_GNB_MAX_UES; i++) {
        if (g_server.ue_contexts[i] != NULL &&
            g_server.ue_contexts[i]->ngap_socket == socket) {
            ue_ctx = g_server.ue_contexts[i];
            break;
        }
    }
    
#ifdef _WIN32
    ReleaseMutex(g_server.ue_mutex);
#else
    pthread_mutex_unlock(&g_server.ue_mutex);
#endif
    
    return ue_ctx;
}

mock_gnb_ue_context_t* mock_gnb_find_ue_by_ran_id(uint32_t ran_ue_ngap_id) {
#ifdef _WIN32
    WaitForSingleObject(g_server.ue_mutex, INFINITE);
#else
    pthread_mutex_lock(&g_server.ue_mutex);
#endif
    
    mock_gnb_ue_context_t* ue_ctx = NULL;
    for (int i = 0; i < MOCK_GNB_MAX_UES; i++) {
        if (g_server.ue_contexts[i] != NULL &&
            g_server.ue_contexts[i]->ran_ue_ngap_id == ran_ue_ngap_id) {
            ue_ctx = g_server.ue_contexts[i];
            break;
        }
    }
    
#ifdef _WIN32
    ReleaseMutex(g_server.ue_mutex);
#else
    pthread_mutex_unlock(&g_server.ue_mutex);
#endif
    
    return ue_ctx;
}

void mock_gnb_remove_ue_context(mock_gnb_ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) return;
    
#ifdef _WIN32
    WaitForSingleObject(g_server.ue_mutex, INFINITE);
#else
    pthread_mutex_lock(&g_server.ue_mutex);
#endif
    
    for (int i = 0; i < MOCK_GNB_MAX_UES; i++) {
        if (g_server.ue_contexts[i] == ue_ctx) {
            g_server.ue_contexts[i] = NULL;
            g_server.num_active_ues--;
            break;
        }
    }
    
#ifdef _WIN32
    ReleaseMutex(g_server.ue_mutex);
#else
    pthread_mutex_unlock(&g_server.ue_mutex);
#endif
    
    printf("[Mock gNB] Removed UE context: RAN-UE-NGAP-ID=%u\n", ue_ctx->ran_ue_ngap_id);
    free(ue_ctx);
}

/* ============== Server Initialization ============== */

mock_gnb_error_t mock_gnb_server_init(const mock_gnb_config_t* config) {
    if (config == NULL) {
        return MOCK_GNB_ERROR_INVALID_PARAM;
    }
    
    memcpy(&g_server.config, config, sizeof(mock_gnb_config_t));
    
    /* Initialize mutexes */
#ifdef _WIN32
    g_server.ue_mutex = CreateMutex(NULL, FALSE, NULL);
    g_server.stats_mutex = CreateMutex(NULL, FALSE, NULL);
    if (g_server.ue_mutex == NULL || g_server.stats_mutex == NULL) {
        return MOCK_GNB_ERROR_THREAD;
    }
    g_server.running = 0;
#else
    if (pthread_mutex_init(&g_server.ue_mutex, NULL) != 0 ||
        pthread_mutex_init(&g_server.stats_mutex, NULL) != 0) {
        return MOCK_GNB_ERROR_THREAD;
    }
    atomic_store(&g_server.running, false);
#endif
    
    /* Initialize Winsock on Windows */
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return MOCK_GNB_ERROR_SOCKET;
    }
#endif
    
    /* Initialize PCAP if configured */
    if (config->pcap_file[0] != '\0') {
        mock_gnb_pcap_init(config->pcap_file);
    }
    
    memset(g_server.ue_contexts, 0, sizeof(g_server.ue_contexts));
    g_server.num_active_ues = 0;
    memset(&g_server.stats, 0, sizeof(g_server.stats));
    
    printf("[Mock gNB] Server initialized\n");
    printf("  NGAP Port: %u\n", config->ngap_port);
    printf("  GTP-U Port: %u\n", config->gtpu_port);
    printf("  Max UEs: %u\n", config->max_ues);
    
    return MOCK_GNB_SUCCESS;
}

/* ============== NGAP Listener Thread ============== */

static void* ngap_listener_thread(void* arg) {
    (void)arg;
    struct sockaddr_in server_addr, client_addr;
    int client_socket;
    socklen_t client_len;
    fd_set read_fds;
    struct timeval tv;
    
    printf("[Mock gNB] NGAP listener thread started on port %u\n", g_server.config.ngap_port);
    
    /* Create TCP socket */
    g_server.ngap_listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server.ngap_listen_socket < 0) {
        perror("socket");
        return NULL;
    }
    
    /* Set socket options */
    int opt = 1;
    setsockopt(g_server.ngap_listen_socket, SOL_SOCKET, SO_REUSEADDR, 
               (const char*)&opt, sizeof(opt));
    
    /* Bind */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(g_server.config.ngap_port);
    server_addr.sin_addr.s_addr = inet_addr(g_server.config.bind_ip);
    
    if (bind(g_server.ngap_listen_socket, (struct sockaddr*)&server_addr, 
             sizeof(server_addr)) < 0) {
        perror("bind");
        close(g_server.ngap_listen_socket);
        return NULL;
    }
    
    /* Listen */
    if (listen(g_server.ngap_listen_socket, 10) < 0) {
        perror("listen");
        close(g_server.ngap_listen_socket);
        return NULL;
    }
    
    printf("[Mock gNB] NGAP listening on %s:%u\n", 
           g_server.config.bind_ip, g_server.config.ngap_port);
    
    /* Main accept loop */
    while (g_server.running
#ifdef _WIN32
           || InterlockedCompareExchange(&g_server.running, 0, 0)
#else
           || atomic_load(&g_server.running)
#endif
           ) {
        FD_ZERO(&read_fds);
        FD_SET(g_server.ngap_listen_socket, &read_fds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int sel = select(g_server.ngap_listen_socket + 1, &read_fds, NULL, NULL, &tv);
        if (sel < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }
        if (sel == 0) continue;  /* Timeout */
        
        client_len = sizeof(client_addr);
        client_socket = accept(g_server.ngap_listen_socket, 
                               (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("accept");
            continue;
        }
        
        printf("[Mock gNB] New connection from %s:%u\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        /* Handle client in this thread (could spawn worker thread) */
        handle_client_connection(client_socket);
    }
    
    close(g_server.ngap_listen_socket);
    printf("[Mock gNB] NGAP listener thread stopped\n");
    return NULL;
}

/* ============== GTP-U Listener Thread ============== */

static void* gtpu_listener_thread(void* arg) {
    (void)arg;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    uint8_t buffer[MOCK_GNB_BUFFER_SIZE];
    ssize_t received;
    
    printf("[Mock gNB] GTP-U listener thread started on port %u\n", g_server.config.gtpu_port);
    
    /* Create UDP socket */
    g_server.gtpu_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_server.gtpu_socket < 0) {
        perror("socket");
        return NULL;
    }
    
    /* Bind */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(g_server.config.gtpu_port);
    server_addr.sin_addr.s_addr = inet_addr(g_server.config.bind_ip);
    
    if (bind(g_server.gtpu_socket, (struct sockaddr*)&server_addr, 
             sizeof(server_addr)) < 0) {
        perror("bind");
        close(g_server.gtpu_socket);
        return NULL;
    }
    
    printf("[Mock gNB] GTP-U listening on %s:%u\n",
           g_server.config.bind_ip, g_server.config.gtpu_port);
    
    /* Main receive loop */
    while (g_server.running
#ifdef _WIN32
           || InterlockedCompareExchange(&g_server.running, 0, 0)
#else
           || atomic_load(&g_server.running)
#endif
           ) {
        fd_set read_fds;
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        FD_ZERO(&read_fds);
        FD_SET(g_server.gtpu_socket, &read_fds);
        
        int sel = select(g_server.gtpu_socket + 1, &read_fds, NULL, NULL, &tv);
        if (sel < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }
        if (sel == 0) continue;  /* Timeout */
        
        client_len = sizeof(client_addr);
        received = recvfrom(g_server.gtpu_socket, buffer, sizeof(buffer), 0,
                           (struct sockaddr*)&client_addr, &client_len);
        if (received > 0) {
            /* Log GTP-U packet */
            if (g_server.config.log_messages) {
                printf("[Mock gNB] GTP-U packet received: %zd bytes from %s:%u\n",
                       received, inet_ntoa(client_addr.sin_addr), 
                       ntohs(client_addr.sin_port));
            }
            
            /* Write to PCAP */
            struct sockaddr_in local_addr;
            memset(&local_addr, 0, sizeof(local_addr));
            local_addr.sin_family = AF_INET;
            local_addr.sin_port = htons(g_server.config.gtpu_port);
            local_addr.sin_addr.s_addr = inet_addr(g_server.config.bind_ip);
            
            mock_gnb_pcap_write_packet(buffer, received, &client_addr, &local_addr, 17);
            
            /* Update stats */
#ifdef _WIN32
            WaitForSingleObject(g_server.stats_mutex, INFINITE);
#else
            pthread_mutex_lock(&g_server.stats_mutex);
#endif
            g_server.stats.gtpu_packets_received++;
            g_server.stats.total_rx_bytes += received;
#ifdef _WIN32
            ReleaseMutex(g_server.stats_mutex);
#else
            pthread_mutex_unlock(&g_server.stats_mutex);
#endif
            
            /* Handle GTP-U packet */
            mock_gnb_handle_gtpu_packet(g_server.gtpu_socket, buffer, received, &client_addr);
        }
    }
    
    close(g_server.gtpu_socket);
    printf("[Mock gNB] GTP-U listener thread stopped\n");
    return NULL;
}

/* ============== Client Connection Handler ============== */

static mock_gnb_error_t handle_client_connection(int client_socket) {
    uint8_t buffer[MOCK_GNB_BUFFER_SIZE];
    ssize_t received;
    mock_gnb_ue_context_t* ue_ctx = NULL;
    
    /* Create UE context */
    ue_ctx = mock_gnb_create_ue_context(client_socket);
    if (ue_ctx == NULL) {
        close(client_socket);
        return MOCK_GNB_ERROR_MEMORY;
    }
    
    /* Update stats */
#ifdef _WIN32
    WaitForSingleObject(g_server.stats_mutex, INFINITE);
#else
    pthread_mutex_lock(&g_server.stats_mutex);
#endif
    g_server.stats.total_connections++;
    g_server.stats.active_connections++;
#ifdef _WIN32
    ReleaseMutex(g_server.stats_mutex);
#else
    pthread_mutex_unlock(&g_server.stats_mutex);
#endif
    
    /* Receive loop */
    while (g_server.running
#ifdef _WIN32
           || InterlockedCompareExchange(&g_server.running, 0, 0)
#else
           || atomic_load(&g_server.running)
#endif
           ) {
        fd_set read_fds;
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        FD_ZERO(&read_fds);
        FD_SET(client_socket, &read_fds);
        
        int sel = select(client_socket + 1, &read_fds, NULL, NULL, &tv);
        if (sel < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (sel == 0) continue;  /* Timeout */
        
        received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (received <= 0) {
            printf("[Mock gNB] Connection closed for UE %u\n", ue_ctx->ran_ue_ngap_id);
            break;
        }
        
        /* Log received message */
        if (g_server.config.log_messages) {
            printf("[Mock gNB] Received %zd bytes from UE %u (state=%s)\n",
                   received, ue_ctx->ran_ue_ngap_id, 
                   mock_gnb_ue_state_to_string(ue_ctx->state));
        }
        
        /* Write to PCAP */
        struct sockaddr_in local_addr, remote_addr;
        memset(&local_addr, 0, sizeof(local_addr));
        local_addr.sin_family = AF_INET;
        local_addr.sin_port = htons(g_server.config.ngap_port);
        local_addr.sin_addr.s_addr = inet_addr(g_server.config.bind_ip);
        
        socklen_t addr_len = sizeof(remote_addr);
        getpeername(client_socket, (struct sockaddr*)&remote_addr, &addr_len);
        
        mock_gnb_pcap_write_packet(buffer, received, &remote_addr, &local_addr, 6);
        
        /* Update stats */
#ifdef _WIN32
        WaitForSingleObject(g_server.stats_mutex, INFINITE);
#else
        pthread_mutex_lock(&g_server.stats_mutex);
#endif
        g_server.stats.ngap_messages_received++;
        g_server.stats.total_rx_bytes += received;
        ue_ctx->rx_bytes += received;
        ue_ctx->rx_packets++;
        ue_ctx->last_activity = time(NULL);
#ifdef _WIN32
        ReleaseMutex(g_server.stats_mutex);
#else
        pthread_mutex_unlock(&g_server.stats_mutex);
#endif
        
        /* Process message */
        process_ngap_message(ue_ctx, buffer, received);
    }
    
    /* Cleanup */
    close(client_socket);
    
#ifdef _WIN32
    WaitForSingleObject(g_server.stats_mutex, INFINITE);
#else
    pthread_mutex_lock(&g_server.stats_mutex);
#endif
    g_server.stats.active_connections--;
#ifdef _WIN32
    ReleaseMutex(g_server.stats_mutex);
#else
    pthread_mutex_unlock(&g_server.stats_mutex);
#endif
    
    mock_gnb_remove_ue_context(ue_ctx);
    
    return MOCK_GNB_SUCCESS;
}

/* ============== Message Processing ============== */

/* Simple message format for testing:
 * [4 bytes: message_type][4 bytes: transaction_id][4 bytes: data_len][data]
 */

typedef struct {
    uint32_t message_type;
    uint32_t transaction_id;
    uint32_t data_len;
    uint8_t data[];
} mock_message_header_t;

/* Detected RRC message type */
typedef enum {
    RRC_MSG_TYPE_UNKNOWN = 0,
    RRC_MSG_TYPE_SETUP_REQUEST,
    RRC_MSG_TYPE_SETUP_COMPLETE,
    RRC_MSG_TYPE_REESTABLISHMENT_REQUEST,
    RRC_MSG_TYPE_RECONFIGURATION_COMPLETE,
    RRC_MSG_TYPE_MEASUREMENT_REPORT,
    RRC_MSG_TYPE_HANDOVER_PREPARATION,
    RRC_MSG_TYPE_UE_CAPABILITY_INFO,
    RRC_MSG_TYPE_SECURITY_MODE_COMPLETE
} rrc_detected_type_t;

/* Detect RRC message type from ASN.1 PER encoded data */
static rrc_detected_type_t detect_rrc_message_type(const uint8_t* data, size_t len) {
    if (data == NULL || len < 1) {
        return RRC_MSG_TYPE_UNKNOWN;
    }
    
    uint8_t first_byte = data[0];
    
    /* RRCSetupRequest: starts with establishment cause (3 bits, values 0-5) + spare (1 bit, 0) + UE ID type (1 bit, 0) */
    uint8_t cause = (first_byte >> 5) & 0x07;
    uint8_t spare = (first_byte >> 4) & 0x01;
    uint8_t id_type = (first_byte >> 3) & 0x01;
    if (cause <= 5 && spare == 0 && id_type == 0 && len >= 6) {
        return RRC_MSG_TYPE_SETUP_REQUEST;
    }
    
    /* RRCSetupComplete: starts with RRC transaction ID (2 bits) + selected PLMN (4 bits) */
    uint8_t tid = (first_byte >> 6) & 0x03;
    uint8_t plmn = (first_byte >> 2) & 0x0F;
    uint8_t spare2 = first_byte & 0x03;
    if (spare2 == 0 && plmn > 0 && plmn < 12 && len >= 2) {
        return RRC_MSG_TYPE_SETUP_COMPLETE;
    }
    
    /* RRCReestablishmentRequest: starts with reestablishment cause (2 bits) */
    uint8_t reest_cause = (first_byte >> 6) & 0x03;
    if (reest_cause <= 2 && len >= 12) {
        /* Additional check: PCI should be valid (0-1007) */
        if (len >= 3) {
            uint16_t pci = ((uint16_t)data[1] << 8) | data[2];
            if (pci <= 1007) {
                return RRC_MSG_TYPE_REESTABLISHMENT_REQUEST;
            }
        }
    }
    
    /* RRCReconfigurationComplete: starts with RRC transaction ID (2 bits) + spare (6 bits = 0) */
    if ((first_byte & 0x3F) == 0 && len >= 1) {
        return RRC_MSG_TYPE_RECONFIGURATION_COMPLETE;
    }
    
    /* RRCMeasurementReport: starts with meas_id (6 bits) + spare (2 bits) */
    uint8_t meas_id = (first_byte >> 2) & 0x3F;
    uint8_t meas_spare = first_byte & 0x03;
    if (meas_spare == 0 && meas_id > 0 && len >= 10) {
        return RRC_MSG_TYPE_MEASUREMENT_REPORT;
    }
    
    /* RRCHandoverPreparation: similar to measurement report */
    if (meas_spare == 0 && len >= 9) {
        return RRC_MSG_TYPE_HANDOVER_PREPARATION;
    }
    
    /* RRCUECapabilityInformation: starts with rat_type (4 bits) + spare (4 bits) */
    uint8_t rat_type = (first_byte >> 4) & 0x0F;
    uint8_t rat_spare = first_byte & 0x0F;
    if (rat_spare == 0 && rat_type <= 2 && len >= 2) {
        return RRC_MSG_TYPE_UE_CAPABILITY_INFO;
    }
    
    /* RRCSecurityModeComplete: starts with RRC transaction ID (2 bits) + spare (6 bits) */
    if ((first_byte & 0x3F) == 0 && len == 1) {
        return RRC_MSG_TYPE_SECURITY_MODE_COMPLETE;
    }
    
    return RRC_MSG_TYPE_UNKNOWN;
}

/* Check if data looks like ASN.1 PER encoded RRCSetupRequest */
static bool is_asn1_per_rrc_setup_request(const uint8_t* data, size_t len) {
    return detect_rrc_message_type(data, len) == RRC_MSG_TYPE_SETUP_REQUEST;
}

/* Generate ASN.1 PER encoded RRCSetup message */
static mock_gnb_error_t generate_rrc_setup_asn1(uint8_t transaction_id, void** response, size_t* len){
    asn1_buffer_t buf;
    uesim_error_t result = asn1_buffer_alloc(&buf, 64);
    if (result != 0) {
        return MOCK_GNB_ERROR_MEMORY;
    }
    
    /* RRCSetup message structure (3GPP TS 38.331):
     * - RRC-TransactionIdentifier (1 byte)
     * - criticalExtensions choice (1 bit) - rrcSetup present
     * - masterCellGroupConfig (variable)
     * - radioBearerConfig (optional)
     */
    
    /* Encode RRC Transaction ID */
    asn1_encode_bits(&buf, transaction_id, 8);
    
    /* criticalExtensions choice: rrcSetup (index 0) */
    asn1_encode_choice(&buf, 0, 2);
    
    /* rrcSetup content - minimal radio bearer config for SRB1 */
    /* masterCellGroup (simplified) */
    asn1_encode_length(&buf, 1);
    asn1_encode_octet_string(&buf, (const uint8_t*)"\x01", 1);
    
    *len = asn1_buffer_length(&buf);
    *response = buf.data;
    buf.own_data = false;
    asn1_buffer_free(&buf);
    
    printf("[Mock gNB] Generated ASN.1 PER RRCSetup (trans_id=%u, len=%zu)\n", transaction_id, *len);
    return MOCK_GNB_SUCCESS;
}

static void process_ngap_message(mock_gnb_ue_context_t* ue_ctx, const void* data, size_t len) {
    if (ue_ctx == NULL || data == NULL || len < 1) {
        return;
    }
    
    const uint8_t* msg_data = (const uint8_t*)data;
    
    /* Detect RRC message type from ASN.1 PER encoded data */
    rrc_detected_type_t rrc_type = detect_rrc_message_type(msg_data, len);
    
    switch (rrc_type) {
        case RRC_MSG_TYPE_SETUP_REQUEST: {
            printf("[Mock gNB] Detected ASN.1 PER encoded RRCSetupRequest (%zu bytes)\n", len);
            
            /* Decode RRCSetupRequest */
            rrc_setup_request_t setup_req = {0};
            uesim_error_t decode_result = rrc_decode_setup_request(msg_data, len, &setup_req);
            
            if (decode_result == 0) {
                printf("[Mock gNB] Decoded RRCSetupRequest:\n");
                printf("  Establishment Cause: %u\n", setup_req.establishment_cause);
                printf("  UE Identity Type: %u\n", setup_req.ue_identity.type);
                printf("  UE Identity Value: 0x%016llX\n", (unsigned long long)setup_req.ue_identity.random_value);
                
                /* Store UE identity in context */
                ue_ctx->ue_identity = setup_req.ue_identity.random_value;
                ue_ctx->state = MOCK_GNB_UE_STATE_CONNECTED;
                
                /* Generate and send RRCSetup response using ASN.1 PER */
                if (g_server.config.auto_respond) {
                    void* response = NULL;
                    size_t resp_len = 0;
                    uint8_t trans_id = (uint8_t)(ue_ctx->ran_ue_ngap_id & 0xFF);
                    
                    if (generate_rrc_setup_asn1(trans_id, &response, &resp_len) == MOCK_GNB_SUCCESS) {
                        if (g_server.config.response_delay_ms > 0) {
#ifdef _WIN32
                            Sleep(g_server.config.response_delay_ms);
#else
                            usleep(g_server.config.response_delay_ms * 1000);
#endif
                        }
                        send_ngap_response(ue_ctx, response, resp_len);
                        free(response);
                    }
                }
            } else {
                printf("[Mock gNB] Failed to decode RRCSetupRequest: %d\n", decode_result);
            }
            return;
        }
        
        case RRC_MSG_TYPE_SETUP_COMPLETE: {
            printf("[Mock gNB] Detected ASN.1 PER encoded RRCSetupComplete (%zu bytes)\n", len);
            
            rrc_setup_complete_t setup_complete = {0};
            uesim_error_t decode_result = rrc_decode_setup_complete(msg_data, len, &setup_complete);
            
            if (decode_result == 0) {
                printf("[Mock gNB] Decoded RRCSetupComplete:\n");
                printf("  RRC Transaction ID: %u\n", setup_complete.rrc_transaction_id);
                printf("  Selected PLMN: %u\n", setup_complete.selected_plmn);
                printf("  NAS PDU Length: %zu\n", setup_complete.nas_pdu_len);
                
                ue_ctx->state = MOCK_GNB_UE_STATE_REGISTERED;
                
#ifdef _WIN32
                WaitForSingleObject(g_server.stats_mutex, INFINITE);
#else
                pthread_mutex_lock(&g_server.stats_mutex);
#endif
                g_server.stats.total_registrations++;
                g_server.stats.successful_registrations++;
#ifdef _WIN32
                ReleaseMutex(g_server.stats_mutex);
#else
                pthread_mutex_unlock(&g_server.stats_mutex);
#endif
            }
            return;
        }
        
        case RRC_MSG_TYPE_REESTABLISHMENT_REQUEST: {
            printf("[Mock gNB] Detected ASN.1 PER encoded RRCReestablishmentRequest (%zu bytes)\n", len);
            
            rrc_reest_request_t reest_req = {0};
            uesim_error_t decode_result = rrc_decode_reest_request(msg_data, len, &reest_req);
            
            if (decode_result == 0) {
                printf("[Mock gNB] Decoded RRCReestablishmentRequest:\n");
                printf("  Reestablishment Cause: %u\n", reest_req.reestablishment_cause);
                printf("  PCI: %u\n", reest_req.pci);
                printf("  C-RNTI: %u\n", reest_req.c_rnti);
                
                ue_ctx->state = MOCK_GNB_UE_STATE_CONNECTED;
                
                /* Generate RRCReestablishment response */
                if (g_server.config.auto_respond) {
                    asn1_buffer_t buf;
                    if (asn1_buffer_alloc(&buf, 64) == 0) {
                        rrc_reestablishment_t reest_resp = {0};
                        reest_resp.rrc_transaction_id = (uint8_t)(ue_ctx->ran_ue_ngap_id & 0x03);
                        reest_resp.config_len = 0;
                        
                        rrc_encode_reestablishment(&buf, &reest_resp);
                        
                        if (g_server.config.response_delay_ms > 0) {
#ifdef _WIN32
                            Sleep(g_server.config.response_delay_ms);
#else
                            usleep(g_server.config.response_delay_ms * 1000);
#endif
                        }
                        send_ngap_response(ue_ctx, buf.data, asn1_buffer_length(&buf));
                        asn1_buffer_free(&buf);
                    }
                }
            }
            return;
        }
        
        case RRC_MSG_TYPE_RECONFIGURATION_COMPLETE: {
            printf("[Mock gNB] Detected ASN.1 PER encoded RRCReconfigurationComplete (%zu bytes)\n", len);
            
            rrc_reconfig_complete_t reconfig_complete = {0};
            uesim_error_t decode_result = rrc_decode_reconfig_complete(msg_data, len, &reconfig_complete);
            
            if (decode_result == 0) {
                printf("[Mock gNB] Decoded RRCReconfigurationComplete:\n");
                printf("  RRC Transaction ID: %u\n", reconfig_complete.rrc_transaction_id);
            }
            return;
        }
        
        case RRC_MSG_TYPE_MEASUREMENT_REPORT: {
            printf("[Mock gNB] Detected ASN.1 PER encoded RRCMeasurementReport (%zu bytes)\n", len);
            
            rrc_measurement_report_t meas_report = {0};
            uesim_error_t decode_result = rrc_decode_measurement_report(msg_data, len, &meas_report);
            
            if (decode_result == 0) {
                printf("[Mock gNB] Decoded RRCMeasurementReport:\n");
                printf("  Meas ID: %u\n", meas_report.meas_id);
                printf("  RSRP: %d dBm\n", meas_report.rsrp);
                printf("  RSRQ: %d dB\n", meas_report.rsrq);
                printf("  PCI: %u\n", meas_report.pci);
                printf("  Cell ID: %u\n", meas_report.cell_id);
            }
            return;
        }
        
        case RRC_MSG_TYPE_HANDOVER_PREPARATION: {
            printf("[Mock gNB] Detected ASN.1 PER encoded RRCHandoverPreparation (%zu bytes)\n", len);
            
            /* Handover preparation uses similar format to measurement report */
            rrc_measurement_report_t ho_prep = {0};
            uesim_error_t decode_result = rrc_decode_measurement_report(msg_data, len, &ho_prep);
            
            if (decode_result == 0) {
                printf("[Mock gNB] Decoded HandoverPreparation:\n");
                printf("  Target PCI: %u\n", ho_prep.pci);
                printf("  Target Cell ID: %u\n", ho_prep.cell_id);
                printf("  RSRP: %d dBm\n", ho_prep.rsrp);
                
                /* Generate Handover Command */
                if (g_server.config.auto_respond) {
                    asn1_buffer_t buf;
                    if (asn1_buffer_alloc(&buf, 128) == 0) {
                        rrc_handover_command_t ho_cmd = {0};
                        ho_cmd.rrc_transaction_id = (uint8_t)(ue_ctx->ran_ue_ngap_id & 0x03);
                        ho_cmd.target_pci = ho_prep.pci;
                        ho_cmd.target_cell_id = ho_prep.cell_id;
                        ho_cmd.new_c_rnti = (uint8_t)(rand() & 0xFF);
                        ho_cmd.config_len = 0;
                        
                        rrc_encode_handover_command(&buf, &ho_cmd);
                        
                        if (g_server.config.response_delay_ms > 0) {
#ifdef _WIN32
                            Sleep(g_server.config.response_delay_ms);
#else
                            usleep(g_server.config.response_delay_ms * 1000);
#endif
                        }
                        send_ngap_response(ue_ctx, buf.data, asn1_buffer_length(&buf));
                        asn1_buffer_free(&buf);
                    }
                }
            }
            return;
        }
        
        case RRC_MSG_TYPE_UE_CAPABILITY_INFO: {
            printf("[Mock gNB] Detected ASN.1 PER encoded RRCUECapabilityInformation (%zu bytes)\n", len);
            printf("[Mock gNB] UE Capability Information received\n");
            return;
        }
        
        case RRC_MSG_TYPE_SECURITY_MODE_COMPLETE: {
            printf("[Mock gNB] Detected ASN.1 PER encoded RRCSecurityModeComplete (%zu bytes)\n", len);
            printf("[Mock gNB] Security Mode Complete received\n");
            return;
        }
        
        default:
            break;
    }
    
    /* Fall back to simple message format */
    if (len < 12) {
        printf("[Mock gNB] Message too short for simple format: %zu bytes\n", len);
        return;
    }
    
    const mock_message_header_t* msg = (const mock_message_header_t*)data;
    uint32_t msg_type = ntohl(msg->message_type);
    uint32_t trans_id = ntohl(msg->transaction_id);
    
    printf("[Mock gNB] Processing simple message type=%u, trans_id=%u, len=%u\n",
           msg_type, trans_id, ntohl(msg->data_len));
    
    /* Process based on message type */
    switch (msg_type) {
        case MOCK_NGAP_NG_SETUP_REQUEST:
            /* NG Setup - respond with NG Setup Response */
            if (g_server.config.auto_respond) {
                void* response = NULL;
                size_t resp_len = 0;
                if (mock_gnb_generate_ng_setup_response(&response, &resp_len) == MOCK_GNB_SUCCESS) {
                    /* Apply delay */
                    if (g_server.config.response_delay_ms > 0) {
#ifdef _WIN32
                        Sleep(g_server.config.response_delay_ms);
#else
                        usleep(g_server.config.response_delay_ms * 1000);
#endif
                    }
                    send_ngap_response(ue_ctx, response, resp_len);
                    free(response);
                }
            }
            break;
            
        case MOCK_NGAP_INITIAL_UE_MESSAGE:
            /* Initial UE Message - transition to connected */
            ue_ctx->state = MOCK_GNB_UE_STATE_CONNECTED;
            
            /* Send RRC Setup */
            if (g_server.config.auto_respond) {
                void* response = NULL;
                size_t resp_len = 0;
                if (mock_gnb_generate_rrc_setup(trans_id, &response, &resp_len) == MOCK_GNB_SUCCESS) {
                    if (g_server.config.response_delay_ms > 0) {
#ifdef _WIN32
                        Sleep(g_server.config.response_delay_ms);
#else
                        usleep(g_server.config.response_delay_ms * 1000);
#endif
                    }
                    send_ngap_response(ue_ctx, response, resp_len);
                    free(response);
                }
            }
            break;
            
        case MOCK_NGAP_UPLINK_NAS_TRANSPORT:
            /* NAS message from UE */
            {
                const uint8_t* nas_data = msg->data;
                uint32_t nas_len = ntohl(msg->data_len);
                
                if (nas_len > 0) {
                    /* Parse NAS message type (first byte) */
                    uint8_t nas_msg_type = nas_data[0];
                    printf("[Mock gNB] NAS message type: %u\n", nas_msg_type);
                    
                    if (nas_msg_type == MOCK_NAS_REGISTRATION_REQUEST && g_server.config.auto_respond) {
                        /* Send Registration Accept */
                        void* response = NULL;
                        size_t resp_len = 0;
                        if (mock_gnb_generate_registration_accept(ue_ctx, &response, &resp_len) == MOCK_GNB_SUCCESS) {
                            if (g_server.config.response_delay_ms > 0) {
#ifdef _WIN32
                                Sleep(g_server.config.response_delay_ms);
#else
                                usleep(g_server.config.response_delay_ms * 1000);
#endif
                            }
                            send_ngap_response(ue_ctx, response, resp_len);
                            free(response);
                            
                            ue_ctx->state = MOCK_GNB_UE_STATE_REGISTERED;
                            
#ifdef _WIN32
                            WaitForSingleObject(g_server.stats_mutex, INFINITE);
#else
                            pthread_mutex_lock(&g_server.stats_mutex);
#endif
                            g_server.stats.total_registrations++;
                            g_server.stats.successful_registrations++;
#ifdef _WIN32
                            ReleaseMutex(g_server.stats_mutex);
#else
                            pthread_mutex_unlock(&g_server.stats_mutex);
#endif
                        }
                    }
                    else if (nas_msg_type == MOCK_NAS_PDU_SESSION_ESTABLISHMENT_REQUEST && g_server.config.auto_respond) {
                        /* Find free PDU session ID */
                        uint8_t session_id = 0;
                        for (int i = 1; i < MOCK_GNB_MAX_PDU_SESSIONS; i++) {
                            if (ue_ctx->pdu_sessions[i].state == MOCK_GNB_PDU_STATE_INACTIVE) {
                                session_id = i;
                                break;
                            }
                        }
                        
                        if (session_id > 0) {
                            void* response = NULL;
                            size_t resp_len = 0;
                            if (mock_gnb_generate_pdu_session_accept(ue_ctx, session_id, &response, &resp_len) == MOCK_GNB_SUCCESS) {
                                if (g_server.config.response_delay_ms > 0) {
#ifdef _WIN32
                                    Sleep(g_server.config.response_delay_ms);
#else
                                    usleep(g_server.config.response_delay_ms * 1000);
#endif
                                }
                                send_ngap_response(ue_ctx, response, resp_len);
                                free(response);
                                
                                ue_ctx->pdu_sessions[session_id].state = MOCK_GNB_PDU_STATE_ACTIVE;
                                ue_ctx->pdu_sessions[session_id].pdu_session_id = session_id;
                                ue_ctx->pdu_sessions[session_id].ue_ip_address = 0x0A000001 + ue_ctx->ran_ue_ngap_id;
                                ue_ctx->num_active_sessions++;
                            }
                        }
                    }
                }
            }
            break;
            
        case MOCK_NGAP_UE_CONTEXT_RELEASE_COMPLETE:
            /* UE Context Release Complete */
            ue_ctx->state = MOCK_GNB_UE_STATE_RELEASED;
            break;
            
        case MOCK_NGAP_HANDOVER_NOTIFY:
            /* Handover Notify */
            ue_ctx->state = MOCK_GNB_UE_STATE_CONNECTED;
#ifdef _WIN32
            WaitForSingleObject(g_server.stats_mutex, INFINITE);
#else
            pthread_mutex_lock(&g_server.stats_mutex);
#endif
            g_server.stats.successful_handovers++;
#ifdef _WIN32
            ReleaseMutex(g_server.stats_mutex);
#else
            pthread_mutex_unlock(&g_server.stats_mutex);
#endif
            break;
            
        default:
            printf("[Mock gNB] Unhandled message type: %u\n", msg_type);
            break;
    }
}

static void send_ngap_response(mock_gnb_ue_context_t* ue_ctx, const void* data, size_t len) {
    if (ue_ctx == NULL || data == NULL || len == 0) {
        return;
    }
    
    ssize_t sent = send(ue_ctx->ngap_socket, data, len, 0);
    if (sent < 0) {
        perror("send");
        return;
    }
    
    /* Write to PCAP */
    struct sockaddr_in local_addr, remote_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(g_server.config.ngap_port);
    local_addr.sin_addr.s_addr = inet_addr(g_server.config.bind_ip);
    
    socklen_t addr_len = sizeof(remote_addr);
    getpeername(ue_ctx->ngap_socket, (struct sockaddr*)&remote_addr, &addr_len);
    
    mock_gnb_pcap_write_packet(data, len, &local_addr, &remote_addr, 6);
    
    /* Update stats */
#ifdef _WIN32
    WaitForSingleObject(g_server.stats_mutex, INFINITE);
#else
    pthread_mutex_lock(&g_server.stats_mutex);
#endif
    g_server.stats.ngap_messages_sent++;
    g_server.stats.total_tx_bytes += sent;
    ue_ctx->tx_bytes += sent;
    ue_ctx->tx_packets++;
#ifdef _WIN32
    ReleaseMutex(g_server.stats_mutex);
#else
    pthread_mutex_unlock(&g_server.stats_mutex);
#endif
    
    if (g_server.config.log_messages) {
        printf("[Mock gNB] Sent %zd bytes to UE %u\n", sent, ue_ctx->ran_ue_ngap_id);
    }
}

/* ============== Response Generation ============== */

mock_gnb_error_t mock_gnb_generate_ng_setup_response(void** response, size_t* len) {
    size_t msg_len = 12;  /* Header only */
    void* data = malloc(msg_len);
    if (data == NULL) {
        return MOCK_GNB_ERROR_MEMORY;
    }
    
    mock_message_header_t* msg = (mock_message_header_t*)data;
    msg->message_type = htonl(MOCK_NGAP_NG_SETUP_RESPONSE);
    msg->transaction_id = htonl(0);
    msg->data_len = htonl(0);
    
    *response = data;
    *len = msg_len;
    
    printf("[Mock gNB] Generated NG Setup Response\n");
    return MOCK_GNB_SUCCESS;
}

mock_gnb_error_t mock_gnb_generate_rrc_setup(uint32_t transaction_id, void** response, size_t* len) {
    size_t msg_len = 12 + 8;  /* Header + RRC data */
    void* data = malloc(msg_len);
    if (data == NULL) {
        return MOCK_GNB_ERROR_MEMORY;
    }
    
    mock_message_header_t* msg = (mock_message_header_t*)data;
    msg->message_type = htonl(MOCK_NGAP_INITIAL_CONTEXT_SETUP_RESPONSE);
    msg->transaction_id = htonl(transaction_id);
    msg->data_len = htonl(8);
    
    /* RRC Setup data: cell_id, rnti */
    uint32_t* rrc_data = (uint32_t*)msg->data;
    rrc_data[0] = htonl(g_server.config.cell_config.cell_id);
    rrc_data[1] = htonl(0);  /* RNTI placeholder */
    
    *response = data;
    *len = msg_len;
    
    printf("[Mock gNB] Generated RRC Setup (trans_id=%u)\n", transaction_id);
    return MOCK_GNB_SUCCESS;
}

mock_gnb_error_t mock_gnb_generate_registration_accept(mock_gnb_ue_context_t* ue_ctx,
                                                        void** response, size_t* len) {
    size_t msg_len = 12 + 24;  /* Header + Registration Accept data */
    void* data = malloc(msg_len);
    if (data == NULL) {
        return MOCK_GNB_ERROR_MEMORY;
    }
    
    mock_message_header_t* msg = (mock_message_header_t*)data;
    msg->message_type = htonl(MOCK_NGAP_DOWNLINK_NAS_TRANSPORT);
    msg->transaction_id = htonl(ue_ctx->ran_ue_ngap_id);
    msg->data_len = htonl(24);
    
    /* NAS Registration Accept data */
    uint8_t* nas_data = msg->data;
    nas_data[0] = MOCK_NAS_REGISTRATION_ACCEPT;
    nas_data[1] = 0;  /* Registration result */
    memcpy(&nas_data[2], &ue_ctx->ran_ue_ngap_id, 4);  /* UE ID */
    memcpy(&nas_data[6], &g_server.config.cell_config.tac, 2);  /* TAC */
    memcpy(&nas_data[8], &g_server.config.cell_config.plmn_id, 4);  /* PLMN */
    /* GUTI */
    memset(&nas_data[12], 0, 12);
    
    *response = data;
    *len = msg_len;
    
    printf("[Mock gNB] Generated Registration Accept for UE %u\n", ue_ctx->ran_ue_ngap_id);
    return MOCK_GNB_SUCCESS;
}

mock_gnb_error_t mock_gnb_generate_pdu_session_accept(mock_gnb_ue_context_t* ue_ctx,
                                                       uint8_t session_id,
                                                       void** response, size_t* len) {
    size_t msg_len = 12 + 16;  /* Header + PDU Session Accept data */
    void* data = malloc(msg_len);
    if (data == NULL) {
        return MOCK_GNB_ERROR_MEMORY;
    }
    
    mock_message_header_t* msg = (mock_message_header_t*)data;
    msg->message_type = htonl(MOCK_NGAP_DOWNLINK_NAS_TRANSPORT);
    msg->transaction_id = htonl(ue_ctx->ran_ue_ngap_id);
    msg->data_len = htonl(16);
    
    /* NAS PDU Session Establishment Accept data */
    uint8_t* nas_data = msg->data;
    nas_data[0] = MOCK_NAS_PDU_SESSION_ESTABLISHMENT_ACCEPT;
    nas_data[1] = session_id;
    nas_data[2] = 1;  /* PDU session type: IPv4 */
    /* UE IP address */
    uint32_t ue_ip = htonl(0x0A000001 + ue_ctx->ran_ue_ngap_id);
    memcpy(&nas_data[3], &ue_ip, 4);
    /* QoS info */
    nas_data[7] = 1;  /* QFI */
    nas_data[8] = 9;  /* 5QI */
    memset(&nas_data[9], 0, 7);  /* Padding */
    
    *response = data;
    *len = msg_len;
    
    printf("[Mock gNB] Generated PDU Session Accept for UE %u, session %u\n", 
           ue_ctx->ran_ue_ngap_id, session_id);
    return MOCK_GNB_SUCCESS;
}

mock_gnb_error_t mock_gnb_generate_handover_command(mock_gnb_ue_context_t* ue_ctx,
                                                    uint16_t target_pci,
                                                    void** response, size_t* len) {
    size_t msg_len = 12 + 16;  /* Header + Handover Command data */
    void* data = malloc(msg_len);
    if (data == NULL) {
        return MOCK_GNB_ERROR_MEMORY;
    }
    
    mock_message_header_t* msg = (mock_message_header_t*)data;
    msg->message_type = htonl(MOCK_NGAP_HANDOVER_COMMAND);
    msg->transaction_id = htonl(ue_ctx->ran_ue_ngap_id);
    msg->data_len = htonl(16);
    
    /* Handover Command data */
    uint8_t* ho_data = msg->data;
    ho_data[0] = 0;  /* Handover type */
    ho_data[1] = 0;  /* Cause */
    uint16_t pci_net = htons(target_pci);
    memcpy(&ho_data[2], &pci_net, 2);
    uint32_t target_cell_id = htonl(target_pci);
    memcpy(&ho_data[4], &target_cell_id, 4);
    memset(&ho_data[8], 0, 8);  /* Additional config */
    
    *response = data;
    *len = msg_len;
    
    printf("[Mock gNB] Generated Handover Command for UE %u to PCI %u\n",
           ue_ctx->ran_ue_ngap_id, target_pci);
    return MOCK_GNB_SUCCESS;
}

/* ============== GTP-U Handler ============== */

mock_gnb_error_t mock_gnb_handle_gtpu_packet(int socket, const void* data, size_t len,
                                              const struct sockaddr_in* src_addr) {
    (void)socket;
    (void)src_addr;
    
    if (data == NULL || len < 8) {
        return MOCK_GNB_ERROR_INVALID_PARAM;
    }
    
    /* Parse GTP-U header */
    const uint8_t* gtp_hdr = (const uint8_t*)data;
    uint8_t version = (gtp_hdr[0] >> 5) & 0x07;
    uint8_t pt = (gtp_hdr[0] >> 4) & 0x01;
    uint8_t message_type = gtp_hdr[1];
    uint16_t length = ntohs(*(uint16_t*)&gtp_hdr[2]);
    uint32_t teid = ntohl(*(uint32_t*)&gtp_hdr[4]);
    
    (void)version;
    (void)pt;
    (void)length;
    
    if (g_server.config.log_messages) {
        printf("[Mock gNB] GTP-U: type=%u, len=%u, TEID=%u\n", message_type, length, teid);
    }
    
    /* Handle GTP-U echo request */
    if (message_type == 1) {  /* Echo Request */
        uint8_t echo_response[8];
        echo_response[0] = 0x30;  /* Version 2, PT=1 */
        echo_response[1] = 2;     /* Echo Response */
        echo_response[2] = 0;     /* Length */
        echo_response[3] = 0;
        echo_response[4] = 0;     /* TEID */
        echo_response[5] = 0;
        echo_response[6] = 0;
        echo_response[7] = 0;
        
        sendto(socket, echo_response, sizeof(echo_response), 0,
               (const struct sockaddr*)src_addr, sizeof(*src_addr));
    }
    
    /* For user data (message_type = 255), just log and forward */
    if (message_type == 255) {
        /* User data packet - could forward to simulated UPF */
        if (g_server.config.log_messages) {
            printf("[Mock gNB] GTP-U user data: %zu bytes, TEID=%u\n", len, teid);
        }
    }
    
    return MOCK_GNB_SUCCESS;
}

/* ============== Server Control ============== */

mock_gnb_error_t mock_gnb_server_start(void) {
#ifdef _WIN32
    InterlockedExchange(&g_server.running, 1);
#else
    atomic_store(&g_server.running, true);
#endif
    g_server.stats.start_time = time(NULL);
    
    /* Create listener threads */
#ifdef _WIN32
    g_server.ngap_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ngap_listener_thread, NULL, 0, NULL);
    g_server.gtpu_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)gtpu_listener_thread, NULL, 0, NULL);
    if (g_server.ngap_thread == NULL || g_server.gtpu_thread == NULL) {
        mock_gnb_server_stop();
        return MOCK_GNB_ERROR_THREAD;
    }
#else
    if (pthread_create(&g_server.ngap_thread, NULL, ngap_listener_thread, NULL) != 0 ||
        pthread_create(&g_server.gtpu_thread, NULL, gtpu_listener_thread, NULL) != 0) {
        mock_gnb_server_stop();
        return MOCK_GNB_ERROR_THREAD;
    }
#endif
    
    printf("[Mock gNB] Server started\n");
    return MOCK_GNB_SUCCESS;
}

void mock_gnb_server_stop(void) {
#ifdef _WIN32
    InterlockedExchange(&g_server.running, 0);
#else
    atomic_store(&g_server.running, false);
#endif
    
    /* Close sockets to unblock threads */
    if (g_server.ngap_listen_socket >= 0) {
        close(g_server.ngap_listen_socket);
        g_server.ngap_listen_socket = -1;
    }
    if (g_server.gtpu_socket >= 0) {
        close(g_server.gtpu_socket);
        g_server.gtpu_socket = -1;
    }
    
    /* Wait for threads */
#ifdef _WIN32
    if (g_server.ngap_thread != NULL) {
        WaitForSingleObject(g_server.ngap_thread, 5000);
        CloseHandle(g_server.ngap_thread);
    }
    if (g_server.gtpu_thread != NULL) {
        WaitForSingleObject(g_server.gtpu_thread, 5000);
        CloseHandle(g_server.gtpu_thread);
    }
#else
    if (g_server.ngap_thread != 0) {
        pthread_join(g_server.ngap_thread, NULL);
    }
    if (g_server.gtpu_thread != 0) {
        pthread_join(g_server.gtpu_thread, NULL);
    }
#endif
    
    /* Close PCAP */
    mock_gnb_pcap_close();
    
    /* Cleanup mutexes */
#ifdef _WIN32
    if (g_server.ue_mutex != NULL) CloseHandle(g_server.ue_mutex);
    if (g_server.stats_mutex != NULL) CloseHandle(g_server.stats_mutex);
#else
    pthread_mutex_destroy(&g_server.ue_mutex);
    pthread_mutex_destroy(&g_server.stats_mutex);
#endif
    
    /* Cleanup Winsock */
#ifdef _WIN32
    WSACleanup();
#endif
    
    printf("[Mock gNB] Server stopped\n");
}

mock_gnb_error_t mock_gnb_server_get_stats(mock_gnb_stats_t* stats) {
    if (stats == NULL) {
        return MOCK_GNB_ERROR_INVALID_PARAM;
    }
    
#ifdef _WIN32
    WaitForSingleObject(g_server.stats_mutex, INFINITE);
#else
    pthread_mutex_lock(&g_server.stats_mutex);
#endif
    memcpy(stats, &g_server.stats, sizeof(mock_gnb_stats_t));
#ifdef _WIN32
    ReleaseMutex(g_server.stats_mutex);
#else
    pthread_mutex_unlock(&g_server.stats_mutex);
#endif
    
    return MOCK_GNB_SUCCESS;
}

bool mock_gnb_server_is_running(void) {
#ifdef _WIN32
    return InterlockedCompareExchange(&g_server.running, 0, 0) != 0;
#else
    return atomic_load(&g_server.running);
#endif
}
