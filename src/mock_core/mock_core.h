/*
 * 5G UE Simulation Application
 * Mock Core Network - AMF/SMF/UPF Simulation
 * 3GPP TS 38.413 (NGAP), TS 29.281 (GTP-U), TS 24.501 (NAS)
 */

#ifndef MOCK_CORE_H
#define MOCK_CORE_H

#include "../uesim.h"
#include "../protocol/ngap_messages.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef HANDLE pthread_t;
typedef HANDLE pthread_mutex_t;
typedef HANDLE pthread_cond_t;
#else
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

/* ============== Constants ============== */

#define MOCK_CORE_NGAP_PORT     38412
#define MOCK_CORE_GTPU_PORT     2152
#define MOCK_CORE_MAX_UES       1024
#define MOCK_CORE_MAX_SESSIONS  16
#define MOCK_CORE_BUFFER_SIZE   65536

/* ============== Error Codes ============== */

typedef enum {
    MOCK_CORE_SUCCESS = 0,
    MOCK_CORE_ERROR_INVALID_PARAM = -1,
    MOCK_CORE_ERROR_SOCKET = -2,
    MOCK_CORE_ERROR_MEMORY = -3,
    MOCK_CORE_ERROR_THREAD = -4,
    MOCK_CORE_ERROR_TIMEOUT = -5,
    MOCK_CORE_ERROR_PROTOCOL = -6,
    MOCK_CORE_ERROR_NOT_FOUND = -7,
    MOCK_CORE_ERROR_CAPACITY = -8
} mock_core_error_t;

/* ============== AMF States ============== */

typedef enum {
    AMF_UE_STATE_IDLE = 0,
    AMF_UE_STATE_REGISTERED,
    AMF_UE_STATE_CONNECTING,
    AMF_UE_STATE_DEREGISTERING,
    AMF_UE_STATE_MAX
} amf_ue_state_t;

/* ============== AMF UE Context ============== */

typedef struct {
    uint64_t amf_ue_ngap_id;
    uint32_t ran_ue_ngap_id;
    amf_ue_state_t state;
    
    /* UE Identity */
    char suci[64];
    char guti[64];
    char imsi[16];
    
    /* Security Context */
    uint8_t kamf[32];
    uint8_t knas_enc[16];
    uint8_t knas_int[16];
    uint8_t ciphering_alg;
    uint8_t integrity_alg;
    uint32_t ul_count;
    uint32_t dl_count;
    bool security_active;
    
    /* Authentication */
    uint8_t rand[16];
    uint8_t autn[16];
    uint8_t xres[16];
    bool authenticated;
    
    /* Connection Info */
    int ngap_socket;
    struct sockaddr_in gnb_addr;
    time_t connect_time;
    time_t last_activity;
    
    /* PDU Sessions */
    uint8_t pdu_session_ids[MOCK_CORE_MAX_SESSIONS];
    uint8_t num_active_sessions;
} amf_ue_context_t;

/* ============== SMF PDU Session Context ============== */

typedef struct {
    uint8_t pdu_session_id;
    uint8_t session_state;
    uint32_t ue_ip_addr;
    uint8_t sst;
    uint32_t sd;
    
    /* QoS */
    uint8_t default_qfi;
    uint8_t five_qi;
    
    /* Tunnel Info */
    uint32_t upf_dl_teid;
    uint32_t gnb_ul_teid;
    uint32_t upf_ip;
    uint32_t gnb_ip;
    
    /* AMF UE reference */
    uint64_t amf_ue_ngap_id;
    
    time_t establish_time;
    bool active;
} smf_pdu_session_t;

/* ============== UPF Tunnel Context ============== */

typedef struct {
    uint32_t teid;
    uint32_t ue_ip_addr;
    uint32_t peer_ip;
    uint16_t peer_port;
    uint8_t pdu_session_id;
    uint64_t amf_ue_ngap_id;
    bool uplink;
    time_t create_time;
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
} upf_tunnel_t;

/* ============== AMF Configuration ============== */

