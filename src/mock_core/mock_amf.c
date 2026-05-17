/*
 * 5G UE Simulation Application
 * Mock AMF (Access and Mobility Management Function)
 * 3GPP TS 38.413 (NGAP), TS 24.501 (NAS)
 */

#include "mock_core.h"
#include <string.h>

/* ============== Forward Declarations ============== */

static void* ngap_listener_thread(void* arg);
static mock_core_error_t process_ngap_message(amf_server_t* amf, const uint8_t* data, 
                                               size_t len, int socket);
static void generate_auth_vectors(uint8_t rand[16], uint8_t autn[16], 
                                   uint8_t xres[16], uint8_t kausf[32]);

/* ============== Utility Functions ============== */

const char* amf_ue_state_to_string(amf_ue_state_t state) {
    static const char* state_names[] = {
        "IDLE", "REGISTERED", "CONNECTING", "DEREGISTERING"
    };
    if (state >= AMF_UE_STATE_MAX) return "UNKNOWN";
    return state_names[state];
}

void amf_get_default_config(amf_config_t* config) {
    if (!config) return;
    memset(config, 0, sizeof(amf_config_t));
    strncpy(config->bind_ip, "0.0.0.0", sizeof(config->bind_ip) - 1);
    config->ngap_port = MOCK_CORE_NGAP_PORT;
    config->max_ues = MOCK_CORE_MAX_UES;
    config->amf_id = 1;
    strncpy(config->amf_name, "MockAMF-1", sizeof(config->amf_name) - 1);
    config->amf_set_id = 1;
    config->amf_pointer = 0;
    /* PLMN: 00101 (MCC=001, MNC=01) */
    config->plmn_id[0] = 0x00;
    config->plmn_id[1] = 0x01;
    config->plmn_id[2] = 0xF1;
    config->tac = 1;
    config->auto_respond = true;
    config->response_delay_ms = 0;
    config->log_messages = true;
}

/* ============== AMF Server Lifecycle ============== */

amf_server_t* amf_create(const amf_config_t* config) {
    amf_server_t* amf = (amf_server_t*)uesim_calloc(1, sizeof(amf_server_t));
    if (!amf) return NULL;
    
    if (config) {
        memcpy(&amf->config, config, sizeof(amf_config_t));
    } else {
        amf_get_default_config(&amf->config);
    }
    
    amf->ngap_socket = -1;
    amf->num_active_ues = 0;
    amf->next_amf_ue_id = 1;
    atomic_store(&amf->running, 0);
    amf->ngap_messages_rx = 0;
    amf->ngap_messages_tx = 0;
    amf->registrations = 0;
    amf->authentications = 0;
    
    if (pthread_mutex_init(&amf->ue_mutex, NULL) != 0) {
        uesim_free(amf);
        return NULL;
    }
    
    return amf;
}

void amf_destroy(amf_server_t* amf) {
    if (!amf) return;
    
    amf_stop(amf);
    
    /* Free all UE contexts */
    pthread_mutex_lock(&amf->ue_mutex);
    for (uint32_t i = 0; i < amf->config.max_ues; i++) {
        if (amf->ue_contexts[i]) {
            uesim_free(amf->ue_contexts[i]);
            amf->ue_contexts[i] = NULL;
        }
    }
    pthread_mutex_unlock(&amf->ue_mutex);
    pthread_mutex_destroy(&amf->ue_mutex);
    
    uesim_free(amf);
}

