/*
 * 5G UE Simulation Application
 * Mock SMF (Session Management Function)
 * 3GPP TS 24.501 (NAS), TS 29.244 (PFCP), TS 29.281 (GTP-U)
 */

#include "mock_core.h"
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

/* ============== PFCP Constants ============== */

#define PFCP_PORT               8805
#define PFCP_VERSION            1
#define PFCP_MAX_MESSAGE_SIZE   65535

/* PFCP Message Types */
#define PFCP_MSG_HEARTBEAT_REQUEST              1
#define PFCP_MSG_HEARTBEAT_RESPONSE             2
#define PFCP_MSG_PFD_MANAGEMENT_REQUEST         3
#define PFCP_MSG_PFD_MANAGEMENT_RESPONSE        4
#define PFCP_MSG_ASSOCIATION_SETUP_REQUEST      5
#define PFCP_MSG_ASSOCIATION_SETUP_RESPONSE     6
#define PFCP_MSG_ASSOCIATION_UPDATE_REQUEST     7
#define PFCP_MSG_ASSOCIATION_UPDATE_RESPONSE    8
#define PFCP_MSG_ASSOCIATION_RELEASE_REQUEST    9
#define PFCP_MSG_ASSOCIATION_RELEASE_RESPONSE   10
#define PFCP_MSG_SESSION_ESTABLISHMENT_REQUEST  50
#define PFCP_MSG_SESSION_ESTABLISHMENT_RESPONSE 51
#define PFCP_MSG_SESSION_MODIFICATION_REQUEST   52
#define PFCP_MSG_SESSION_MODIFICATION_RESPONSE  53
#define PFCP_MSG_SESSION_DELETION_REQUEST       54
#define PFCP_MSG_SESSION_DELETION_RESPONSE      55

/* PFCP IE Types */
#define PFCP_IE_CREATE_PDR                      1
#define PFCP_IE_PDI                             2
#define PFCP_IE_CREATE_FAR                      3
#define PFCP_IE_FORWARDING_PARAMETERS           4
#define PFCP_IE_UE_IP_ADDRESS                   93
#define PFCP_IE_F_SEID                          57
#define PFCP_IE_NODE_ID                        60

/* ============== SMF Server Context ============== */

struct smf_server_s {
    smf_config_t config;
    
    /* PFCP Socket */
    int pfcp_socket;
    struct sockaddr_in pfcp_addr;
    
    /* UPF Connection */
    struct sockaddr_in upf_addr;
    bool upf_associated;
    uint64_t upf_f_seid;
    
    /* Sessions */
    smf_pdu_session_t sessions[MOCK_CORE_MAX_UES * MOCK_CORE_MAX_SESSIONS];
    uint32_t num_sessions;
    uint64_t sessions_created;
    uint64_t sessions_released;
    
    /* State */
    pthread_mutex_t session_mutex;
    pthread_t pfcp_thread;
#ifdef _WIN32
    volatile LONG running;
#else
    atomic_bool running;
#endif
    
    /* Statistics */
    uint64_t pfcp_messages_tx;
    uint64_t pfcp_messages_rx;
    uint64_t pfcp_sessions_created;
    uint64_t pfcp_sessions_deleted;
};

/* ============== PFCP Message Structures ============== */

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

/* ============== PFCP Encoding ============== */

static size_t pfcp_encode_node_id(uint8_t* buf, const char* ip) {
    size_t len = 0;
    buf[len++] = PFCP_IE_NODE_ID >> 8;
    buf[len++] = PFCP_IE_NODE_ID & 0xFF;
    buf[len++] = 0; /* Length high */
    buf[len++] = 7; /* Length low: 1 (type) + 4 (IPv4) */
    buf[len++] = 0; /* Node ID type: IPv4 */
    struct in_addr addr;
    inet_pton(AF_INET, ip, &addr);
    memcpy(&buf[len], &addr, 4);
    len += 4;
    return len;
}

static size_t pfcp_encode_f_seid(uint8_t* buf, uint64_t seid, uint32_t ip) {
    size_t len = 0;
    buf[len++] = PFCP_IE_F_SEID >> 8;
    buf[len++] = PFCP_IE_F_SEID & 0xFF;
    buf[len++] = 0; /* Length high */
    buf[len++] = 13; /* Length low */
    buf[len++] = 0x02; /* V4 bit set */
    memcpy(&buf[len], &seid, 8);
    len += 8;
    memcpy(&buf[len], &ip, 4);
    len += 4;
    return len;
}