typedef struct {
    char bind_ip[46];
    uint16_t ngap_port;
    uint32_t max_ues;
    
    /* AMF Info */
    uint32_t amf_id;
    char amf_name[64];
    uint32_t amf_set_id;
    uint8_t amf_pointer;
    
    /* PLMN Info */
    uint8_t plmn_id[3];
    uint32_t tac;
    
    /* Behavior */
    bool auto_respond;
    uint32_t response_delay_ms;
    bool log_messages;
    
    /* PCAP */
    char pcap_file[256];
} amf_config_t;

/* ============== SMF Configuration ============== */

typedef struct {
    char bind_ip[46];
    uint32_t smf_id;
    char smf_name[64];
    
    /* Network Slicing */
    uint8_t default_sst;
    uint32_t default_sd;
    
    /* IP Pool */
    uint32_t ue_ip_pool_start;
    uint32_t ue_ip_pool_end;
    uint32_t next_ue_ip;
    
    /* Behavior */
    bool auto_respond;
    bool log_messages;
} smf_config_t;

/* ============== UPF Configuration ============== */

typedef struct {
    char bind_ip[46];
    uint16_t gtpu_port;
    
    /* Tunnel Table */
    upf_tunnel_t tunnels[MOCK_CORE_MAX_UES * MOCK_CORE_MAX_SESSIONS];
    uint32_t num_tunnels;
    
    /* Behavior */
    bool log_packets;
    bool forward_data;
    
    /* PCAP */
    char pcap_file[256];
} upf_config_t;

/* ============== AMF Server Context ============== */

typedef struct {
    amf_config_t config;
    
    /* Sockets */
    int ngap_socket;
    
    /* UE Contexts */
    amf_ue_context_t* ue_contexts[MOCK_CORE_MAX_UES];
    uint32_t num_active_ues;
    uint64_t next_amf_ue_id;
    
    /* Server State */
#ifdef _WIN32
    volatile LONG running;
#else
    atomic_bool running;
#endif
    pthread_t ngap_thread;
    pthread_mutex_t ue_mutex;
    
    /* Statistics */
    uint64_t ngap_messages_rx;
    uint64_t ngap_messages_tx;
    uint64_t registrations;
    uint64_t authentications;
} amf_server_t;

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
    
    /* PDU Sessions */
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

/* Forward declaration - defined above */
typedef struct smf_server_s smf_server_t;

/* ============== UPF Server Context ============== */

struct upf_server_s {
    upf_config_t config;
    
    /* Sockets */
    int gtpu_socket;
    int pfcp_socket;
    
    /* SMF Connection */
    struct sockaddr_in smf_addr;
    bool smf_associated;
    
    /* Server State */
#ifdef _WIN32
    volatile LONG running;
#else
    atomic_bool running;
#endif
    pthread_t gtpu_thread;
    pthread_t pfcp_thread;
    pthread_mutex_t tunnel_mutex;
    
    /* Statistics */
    uint64_t packets_rx;
    uint64_t packets_tx;
    uint64_t bytes_rx;
    uint64_t bytes_tx;
    uint64_t pfcp_messages_rx;
    uint64_t pfcp_messages_tx;
    uint64_t pfcp_sessions_created;
    uint64_t pfcp_sessions_deleted;
};

/* Forward declaration - defined above */
typedef struct upf_server_s upf_server_t;

/* ============== AMF API Functions ============== */

amf_server_t* amf_create(const amf_config_t* config);
void amf_destroy(amf_server_t* amf);
mock_core_error_t amf_start(amf_server_t* amf);
void amf_stop(amf_server_t* amf);

amf_ue_context_t* amf_find_ue_by_amf_id(amf_server_t* amf, uint64_t amf_ue_id);
amf_ue_context_t* amf_find_ue_by_ran_id(amf_server_t* amf, uint32_t ran_ue_id);
amf_ue_context_t* amf_create_ue_context(amf_server_t* amf, uint32_t ran_ue_id);
void amf_remove_ue_context(amf_server_t* amf, amf_ue_context_t* ue);

