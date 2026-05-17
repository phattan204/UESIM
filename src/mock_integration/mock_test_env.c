/*
 * 5G UE Simulation Application
 * Mock Test Environment - Implementation
 */

#include "mock_test_env.h"
#include "../mock_core/mock_core.h"
#include "../mock_gnb/mock_gnb_server.h"
#include "../core/ue_context.h"
#include "../utils/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define sleep_ms(ms) usleep((ms) * 1000)
#endif

/* ============== Default Configuration ============== */

void mock_test_env_get_default_config(mock_test_env_config_t* config) {
    if (!config) return;
    
    memset(config, 0, sizeof(mock_test_env_config_t));
    
    /* AMF Configuration */
    config->amf.enabled = true;
    strncpy(config->amf.bind_ip, "127.0.0.1", sizeof(config->amf.bind_ip) - 1);
    config->amf.port = 38412;
    config->amf.log_messages = true;
    
    /* SMF Configuration */
    config->smf.enabled = true;
    strncpy(config->smf.bind_ip, "127.0.0.1", sizeof(config->smf.bind_ip) - 1);
    config->smf.port = 8805;  /* PFCP port */
    config->smf.log_messages = true;
    
    /* UPF Configuration */
    config->upf.enabled = true;
    strncpy(config->upf.bind_ip, "127.0.0.1", sizeof(config->upf.bind_ip) - 1);
    config->upf.port = 2152;  /* GTP-U port */
    config->upf.log_messages = true;
    
    /* CU-CP Configuration */
    config->cu_cp.enabled = true;
    strncpy(config->cu_cp.bind_ip, "127.0.0.1", sizeof(config->cu_cp.bind_ip) - 1);
    config->cu_cp.port = 38472;  /* F1AP port */
    config->cu_cp.log_messages = true;
    
    /* DU Configuration */
    config->du.enabled = true;
    strncpy(config->du.bind_ip, "127.0.0.1", sizeof(config->du.bind_ip) - 1);
    config->du.port = 38472;
    config->du.log_messages = true;
    
    /* CU-UP Configuration */
    config->cu_up.enabled = true;
    strncpy(config->cu_up.bind_ip, "127.0.0.1", sizeof(config->cu_up.bind_ip) - 1);
    config->cu_up.port = 38470;  /* E1AP port */
    config->cu_up.log_messages = true;
    
    /* XnAP Configuration */
    config->xnap.enabled = true;
    strncpy(config->xnap.bind_ip, "127.0.0.1", sizeof(config->xnap.bind_ip) - 1);
    config->xnap.port = 38422;
    config->xnap.log_messages = true;
    
    /* gNB Server Configuration */
    config->gnb_server.enabled = true;
    strncpy(config->gnb_server.bind_ip, "127.0.0.1", sizeof(config->gnb_server.bind_ip) - 1);
    config->gnb_server.port = 38412;
    config->gnb_server.log_messages = true;
    config->gnb_gtpu_port = 2152;
    
    /* gNB Cell Configuration */
    config->gnb_id = 1;
    strncpy(config->gnb_name, "Test-gNB-01", sizeof(config->gnb_name) - 1);
    config->tac = 1;
    config->pci = 1;
    config->cell_id = 0x12345;
    
    /* Test Configuration */
    config->max_ues = 1024;
    config->response_delay_ms = 10;
    config->auto_respond = true;
    config->capture_pcap = false;
    config->pcap_file[0] = '\0';
    
    /* Logging */
    config->verbose = true;
    config->log_to_console = true;
    config->log_file[0] = '\0';
}

/* ============== Environment Management ============== */

mock_test_env_t* mock_test_env_create(const mock_test_env_config_t* config) {
    mock_test_env_t* env = (mock_test_env_t*)calloc(1, sizeof(mock_test_env_t));
    if (!env) {
        return NULL;
    }
    
    /* Apply configuration */
    if (config) {
        memcpy(&env->config, config, sizeof(mock_test_env_config_t));
    } else {
        mock_test_env_get_default_config(&env->config);
    }
    
    env->state = MOCK_TEST_STATE_IDLE;
    env->last_error[0] = '\0';
    
    return env;
}

void mock_test_env_destroy(mock_test_env_t* env) {
    if (!env) return;
    
    /* Ensure stopped */
    if (env->state != MOCK_TEST_STATE_IDLE) {
        mock_test_env_stop(env);
    }
    
    /* Free environment */
    free(env);
}