static size_t pfcp_encode_ue_ip_address(uint8_t* buf, uint32_t ue_ip) {
    size_t len = 0;
    buf[len++] = PFCP_IE_UE_IP_ADDRESS >> 8;
    buf[len++] = PFCP_IE_UE_IP_ADDRESS & 0xFF;
    buf[len++] = 0; /* Length high */
    buf[len++] = 5; /* Length low */
    buf[len++] = 0x02; /* V4 bit set */
    memcpy(&buf[len], &ue_ip, 4);
    len += 4;
    return len;
}

static int pfcp_send_association_setup_request(smf_server_t* smf) {
    uint8_t buffer[PFCP_MAX_MESSAGE_SIZE];
    size_t len = 0;
    
    /* Header */
    pfcp_header_t* hdr = (pfcp_header_t*)buffer;
    memset(hdr, 0, sizeof(pfcp_header_t));
    hdr->version = PFCP_VERSION;
    hdr->message_type = htons(PFCP_MSG_ASSOCIATION_SETUP_REQUEST);
    hdr->seid = 0; /* No SEID for association */
    hdr->sequence_number = htonl(1);
    
    len = sizeof(pfcp_header_t);
    
    /* Node ID IE */
    len += pfcp_encode_node_id(&buffer[len], smf->config.bind_ip);
    
    /* Recovery Time Stamp (simplified) */
    buffer[len++] = 0x00; /* IE Type high */
    buffer[len++] = 0x90; /* IE Type low: Recovery Time Stamp */
    buffer[len++] = 0x00; /* Length high */
    buffer[len++] = 0x04; /* Length low */
    uint32_t ts = htonl((uint32_t)time(NULL));
    memcpy(&buffer[len], &ts, 4);
    len += 4;
    
    /* UP Function Features (simplified) */
    buffer[len++] = 0x00;
    buffer[len++] = 0x5B; /* UP Function Features */
    buffer[len++] = 0x00;
    buffer[len++] = 0x02;
    buffer[len++] = 0x00;
    buffer[len++] = 0x00;
    len += 2;
    
    /* Update length */
    hdr->message_length = htons((uint16_t)(len - 4));
    
    /* Send */
    ssize_t sent = sendto(smf->pfcp_socket, (char*)buffer, len, 0,
                          (struct sockaddr*)&smf->upf_addr, sizeof(smf->upf_addr));
    
    if (sent > 0) {
        smf->pfcp_messages_tx++;
        if (smf->config.log_messages) {
            printf("[SMF] Sent PFCP Association Setup Request to UPF\n");
        }
        return 0;
    }
    
    return -1;
}