/* AMF Message Handlers */
mock_core_error_t amf_handle_ng_setup(amf_server_t* amf, const ngap_message_t* msg, 
                                       int socket, ngap_message_t* response);
mock_core_error_t amf_handle_initial_ue(amf_server_t* amf, const ngap_message_t* msg,
                                         int socket, ngap_message_t* response);
mock_core_error_t amf_handle_uplink_nas(amf_server_t* amf, const ngap_message_t* msg,
                                         int socket, ngap_message_t* response);

/* AMF NAS Message Handlers */
mock_core_error_t amf_handle_registration_request(amf_server_t* amf, amf_ue_context_t* ue,
                                                   const uint8_t* nas_pdu, size_t nas_len,
                                                   ngap_message_t* response);
mock_core_error_t amf_handle_authentication_response(amf_server_t* amf, amf_ue_context_t* ue,
                                                      const uint8_t* nas_pdu, size_t nas_len,
                                                      ngap_message_t* response);
mock_core_error_t amf_handle_security_mode_complete(amf_server_t* amf, amf_ue_context_t* ue,
                                                     const uint8_t* nas_pdu, size_t nas_len,
                                                     ngap_message_t* response);
mock_core_error_t amf_handle_registration_complete(amf_server_t* amf, amf_ue_context_t* ue,
                                                    const uint8_t* nas_pdu, size_t nas_len,
                                                    ngap_message_t* response);
mock_core_error_t amf_handle_pdu_session_request(amf_server_t* amf, amf_ue_context_t* ue,
                                                  const uint8_t* nas_pdu, size_t nas_len,
                                                  ngap_message_t* response);

/* ============== SMF API Functions ============== */

smf_server_t* smf_create(const smf_config_t* config);
void smf_destroy(smf_server_t* smf);

smf_pdu_session_t* smf_create_session(smf_server_t* smf, uint64_t amf_ue_id,
                                       uint8_t pdu_session_id, uint8_t sst, uint32_t sd);
void smf_release_session(smf_server_t* smf, uint8_t pdu_session_id, uint64_t amf_ue_id);
smf_pdu_session_t* smf_find_session(smf_server_t* smf, uint8_t pdu_session_id, uint64_t amf_ue_id);

/* ============== UPF API Functions ============== */

upf_server_t* upf_create(const upf_config_t* config);
void upf_destroy(upf_server_t* upf);
mock_core_error_t upf_start(upf_server_t* upf);
void upf_stop(upf_server_t* upf);

upf_tunnel_t* upf_create_tunnel(upf_server_t* upf, uint32_t teid, uint32_t ue_ip,
                                 uint32_t peer_ip, uint16_t peer_port, bool uplink);
void upf_remove_tunnel(upf_server_t* upf, uint32_t teid);
upf_tunnel_t* upf_find_tunnel(upf_server_t* upf, uint32_t teid);

/* GTP-U Handling */
mock_core_error_t upf_handle_gtpu_packet(upf_server_t* upf, const uint8_t* data, size_t len,
                                          const struct sockaddr_in* src_addr);

/* Data Forwarding */
mock_core_error_t upf_send_gtpu_data(upf_server_t* upf, uint32_t teid,
                                      const uint8_t* data, size_t len,
                                      const struct sockaddr_in* dst_addr);
mock_core_error_t upf_forward_data(upf_server_t* upf, uint32_t src_teid,
                                    uint32_t dst_teid, const uint8_t* data, size_t len);

/* UPF Statistics */
void upf_print_statistics(const upf_server_t* upf);

/* ============== SMF Extended API Functions ============== */

/* SMF Lifecycle */
mock_core_error_t smf_start(smf_server_t* smf);
void smf_stop(smf_server_t* smf);

