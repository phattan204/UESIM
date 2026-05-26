/*
 * 5G UE Simulation Application
 * Mock UPF (User Plane Function)
 * 3GPP TS 29.281 (GTP-U), TS 29.244 (PFCP)
 */

#include "mock_core.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

/* ============== Forward Declarations ============== */

static void* gtpu_listener_thread(void* arg);
static void* pfcp_listener_thread(void* arg);
static mock_core_error_t process_gtpu_packet(upf_server_t* upf, const uint8_t* data,
                                              size_t len, const struct sockaddr_in* src_addr);
static mock_core_error_t process_pfcp_message(upf_server_t* upf, const uint8_t* data,
                                               size_t len, const struct sockaddr_in* src_addr);

/* ============== GTP-U Constants ============== */

#define GTPU_VERSION            1
#define GTPU_PORT               2152
#define GTPU_HEADER_SIZE        8
#define GTPU_FLAG_PT            0x10
#define GTPU_FLAG_E             0x04
#define GTPU_FLAG_S             0x02
#define GTPU_FLAG_PN            0x01

/* GTP-U Message Types */
#define GTPU_MSG_ECHO_REQUEST   1
#define GTPU_MSG_ECHO_RESPONSE  2
#define GTPU_MSG_ERROR_IND      26
#define GTPU_MSG_END_MARKER     254
#define GTPU_MSG_G_PDU          255

/* ============== PFCP Constants ============== */

#define PFCP_PORT               8805
#define PFCP_VERSION            1
#define PFCP_HEADER_SIZE        16
#define PFCP_MAX_MESSAGE_SIZE   65535

/* PFCP Message Types */
#define PFCP_MSG_HEARTBEAT_REQUEST              1
#define PFCP_MSG_HEARTBEAT_RESPONSE             2
#define PFCP_MSG_ASSOCIATION_SETUP_REQUEST      5
#define PFCP_MSG_ASSOCIATION_SETUP_RESPONSE     6
#define PFCP_MSG_SESSION_ESTABLISHMENT_REQUEST  50
#define PFCP_MSG_SESSION_ESTABLISHMENT_RESPONSE 51
#define PFCP_MSG_SESSION_DELETION_REQUEST       54
#define PFCP_MSG_SESSION_DELETION_RESPONSE      55

/* PFCP Cause Values */
#define PFCP_CAUSE_SUCCESS              1
#define PFCP_CAUSE_REQUEST_REJECTED     64

/* ============== Utility Functions ============== */

void upf_get_default_config(upf_config_t* config) {
    if (!config) return;
    memset(config, 0, sizeof(upf_config_t));
    strncpy(config->bind_ip, "0.0.0.0", sizeof(config->bind_ip) - 1);
    config->gtpu_port = MOCK_CORE_GTPU_PORT;
    config->num_tunnels = 0;
    config->log_packets = true;
    config->forward_data = false; /* Don't forward in mock mode */
}

/* ============== UPF Server Lifecycle ============== */

upf_server_t* upf_create(const upf_config_t* config) {
    upf_server_t* upf = (upf_server_t*)uesim_calloc(1, sizeof(upf_server_t));
    if (!upf) return NULL;
    
    if (config) {
        memcpy(&upf->config, config, sizeof(upf_config_t));
    } else {
        upf_get_default_config(&upf->config);
    }
    
    upf->gtpu_socket = -1;
    atomic_store(&upf->running, 0);
    upf->packets_rx = 0;
    upf->packets_tx = 0;
    upf->bytes_rx = 0;
    upf->bytes_tx = 0;
    
    if (pthread_mutex_init(&upf->tunnel_mutex, NULL) != 0) {
        uesim_free(upf);
        return NULL;
    }
    
    return upf;
}

void upf_destroy(upf_server_t* upf) {
    if (!upf) return;
    
    upf_stop(upf);
    pthread_mutex_destroy(&upf->tunnel_mutex);
    uesim_free(upf);
}