mock_test_error_t mock_test_env_start(mock_test_env_t* env) {
    if (!env) return MOCK_TEST_ERROR_INVALID_PARAM;
    
    if (env->state != MOCK_TEST_STATE_IDLE) {
        strncpy(env->last_error, "Environment not in IDLE state", sizeof(env->last_error) - 1);
        return MOCK_TEST_ERROR_ALREADY_RUNNING;
    }
    
    env->state = MOCK_TEST_STATE_INITIALIZING;
    
    /* Start core network */
    mock_test_error_t err = mock_test_env_start_core(env);
    if (err != MOCK_TEST_SUCCESS) {
        env->state = MOCK_TEST_STATE_ERROR;
        return err;
    }
    
    env->state = MOCK_TEST_STATE_STARTING_CORE;
    
    /* Start gNB components */
    err = mock_test_env_start_gnb(env);
    if (err != MOCK_TEST_SUCCESS) {
        mock_test_env_stop_core(env);
        env->state = MOCK_TEST_STATE_ERROR;
        return err;
    }
    
    env->state = MOCK_TEST_STATE_STARTING_GNB;
    
    /* Connect components */
    err = mock_test_env_connect_components(env);
    if (err != MOCK_TEST_SUCCESS) {
        mock_test_env_stop_gnb(env);
        mock_test_env_stop_core(env);
        env->state = MOCK_TEST_STATE_ERROR;
        return err;
    }
    
    env->state = MOCK_TEST_STATE_READY;
    env->running = true;
    env->stats.start_time = time(NULL);
    
    if (env->config.verbose) {
        printf("[TestEnv] Environment started successfully\n");
    }
    
    return MOCK_TEST_SUCCESS;
}

void mock_test_env_stop(mock_test_env_t* env) {
    if (!env) return;
    
    env->state = MOCK_TEST_STATE_STOPPING;
    env->running = false;
    
    /* Stop in reverse order */
    mock_test_env_stop_gnb(env);
    mock_test_env_stop_core(env);
    
    /* Clear UE instances */
    memset(env->ue_instances, 0, sizeof(env->ue_instances));
    env->num_active_ues = 0;
    
    env->state = MOCK_TEST_STATE_IDLE;
    
    if (env->config.verbose) {
        printf("[TestEnv] Environment stopped\n");
    }
}

bool mock_test_env_is_running(const mock_test_env_t* env) {
    return env ? env->running : false;
}

mock_test_state_t mock_test_env_get_state(const mock_test_env_t* env) {
    return env ? env->state : MOCK_TEST_STATE_IDLE;
}

mock_test_error_t mock_test_env_get_stats(const mock_test_env_t* env, 
                                          mock_test_stats_t* stats) {
    if (!env || !stats) return MOCK_TEST_ERROR_INVALID_PARAM;
    
    memcpy(stats, &env->stats, sizeof(mock_test_stats_t));
    return MOCK_TEST_SUCCESS;
}

const char* mock_test_env_get_last_error(const mock_test_env_t* env) {
    return env ? env->last_error : "NULL environment";
}

/* ============== Component Control ============== */