/* SMF PFCP Interface */
mock_core_error_t smf_connect_upf(smf_server_t* smf, const char* upf_ip, uint16_t pfcp_port);
mock_core_error_t smf_send_pfcp_association_setup(smf_server_t* smf);
mock_core_error_t smf_send_session_establishment(smf_server_t* smf, smf_pdu_session_t* session);
mock_core_error_t smf_send_session_deletion(smf_server_t* smf, smf_pdu_session_t* session);

/* SMF Statistics */
void smf_print_statistics(const smf_server_t* smf);

/* ============== Utility Functions ============== */

const char* amf_ue_state_to_string(amf_ue_state_t state);
void amf_get_default_config(amf_config_t* config);
void smf_get_default_config(smf_config_t* config);
void upf_get_default_config(upf_config_t* config);

/* ============== CU-CP Constants ============== */

#define CU_CP_MAX_DU_CONNECTIONS   16
#define CU_CP_MAX_UE_CONTEXTS      1024
#define CU_CP_F1AP_PORT            38472
#define CU_CP_BUFFER_SIZE          65536

/* ============== CU-CP States ============== */

typedef enum {
    CU_CP_STATE_IDLE = 0,
    CU_CP_STATE_F1_SETUP_PENDING,
    CU_CP_STATE_ACTIVE,
    CU_CP_STATE_RESETTING,
    CU_CP_STATE_MAX
} cu_cp_state_t;

/* ============== CU-CP Configuration ============== */

typedef struct {
    char bind_ip[46];
    uint16_t f1ap_port;
    
    /* gNB-CU Identity */
    uint64_t gnb_cu_id;
    char gnb_cu_name[64];
    
    /* PLMN Info */
    uint8_t plmn_mcc[3];
    uint8_t plmn_mnc[3];
    uint8_t mnc_length;
    uint32_t tac;
    
    /* RRC Version */
    uint8_t rrc_version[4];
    
    /* Behavior */
    bool auto_respond;
    bool log_messages;
    
    /* PCAP */
    char pcap_file[256];
} cu_cp_config_t;

/* ============== CU-CP Server Context (Forward Declaration) ============== */

typedef struct cu_cp_server_s cu_cp_server_t;

/* ============== CU-CP API Functions ============== */

cu_cp_server_t* cu_cp_create(const cu_cp_config_t* config);
void cu_cp_destroy(cu_cp_server_t* cu_cp);
mock_core_error_t cu_cp_start(cu_cp_server_t* cu_cp);
void cu_cp_stop(cu_cp_server_t* cu_cp);
void cu_cp_get_default_config(cu_cp_config_t* config);

/* CU-CP Message Processing */
mock_core_error_t cu_cp_process_f1ap_message(cu_cp_server_t* cu_cp, 
                                              const uint8_t* data, size_t len,
                                              int socket);

/* CU-CP Statistics */
void cu_cp_print_statistics(const cu_cp_server_t* cu_cp);

/* ============== DU Constants ============== */

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
    void* served_cells;  /* f1ap_served_cell_info_t* */
    
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

/* ============== DU Server Context (Forward Declaration) ============== */

typedef struct du_server_s du_server_t;

/* ============== DU API Functions ============== */

du_server_t* du_create(const du_config_t* config);
void du_destroy(du_server_t* du);
mock_core_error_t du_start(du_server_t* du);
void du_stop(du_server_t* du);
void du_get_default_config(du_config_t* config);

/* DU F1 Connection */
mock_core_error_t du_connect_cu(du_server_t* du, const char* cu_ip, uint16_t port);
mock_core_error_t du_send_f1_setup_request(du_server_t* du);

/* DU Message Processing */
mock_core_error_t du_process_f1ap_message(du_server_t* du, 
                                           const uint8_t* data, size_t len);

/* DU Statistics */
void du_print_statistics(const du_server_t* du);

/* NAS Message Generation */
mock_core_error_t nas_generate_authentication_request(uint8_t rand[16], uint8_t autn[16],
                                                       uint8_t** nas_pdu, size_t* nas_len);
mock_core_error_t nas_generate_security_mode_command(uint8_t cipher_alg, uint8_t integrity_alg,
                                                      uint8_t** nas_pdu, size_t* nas_len);