mock_core_error_t amf_start(amf_server_t* amf) {
    if (!amf) return MOCK_CORE_ERROR_INVALID_PARAM;
    
    struct sockaddr_in addr;
    
    /* Create SCTP socket for NGAP (using TCP for mock) */
    amf->ngap_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (amf->ngap_socket < 0) {
        return MOCK_CORE_ERROR_SOCKET;
    }
    
    /* Set socket options */
    int opt = 1;
    setsockopt(amf->ngap_socket, SOL_SOCKET, SO_REUSEADDR, 
               (const char*)&opt, sizeof(opt));
    
    /* Bind */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(amf->config.ngap_port);
    inet_pton(AF_INET, amf->config.bind_ip, &addr.sin_addr);
    
    if (bind(amf->ngap_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        uesim_sock_close(amf->ngap_socket);
        amf->ngap_socket = -1;
        return MOCK_CORE_ERROR_SOCKET;
    }
    
    /* Listen */
    if (listen(amf->ngap_socket, 10) < 0) {
        uesim_sock_close(amf->ngap_socket);
        amf->ngap_socket = -1;
        return MOCK_CORE_ERROR_SOCKET;
    }
    
    /* Start NGAP listener thread */
    atomic_store(&amf->running, 1);
    if (pthread_create(&amf->ngap_thread, NULL, ngap_listener_thread, amf) != 0) {
        atomic_store(&amf->running, 0);
        uesim_sock_close(amf->ngap_socket);
        amf->ngap_socket = -1;
        return MOCK_CORE_ERROR_THREAD;
    }
    
    return MOCK_CORE_SUCCESS;
}

void amf_stop(amf_server_t* amf) {
    if (!amf) return;
    
    atomic_store(&amf->running, 0);
    
    if (amf->ngap_socket >= 0) {
        uesim_sock_close(amf->ngap_socket);
        amf->ngap_socket = -1;
    }
    
    /* Wait for thread to terminate */
#ifdef _WIN32
    WaitForSingleObject(amf->ngap_thread, 5000);
#else
    pthread_join(amf->ngap_thread, NULL);
#endif
}

/* ============== UE Context Management ============== */

amf_ue_context_t* amf_find_ue_by_amf_id(amf_server_t* amf, uint64_t amf_ue_id) {
    if (!amf) return NULL;
    
    pthread_mutex_lock(&amf->ue_mutex);
    for (uint32_t i = 0; i < amf->config.max_ues; i++) {
        if (amf->ue_contexts[i] && 
            amf->ue_contexts[i]->amf_ue_ngap_id == amf_ue_id) {
            pthread_mutex_unlock(&amf->ue_mutex);
            return amf->ue_contexts[i];
        }
    }
    pthread_mutex_unlock(&amf->ue_mutex);
    return NULL;
}

amf_ue_context_t* amf_find_ue_by_ran_id(amf_server_t* amf, uint32_t ran_ue_id) {
    if (!amf) return NULL;
    
    pthread_mutex_lock(&amf->ue_mutex);
    for (uint32_t i = 0; i < amf->config.max_ues; i++) {
        if (amf->ue_contexts[i] && 
            amf->ue_contexts[i]->ran_ue_ngap_id == ran_ue_id) {
            pthread_mutex_unlock(&amf->ue_mutex);
            return amf->ue_contexts[i];
        }
    }
    pthread_mutex_unlock(&amf->ue_mutex);
    return NULL;
}

amf_ue_context_t* amf_create_ue_context(amf_server_t* amf, uint32_t ran_ue_id) {
    if (!amf) return NULL;
    
    amf_ue_context_t* ue = (amf_ue_context_t*)uesim_calloc(1, sizeof(amf_ue_context_t));
    if (!ue) return NULL;
    
    pthread_mutex_lock(&amf->ue_mutex);
    
    /* Find free slot */
    int slot = -1;
    for (uint32_t i = 0; i < amf->config.max_ues; i++) {
        if (!amf->ue_contexts[i]) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        pthread_mutex_unlock(&amf->ue_mutex);
        uesim_free(ue);
        return NULL;
    }
    
    /* Initialize UE context */
    ue->amf_ue_ngap_id = amf->next_amf_ue_id++;
    ue->ran_ue_ngap_id = ran_ue_id;
    ue->state = AMF_UE_STATE_IDLE;
    ue->ngap_socket = -1;
    ue->connect_time = time(NULL);
    ue->last_activity = time(NULL);
    ue->authenticated = false;
    ue->security_active = false;
    ue->num_active_sessions = 0;
    
    /* Generate default IMSI from AMF UE ID */
    snprintf(ue->imsi, sizeof(ue->imsi), "001010%010lu", ue->amf_ue_ngap_id);
    
    amf->ue_contexts[slot] = ue;
    amf->num_active_ues++;
    
    pthread_mutex_unlock(&amf->ue_mutex);
    return ue;
}

void amf_remove_ue_context(amf_server_t* amf, amf_ue_context_t* ue) {
    if (!amf || !ue) return;
    
    pthread_mutex_lock(&amf->ue_mutex);
    for (uint32_t i = 0; i < amf->config.max_ues; i++) {
        if (amf->ue_contexts[i] == ue) {
            amf->ue_contexts[i] = NULL;
            amf->num_active_ues--;
            uesim_free(ue);
            break;
        }
    }
    pthread_mutex_unlock(&amf->ue_mutex);
}

/* ============== NGAP Listener Thread ============== */

static void* ngap_listener_thread(void* arg) {
    amf_server_t* amf = (amf_server_t*)arg;
    fd_set read_fds;
    struct timeval tv;
    uint8_t buffer[MOCK_CORE_BUFFER_SIZE];
    
    while (atomic_load(&amf->running)) {
        FD_ZERO(&read_fds);
        FD_SET(amf->ngap_socket, &read_fds);
        
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int ret = select((int)amf->ngap_socket + 1, &read_fds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (ret == 0) continue; /* Timeout */
        
        /* Accept new connection */
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_sock = accept(amf->ngap_socket, 
                                  (struct sockaddr*)&client_addr, &addr_len);
        if (client_sock < 0) continue;
        
        /* Receive data */
        ssize_t bytes_recv = recv(client_sock, (char*)buffer, sizeof(buffer), 0);
        if (bytes_recv > 0) {
            amf->ngap_messages_rx++;
            process_ngap_message(amf, buffer, (size_t)bytes_recv, client_sock);
        }
        
        uesim_sock_close(client_sock);
    }
    
    return NULL;
}

/* ============== NGAP Message Processing ============== */

static mock_core_error_t process_ngap_message(amf_server_t* amf, const uint8_t* data, 
                                               size_t len, int socket) {
    ngap_message_t msg;
    ngap_message_t response;
    mock_core_error_t err;
    
    memset(&msg, 0, sizeof(msg));
    memset(&response, 0, sizeof(response));
    
    /* Decode NGAP message */
    err = ngap_decode_message(data, len, &msg);
    if (err != UESIM_SUCCESS) {
        return MOCK_CORE_ERROR_PROTOCOL;
    }
    
    if (amf->config.log_messages) {
        printf("[AMF] RX: %s\n", ngap_message_type_to_string(msg.message_type));
    }
    
    /* Process based on message type */
    switch (msg.message_type) {
        case NGAP_MSG_NG_SETUP_REQUEST:
            err = amf_handle_ng_setup(amf, &msg, socket, &response);
            break;
            
        case NGAP_MSG_INITIAL_UE_MESSAGE:
            err = amf_handle_initial_ue(amf, &msg, socket, &response);
            break;
            
        case NGAP_MSG_UPLINK_NAS_TRANSPORT:
            err = amf_handle_uplink_nas(amf, &msg, socket, &response);
            break;
            
        default:
            if (amf->config.log_messages) {
                printf("[AMF] Unhandled message type: %d\n", msg.message_type);
            }
            err = MOCK_CORE_SUCCESS;
            break;
    }
    
    /* Send response if generated */
    if (err == MOCK_CORE_SUCCESS && response.message_type != NGAP_MSG_MAX) {
        uint8_t* resp_data = NULL;
        size_t resp_len = 0;
        
        err = ngap_encode_message(&response, &resp_data, &resp_len);
        if (err == UESIM_SUCCESS && resp_data) {
            send(socket, (const char*)resp_data, resp_len, 0);
            amf->ngap_messages_tx++;
            
            if (amf->config.log_messages) {
                printf("[AMF] TX: %s (%zu bytes)\n", 
                       ngap_message_type_to_string(response.message_type), resp_len);
            }
            
            uesim_free(resp_data);
        }
    }
    
    ngap_free_message(&msg);
    return err;
}

/* ============== NGAP Message Handlers ============== */

mock_core_error_t amf_handle_ng_setup(amf_server_t* amf, const ngap_message_t* msg,
                                       int socket, ngap_message_t* response) {
    (void)socket;
    
    if (amf->config.log_messages) {
        printf("[AMF] NG Setup Request from gNB: %s\n", 
               msg->payload.ng_setup_request.gnb_name);
    }
    
    /* Build NG Setup Response */
    memset(response, 0, sizeof(ngap_message_t));
    response->message_type = NGAP_MSG_NG_SETUP_RESPONSE;
    response->procedure_code = NGAP_PROC_NG_SETUP;
    
    ngap_ng_setup_response_t* resp = &response->payload.ng_setup_response;
    memcpy(resp->plmn_id, amf->config.plmn_id, 3);
    resp->amf_id = amf->config.amf_id;
    strncpy(resp->amf_name, amf->config.amf_name, sizeof(resp->amf_name) - 1);
    resp->relative_amf_capacity = 100;
    resp->num_tai = 1;
    resp->tai_list[0].plmn_id[0] = amf->config.plmn_id[0];
    resp->tai_list[0].plmn_id[1] = amf->config.plmn_id[1];
    resp->tai_list[0].plmn_id[2] = amf->config.plmn_id[2];
    resp->tai_list[0].tac = amf->config.tac;
    resp->num_snssai = 1;
    resp->snssai_list[0].nssai_sst = 1;
    resp->snssai_list[0].sd_present = false;
    
    return MOCK_CORE_SUCCESS;
}

mock_core_error_t amf_handle_initial_ue(amf_server_t* amf, const ngap_message_t* msg,
                                         int socket, ngap_message_t* response) {
    const ngap_initial_ue_message_t* init_ue = &msg->payload.initial_ue_message;
    
    /* Create UE context */
    amf_ue_context_t* ue = amf_create_ue_context(amf, init_ue->ue_ids.ran_ue_ngap_id);
    if (!ue) {
        return MOCK_CORE_ERROR_CAPACITY;
    }
    
    ue->ngap_socket = socket;
    memcpy(&ue->gnb_addr, &init_ue->user_location.nr_cgi, sizeof(ue->gnb_addr));
    ue->last_activity = time(NULL);
    
    if (amf->config.log_messages) {
        printf("[AMF] Initial UE Message: RAN-UE-ID=%u, AMF-UE-ID=%lu\n",
               ue->ran_ue_ngap_id, ue->amf_ue_ngap_id);
    }
    
    /* Process NAS PDU if present */
    if (init_ue->nas_pdu_len > 0) {
        return amf_handle_registration_request(amf, ue, init_ue->nas_pdu, 
                                                init_ue->nas_pdu_len, response);
    }
    
    return MOCK_CORE_SUCCESS;
}

mock_core_error_t amf_handle_uplink_nas(amf_server_t* amf, const ngap_message_t* msg,
                                         int socket, ngap_message_t* response) {
    const ngap_uplink_nas_transport_t* ul_nas = &msg->payload.uplink_nas_transport;
    (void)socket;
    
    /* Find UE context */
    amf_ue_context_t* ue = amf_find_ue_by_amf_id(amf, ul_nas->ue_ids.amf_ue_ngap_id);
    if (!ue) {
        ue = amf_find_ue_by_ran_id(amf, ul_nas->ue_ids.ran_ue_ngap_id);
        if (!ue) {
            return MOCK_CORE_ERROR_NOT_FOUND;
        }
    }
    
    ue->last_activity = time(NULL);
    
    if (ul_nas->nas_pdu_len == 0) {
        return MOCK_CORE_ERROR_PROTOCOL;
    }
    
    /* Determine NAS message type (first byte contains type) */
    uint8_t nas_type = ul_nas->nas_pdu[0] & 0x0F;
    uint8_t nas_sec_header = (ul_nas->nas_pdu[0] >> 4) & 0x0F;
    
    (void)nas_sec_header; /* Ignore for now */
    
    /* Handle based on NAS message type */
    switch (nas_type) {
        case 0x02: /* Registration Request */
            return amf_handle_registration_request(amf, ue, ul_nas->nas_pdu,
                                                    ul_nas->nas_pdu_len, response);
            
        case 0x04: /* Service Request */
            if (amf->config.log_messages) {
                printf("[AMF] Service Request from UE %lu\n", ue->amf_ue_ngap_id);
            }
            /* For simplicity, accept service request */
            return MOCK_CORE_SUCCESS;
            
        case 0x44: /* Authentication Response */
            return amf_handle_authentication_response(amf, ue, ul_nas->nas_pdu,
                                                       ul_nas->nas_pdu_len, response);
            
        case 0x5D: /* Security Mode Complete */
            return amf_handle_security_mode_complete(amf, ue, ul_nas->nas_pdu,
                                                      ul_nas->nas_pdu_len, response);
            
        case 0x45: /* Registration Complete */
            return amf_handle_registration_complete(amf, ue, ul_nas->nas_pdu,
                                                     ul_nas->nas_pdu_len, response);
            
        case 0xC1: /* UL NAS Transport (PDU Session) */
            return amf_handle_pdu_session_request(amf, ue, ul_nas->nas_pdu,
                                                   ul_nas->nas_pdu_len, response);
            
        default:
            if (amf->config.log_messages) {
                printf("[AMF] Unhandled NAS message type: 0x%02X\n", nas_type);
            }
            return MOCK_CORE_SUCCESS;
    }
}

/* ============== NAS Message Handlers ============== */

mock_core_error_t amf_handle_registration_request(amf_server_t* amf, amf_ue_context_t* ue,
                                                   const uint8_t* nas_pdu, size_t nas_len,
                                                   ngap_message_t* response) {
    (void)nas_pdu;
    (void)nas_len;
    
    if (amf->config.log_messages) {
        printf("[AMF] Registration Request from UE %lu\n", ue->amf_ue_ngap_id);
    }
    
    ue->state = AMF_UE_STATE_CONNECTING;
    
    /* Generate authentication vectors */
    generate_auth_vectors(ue->rand, ue->autn, ue->xres, ue->kamf);
    
    /* Build Authentication Request NAS message */
    uint8_t* auth_req_nas = NULL;
    size_t auth_req_len = 0;
    
    mock_core_error_t err = nas_generate_authentication_request(ue->rand, ue->autn,
                                                                 &auth_req_nas, &auth_req_len);
    if (err != MOCK_CORE_SUCCESS) {
        return err;
    }
    
    /* Build Downlink NAS Transport with Authentication Request */
    memset(response, 0, sizeof(ngap_message_t));
    response->message_type = NGAP_MSG_DOWNLINK_NAS_TRANSPORT;
    response->procedure_code = NGAP_PROC_DOWNLINK_NAS_TRANSPORT;
    
    ngap_downlink_nas_transport_t* dl_nas = &response->payload.downlink_nas_transport;
    dl_nas->ue_ids.ran_ue_ngap_id = ue->ran_ue_ngap_id;
    dl_nas->ue_ids.amf_ue_ngap_id = ue->amf_ue_ngap_id;
    memcpy(dl_nas->nas_pdu, auth_req_nas, auth_req_len);
    dl_nas->nas_pdu_len = auth_req_len;
    dl_nas->cause_present = false;
    
    uesim_free(auth_req_nas);
    amf->authentications++;
    
    return MOCK_CORE_SUCCESS;
}

mock_core_error_t amf_handle_authentication_response(amf_server_t* amf, amf_ue_context_t* ue,
                                                      const uint8_t* nas_pdu, size_t nas_len,
                                                      ngap_message_t* response) {
    (void)nas_pdu;
    (void)nas_len;
    
    if (amf->config.log_messages) {
        printf("[AMF] Authentication Response from UE %lu\n", ue->amf_ue_ngap_id);
    }
    
    /* For mock, always accept authentication */
    ue->authenticated = true;
    
    /* Derive NAS keys (mock - just fill with test values) */
    memset(ue->knas_enc, 0xAA, 16);
    memset(ue->knas_int, 0xBB, 16);
    ue->ciphering_alg = 0x01; /* NEA1 */
    ue->integrity_alg = 0x01; /* NIA1 */
    
    /* Build Security Mode Command */
    uint8_t* smc_nas = NULL;
    size_t smc_len = 0;
    
    mock_core_error_t err = nas_generate_security_mode_command(ue->ciphering_alg,
                                                                ue->integrity_alg,
                                                                &smc_nas, &smc_len);
    if (err != MOCK_CORE_SUCCESS) {
        return err;
    }
    
    /* Build Downlink NAS Transport */
    memset(response, 0, sizeof(ngap_message_t));
    response->message_type = NGAP_MSG_DOWNLINK_NAS_TRANSPORT;
    response->procedure_code = NGAP_PROC_DOWNLINK_NAS_TRANSPORT;
    
    ngap_downlink_nas_transport_t* dl_nas = &response->payload.downlink_nas_transport;
    dl_nas->ue_ids.ran_ue_ngap_id = ue->ran_ue_ngap_id;
    dl_nas->ue_ids.amf_ue_ngap_id = ue->amf_ue_ngap_id;
    memcpy(dl_nas->nas_pdu, smc_nas, smc_len);
    dl_nas->nas_pdu_len = smc_len;
    
    uesim_free(smc_nas);
    
    return MOCK_CORE_SUCCESS;
}

mock_core_error_t amf_handle_security_mode_complete(amf_server_t* amf, amf_ue_context_t* ue,
                                                     const uint8_t* nas_pdu, size_t nas_len,
                                                     ngap_message_t* response) {
    (void)nas_pdu;
    (void)nas_len;
    
    if (amf->config.log_messages) {
        printf("[AMF] Security Mode Complete from UE %lu\n", ue->amf_ue_ngap_id);
    }
    
    ue->security_active = true;
    
    /* Generate GUTI */
    snprintf(ue->guti, sizeof(ue->guti), "5G-GUTI-%03d-%02d-%010lu",
             amf->config.amf_id, ue->ran_ue_ngap_id % 100, ue->amf_ue_ngap_id);
    
    /* Build Registration Accept */
    uint8_t* reg_accept_nas = NULL;
    size_t reg_accept_len = 0;
    
    mock_core_error_t err = nas_generate_registration_accept(ue->guti, amf->config.tac,
                                                              &reg_accept_nas, &reg_accept_len);
    if (err != MOCK_CORE_SUCCESS) {
        return err;
    }
    
    /* Build Downlink NAS Transport with Registration Accept */
    memset(response, 0, sizeof(ngap_message_t));
    response->message_type = NGAP_MSG_DOWNLINK_NAS_TRANSPORT;
    response->procedure_code = NGAP_PROC_DOWNLINK_NAS_TRANSPORT;
    
    ngap_downlink_nas_transport_t* dl_nas = &response->payload.downlink_nas_transport;
    dl_nas->ue_ids.ran_ue_ngap_id = ue->ran_ue_ngap_id;
    dl_nas->ue_ids.amf_ue_ngap_id = ue->amf_ue_ngap_id;
    memcpy(dl_nas->nas_pdu, reg_accept_nas, reg_accept_len);
    dl_nas->nas_pdu_len = reg_accept_len;
    
    uesim_free(reg_accept_nas);
    
    return MOCK_CORE_SUCCESS;
}

mock_core_error_t amf_handle_registration_complete(amf_server_t* amf, amf_ue_context_t* ue,
                                                    const uint8_t* nas_pdu, size_t nas_len,
                                                    ngap_message_t* response) {
    (void)nas_pdu;
    (void)nas_len;
    (void)response;
    
    if (amf->config.log_messages) {
        printf("[AMF] Registration Complete from UE %lu\n", ue->amf_ue_ngap_id);
    }
    
    ue->state = AMF_UE_STATE_REGISTERED;
    amf->registrations++;
    
    return MOCK_CORE_SUCCESS;
}

mock_core_error_t amf_handle_pdu_session_request(amf_server_t* amf, amf_ue_context_t* ue,
                                                  const uint8_t* nas_pdu, size_t nas_len,
                                                  ngap_message_t* response) {
    (void)nas_pdu;
    (void)nas_len;
    
    if (amf->config.log_messages) {
        printf("[AMF] PDU Session Request from UE %lu\n", ue->amf_ue_ngap_id);
    }
    
    /* Allocate PDU Session ID */
    uint8_t pdu_session_id = 1;
    for (int i = 0; i < MOCK_CORE_MAX_SESSIONS; i++) {
        if (ue->pdu_session_ids[i] == 0) {
            pdu_session_id = (uint8_t)(i + 1);
            ue->pdu_session_ids[i] = pdu_session_id;
            ue->num_active_sessions++;
            break;
        }
    }
    
    /* Generate PDU Session Accept */
    uint8_t* pdu_accept_nas = NULL;
    size_t pdu_accept_len = 0;
    uint32_t ue_ip = 0x0A000001 + (uint32_t)ue->amf_ue_ngap_id; /* 10.0.0.x */
    
    mock_core_error_t err = nas_generate_pdu_session_accept(pdu_session_id, ue_ip,
                                                             &pdu_accept_nas, &pdu_accept_len);
    if (err != MOCK_CORE_SUCCESS) {
        return err;
    }
    
    /* Build PDU Session Setup Response */
    memset(response, 0, sizeof(ngap_message_t));
    response->message_type = NGAP_MSG_PDU_SESSION_SETUP_RESPONSE;
    response->procedure_code = NGAP_PROC_PDU_SESSION_SETUP;
    
    ngap_pdu_session_setup_response_t* resp = &response->payload.pdu_session_setup_response;
    resp->ue_ids.ran_ue_ngap_id = ue->ran_ue_ngap_id;
    resp->ue_ids.amf_ue_ngap_id = ue->amf_ue_ngap_id;
    resp->success = true;
    resp->pdu_session.pdu_session_id = pdu_session_id;
    resp->pdu_session.upf_teid = (uint32_t)ue->amf_ue_ngap_id * 1000;
    resp->pdu_session.upf_ip = 0x7F000001; /* 127.0.0.1 */
    resp->pdu_session.num_qos_flows = 1;
    resp->pdu_session.qos_flows[0].qfi = 1;
    resp->pdu_session.qos_flows[0].five_qi = 9; /* Default QoS */
    
    uesim_free(pdu_accept_nas);
    
    return MOCK_CORE_SUCCESS;
}

/* ============== NAS Message Generation ============== */

mock_core_error_t nas_generate_authentication_request(uint8_t rand[16], uint8_t autn[16],
                                                       uint8_t** nas_pdu, size_t* nas_len) {
    /* Build simple Authentication Request NAS message */
    /* Format: Header (2) + RAND (16) + AUTN (16) */
    
    *nas_len = 36;
    *nas_pdu = (uint8_t*)uesim_malloc(*nas_len);
    if (!*nas_pdu) return MOCK_CORE_ERROR_MEMORY;
    
    uint8_t* p = *nas_pdu;
    
    /* NAS Header: Protocol Discriminator + Security Header Type */
    *p++ = 0x00; /* 5G NAS | Plain NAS */
    *p++ = 0x56; /* Authentication Request (0x56 = 86 in decimal, message type) */
    
    /* RAND (16 bytes) */
    memcpy(p, rand, 16);
    p += 16;
    
    /* AUTN (16 bytes) */
    memcpy(p, autn, 16);
    
    return MOCK_CORE_SUCCESS;
}

mock_core_error_t nas_generate_security_mode_command(uint8_t cipher_alg, uint8_t integrity_alg,
                                                      uint8_t** nas_pdu, size_t* nas_len) {
    /* Build Security Mode Command NAS message */
    *nas_len = 6;
    *nas_pdu = (uint8_t*)uesim_malloc(*nas_len);
    if (!*nas_pdu) return MOCK_CORE_ERROR_MEMORY;
    
    uint8_t* p = *nas_pdu;
    
    /* NAS Header */
    *p++ = 0x00; /* 5G NAS | Plain NAS */
    *p++ = 0x5D; /* Security Mode Command */
    
    /* Security Algorithms */
    *p++ = cipher_alg;
    *p++ = integrity_alg;
    
    /* UE Security Capability (dummy) */
    *p++ = 0x00;
    *p++ = 0x00;
    
    return MOCK_CORE_SUCCESS;
}

mock_core_error_t nas_generate_registration_accept(const char* guti, uint32_t tac,
                                                    uint8_t** nas_pdu, size_t* nas_len) {
    /* Build Registration Accept NAS message */
    size_t guti_len = strlen(guti);
    *nas_len = 4 + guti_len + 4;
    *nas_pdu = (uint8_t*)uesim_malloc(*nas_len);
    if (!*nas_pdu) return MOCK_CORE_ERROR_MEMORY;
    
    uint8_t* p = *nas_pdu;
    
    /* NAS Header */
    *p++ = 0x00; /* 5G NAS | Plain NAS */
    *p++ = 0x42; /* Registration Accept */
    
    /* Registration Result */
    *p++ = 0x01; /* 3GPP access */
    
    /* GUTI IE */
    *p++ = (uint8_t)guti_len;
    memcpy(p, guti, guti_len);
    p += guti_len;
    
    /* TAI */
    *p++ = (tac >> 16) & 0xFF;
    *p++ = (tac >> 8) & 0xFF;
    *p++ = tac & 0xFF;
    *p++ = 0x00; /* PLMN dummy */
    
    return MOCK_CORE_SUCCESS;
}

mock_core_error_t nas_generate_pdu_session_accept(uint8_t pdu_session_id, uint32_t ue_ip,
                                                   uint8_t** nas_pdu, size_t* nas_len) {
    /* Build PDU Session Establishment Accept NAS message */
    *nas_len = 12;
    *nas_pdu = (uint8_t*)uesim_malloc(*nas_len);
    if (!*nas_pdu) return MOCK_CORE_ERROR_MEMORY;
    
    uint8_t* p = *nas_pdu;
    
    /* NAS Header */
    *p++ = 0x00; /* 5G NAS | Plain NAS */
    *p++ = 0xC2; /* PDU Session Establishment Accept */
    
    /* PDU Session ID */
    *p++ = pdu_session_id;
    
    /* PDU Session Type (IPv4) */
    *p++ = 0x01;
    
    /* QoS Flow ID */
    *p++ = 0x01;
    
    /* UE IP Address (IPv4) */
    *p++ = (ue_ip >> 24) & 0xFF;
    *p++ = (ue_ip >> 16) & 0xFF;
    *p++ = (ue_ip >> 8) & 0xFF;
    *p++ = ue_ip & 0xFF;
    
    /* DNN (dummy) */
    *p++ = 0x03;
    *p++ = 'i';
    *p++ = 'm';
    *p++ = 's';
    
    return MOCK_CORE_SUCCESS;
}

/* ============== Authentication Vector Generation ============== */

static void generate_auth_vectors(uint8_t rand_out[16], uint8_t autn[16], 
                                   uint8_t xres[16], uint8_t kausf[32]) {
    /* Mock authentication vector generation */
    /* In real implementation, this would use Milenage algorithm */
    
    srand((unsigned int)time(NULL));
    
    /* Generate random RAND */
    for (int i = 0; i < 16; i++) {
        rand_out[i] = (uint8_t)(rand() & 0xFF);
    }
    
    /* Generate mock AUTN (SQN XOR AK || AMF || MAC-A) */
    for (int i = 0; i < 16; i++) {
        autn[i] = (uint8_t)(rand() & 0xFF);
    }
    
    /* Generate mock XRES */
    for (int i = 0; i < 16; i++) {
        xres[i] = (uint8_t)(rand() & 0xFF);
    }
    
    /* Generate mock KAUSF */
    for (int i = 0; i < 32; i++) {
        kausf[i] = (uint8_t)(rand() & 0xFF);
    }
}