mock_test_error_t mock_test_env_start_core(mock_test_env_t* env) {
    if (!env) return MOCK_TEST_ERROR_INVALID_PARAM;
    
    /* Start AMF */
    if (env->config.amf.enabled) {
        amf_config_t amf_cfg;
        amf_get_default_config(&amf_cfg);
        strncpy(amf_cfg.bind_ip, env->config.amf.bind_ip, sizeof(amf_cfg.bind_ip) - 1);
        amf_cfg.ngap_port = env->config.amf.port;
        amf_cfg.log_messages = env->config.amf.log_messages;
        
        env->amf = amf_create(&amf_cfg);
        if (!env->amf) {
            strncpy(env->last_error, "Failed to create AMF", sizeof(env->last_error) - 1);
            return MOCK_TEST_ERROR_COMPONENT;
        }
        
        mock_core_error_t err = amf_start(env->amf);
        if (err != MOCK_CORE_SUCCESS) {
            strncpy(env->last_error, "Failed to start AMF", sizeof(env->last_error) - 1);
            amf_destroy(env->amf);
            env->amf = NULL;
            return MOCK_TEST_ERROR_COMPONENT;
        }
        
        if (env->config.verbose) {
            printf("[TestEnv] AMF started on %s:%u\n", 
                   env->config.amf.bind_ip, env->config.amf.port);
        }
    }
    
    /* Start SMF */
    if (env->config.smf.enabled) {
        smf_config_t smf_cfg;
        smf_get_default_config(&smf_cfg);
        strncpy(smf_cfg.bind_ip, env->config.smf.bind_ip, sizeof(smf_cfg.bind_ip) - 1);
        smf_cfg.log_messages = env->config.smf.log_messages;
        
        env->smf = smf_create(&smf_cfg);
        if (!env->smf) {
            strncpy(env->last_error, "Failed to create SMF", sizeof(env->last_error) - 1);
            mock_test_env_stop_core(env);
            return MOCK_TEST_ERROR_COMPONENT;
        }
        
        if (env->config.verbose) {
            printf("[TestEnv] SMF started\n");
        }
    }
    
    /* Start UPF */
    if (env->config.upf.enabled) {
        upf_config_t upf_cfg;
        upf_get_default_config(&upf_cfg);
        strncpy(upf_cfg.bind_ip, env->config.upf.bind_ip, sizeof(upf_cfg.bind_ip) - 1);
        upf_cfg.gtpu_port = env->config.upf.port;
        upf_cfg.log_packets = env->config.upf.log_messages;
        
        env->upf = upf_create(&upf_cfg);
        if (!env->upf) {
            strncpy(env->last_error, "Failed to create UPF", sizeof(env->last_error) - 1);
            mock_test_env_stop_core(env);
            return MOCK_TEST_ERROR_COMPONENT;
        }
        
        mock_core_error_t err = upf_start(env->upf);
        if (err != MOCK_CORE_SUCCESS) {
            strncpy(env->last_error, "Failed to start UPF", sizeof(env->last_error) - 1);
            upf_destroy(env->upf);
            env->upf = NULL;
            return MOCK_TEST_ERROR_COMPONENT;
        }
        
        if (env->config.verbose) {
            printf("[TestEnv] UPF started on %s:%u\n",
                   env->config.upf.bind_ip, env->config.upf.port);
        }
    }
    
    return MOCK_TEST_SUCCESS;
}