mock_core_error_t nas_generate_registration_accept(const char* guti, uint32_t tac,
                                                    uint8_t** nas_pdu, size_t* nas_len);
mock_core_error_t nas_generate_pdu_session_accept(uint8_t pdu_session_id, uint32_t ue_ip,
                                                   uint8_t** nas_pdu, size_t* nas_len);

/* ============== CU-UP Constants ============== */

#define CU_UP_MAX_CU_CP_CONNECTIONS  4
#define CU_UP_MAX_BEARER_CONTEXTS    1024
#define CU_UP_E1AP_PORT              38462
#define CU_UP_BUFFER_SIZE            65536

/* ============== CU-UP States ============== */

typedef enum {
    CU_UP_STATE_IDLE = 0,
    CU_UP_STATE_E1_SETUP_PENDING,
    CU_UP_STATE_ACTIVE,
    CU_UP_STATE_RESETTING,
    CU_UP_STATE_MAX
} cu_up_state_t;

/* ============== CU-UP Configuration ============== */

typedef struct {
    char bind_ip[46];
    uint16_t e1ap_port;
    
    /* CU-UP Identity */
    uint64_t gnb_cu_up_id;
    char gnb_cu_up_name[64];
    
    /* PLMN Info */
    uint8_t plmn_mcc[3];
    uint8_t plmn_mnc[3];
    uint8_t mnc_length;
    
    /* Behavior */
    bool auto_respond;
    bool log_messages;
    
    /* PCAP */
    char pcap_file[256];
} cu_up_config_t;

/* ============== CU-UP Server Context (Forward Declaration) ============== */

typedef struct cu_up_server_s cu_up_server_t;

/* ============== CU-UP API Functions ============== */

cu_up_server_t* cu_up_create(const cu_up_config_t* config);
void cu_up_destroy(cu_up_server_t* cu_up);
mock_core_error_t cu_up_start(cu_up_server_t* cu_up);
void cu_up_stop(cu_up_server_t* cu_up);
void cu_up_get_default_config(void* config);

/* CU-UP E1 Connection */
mock_core_error_t cu_up_connect_cu_cp(cu_up_server_t* cu_up, const char* cu_cp_ip, uint16_t port);

/* CU-UP Message Processing */
mock_core_error_t cu_up_process_e1ap_message(cu_up_server_t* cu_up,
                                              const uint8_t* data, size_t len);

/* CU-UP Statistics */
void cu_up_print_statistics(const cu_up_server_t* cu_up);

/* ============== XnAP Constants ============== */

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

/* ============== XnAP Configuration ============== */

typedef struct {
    char bind_ip[46];
    uint16_t port;
    
    /* gNB Identity */
    uint64_t gnb_id;
    char gnb_name[64];
    
    /* Neighbor Configuration */
    uint8_t max_neighbors;
    
    /* Behavior */
    bool auto_respond;
    bool log_messages;
    
    /* PCAP */
    char pcap_file[256];
} xnap_config_t;

/* ============== XnAP Server Context (Forward Declaration) ============== */

typedef struct xnap_server_s xnap_server_t;

/* ============== XnAP API Functions ============== */

xnap_server_t* xnap_create(const xnap_config_t* config);
void xnap_destroy(xnap_server_t* server);
mock_core_error_t xnap_start(xnap_server_t* server);
void xnap_stop(xnap_server_t* server);
void xnap_get_default_config(void* config);

/* XnAP Message Processing */
mock_core_error_t xnap_process_message(xnap_server_t* server,
                                         const uint8_t* data, size_t len,
                                         int neighbor_idx);

/* XnAP Handover */
mock_core_error_t xnap_initiate_handover(xnap_server_t* server,
                                          uint32_t source_ue_id,
                                          uint64_t target_gnb_id,
                                          uint64_t target_cell_id);

/* XnAP Statistics */
void xnap_print_statistics(const xnap_server_t* server);

#endif /* MOCK_CORE_H */