static int pfcp_send_session_establishment_request(smf_server_t* smf,
                                                    smf_pdu_session_t* session) {
    uint8_t buffer[PFCP_MAX_MESSAGE_SIZE];
    size_t len = 0;
    
    /* Header */
    pfcp_header_t* hdr = (pfcp_header_t*)buffer;
    memset(hdr, 0, sizeof(pfcp_header_t));
    hdr->version = PFCP_VERSION;
    hdr->message_type = htons(PFCP_MSG_SESSION_ESTABLISHMENT_REQUEST);
    hdr->seid = 0; /* No SEID yet */
    static uint32_t seq = 1;
    hdr->sequence_number = htonl(seq++);
    
    len = sizeof(pfcp_header_t);
    
    /* Node ID IE */
    len += pfcp_encode_node_id(&buffer[len], smf->config.bind_ip);
    
    /* F-SEID (SMF) */
    uint64_t smf_seid = ((uint64_t)session->pdu_session_id << 32) | session->amf_ue_ngap_id;
    len += pfcp_encode_f_seid(&buffer[len], smf_seid, inet_addr(smf->config.bind_ip));
    
    /* Create PDR (Packet Detection Rule) - simplified */
    buffer[len++] = PFCP_IE_CREATE_PDR >> 8;
    buffer[len++] = PFCP_IE_CREATE_PDR & 0xFF;
    buffer[len++] = 0;
    buffer[len++] = 14; /* Length */
    buffer[len++] = 0x00; /* PDR ID high */
    buffer[len++] = 0x01; /* PDR ID low */
    buffer[len++] = 0x02; /* Precedence */
    buffer[len++] = PFCP_IE_PDI >> 8;
    buffer[len++] = PFCP_IE_PDI & 0xFF;
    buffer[len++] = 0;
    buffer[len++] = 7;
    buffer[len++] = 0x02; /* Source Interface: Access */
    len += pfcp_encode_ue_ip_address(&buffer[len], session->ue_ip_addr);
    
    /* Create FAR (Forwarding Action Rule) - simplified */
    buffer[len++] = PFCP_IE_CREATE_FAR >> 8;
    buffer[len++] = PFCP_IE_CREATE_FAR & 0xFF;
    buffer[len++] = 0;
    buffer[len++] = 6;
    buffer[len++] = 0x00; /* FAR ID */
    buffer[len++] = 0x01;
    buffer[len++] = 0x02; /* Apply Action: FORW */
    len += 3;
    
    /* Update length */
    hdr->message_length = htons((uint16_t)(len - 4));
    
    /* Store SEID for this session */
    session->upf_dl_teid = (uint32_t)smf_seid;
    
    /* Send */
    ssize_t sent = sendto(smf->pfcp_socket, (char*)buffer, len, 0,
                          (struct sockaddr*)&smf->upf_addr, sizeof(smf->upf_addr));
    
    if (sent > 0) {
        smf->pfcp_messages_tx++;
        smf->pfcp_sessions_created++;
        if (smf->config.log_messages) {
            printf("[SMF] Sent PFCP Session Establishment Request for UE %llu\n",
                   (unsigned long long)session->amf_ue_ngap_id);
        }
        return 0;
    }
    
    return -1;
}

static int pfcp_send_session_deletion_request(smf_server_t* smf,
                                               smf_pdu_session_t* session) {
    uint8_t buffer[PFCP_MAX_MESSAGE_SIZE];
    size_t len = 0;
    
    /* Header */
    pfcp_header_t* hdr = (pfcp_header_t*)buffer;
    memset(hdr, 0, sizeof(pfcp_header_t));
    hdr->version = PFCP_VERSION;
    hdr->message_type = htons(PFCP_MSG_SESSION_DELETION_REQUEST);
    uint64_t seid = ((uint64_t)session->pdu_session_id << 32) | session->amf_ue_ngap_id;
    hdr->seid = htobe64(seid);
    static uint32_t seq = 100;
    hdr->sequence_number = htonl(seq++);
    
    len = sizeof(pfcp_header_t);
    
    /* Update length */
    hdr->message_length = htons((uint16_t)(len - 4));
    
    /* Send */
    ssize_t sent = sendto(smf->pfcp_socket, (char*)buffer, len, 0,
                          (struct sockaddr*)&smf->upf_addr, sizeof(smf->upf_addr));
    
    if (sent > 0) {
        smf->pfcp_messages_tx++;
        smf->pfcp_sessions_deleted++;
        if (smf->config.log_messages) {
            printf("[SMF] Sent PFCP Session Deletion Request for UE %llu\n",
                   (unsigned long long)session->amf_ue_ngap_id);
        }
        return 0;
    }
    
    return -1;
}

/* ============== Utility Functions ============== */

void smf_get_default_config(smf_config_t* config) {
    if (!config) return;
    memset(config, 0, sizeof(smf_config_t));
    strncpy(config->bind_ip, "0.0.0.0", sizeof(config->bind_ip) - 1);
    config->pfcp_port = PFCP_PORT;
    config->smf_id = 1;
    strncpy(config->smf_name, "MockSMF-1", sizeof(config->smf_name) - 1);
    strncpy(config->upf_ip, "127.0.0.1", sizeof(config->upf_ip) - 1);
    config->default_sst = 1;
    config->default_sd = 0;
    /* IP Pool: 10.0.0.0/24 */
    config->ue_ip_pool_start = 0x0A000001; /* 10.0.0.1 */
    config->ue_ip_pool_end = 0x0A0000FE;   /* 10.0.0.254 */
    config->next_ue_ip = config->ue_ip_pool_start;
    config->auto_respond = true;
    config->log_messages = true;
}