mock_test_error_t mock_test_env_start_gnb(mock_test_env_t* env) {
    if (!env) return MOCK_TEST_ERROR_INVALID_PARAM;
    
    /* Start CU-CP */
    if (env->config.cu_cp.enabled) {
        cu_cp_config_t cu_cp_cfg;
        cu_cp_get_default_config(&cu_cp_cfg);
        strncpy(cu_cp_cfg.bind_ip, env->config.cu_cp.bind_ip, sizeof(cu_cp_cfg.bind_ip) - 1);
        cu_cp_cfg.log_messages = env->config.cu_cp.log_messages;
        
        env->cu_cp = cu_cp_create(&cu_cp_cfg);
        if (!env->cu_cp) {
            strncpy(env->last_error, "Failed to create CU-CP", sizeof(env->last_error) - 1);
            return MOCK_TEST_ERROR_COMPONENT;
        }
        
        mock_core_error_t err = cu_cp_start(env->cu_cp);
        if (err != MOCK_CORE_SUCCESS) {
            strncpy(env->last_error, "Failed to start CU-CP", sizeof(env->last_error) - 1);
            cu_cp_destroy(env->cu_cp);
            env->cu_cp = NULL;
            return MOCK_TEST_ERROR_COMPONENT;
        }
        
        if (env->config.verbose) {
            printf("[TestEnv] CU-CP started on %s:%u\n",
                   env->config.cu_cp.bind_ip, env->config.cu_cp.port);
        }
    }
    
    /* Start DU */
    if (env->config.du.enabled) {
        du_config_t du_cfg;
        du_get_default_config(&du_cfg);
        strncpy(du_cfg.bind_ip, env->config.du.bind_ip, sizeof(du_cfg.bind_ip) - 1);
        du_cfg.log_messages = env->config.du.log_messages;
        
        env->du = du_create(&du_cfg);
        if (!env->du) {
            strncpy(env->last_error, "Failed to create DU", sizeof(env->last_error) - 1);
            mock_test_env_stop_gnb(env);
            return MOCK_TEST_ERROR_COMPONENT;
        }
        
        if (env->config.verbose) {
            printf("[TestEnv] DU created\n");
        }
    }
    
    /* Start CU-UP */
    if (env->config.cu_up.enabled) {
        cu_up_config_t cu_up_cfg;
        cu_up_get_default_config(&cu_up_cfg);
        strncpy(cu_up_cfg.bind_ip, env->config.cu_up.bind_ip, sizeof(cu_up_cfg.bind_ip) - 1);
        cu_up_cfg.log_messages = env->config.cu_up.log_messages;
        
        env->cu_up = cu_up_create(&cu_up_cfg);
        if (!env->cu_up) {
            strncpy(env->last_error, "Failed to create CU-UP", sizeof(env->last_error) - 1);
            mock_test_env_stop_gnb(env);
            return MOCK_TEST_ERROR_COMPONENT;
        }
        
        if (env->config.verbose) {
            printf("[TestEnv] CU-UP created\n");
        }
    }
    
    /* Start XnAP */
    if (env->config.xnap.enabled) {
        xnap_config_t xnap_cfg;
        xnap_get_default_config(&xnap_cfg);
        strncpy(xnap_cfg.bind_ip, env->config.xnap.bind_ip, sizeof(xnap_cfg.bind_ip) - 1);
        xnap_cfg.log_messages = env->config.xnap.log_messages;
        
        env->xnap = xnap_create(&xnap_cfg);
        if (!env->xnap) {
            strncpy(env->last_error, "Failed to create XnAP", sizeof(env->last_error) - 1);
            mock_test_env_stop_gnb(env);
            return MOCK_TEST_ERROR_COMPONENT;
        }
        
        if (env->config.verbose) {
            printf("[TestEnv] XnAP created\n");
        }
    }
    
    /* Start gNB Server */
    if (env->config.gnb_server.enabled) {
        mock_gnb_config_t gnb_cfg;
        mock_gnb_get_default_config(&gnb_cfg);
        strncpy(gnb_cfg.bind_ip, env->config.gnb_server.bind_ip, sizeof(gnb_cfg.bind_ip) - 1);
        gnb_cfg.ngap_port = env->config.gnb_server.port;
        gnb_cfg.gtpu_port = env->config.gnb_gtpu_port;
        gnb_cfg.log_messages = env->config.gnb_server.log_messages;
        gnb_cfg.auto_respond = env->config.auto_respond;
        gnb_cfg.response_delay_ms = env->config.response_delay_ms;
        
        gnb_cfg.cell_config.gnb_id = env->config.gnb_id;
        strncpy(gnb_cfg.cell_config.gnb_name, env->config.gnb_name, 
                sizeof(gnb_cfg.cell_config.gnb_name) - 1);
        gnb_cfg.cell_config.tac = env->config.tac;
        gnb_cfg.cell_config.pci = env->config.pci;
        gnb_cfg.cell_config.cell_id = env->config.cell_id;
        
        if (env->config.capture_pcap && env->config.pcap_file[0]) {
            strncpy(gnb_cfg.pcap_file, env->config.pcap_file, sizeof(gnb_cfg.pcap_file) - 1);
        }
        
        mock_gnb_error_t err = mock_gnb_server_init(&gnb_cfg);
        if (err != MOCK_GNB_SUCCESS) {
            strncpy(env->last_error, "Failed to initialize gNB server", sizeof(env->last_error) - 1);
            mock_test_env_stop_gnb(env);
            return MOCK_TEST_ERROR_COMPONENT;
        }
        
        err = mock_gnb_server_start();
        if (err != MOCK_GNB_SUCCESS) {
            strncpy(env->last_error, "Failed to start gNB server", sizeof(env->last_error) - 1);
            return MOCK_TEST_ERROR_COMPONENT;
        }
        
        if (env->config.verbose) {
            printf("[TestEnv] gNB Server started on %s:%u (GTP-U: %u)\n",
                   env->config.gnb_server.bind_ip, 
                   env->config.gnb_server.port,
                   env->config.gnb_gtpu_port);
        }
    }
    
    return MOCK_TEST_SUCCESS;
}