mock_core_error_t upf_start(upf_server_t* upf) {
    if (!upf) return MOCK_CORE_ERROR_INVALID_PARAM;
    
    struct sockaddr_in addr;
    
    /* Create UDP socket for GTP-U */
    upf->gtpu_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (upf->gtpu_socket < 0) {
        return MOCK_CORE_ERROR_SOCKET;
    }
    
    /* Set socket options */
    int opt = 1;
    setsockopt(upf->gtpu_socket, SOL_SOCKET, SO_REUSEADDR,
               (const char*)&opt, sizeof(opt));
    
    /* Bind */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(upf->config.gtpu_port);
    inet_pton(AF_INET, upf->config.bind_ip, &addr.sin_addr);
    
    if (bind(upf->gtpu_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        uesim_sock_close(upf->gtpu_socket);
        upf->gtpu_socket = -1;
        return MOCK_CORE_ERROR_SOCKET;
    }
    
    /* Start GTP-U listener thread */
    atomic_store(&upf->running, 1);
    if (pthread_create(&upf->gtpu_thread, NULL, gtpu_listener_thread, upf) != 0) {
        atomic_store(&upf->running, 0);
        uesim_sock_close(upf->gtpu_socket);
        upf->gtpu_socket = -1;
        return MOCK_CORE_ERROR_THREAD;
    }
    
    return MOCK_CORE_SUCCESS;
}

void upf_stop(upf_server_t* upf) {
    if (!upf) return;
    
    atomic_store(&upf->running, 0);
    
    if (upf->gtpu_socket >= 0) {
        uesim_sock_close(upf->gtpu_socket);
        upf->gtpu_socket = -1;
    }
    
#ifdef _WIN32
    WaitForSingleObject(upf->gtpu_thread, 5000);
#else
    pthread_join(upf->gtpu_thread, NULL);
#endif
}

/* ============== GTP-U Listener Thread ============== */

static void* gtpu_listener_thread(void* arg) {
    upf_server_t* upf = (upf_server_t*)arg;
    uint8_t buffer[MOCK_CORE_BUFFER_SIZE];
    struct sockaddr_in src_addr;
    socklen_t addr_len;
    ssize_t bytes_recv;
    
    while (atomic_load(&upf->running)) {
        addr_len = sizeof(src_addr);
        bytes_recv = recvfrom(upf->gtpu_socket, (char*)buffer, sizeof(buffer), 0,
                               (struct sockaddr*)&src_addr, &addr_len);
        
        if (bytes_recv > 0) {
            upf->packets_rx++;
            upf->bytes_rx += (uint64_t)bytes_recv;
            process_gtpu_packet(upf, buffer, (size_t)bytes_recv, &src_addr);
        }
    }
    
    return NULL;
}

/* ============== GTP-U Packet Processing ============== */

static mock_core_error_t process_gtpu_packet(upf_server_t* upf, const uint8_t* data,
                                              size_t len, const struct sockaddr_in* src_addr) {
    if (len < GTPU_HEADER_SIZE) {
        return MOCK_CORE_ERROR_PROTOCOL;
    }
    
    /* Parse GTP-U header */
    uint8_t flags = data[0];
    uint8_t message_type = data[1];
    uint16_t length = ((uint16_t)data[2] << 8) | data[3];
    uint32_t teid = ((uint32_t)data[4] << 24) | ((uint32_t)data[5] << 16) |
                    ((uint32_t)data[6] << 8) | data[7];
    
    (void)flags;
    (void)length;
    
    if (upf->config.log_packets) {
        printf("[UPF] RX GTP-U: type=%u, teid=%u, len=%zu from %s:%u\n",
               message_type, teid, len,
               inet_ntoa(src_addr->sin_addr), ntohs(src_addr->sin_port));
    }
    
    switch (message_type) {
        case GTPU_MSG_ECHO_REQUEST:
            /* Send Echo Response */
            {
                uint8_t resp[8];
                resp[0] = GTPU_FLAG_PT | GTPU_VERSION;
                resp[1] = GTPU_MSG_ECHO_RESPONSE;
                resp[2] = 0;
                resp[3] = 0;
                resp[4] = 0;
                resp[5] = 0;
                resp[6] = 0;
                resp[7] = 0;
                
                sendto(upf->gtpu_socket, (const char*)resp, sizeof(resp), 0,
                       (const struct sockaddr*)src_addr, sizeof(*src_addr));
                upf->packets_tx++;
            }
            break;
            
        case GTPU_MSG_G_PDU:
            /* User data packet */
            {
                upf_tunnel_t* tunnel = upf_find_tunnel(upf, teid);
                if (tunnel) {
                    tunnel->rx_packets++;
                    tunnel->rx_bytes += len;
                    
                    if (upf->config.forward_data) {
                        /* Forward to peer (not implemented in mock) */
                    }
                } else {
                    if (upf->config.log_packets) {
                        printf("[UPF] Unknown TEID: %u\n", teid);
                    }
                }
            }
            break;
            
        case GTPU_MSG_ERROR_IND:
            if (upf->config.log_packets) {
                printf("[UPF] Error Indication received\n");
            }
            break;
            
        case GTPU_MSG_END_MARKER:
            if (upf->config.log_packets) {
                printf("[UPF] End Marker received\n");
            }
            break;
            
        default:
            if (upf->config.log_packets) {
                printf("[UPF] Unknown GTP-U message type: %u\n", message_type);
            }
            break;
    }
    
    return MOCK_CORE_SUCCESS;
}

/* ============== Tunnel Management ============== */

upf_tunnel_t* upf_create_tunnel(upf_server_t* upf, uint32_t teid, uint32_t ue_ip,
                                 uint32_t peer_ip, uint16_t peer_port, bool uplink) {
    if (!upf) return NULL;
    
    pthread_mutex_lock(&upf->tunnel_mutex);
    
    /* Find free slot */
    int slot = -1;
    for (uint32_t i = 0; i < MOCK_CORE_MAX_UES * MOCK_CORE_MAX_SESSIONS; i++) {
        if (upf->config.tunnels[i].teid == 0) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        pthread_mutex_unlock(&upf->tunnel_mutex);
        return NULL;
    }
    
    upf_tunnel_t* tunnel = &upf->config.tunnels[slot];
    memset(tunnel, 0, sizeof(upf_tunnel_t));
    
    tunnel->teid = teid;
    tunnel->ue_ip_addr = ue_ip;
    tunnel->peer_ip = peer_ip;
    tunnel->peer_port = peer_port;
    tunnel->uplink = uplink;
    tunnel->create_time = time(NULL);
    tunnel->rx_packets = 0;
    tunnel->tx_packets = 0;
    tunnel->rx_bytes = 0;
    tunnel->tx_bytes = 0;
    
    upf->config.num_tunnels++;
    
    pthread_mutex_unlock(&upf->tunnel_mutex);
    return tunnel;
}

void upf_remove_tunnel(upf_server_t* upf, uint32_t teid) {
    if (!upf) return;
    
    pthread_mutex_lock(&upf->tunnel_mutex);
    
    for (uint32_t i = 0; i < MOCK_CORE_MAX_UES * MOCK_CORE_MAX_SESSIONS; i++) {
        if (upf->config.tunnels[i].teid == teid) {
            memset(&upf->config.tunnels[i], 0, sizeof(upf_tunnel_t));
            upf->config.num_tunnels--;
            break;
        }
    }
    
    pthread_mutex_unlock(&upf->tunnel_mutex);
}

upf_tunnel_t* upf_find_tunnel(upf_server_t* upf, uint32_t teid) {
    if (!upf) return NULL;
    
    pthread_mutex_lock(&upf->tunnel_mutex);
    
    for (uint32_t i = 0; i < MOCK_CORE_MAX_UES * MOCK_CORE_MAX_SESSIONS; i++) {
        if (upf->config.tunnels[i].teid == teid) {
            pthread_mutex_unlock(&upf->tunnel_mutex);
            return &upf->config.tunnels[i];
        }
    }
    
    pthread_mutex_unlock(&upf->tunnel_mutex);
    return NULL;
}

/* ============== GTP-U Packet Handling ============== */

mock_core_error_t upf_handle_gtpu_packet(upf_server_t* upf, const uint8_t* data, size_t len,
                                          const struct sockaddr_in* src_addr) {
    return process_gtpu_packet(upf, data, len, src_addr);
}

/* ============== PFCP Listener Thread ============== */

static void* pfcp_listener_thread(void* arg) {
    upf_server_t* upf = (upf_server_t*)arg;
    uint8_t buffer[PFCP_MAX_MESSAGE_SIZE];
    struct sockaddr_in src_addr;
    socklen_t addr_len;
    ssize_t bytes_recv;
    
    printf("[UPF] PFCP listener thread started on port %u\n", PFCP_PORT);
    
    while (atomic_load(&upf->running)) {
        addr_len = sizeof(src_addr);
        bytes_recv = recvfrom(upf->pfcp_socket, (char*)buffer, sizeof(buffer), 0,
                               (struct sockaddr*)&src_addr, &addr_len);
        
        if (bytes_recv > 0) {
            upf->pfcp_messages_rx++;
            process_pfcp_message(upf, buffer, (size_t)bytes_recv, &src_addr);
        }
    }
    
    printf("[UPF] PFCP listener thread stopped\n");
    return NULL;
}

/* ============== PFCP Message Processing ============== */

typedef struct __attribute__((packed)) {
    uint8_t version;
    uint8_t spare_octet;
    uint16_t message_type;
    uint16_t message_length;
    uint64_t seid;
    uint8_t spare_octet2;
    uint8_t message_priority;
    uint32_t sequence_number;
} pfcp_header_t;

static int pfcp_send_response(upf_server_t* upf, uint16_t msg_type, uint64_t seid,
                               uint32_t seq_num, const struct sockaddr_in* dst_addr) {
    uint8_t buffer[PFCP_MAX_MESSAGE_SIZE];
    size_t len = 0;
    
    pfcp_header_t* hdr = (pfcp_header_t*)buffer;
    memset(hdr, 0, sizeof(pfcp_header_t));
    hdr->version = PFCP_VERSION;
    hdr->message_type = htons(msg_type);
    hdr->seid = htobe64(seid);
    hdr->sequence_number = htonl(seq_num);
    
    len = sizeof(pfcp_header_t);
    
    /* Add Cause IE */
    buffer[len++] = 0x00;
    buffer[len++] = 0x13; /* Cause IE type */
    buffer[len++] = 0x00;
    buffer[len++] = 0x01; /* Length */
    buffer[len++] = PFCP_CAUSE_SUCCESS;
    
    /* Add Node ID IE */
    buffer[len++] = 0x00;
    buffer[len++] = 0x3C; /* Node ID IE type */
    buffer[len++] = 0x00;
    buffer[len++] = 0x07; /* Length */
    buffer[len++] = 0x00; /* IPv4 type */
    uint32_t ip = inet_addr(upf->config.bind_ip);
    memcpy(&buffer[len], &ip, 4);
    len += 4;
    
    hdr->message_length = htons((uint16_t)(len - 4));
    
    ssize_t sent = sendto(upf->pfcp_socket, (char*)buffer, len, 0,
                          (const struct sockaddr*)dst_addr, sizeof(*dst_addr));
    
    if (sent > 0) {
        upf->pfcp_messages_tx++;
        return 0;
    }
    
    return -1;
}

static mock_core_error_t process_pfcp_message(upf_server_t* upf, const uint8_t* data,
                                               size_t len, const struct sockaddr_in* src_addr) {
    if (len < PFCP_HEADER_SIZE) {
        return MOCK_CORE_ERROR_PROTOCOL;
    }
    
    const pfcp_header_t* hdr = (const pfcp_header_t*)data;
    uint16_t msg_type = ntohs(hdr->message_type);
    uint32_t seq_num = ntohl(hdr->sequence_number);
    uint64_t seid = be64toh(hdr->seid);
    
    if (upf->config.log_packets) {
        printf("[UPF] RX PFCP: type=%u, seid=%llu, seq=%u from %s:%u\n",
               msg_type, (unsigned long long)seid, seq_num,
               inet_ntoa(src_addr->sin_addr), ntohs(src_addr->sin_port));
    }
    
    switch (msg_type) {
        case PFCP_MSG_HEARTBEAT_REQUEST:
            pfcp_send_response(upf, PFCP_MSG_HEARTBEAT_RESPONSE, 0, seq_num, src_addr);
            break;
            
        case PFCP_MSG_ASSOCIATION_SETUP_REQUEST:
            upf->smf_addr = *src_addr;
            upf->smf_associated = true;
            pfcp_send_response(upf, PFCP_MSG_ASSOCIATION_SETUP_RESPONSE, 0, seq_num, src_addr);
            if (upf->config.log_packets) {
                printf("[UPF] PFCP Association established with SMF\n");
            }
            break;
            
        case PFCP_MSG_SESSION_ESTABLISHMENT_REQUEST: {
            /* Parse F-SEID and create tunnel */
            uint32_t upf_teid = (uint32_t)rand() + 1;
            upf_tunnel_t* tunnel = upf_create_tunnel(upf, upf_teid, 0,
                                                      src_addr->sin_addr.s_addr, 
                                                      upf->config.gtpu_port, false);
            if (tunnel) {
                tunnel->pfcp_seid = seid;
                tunnel->smf_addr = *src_addr;
                upf->pfcp_sessions_created++;
                
                /* Send response with allocated TEID */
                uint8_t resp[PFCP_MAX_MESSAGE_SIZE];
                size_t resp_len = sizeof(pfcp_header_t);
                
                pfcp_header_t* resp_hdr = (pfcp_header_t*)resp;
                memset(resp_hdr, 0, sizeof(pfcp_header_t));
                resp_hdr->version = PFCP_VERSION;
                resp_hdr->message_type = htons(PFCP_MSG_SESSION_ESTABLISHMENT_RESPONSE);
                resp_hdr->seid = htobe64(seid);
                resp_hdr->sequence_number = htonl(seq_num);
                
                /* Cause IE */
                resp[resp_len++] = 0x00;
                resp[resp_len++] = 0x13;
                resp[resp_len++] = 0x00;
                resp[resp_len++] = 0x01;
                resp[resp_len++] = PFCP_CAUSE_SUCCESS;
                
                /* F-SEID IE (UPF side) */
                resp[resp_len++] = 0x00;
                resp[resp_len++] = 0x39; /* F-SEID */
                resp[resp_len++] = 0x00;
                resp[resp_len++] = 0x0D;
                resp[resp_len++] = 0x02; /* V4 */
                uint64_t upf_seid = ((uint64_t)upf_teid << 32) | seid;
                memcpy(&resp[resp_len], &upf_seid, 8);
                resp_len += 8;
                uint32_t upf_ip = upf->config.tunnels[0].peer_ip; /* UPF IP address */
                memcpy(&resp[resp_len], &upf_ip, 4);
                resp_len += 4;
                
                resp_hdr->message_length = htons((uint16_t)(resp_len - 4));
                
                sendto(upf->pfcp_socket, (char*)resp, resp_len, 0,
                       (const struct sockaddr*)src_addr, sizeof(*src_addr));
                upf->pfcp_messages_tx++;
                
                if (upf->config.log_packets) {
                    printf("[UPF] PFCP Session created: TEID=%u\n", upf_teid);
                }
            }
            break;
        }
            
        case PFCP_MSG_SESSION_DELETION_REQUEST: {
            /* Find and remove tunnel by SEID */
            pthread_mutex_lock(&upf->tunnel_mutex);
            for (uint32_t i = 0; i < MOCK_CORE_MAX_UES * MOCK_CORE_MAX_SESSIONS; i++) {
                if (upf->config.tunnels[i].pfcp_seid == seid) {
                    upf->config.tunnels[i].teid = 0;
                    upf->config.tunnels[i].active = false;
                    upf->config.num_tunnels--;
                    upf->pfcp_sessions_deleted++;
                    break;
                }
            }
            pthread_mutex_unlock(&upf->tunnel_mutex);
            
            pfcp_send_response(upf, PFCP_MSG_SESSION_DELETION_RESPONSE, seid, seq_num, src_addr);
            if (upf->config.log_packets) {
                printf("[UPF] PFCP Session deleted: SEID=%llu\n", (unsigned long long)seid);
            }
            break;
        }
            
        default:
            if (upf->config.log_packets) {
                printf("[UPF] Unhandled PFCP message type: %u\n", msg_type);
            }
            break;
    }
    
    return MOCK_CORE_SUCCESS;
}

/* ============== Data Forwarding ============== */

mock_core_error_t upf_send_gtpu_data(upf_server_t* upf, uint32_t teid,
                                      const uint8_t* data, size_t len,
                                      const struct sockaddr_in* dst_addr) {
    if (!upf || !data || !dst_addr || len == 0) {
        return MOCK_CORE_ERROR_INVALID_PARAM;
    }
    
    uint8_t buffer[MOCK_CORE_BUFFER_SIZE];
    size_t total_len = GTPU_HEADER_SIZE + len;
    
    if (total_len > sizeof(buffer)) {
        return MOCK_CORE_ERROR_PROTOCOL;
    }
    
    /* Build GTP-U header */
    buffer[0] = GTPU_FLAG_PT | GTPU_VERSION;
    buffer[1] = GTPU_MSG_G_PDU;
    buffer[2] = (uint8_t)(len >> 8);
    buffer[3] = (uint8_t)(len & 0xFF);
    buffer[4] = (uint8_t)(teid >> 24);
    buffer[5] = (uint8_t)((teid >> 16) & 0xFF);
    buffer[6] = (uint8_t)((teid >> 8) & 0xFF);
    buffer[7] = (uint8_t)(teid & 0xFF);
    
    memcpy(&buffer[8], data, len);
    
    ssize_t sent = sendto(upf->gtpu_socket, (char*)buffer, total_len, 0,
                          (const struct sockaddr*)dst_addr, sizeof(*dst_addr));
    
    if (sent > 0) {
        upf->packets_tx++;
        upf->bytes_tx += (uint64_t)sent;
        
        upf_tunnel_t* tunnel = upf_find_tunnel(upf, teid);
        if (tunnel) {
            tunnel->tx_packets++;
            tunnel->tx_bytes += (uint64_t)sent;
        }
        
        return MOCK_CORE_SUCCESS;
    }
    
    return MOCK_CORE_ERROR_SOCKET;
}

mock_core_error_t upf_forward_data(upf_server_t* upf, uint32_t src_teid,
                                    uint32_t dst_teid, const uint8_t* data, size_t len) {
    if (!upf || !data || len == 0) {
        return MOCK_CORE_ERROR_INVALID_PARAM;
    }
    
    upf_tunnel_t* src_tunnel = upf_find_tunnel(upf, src_teid);
    upf_tunnel_t* dst_tunnel = upf_find_tunnel(upf, dst_teid);
    
    if (!src_tunnel || !dst_tunnel) {
        return MOCK_CORE_ERROR_NOT_FOUND;
    }
    
    struct sockaddr_in dst_addr;
    memset(&dst_addr, 0, sizeof(dst_addr));
    dst_addr.sin_family = AF_INET;
    dst_addr.sin_addr.s_addr = dst_tunnel->peer_ip;
    dst_addr.sin_port = htons(dst_tunnel->peer_port);
    
    return upf_send_gtpu_data(upf, dst_teid, data, len, &dst_addr);
}

/* ============== UPF Statistics ============== */

void upf_print_statistics(const upf_server_t* upf) {
    if (!upf) return;
    
    printf("\n[UPF] Statistics:\n");
    printf("  GTP-U Packets RX: %llu\n", (unsigned long long)upf->packets_rx);
    printf("  GTP-U Packets TX: %llu\n", (unsigned long long)upf->packets_tx);
    printf("  GTP-U Bytes RX: %llu\n", (unsigned long long)upf->bytes_rx);
    printf("  GTP-U Bytes TX: %llu\n", (unsigned long long)upf->bytes_tx);
    printf("  PFCP Messages RX: %llu\n", (unsigned long long)upf->pfcp_messages_rx);
    printf("  PFCP Messages TX: %llu\n", (unsigned long long)upf->pfcp_messages_tx);
    printf("  PFCP Sessions Created: %llu\n", (unsigned long long)upf->pfcp_sessions_created);
    printf("  PFCP Sessions Deleted: %llu\n", (unsigned long long)upf->pfcp_sessions_deleted);
    printf("  Active Tunnels: %u\n", upf->config.num_tunnels);
}