/* ============== SMF Server Lifecycle ============== */

smf_server_t* smf_create(const smf_config_t* config) {
    smf_server_t* smf = (smf_server_t*)uesim_calloc(1, sizeof(smf_server_t));
    if (!smf) return NULL;
    
    if (config) {
        memcpy(&smf->config, config, sizeof(smf_config_t));
    } else {
        smf_get_default_config(&smf->config);
    }
    
    smf->num_sessions = 0;
    smf->sessions_created = 0;
    smf->sessions_released = 0;
    
    if (pthread_mutex_init(&smf->session_mutex, NULL) != 0) {
        uesim_free(smf);
        return NULL;
    }
    
    return smf;
}

void smf_destroy(smf_server_t* smf) {
    if (!smf) return;
    
    pthread_mutex_destroy(&smf->session_mutex);
    uesim_free(smf);
}

/* ============== Session Management ============== */

smf_pdu_session_t* smf_create_session(smf_server_t* smf, uint64_t amf_ue_id,
                                       uint8_t pdu_session_id, uint8_t sst, uint32_t sd) {
    if (!smf) return NULL;
    
    pthread_mutex_lock(&smf->session_mutex);
    
    /* Find free slot */
    int slot = -1;
    for (uint32_t i = 0; i < MOCK_CORE_MAX_UES * MOCK_CORE_MAX_SESSIONS; i++) {
        if (!smf->sessions[i].active) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        pthread_mutex_unlock(&smf->session_mutex);
        return NULL;
    }
    
    smf_pdu_session_t* session = &smf->sessions[slot];
    memset(session, 0, sizeof(smf_pdu_session_t));
    
    session->pdu_session_id = pdu_session_id;
    session->session_state = 1; /* Active */
    session->amf_ue_ngap_id = amf_ue_id;
    session->sst = sst;
    session->sd = sd;
    
    /* Allocate UE IP from pool */
    session->ue_ip_addr = smf->config.next_ue_ip;
    smf->config.next_ue_ip++;
    if (smf->config.next_ue_ip > smf->config.ue_ip_pool_end) {
        smf->config.next_ue_ip = smf->config.ue_ip_pool_start;
    }
    
    /* Default QoS */
    session->default_qfi = 1;
    session->five_qi = 9; /* Default QoS */
    
    /* Allocate UPF TEID (mock) */
    session->upf_dl_teid = (uint32_t)amf_ue_id * 1000 + pdu_session_id;
    session->upf_ip = 0x7F000001; /* 127.0.0.1 */
    
    session->establish_time = time(NULL);
    session->active = true;
    
    smf->num_sessions++;
    smf->sessions_created++;
    
    pthread_mutex_unlock(&smf->session_mutex);
    return session;
}

void smf_release_session(smf_server_t* smf, uint8_t pdu_session_id, uint64_t amf_ue_id) {
    if (!smf) return;
    
    pthread_mutex_lock(&smf->session_mutex);
    
    for (uint32_t i = 0; i < MOCK_CORE_MAX_UES * MOCK_CORE_MAX_SESSIONS; i++) {
        if (smf->sessions[i].active &&
            smf->sessions[i].pdu_session_id == pdu_session_id &&
            smf->sessions[i].amf_ue_ngap_id == amf_ue_id) {
            
            smf->sessions[i].active = false;
            smf->sessions[i].session_state = 0;
            smf->num_sessions--;
            smf->sessions_released++;
            break;
        }
    }
    
    pthread_mutex_unlock(&smf->session_mutex);
}

smf_pdu_session_t* smf_find_session(smf_server_t* smf, uint8_t pdu_session_id, uint64_t amf_ue_id) {
    if (!smf) return NULL;
    
    pthread_mutex_lock(&smf->session_mutex);
    
    for (uint32_t i = 0; i < MOCK_CORE_MAX_UES * MOCK_CORE_MAX_SESSIONS; i++) {
        if (smf->sessions[i].active &&
            smf->sessions[i].pdu_session_id == pdu_session_id &&
            smf->sessions[i].amf_ue_ngap_id == amf_ue_id) {
            pthread_mutex_unlock(&smf->session_mutex);
            return &smf->sessions[i];
        }
    }
    
    pthread_mutex_unlock(&smf->session_mutex);
    return NULL;
}