mock_test_error_t mock_test_env_connect_components(mock_test_env_t* env) {
    if (!env) return MOCK_TEST_ERROR_INVALID_PARAM;
    
    env->state = MOCK_TEST_STATE_CONNECTING;
    
    /* Connect DU to CU-CP (F1 Setup) */
    if (env->du && env->cu_cp) {
        mock_core_error_t err = du_connect_cu(env->du, 
                                              env->config.cu_cp.bind_ip,
                                              env->config.cu_cp.port);
        if (err != MOCK_CORE_SUCCESS) {
            strncpy(env->last_error, "Failed to connect DU to CU-CP", sizeof(env->last_error) - 1);
            return MOCK_TEST_ERROR_COMPONENT;
        }
        
        /* Send F1 Setup Request */
        du_send_f1_setup_request(env->du);
        
        if (env->config.verbose) {
            printf("[TestEnv] DU connected to CU-CP, F1 Setup sent\n");
        }
    }
    
    /* Connect CU-UP to CU-CP (E1 Setup) - simulated */
    if (env->cu_up && env->config.verbose) {
        printf("[TestEnv] CU-UP ready for E1 connection\n");
    }
    
    /* NG Setup between gNB and AMF - simulated */
    if (env->gnb_server && env->amf && env->config.verbose) {
        printf("[TestEnv] gNB ready for NG connection to AMF\n");
    }
    
    return MOCK_TEST_SUCCESS;
}

void mock_test_env_stop_core(mock_test_env_t* env) {
    if (!env) return;
    
    /* Stop UPF */
    if (env->upf) {
        upf_stop(env->upf);
        upf_destroy(env->upf);
        env->upf = NULL;
        if (env->config.verbose) {
            printf("[TestEnv] UPF stopped\n");
        }
    }
    
    /* Stop SMF */
    if (env->smf) {
        smf_destroy(env->smf);
        env->smf = NULL;
        if (env->config.verbose) {
            printf("[TestEnv] SMF stopped\n");
        }
    }
    
    /* Stop AMF */
    if (env->amf) {
        amf_stop(env->amf);
        amf_destroy(env->amf);
        env->amf = NULL;
        if (env->config.verbose) {
            printf("[TestEnv] AMF stopped\n");
        }
    }
}

void mock_test_env_stop_gnb(mock_test_env_t* env) {
    if (!env) return;
    
    /* Stop gNB Server */
    if (env->gnb_server) {
        mock_gnb_server_stop();
        env->gnb_server = NULL;
        if (env->config.verbose) {
            printf("[TestEnv] gNB Server stopped\n");
        }
    }
    
    /* Stop XnAP */
    if (env->xnap) {
        xnap_stop(env->xnap);
        xnap_destroy(env->xnap);
        env->xnap = NULL;
        if (env->config.verbose) {
            printf("[TestEnv] XnAP stopped\n");
        }
    }
    
    /* Stop CU-UP */
    if (env->cu_up) {
        cu_up_stop(env->cu_up);
        cu_up_destroy(env->cu_up);
        env->cu_up = NULL;
        if (env->config.verbose) {
            printf("[TestEnv] CU-UP stopped\n");
        }
    }
    
    /* Stop DU */
    if (env->du) {
        du_stop(env->du);
        du_destroy(env->du);
        env->du = NULL;
        if (env->config.verbose) {
            printf("[TestEnv] DU stopped\n");
        }
    }
    
    /* Stop CU-CP */
    if (env->cu_cp) {
        cu_cp_stop(env->cu_cp);
        cu_cp_destroy(env->cu_cp);
        env->cu_cp = NULL;
        if (env->config.verbose) {
            printf("[TestEnv] CU-CP stopped\n");
        }
    }
}

/* ============== UE Management ============== */

mock_test_error_t mock_test_env_register_ue(mock_test_env_t* env, ue_context_t* ue) {
    if (!env || !ue) return MOCK_TEST_ERROR_INVALID_PARAM;
    
    if (env->num_active_ues >= env->config.max_ues) {
        strncpy(env->last_error, "Maximum UE limit reached", sizeof(env->last_error) - 1);
        return MOCK_TEST_ERROR_CAPACITY;
    }
    
    /* Find free slot */
    for (uint32_t i = 0; i < MOCK_TEST_MAX_UES; i++) {
        if (env->ue_instances[i] == NULL) {
            env->ue_instances[i] = ue;
            env->num_active_ues++;
            env->stats.active_ues++;
            
            if (env->config.verbose) {
                printf("[TestEnv] UE registered at index %u\n", i);
            }
            
            return MOCK_TEST_SUCCESS;
        }
    }
    
    strncpy(env->last_error, "No free UE slot", sizeof(env->last_error) - 1);
    return MOCK_TEST_ERROR_CAPACITY;
}

mock_test_error_t mock_test_env_unregister_ue(mock_test_env_t* env, uint32_t ue_id) {
    if (!env) return MOCK_TEST_ERROR_INVALID_PARAM;
    
    for (uint32_t i = 0; i < MOCK_TEST_MAX_UES; i++) {
        if (env->ue_instances[i] && env->ue_instances[i]->ue_id == ue_id) {
            env->ue_instances[i] = NULL;
            env->num_active_ues--;
            env->stats.active_ues--;
            
            if (env->config.verbose) {
                printf("[TestEnv] UE %u unregistered\n", ue_id);
            }
            
            return MOCK_TEST_SUCCESS;
        }
    }
    
    return MOCK_TEST_ERROR_NOT_FOUND;
}

ue_context_t* mock_test_env_get_ue(const mock_test_env_t* env, uint32_t index) {
    if (!env || index >= MOCK_TEST_MAX_UES) return NULL;
    return env->ue_instances[index];
}

uint32_t mock_test_env_get_ue_count(const mock_test_env_t* env) {
    return env ? env->num_active_ues : 0;
}

/* ============== Test Flow Control ============== */

mock_test_error_t mock_test_env_run_registration(mock_test_env_t* env,
                                                  uint32_t ue_index,
                                                  uint32_t timeout_ms) {
    if (!env) return MOCK_TEST_ERROR_INVALID_PARAM;
    
    if (env->state != MOCK_TEST_STATE_READY) {
        strncpy(env->last_error, "Environment not ready", sizeof(env->last_error) - 1);
        return MOCK_TEST_ERROR_NOT_INITIALIZED;
    }
    
    ue_context_t* ue = mock_test_env_get_ue(env, ue_index);
    if (!ue) {
        strncpy(env->last_error, "UE not found", sizeof(env->last_error) - 1);
        return MOCK_TEST_ERROR_NOT_FOUND;
    }
    
    env->state = MOCK_TEST_STATE_RUNNING_TESTS;
    env->stats.total_tests++;
    
    if (env->config.verbose) {
        printf("[TestEnv] Starting registration flow for UE %u (timeout: %u ms)\n",
               ue_index, timeout_ms);
    }
    
    /* 
     * Registration flow simulation:
     * 1. UE sends Registration Request
     * 2. AMF responds with Authentication Request
     * 3. UE sends Authentication Response
     * 4. AMF sends Security Mode Command
     * 5. UE sends Security Mode Complete
     * 6. AMF sends Registration Accept
     */
    
    /* Create UE context in AMF */
    if (env->amf) {
        amf_ue_context_t* amf_ue = amf_create_ue_context(env->amf, ue_index + 1);
        if (amf_ue) {
            if (env->config.verbose) {
                printf("[TestEnv] UE context created in AMF (RAN-UE-ID: %u)\n", ue_index + 1);
            }
        }
    }
    
    /* Simulate successful registration */
    bool registration_success = true;
    
    if (registration_success) {
        env->stats.passed_tests++;
        if (env->config.verbose) {
            printf("[TestEnv] Registration flow completed successfully for UE %u\n", ue_index);
        }
    } else {
        env->stats.failed_tests++;
        strncpy(env->last_error, "Registration failed", sizeof(env->last_error) - 1);
        env->state = MOCK_TEST_STATE_READY;
        return MOCK_TEST_ERROR_PROTOCOL;
    }
    
    env->state = MOCK_TEST_STATE_READY;
    return MOCK_TEST_SUCCESS;
}

mock_test_error_t mock_test_env_run_pdu_session(mock_test_env_t* env,
                                                 uint32_t ue_index,
                                                 uint8_t pdu_session_id,
                                                 uint32_t timeout_ms) {
    if (!env) return MOCK_TEST_ERROR_INVALID_PARAM;
    
    if (env->state != MOCK_TEST_STATE_READY) {
        strncpy(env->last_error, "Environment not ready", sizeof(env->last_error) - 1);
        return MOCK_TEST_ERROR_NOT_INITIALIZED;
    }
    
    ue_context_t* ue = mock_test_env_get_ue(env, ue_index);
    if (!ue) {
        strncpy(env->last_error, "UE not found", sizeof(env->last_error) - 1);
        return MOCK_TEST_ERROR_NOT_FOUND;
    }
    
    env->state = MOCK_TEST_STATE_RUNNING_TESTS;
    env->stats.total_tests++;
    
    if (env->config.verbose) {
        printf("[TestEnv] Starting PDU session flow for UE %u, session %u\n",
               ue_index, pdu_session_id);
    }
    
    /* Create PDU session in SMF */
    if (env->smf) {
        uint64_t amf_ue_id = ue_index + 1;
        smf_pdu_session_t* session = smf_create_session(env->smf, amf_ue_id,
                                                         pdu_session_id,
                                                         1, 0);  /* SST=1, SD=0 */
        if (session) {
            if (env->config.verbose) {
                printf("[TestEnv] PDU session created: ID=%u, IP=%u.%u.%u.%u\n",
                       session->pdu_session_id,
                       (session->ue_ip_addr >> 24) & 0xFF,
                       (session->ue_ip_addr >> 16) & 0xFF,
                       (session->ue_ip_addr >> 8) & 0xFF,
                       session->ue_ip_addr & 0xFF);
            }
            
            /* Create GTP-U tunnel in UPF */
            if (env->upf) {
                upf_tunnel_t* tunnel = upf_create_tunnel(env->upf,
                                                         session->upf_dl_teid,
                                                         session->ue_ip_addr,
                                                         0x7F000001,  /* 127.0.0.1 */
                                                         2152, true);
                if (tunnel && env->config.verbose) {
                    printf("[TestEnv] GTP-U tunnel created: TEID=%u\n", 
                           session->upf_dl_teid);
                }
            }
        }
    }
    
    env->stats.passed_tests++;
    env->state = MOCK_TEST_STATE_READY;
    
    if (env->config.verbose) {
        printf("[TestEnv] PDU session flow completed successfully\n");
    }
    
    return MOCK_TEST_SUCCESS;
}

mock_test_error_t mock_test_env_run_handover(mock_test_env_t* env,
                                              uint32_t ue_index,
                                              uint32_t target_gnb_id,
                                              uint32_t target_cell_id,
                                              uint32_t timeout_ms) {
    if (!env) return MOCK_TEST_ERROR_INVALID_PARAM;
    
    if (env->state != MOCK_TEST_STATE_READY) {
        strncpy(env->last_error, "Environment not ready", sizeof(env->last_error) - 1);
        return MOCK_TEST_ERROR_NOT_INITIALIZED;
    }
    
    ue_context_t* ue = mock_test_env_get_ue(env, ue_index);
    if (!ue) {
        strncpy(env->last_error, "UE not found", sizeof(env->last_error) - 1);
        return MOCK_TEST_ERROR_NOT_FOUND;
    }
    
    env->state = MOCK_TEST_STATE_RUNNING_TESTS;
    env->stats.total_tests++;
    
    if (env->config.verbose) {
        printf("[TestEnv] Starting handover flow for UE %u to gNB %u, cell %u\n",
               ue_index, target_gnb_id, target_cell_id);
    }
    
    /* Simulate Xn handover */
    if (env->xnap) {
        mock_core_error_t err = xnap_initiate_handover(env->xnap,
                                                        ue_index + 1,
                                                        target_gnb_id,
                                                        target_cell_id);
        if (err == MOCK_CORE_SUCCESS) {
            if (env->config.verbose) {
                printf("[TestEnv] Xn handover initiated successfully\n");
            }
        }
    }
    
    env->stats.passed_tests++;
    env->state = MOCK_TEST_STATE_READY;
    
    if (env->config.verbose) {
        printf("[TestEnv] Handover flow completed successfully\n");
    }
    
    return MOCK_TEST_SUCCESS;
}

mock_test_error_t mock_test_env_run_deregistration(mock_test_env_t* env,
                                                    uint32_t ue_index,
                                                    uint32_t timeout_ms) {
    if (!env) return MOCK_TEST_ERROR_INVALID_PARAM;
    
    if (env->state != MOCK_TEST_STATE_READY) {
        strncpy(env->last_error, "Environment not ready", sizeof(env->last_error) - 1);
        return MOCK_TEST_ERROR_NOT_INITIALIZED;
    }
    
    ue_context_t* ue = mock_test_env_get_ue(env, ue_index);
    if (!ue) {
        strncpy(env->last_error, "UE not found", sizeof(env->last_error) - 1);
        return MOCK_TEST_ERROR_NOT_FOUND;
    }
    
    env->state = MOCK_TEST_STATE_RUNNING_TESTS;
    env->stats.total_tests++;
    
    if (env->config.verbose) {
        printf("[TestEnv] Starting deregistration flow for UE %u\n", ue_index);
    }
    
    /* Release PDU sessions */
    if (env->smf) {
        smf_release_session(env->smf, 1, ue_index + 1);
    }
    
    /* Remove UE context from AMF */
    if (env->amf) {
        amf_ue_context_t* amf_ue = amf_find_ue_by_ran_id(env->amf, ue_index + 1);
        if (amf_ue) {
            amf_remove_ue_context(env->amf, amf_ue->amf_ue_ngap_id);
        }
    }
    
    env->stats.passed_tests++;
    env->state = MOCK_TEST_STATE_READY;
    
    if (env->config.verbose) {
        printf("[TestEnv] Deregistration flow completed successfully\n");
    }
    
    return MOCK_TEST_SUCCESS;
}

/* ============== Utility Functions ============== */

const char* mock_test_error_to_string(mock_test_error_t error) {
    switch (error) {
        case MOCK_TEST_SUCCESS: return "Success";
        case MOCK_TEST_ERROR_INVALID_PARAM: return "Invalid parameter";
        case MOCK_TEST_ERROR_MEMORY: return "Memory allocation failed";
        case MOCK_TEST_ERROR_SOCKET: return "Socket error";
        case MOCK_TEST_ERROR_THREAD: return "Thread error";
        case MOCK_TEST_ERROR_TIMEOUT: return "Timeout";
        case MOCK_TEST_ERROR_NOT_INITIALIZED: return "Not initialized";
        case MOCK_TEST_ERROR_ALREADY_RUNNING: return "Already running";
        case MOCK_TEST_ERROR_NOT_RUNNING: return "Not running";
        case MOCK_TEST_ERROR_COMPONENT: return "Component error";
        case MOCK_TEST_ERROR_PROTOCOL: return "Protocol error";
        default: return "Unknown error";
    }
}

const char* mock_test_state_to_string(mock_test_state_t state) {
    switch (state) {
        case MOCK_TEST_STATE_IDLE: return "IDLE";
        case MOCK_TEST_STATE_INITIALIZING: return "INITIALIZING";
        case MOCK_TEST_STATE_STARTING_CORE: return "STARTING_CORE";
        case MOCK_TEST_STATE_STARTING_GNB: return "STARTING_GNB";
        case MOCK_TEST_STATE_CONNECTING: return "CONNECTING";
        case MOCK_TEST_STATE_READY: return "READY";
        case MOCK_TEST_STATE_RUNNING_TESTS: return "RUNNING_TESTS";
        case MOCK_TEST_STATE_STOPPING: return "STOPPING";
        case MOCK_TEST_STATE_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

void mock_test_env_print_status(const mock_test_env_t* env) {
    if (!env) {
        printf("[TestEnv] NULL environment\n");
        return;
    }
    
    printf("\n=== Test Environment Status ===\n");
    printf("State: %s\n", mock_test_state_to_string(env->state));
    printf("Running: %s\n", env->running ? "Yes" : "No");
    printf("Active UEs: %u\n", env->num_active_ues);
    
    printf("\nComponents:\n");
    printf("  AMF: %s\n", env->amf ? "Running" : "Stopped");
    printf("  SMF: %s\n", env->smf ? "Running" : "Stopped");
    printf("  UPF: %s\n", env->upf ? "Running" : "Stopped");
    printf("  CU-CP: %s\n", env->cu_cp ? "Running" : "Stopped");
    printf("  DU: %s\n", env->du ? "Running" : "Stopped");
    printf("  CU-UP: %s\n", env->cu_up ? "Running" : "Stopped");
    printf("  XnAP: %s\n", env->xnap ? "Running" : "Stopped");
    printf("  gNB Server: %s\n", env->gnb_server ? "Running" : "Stopped");
    
    if (env->last_error[0]) {
        printf("\nLast Error: %s\n", env->last_error);
    }
    
    printf("==============================\n\n");
}

void mock_test_env_print_stats(const mock_test_env_t* env) {
    if (!env) {
        printf("[TestEnv] NULL environment\n");
        return;
    }
    
    printf("\n=== Test Environment Statistics ===\n");
    printf("Total Tests: %u\n", env->stats.total_tests);
    printf("Passed: %u\n", env->stats.passed_tests);
    printf("Failed: %u\n", env->stats.failed_tests);
    printf("Active UEs: %u\n", env->stats.active_ues);
    
    if (env->stats.start_time > 0) {
        time_t now = time(NULL);
        printf("Duration: %ld seconds\n", (long)(now - env->stats.start_time));
    }
    
    printf("==================================\n\n");
}