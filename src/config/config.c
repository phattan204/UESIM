/*
 * 5G UE Simulation Application
 * Enhanced Configuration Management Implementation
 */

#include "config.h"
#include "../core/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

uesim_error_t config_init(uesim_config_t* config) {
    if (config == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Initialize configuration structure
    memset(config, 0, sizeof(uesim_config_t));
    
    // Load default configuration
    uesim_error_t result = config_load_default(config);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    config->loaded = false;
    config->load_time = 0;
    
    printf("Configuration management initialized\n");
    return UESIM_SUCCESS;
}

uesim_error_t config_load(uesim_config_t* config, const char* config_file) {
    if (config == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Load default configuration first
    uesim_error_t result = config_load_default(config);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    // If config file is provided, parse it
    if (config_file != NULL && strlen(config_file) > 0) {
        result = config_parse_file(config, config_file);
        if (result != UESIM_SUCCESS) {
            fprintf(stderr, "Failed to parse configuration file: %s\n", config_file);
            return result;
        }
        
        // Store config file path
        strncpy(config->config_file, config_file, CONFIG_MAX_STRING_LEN - 1);
        config->config_file[CONFIG_MAX_STRING_LEN - 1] = '\0';
    }
    
    // Validate configuration
    result = config_validate(config);
    if (result != UESIM_SUCCESS) {
        fprintf(stderr, "Invalid configuration\n");
        return result;
    }
    
    config->loaded = true;
    config->load_time = time(NULL);
    
    printf("Configuration loaded successfully from %s\n", 
           config_file ? config_file : "defaults");
    
    return UESIM_SUCCESS;
}

uesim_error_t config_load_default(uesim_config_t* config) {
    if (config == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // General configuration
    config->general.num_instances = 1;
    config->general.log_level = 2;
    config->general.verbose = false;
    config->general.debug = false;
    strncpy(config->general.log_file, "/var/log/uesim/uesim.log", CONFIG_MAX_STRING_LEN - 1);
    config->general.enable_syslog = false;
    config->general.max_log_file_size = 10; // 10MB
    config->general.log_file_count = 5;
    
    // Network configuration
    strncpy(config->network.gnb_ip, "127.0.0.1", CONFIG_MAX_STRING_LEN - 1);
    config->network.gnb_ngap_port = 38412;
    config->network.gnb_gtpu_port = 2152;
    strncpy(config->network.local_ip, "0.0.0.0", CONFIG_MAX_STRING_LEN - 1);
    config->network.local_ngap_port = 0;
    config->network.local_gtpu_port = 0;
    config->network.connection_timeout = 30;
    config->network.keepalive_interval = 30;
    config->network.enable_ipv6 = false;
    config->network.max_connections = 100;
    
    // UE configuration
    strncpy(config->ue.imsi_prefix, "00101", CONFIG_MAX_STRING_LEN - 1);
    config->ue.imsi_start = 1000000000;
    strncpy(config->ue.msisdn_prefix, "12345", CONFIG_MAX_STRING_LEN - 1);
    config->ue.msisdn_start = 100000;
    config->ue.tac = 1;
    strncpy(config->ue.mcc, "001", 7);
    strncpy(config->ue.mnc, "01", 7);
    strncpy(config->ue.imei, "", CONFIG_MAX_STRING_LEN - 1);
    config->ue.random_imei = true;
    strncpy(config->ue.apn, "internet", CONFIG_MAX_STRING_LEN - 1);
    strncpy(config->ue.apn_type, "default", CONFIG_MAX_STRING_LEN - 1);
    
    // RRC configuration
    config->rrc.registration_timeout = 30;
    config->rrc.establishment_timeout = 30;
    config->rrc.reestablishment_timeout = 30;
    config->rrc.handover_timeout = 30;
    config->rrc.enable_registration = true;
    config->rrc.enable_establishment = true;
    config->rrc.enable_reestablishment = true;
    config->rrc.enable_handover = true;
    config->rrc.max_retransmissions = 5;
    config->rrc.t300_value = 1000;
    config->rrc.t301_value = 1000;
    config->rrc.t302_value = 1000;
    config->rrc.t304_value = 1000;
    config->rrc.t310_value = 1000;
    config->rrc.t311_value = 1000;
    config->rrc.n310_value = 20;
    config->rrc.n311_value = 1;
    
    // PDCP configuration
    config->pdcp.max_pdu_size = 8192;
    config->pdcp.discard_timer = 500;
    config->pdcp.status_report_timer = 1000;
    config->pdcp.ciphering_algorithm = 2; // NEA2
    config->pdcp.integrity_algorithm = 2; // NIA2
    config->pdcp.enable_ciphering = true;
    config->pdcp.enable_integrity = true;
    config->pdcp.max_retransmissions = 5;
    config->pdcp.polling_pdu = 1000;
    config->pdcp.polling_byte = 1000000;
    
    // RLC configuration
    config->rlc.am_window_size = 512;
    config->rlc.um_window_size = 256;
    config->rlc.poll_retransmit_timer = 50;
    config->rlc.reassembly_timer = 100;
    config->rlc.status_prohibit_timer = 25;
    config->rlc.max_retransmissions = 5;
    config->rlc.enable_arq = true;
    config->rlc.buffer_size = 65536;
    config->rlc.max_pdu_size = 8192;
    config->rlc.enable_segmentation = true;
    
    // MAC configuration
    config->mac.harq_processes = 16;
    config->mac.max_harq_retransmissions = 4;
    config->mac.tti_length = 1;
    config->mac.rach_preambles = 64;
    config->mac.rach_response_window = 10;
    config->mac.max_rach_transmissions = 7;
    config->mac.scheduling_requests = 32;
    config->mac.ul_grants = 32;
    config->mac.dl_grants = 32;
    config->mac.enable_harq = true;
    config->mac.enable_rach = true;
    config->mac.tb_size = 1024;
    config->mac.mcs = 10;
    
    // NAS configuration
    config->nas.registration_timer = 30;
    config->nas.authentication_timer = 30;
    config->nas.security_timer = 30;
    config->nas.pdu_session_timer = 30;
    config->nas.enable_periodic_registration = true;
    config->nas.periodic_registration_time = 540; // 9 minutes
    config->nas.max_registration_attempts = 5;
    config->nas.max_authentication_attempts = 5;
    config->nas.ciphering_algorithm = 2; // NEA2
    config->nas.integrity_algorithm = 2; // NIA2
    config->nas.enable_5g_features = true;
    config->nas.max_pdu_sessions = 16;
    config->nas.enable_sms = false;
    config->nas.enable_voice = false;
    
    // Performance configuration
    config->performance.thread_pool_size = 0; // Auto-detect
    config->performance.rx_buffer_size = 65536;
    config->performance.tx_buffer_size = 65536;
    config->performance.memory_pool_size = 67108864; // 64MB
    config->performance.use_memory_pool = true;
    config->performance.max_queue_size = 1000;
    config->performance.worker_threads = 4;
    config->performance.io_threads = 2;
    config->performance.enable_thread_affinity = false;
    config->performance.cpu_affinity_mask = 0;
    config->performance.enable_async_io = true;
    config->performance.io_priority = 0;
    
    // Security configuration
    config->security.stack_protection = true;
    config->security.aslr = true;
    config->security.secure_flags = true;
    config->security.enable_encryption = true;
    config->security.enable_integrity_protection = true;
    strncpy(config->security.key_file, "/etc/uesim/keys.pem", CONFIG_MAX_STRING_LEN - 1);
    config->security.enable_certificate_validation = true;
    strncpy(config->security.cert_file, "/etc/uesim/cert.pem", CONFIG_MAX_STRING_LEN - 1);
    strncpy(config->security.ca_file, "/etc/uesim/ca.pem", CONFIG_MAX_STRING_LEN - 1);
    config->security.enable_tls = false;
    config->security.tls_version = 12; // TLS 1.2
    config->security.enable_pki = false;
    
    // Test configuration
    config->test.enable_tests = false;
    config->test.test_duration = 60;
    strncpy(config->test.report_file, "/var/log/uesim/test_report.txt", CONFIG_MAX_STRING_LEN - 1);
    config->test.enable_performance_tests = false;
    config->test.enable_stress_tests = false;
    config->test.stress_test_duration = 300;
    config->test.stress_test_concurrency = 100;
    config->test.enable_compliance_tests = false;
    strncpy(config->test.test_suite, "default", CONFIG_MAX_STRING_LEN - 1);
    config->test.generate_test_report = true;
    config->test.enable_coverage = false;
    
    return UESIM_SUCCESS;
}

uesim_error_t config_save(uesim_config_t* config, const char* config_file) {
    if (config == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    const char* file_to_save = config_file ? config_file : config->config_file;
    if (file_to_save == NULL || strlen(file_to_save) == 0) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    uesim_error_t result = config_write_file(config, file_to_save);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    printf("Configuration saved to %s\n", file_to_save);
    return UESIM_SUCCESS;
}

uesim_error_t config_validate(uesim_config_t* config) {
    if (config == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    uesim_error_t result;
    
    // Validate each section
    result = config_validate_general(&config->general);
    if (result != UESIM_SUCCESS) return result;
    
    result = config_validate_network(&config->network);
    if (result != UESIM_SUCCESS) return result;
    
    result = config_validate_ue(&config->ue);
    if (result != UESIM_SUCCESS) return result;
    
    result = config_validate_rrc(&config->rrc);
    if (result != UESIM_SUCCESS) return result;
    
    result = config_validate_pdcp(&config->pdcp);
    if (result != UESIM_SUCCESS) return result;
    
    result = config_validate_rlc(&config->rlc);
    if (result != UESIM_SUCCESS) return result;
    
    result = config_validate_mac(&config->mac);
    if (result != UESIM_SUCCESS) return result;
    
    result = config_validate_nas(&config->nas);
    if (result != UESIM_SUCCESS) return result;
    
    result = config_validate_performance(&config->performance);
    if (result != UESIM_SUCCESS) return result;
    
    result = config_validate_security(&config->security);
    if (result != UESIM_SUCCESS) return result;
    
    result = config_validate_test(&config->test);
    if (result != UESIM_SUCCESS) return result;
    
    return UESIM_SUCCESS;
}

uesim_error_t config_cleanup(uesim_config_t* config) {
    if (config == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Clear configuration
    memset(config, 0, sizeof(uesim_config_t));
    
    printf("Configuration management cleaned up\n");
    return UESIM_SUCCESS;
}

// Configuration access functions
uesim_error_t config_get_string(uesim_config_t* config, config_section_t section,
                               const char* key, char* value, size_t value_size) {
    if (config == NULL || key == NULL || value == NULL || value_size == 0) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Initialize value
    value[0] = '\0';
    
    // Get value based on section and key
    switch (section) {
        case CONFIG_SECTION_GENERAL:
            if (strcasecmp(key, "log_file") == 0) {
                strncpy(value, config->general.log_file, value_size - 1);
            }
            break;
            
        case CONFIG_SECTION_NETWORK:
            if (strcasecmp(key, "gnb_ip") == 0) {
                strncpy(value, config->network.gnb_ip, value_size - 1);
            } else if (strcasecmp(key, "local_ip") == 0) {
                strncpy(value, config->network.local_ip, value_size - 1);
            }
            break;
            
        case CONFIG_SECTION_UE:
            if (strcasecmp(key, "imsi_prefix") == 0) {
                strncpy(value, config->ue.imsi_prefix, value_size - 1);
            } else if (strcasecmp(key, "msisdn_prefix") == 0) {
                strncpy(value, config->ue.msisdn_prefix, value_size - 1);
            } else if (strcasecmp(key, "mcc") == 0) {
                strncpy(value, config->ue.mcc, value_size - 1);
            } else if (strcasecmp(key, "mnc") == 0) {
                strncpy(value, config->ue.mnc, value_size - 1);
            } else if (strcasecmp(key, "imei") == 0) {
                strncpy(value, config->ue.imei, value_size - 1);
            } else if (strcasecmp(key, "apn") == 0) {
                strncpy(value, config->ue.apn, value_size - 1);
            } else if (strcasecmp(key, "apn_type") == 0) {
                strncpy(value, config->ue.apn_type, value_size - 1);
            }
            break;
            
        case CONFIG_SECTION_SECURITY:
            if (strcasecmp(key, "key_file") == 0) {
                strncpy(value, config->security.key_file, value_size - 1);
            } else if (strcasecmp(key, "cert_file") == 0) {
                strncpy(value, config->security.cert_file, value_size - 1);
            } else if (strcasecmp(key, "ca_file") == 0) {
                strncpy(value, config->security.ca_file, value_size - 1);
            }
            break;
            
        case CONFIG_SECTION_TEST:
            if (strcasecmp(key, "report_file") == 0) {
                strncpy(value, config->test.report_file, value_size - 1);
            } else if (strcasecmp(key, "test_suite") == 0) {
                strncpy(value, config->test.test_suite, value_size - 1);
            }
            break;
            
        default:
            return UESIM_ERROR_INVALID_PARAM;
    }
    
    value[value_size - 1] = '\0';
    return UESIM_SUCCESS;
}

uesim_error_t config_get_int(uesim_config_t* config, config_section_t section,
                            const char* key, int* value) {
    if (config == NULL || key == NULL || value == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    *value = 0;
    
    // Get value based on section and key
    switch (section) {
        case CONFIG_SECTION_GENERAL:
            if (strcasecmp(key, "num_instances") == 0) {
                *value = config->general.num_instances;
            } else if (strcasecmp(key, "log_level") == 0) {
                *value = config->general.log_level;
            } else if (strcasecmp(key, "max_log_file_size") == 0) {
                *value = config->general.max_log_file_size;
            } else if (strcasecmp(key, "log_file_count") == 0) {
                *value = config->general.log_file_count;
            }
            break;
            
        case CONFIG_SECTION_NETWORK:
            if (strcasecmp(key, "gnb_ngap_port") == 0) {
                *value = config->network.gnb_ngap_port;
            } else if (strcasecmp(key, "gnb_gtpu_port") == 0) {
                *value = config->network.gnb_gtpu_port;
            } else if (strcasecmp(key, "local_ngap_port") == 0) {
                *value = config->network.local_ngap_port;
            } else if (strcasecmp(key, "local_gtpu_port") == 0) {
                *value = config->network.local_gtpu_port;
            } else if (strcasecmp(key, "connection_timeout") == 0) {
                *value = config->network.connection_timeout;
            } else if (strcasecmp(key, "keepalive_interval") == 0) {
                *value = config->network.keepalive_interval;
            } else if (strcasecmp(key, "max_connections") == 0) {
                *value = config->network.max_connections;
            }
            break;
            
        case CONFIG_SECTION_UE:
            if (strcasecmp(key, "imsi_start") == 0) {
                *value = config->ue.imsi_start;
            } else if (strcasecmp(key, "msisdn_start") == 0) {
                *value = config->ue.msisdn_start;
            } else if (strcasecmp(key, "tac") == 0) {
                *value = config->ue.tac;
            }
            break;
            
        case CONFIG_SECTION_RRC:
            if (strcasecmp(key, "registration_timeout") == 0) {
                *value = config->rrc.registration_timeout;
            } else if (strcasecmp(key, "establishment_timeout") == 0) {
                *value = config->rrc.establishment_timeout;
            } else if (strcasecmp(key, "reestablishment_timeout") == 0) {
                *value = config->rrc.reestablishment_timeout;
            } else if (strcasecmp(key, "handover_timeout") == 0) {
                *value = config->rrc.handover_timeout;
            } else if (strcasecmp(key, "max_retransmissions") == 0) {
                *value = config->rrc.max_retransmissions;
            } else if (strcasecmp(key, "t300_value") == 0) {
                *value = config->rrc.t300_value;
            } else if (strcasecmp(key, "t301_value") == 0) {
                *value = config->rrc.t301_value;
            } else if (strcasecmp(key, "t302_value") == 0) {
                *value = config->rrc.t302_value;
            } else if (strcasecmp(key, "t304_value") == 0) {
                *value = config->rrc.t304_value;
            } else if (strcasecmp(key, "t310_value") == 0) {
                *value = config->rrc.t310_value;
            } else if (strcasecmp(key, "t311_value") == 0) {
                *value = config->rrc.t311_value;
            } else if (strcasecmp(key, "n310_value") == 0) {
                *value = config->rrc.n310_value;
            } else if (strcasecmp(key, "n311_value") == 0) {
                *value = config->rrc.n311_value;
            }
            break;
            
        case CONFIG_SECTION_PDCP:
            if (strcasecmp(key, "max_pdu_size") == 0) {
                *value = config->pdcp.max_pdu_size;
            } else if (strcasecmp(key, "discard_timer") == 0) {
                *value = config->pdcp.discard_timer;
            } else if (strcasecmp(key, "status_report_timer") == 0) {
                *value = config->pdcp.status_report_timer;
            } else if (strcasecmp(key, "ciphering_algorithm") == 0) {
                *value = config->pdcp.ciphering_algorithm;
            } else if (strcasecmp(key, "integrity_algorithm") == 0) {
                *value = config->pdcp.integrity_algorithm;
            } else if (strcasecmp(key, "max_retransmissions") == 0) {
                *value = config->pdcp.max_retransmissions;
            } else if (strcasecmp(key, "polling_pdu") == 0) {
                *value = config->pdcp.polling_pdu;
            } else if (strcasecmp(key, "polling_byte") == 0) {
                *value = config->pdcp.polling_byte;
            }
            break;
            
        case CONFIG_SECTION_RLC:
            if (strcasecmp(key, "am_window_size") == 0) {
                *value = config->rlc.am_window_size;
            } else if (strcasecmp(key, "um_window_size") == 0) {
                *value = config->rlc.um_window_size;
            } else if (strcasecmp(key, "poll_retransmit_timer") == 0) {
                *value = config->rlc.poll_retransmit_timer;
            } else if (strcasecmp(key, "reassembly_timer") == 0) {
                *value = config->rlc.reassembly_timer;
            } else if (strcasecmp(key, "status_prohibit_timer") == 0) {
                *value = config->rlc.status_prohibit_timer;
            } else if (strcasecmp(key, "max_retransmissions") == 0) {
                *value = config->rlc.max_retransmissions;
            } else if (strcasecmp(key, "buffer_size") == 0) {
                *value = config->rlc.buffer_size;
            } else if (strcasecmp(key, "max_pdu_size") == 0) {
                *value = config->rlc.max_pdu_size;
            }
            break;
            
        case CONFIG_SECTION_MAC:
            if (strcasecmp(key, "harq_processes") == 0) {
                *value = config->mac.harq_processes;
            } else if (strcasecmp(key, "max_harq_retransmissions") == 0) {
                *value = config->mac.max_harq_retransmissions;
            } else if (strcasecmp(key, "tti_length") == 0) {
                *value = config->mac.tti_length;
            } else if (strcasecmp(key, "rach_preambles") == 0) {
                *value = config->mac.rach_preambles;
            } else if (strcasecmp(key, "rach_response_window") == 0) {
                *value = config->mac.rach_response_window;
            } else if (strcasecmp(key, "max_rach_transmissions") == 0) {
                *value = config->mac.max_rach_transmissions;
            } else if (strcasecmp(key, "scheduling_requests") == 0) {
                *value = config->mac.scheduling_requests;
            } else if (strcasecmp(key, "ul_grants") == 0) {
                *value = config->mac.ul_grants;
            } else if (strcasecmp(key, "dl_grants") == 0) {
                *value = config->mac.dl_grants;
            } else if (strcasecmp(key, "tb_size") == 0) {
                *value = config->mac.tb_size;
            } else if (strcasecmp(key, "mcs") == 0) {
                *value = config->mac.mcs;
            }
            break;
            
        case CONFIG_SECTION_NAS:
            if (strcasecmp(key, "registration_timer") == 0) {
                *value = config->nas.registration_timer;
            } else if (strcasecmp(key, "authentication_timer") == 0) {
                *value = config->nas.authentication_timer;
            } else if (strcasecmp(key, "security_timer") == 0) {
                *value = config->nas.security_timer;
            } else if (strcasecmp(key, "pdu_session_timer") == 0) {
                *value = config->nas.pdu_session_timer;
            } else if (strcasecmp(key, "periodic_registration_time") == 0) {
                *value = config->nas.periodic_registration_time;
            } else if (strcasecmp(key, "max_registration_attempts") == 0) {
                *value = config->nas.max_registration_attempts;
            } else if (strcasecmp(key, "max_authentication_attempts") == 0) {
                *value = config->nas.max_authentication_attempts;
            } else if (strcasecmp(key, "ciphering_algorithm") == 0) {
                *value = config->nas.ciphering_algorithm;
            } else if (strcasecmp(key, "integrity_algorithm") == 0) {
                *value = config->nas.integrity_algorithm;
            } else if (strcasecmp(key, "max_pdu_sessions") == 0) {
                *value = config->nas.max_pdu_sessions;
            }
            break;
            
        case CONFIG_SECTION_PERFORMANCE:
            if (strcasecmp(key, "thread_pool_size") == 0) {
                *value = config->performance.thread_pool_size;
            } else if (strcasecmp(key, "rx_buffer_size") == 0) {
                *value = config->performance.rx_buffer_size;
            } else if (strcasecmp(key, "tx_buffer_size") == 0) {
                *value = config->performance.tx_buffer_size;
            } else if (strcasecmp(key, "memory_pool_size") == 0) {
                *value = config->performance.memory_pool_size;
            } else if (strcasecmp(key, "max_queue_size") == 0) {
                *value = config->performance.max_queue_size;
            } else if (strcasecmp(key, "worker_threads") == 0) {
                *value = config->performance.worker_threads;
            } else if (strcasecmp(key, "io_threads") == 0) {
                *value = config->performance.io_threads;
            } else if (strcasecmp(key, "cpu_affinity_mask") == 0) {
                *value = config->performance.cpu_affinity_mask;
            } else if (strcasecmp(key, "io_priority") == 0) {
                *value = config->performance.io_priority;
            }
            break;
            
        case CONFIG_SECTION_SECURITY:
            if (strcasecmp(key, "tls_version") == 0) {
                *value = config->security.tls_version;
            }
            break;
            
        case CONFIG_SECTION_TEST:
            if (strcasecmp(key, "test_duration") == 0) {
                *value = config->test.test_duration;
            } else if (strcasecmp(key, "stress_test_duration") == 0) {
                *value = config->test.stress_test_duration;
            } else if (strcasecmp(key, "stress_test_concurrency") == 0) {
                *value = config->test.stress_test_concurrency;
            }
            break;
            
        default:
            return UESIM_ERROR_INVALID_PARAM;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t config_get_bool(uesim_config_t* config, config_section_t section,
                             const char* key, bool* value) {
    if (config == NULL || key == NULL || value == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    *value = false;
    
    // Get value based on section and key
    switch (section) {
        case CONFIG_SECTION_GENERAL:
            if (strcasecmp(key, "verbose") == 0) {
                *value = config->general.verbose;
            } else if (strcasecmp(key, "debug") == 0) {
                *value = config->general.debug;
            } else if (strcasecmp(key, "enable_syslog") == 0) {
                *value = config->general.enable_syslog;
            }
            break;
            
        case CONFIG_SECTION_NETWORK:
            if (strcasecmp(key, "enable_ipv6") == 0) {
                *value = config->network.enable_ipv6;
            }
            break;
            
        case CONFIG_SECTION_UE:
            if (strcasecmp(key, "random_imei") == 0) {
                *value = config->ue.random_imei;
            }
            break;
            
        case CONFIG_SECTION_RRC:
            if (strcasecmp(key, "enable_registration") == 0) {
                *value = config->rrc.enable_registration;
            } else if (strcasecmp(key, "enable_establishment") == 0) {
                *value = config->rrc.enable_establishment;
            } else if (strcasecmp(key, "enable_reestablishment") == 0) {
                *value = config->rrc.enable_reestablishment;
            } else if (strcasecmp(key, "enable_handover") == 0) {
                *value = config->rrc.enable_handover;
            }
            break;
            
        case CONFIG_SECTION_PDCP:
            if (strcasecmp(key, "enable_ciphering") == 0) {
                *value = config->pdcp.enable_ciphering;
            } else if (strcasecmp(key, "enable_integrity") == 0) {
                *value = config->pdcp.enable_integrity;
            }
            break;
            
        case CONFIG_SECTION_RLC:
            if (strcasecmp(key, "enable_arq") == 0) {
                *value = config->rlc.enable_arq;
            } else if (strcasecmp(key, "enable_segmentation") == 0) {
                *value = config->rlc.enable_segmentation;
            }
            break;
            
        case CONFIG_SECTION_MAC:
            if (strcasecmp(key, "enable_harq") == 0) {
                *value = config->mac.enable_harq;
            } else if (strcasecmp(key, "enable_rach") == 0) {
                *value = config->mac.enable_rach;
            }
            break;
            
        case CONFIG_SECTION_NAS:
            if (strcasecmp(key, "enable_periodic_registration") == 0) {
                *value = config->nas.enable_periodic_registration;
            } else if (strcasecmp(key, "enable_5g_features") == 0) {
                *value = config->nas.enable_5g_features;
            } else if (strcasecmp(key, "enable_sms") == 0) {
                *value = config->nas.enable_sms;
            } else if (strcasecmp(key, "enable_voice") == 0) {
                *value = config->nas.enable_voice;
            }
            break;
            
        case CONFIG_SECTION_PERFORMANCE:
            if (strcasecmp(key, "use_memory_pool") == 0) {
                *value = config->performance.use_memory_pool;
            } else if (strcasecmp(key, "enable_thread_affinity") == 0) {
                *value = config->performance.enable_thread_affinity;
            } else if (strcasecmp(key, "enable_async_io") == 0) {
                *value = config->performance.enable_async_io;
            }
            break;
            
        case CONFIG_SECTION_SECURITY:
            if (strcasecmp(key, "stack_protection") == 0) {
                *value = config->security.stack_protection;
            } else if (strcasecmp(key, "aslr") == 0) {
                *value = config->security.aslr;
            } else if (strcasecmp(key, "secure_flags") == 0) {
                *value = config->security.secure_flags;
            } else if (strcasecmp(key, "enable_encryption") == 0) {
                *value = config->security.enable_encryption;
            } else if (strcasecmp(key, "enable_integrity_protection") == 0) {
                *value = config->security.enable_integrity_protection;
            } else if (strcasecmp(key, "enable_certificate_validation") == 0) {
                *value = config->security.enable_certificate_validation;
            } else if (strcasecmp(key, "enable_tls") == 0) {
                *value = config->security.enable_tls;
            } else if (strcasecmp(key, "enable_pki") == 0) {
                *value = config->security.enable_pki;
            }
            break;
            
        case CONFIG_SECTION_TEST:
            if (strcasecmp(key, "enable_tests") == 0) {
                *value = config->test.enable_tests;
            } else if (strcasecmp(key, "enable_performance_tests") == 0) {
                *value = config->test.enable_performance_tests;
            } else if (strcasecmp(key, "enable_stress_tests") == 0) {
                *value = config->test.enable_stress_tests;
            } else if (strcasecmp(key, "enable_compliance_tests") == 0) {
                *value = config->test.enable_compliance_tests;
            } else if (strcasecmp(key, "generate_test_report") == 0) {
                *value = config->test.generate_test_report;
            } else if (strcasecmp(key, "enable_coverage") == 0) {
                *value = config->test.enable_coverage;
            }
            break;
            
        default:
            return UESIM_ERROR_INVALID_PARAM;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t config_set_string(uesim_config_t* config, config_section_t section,
                               const char* key, const char* value) {
    if (config == NULL || key == NULL || value == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Set value based on section and key
    switch (section) {
        case CONFIG_SECTION_GENERAL:
            if (strcasecmp(key, "log_file") == 0) {
                strncpy(config->general.log_file, value, CONFIG_MAX_STRING_LEN - 1);
                config->general.log_file[CONFIG_MAX_STRING_LEN - 1] = '\0';
            }
            break;
            
        case CONFIG_SECTION_NETWORK:
            if (strcasecmp(key, "gnb_ip") == 0) {
                strncpy(config->network.gnb_ip, value, CONFIG_MAX_STRING_LEN - 1);
                config->network.gnb_ip[CONFIG_MAX_STRING_LEN - 1] = '\0';
            } else if (strcasecmp(key, "local_ip") == 0) {
                strncpy(config->network.local_ip, value, CONFIG_MAX_STRING_LEN - 1);
                config->network.local_ip[CONFIG_MAX_STRING_LEN - 1] = '\0';
            }
            break;
            
        case CONFIG_SECTION_UE:
            if (strcasecmp(key, "imsi_prefix") == 0) {
                strncpy(config->ue.imsi_prefix, value, CONFIG_MAX_STRING_LEN - 1);
                config->ue.imsi_prefix[CONFIG_MAX_STRING_LEN - 1] = '\0';
            } else if (strcasecmp(key, "msisdn_prefix") == 0) {
                strncpy(config->ue.msisdn_prefix, value, CONFIG_MAX_STRING_LEN - 1);
                config->ue.msisdn_prefix[CONFIG_MAX_STRING_LEN - 1] = '\0';
            } else if (strcasecmp(key, "mcc") == 0) {
                strncpy(config->ue.mcc, value, 7);
                config->ue.mcc[7] = '\0';
            } else if (strcasecmp(key, "mnc") == 0) {
                strncpy(config->ue.mnc, value, 7);
                config->ue.mnc[7] = '\0';
            } else if (strcasecmp(key, "imei") == 0) {
                strncpy(config->ue.imei, value, CONFIG_MAX_STRING_LEN - 1);
                config->ue.imei[CONFIG_MAX_STRING_LEN - 1] = '\0';
            } else if (strcasecmp(key, "apn") == 0) {
                strncpy(config->ue.apn, value, CONFIG_MAX_STRING_LEN - 1);
                config->ue.apn[CONFIG_MAX_STRING_LEN - 1] = '\0';
            } else if (strcasecmp(key, "apn_type") == 0) {
                strncpy(config->ue.apn_type, value, CONFIG_MAX_STRING_LEN - 1);
                config->ue.apn_type[CONFIG_MAX_STRING_LEN - 1] = '\0';
            }
            break;
            
        case CONFIG_SECTION_SECURITY:
            if (strcasecmp(key, "key_file") == 0) {
                strncpy(config->security.key_file, value, CONFIG_MAX_STRING_LEN - 1);
                config->security.key_file[CONFIG_MAX_STRING_LEN - 1] = '\0';
            } else if (strcasecmp(key, "cert_file") == 0) {
                strncpy(config->security.cert_file, value, CONFIG_MAX_STRING_LEN - 1);
                config->security.cert_file[CONFIG_MAX_STRING_LEN - 1] = '\0';
            } else if (strcasecmp(key, "ca_file") == 0) {
                strncpy(config->security.ca_file, value, CONFIG_MAX_STRING_LEN - 1);
                config->security.ca_file[CONFIG_MAX_STRING_LEN - 1] = '\0';
            }
            break;
            
        case CONFIG_SECTION_TEST:
            if (strcasecmp(key, "report_file") == 0) {
                strncpy(config->test.report_file, value, CONFIG_MAX_STRING_LEN - 1);
                config->test.report_file[CONFIG_MAX_STRING_LEN - 1] = '\0';
            } else if (strcasecmp(key, "test_suite") == 0) {
                strncpy(config->test.test_suite, value, CONFIG_MAX_STRING_LEN - 1);
                config->test.test_suite[CONFIG_MAX_STRING_LEN - 1] = '\0';
            }
            break;
            
        default:
            return UESIM_ERROR_INVALID_PARAM;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t config_set_int(uesim_config_t* config, config_section_t section,
                            const char* key, int value) {
    if (config == NULL || key == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Set value based on section and key
    switch (section) {
        case CONFIG_SECTION_GENERAL:
            if (strcasecmp(key, "num_instances") == 0) {
                config->general.num_instances = value;
            } else if (strcasecmp(key, "log_level") == 0) {
                config->general.log_level = value;
            } else if (strcasecmp(key, "max_log_file_size") == 0) {
                config->general.max_log_file_size = value;
            } else if (strcasecmp(key, "log_file_count") == 0) {
                config->general.log_file_count = value;
            }
            break;
            
        case CONFIG_SECTION_NETWORK:
            if (strcasecmp(key, "gnb_ngap_port") == 0) {
                config->network.gnb_ngap_port = value;
            } else if (strcasecmp(key, "gnb_gtpu_port") == 0) {
                config->network.gnb_gtpu_port = value;
            } else if (strcasecmp(key, "local_ngap_port") == 0) {
                config->network.local_ngap_port = value;
            } else if (strcasecmp(key, "local_gtpu_port") == 0) {
                config->network.local_gtpu_port = value;
            } else if (strcasecmp(key, "connection_timeout") == 0) {
                config->network.connection_timeout = value;
            } else if (strcasecmp(key, "keepalive_interval") == 0) {
                config->network.keepalive_interval = value;
            } else if (strcasecmp(key, "max_connections") == 0) {
                config->network.max_connections = value;
            }
            break;
            
        case CONFIG_SECTION_UE:
            if (strcasecmp(key, "imsi_start") == 0) {
                config->ue.imsi_start = value;
            } else if (strcasecmp(key, "msisdn_start") == 0) {
                config->ue.msisdn_start = value;
            } else if (strcasecmp(key, "tac") == 0) {
                config->ue.tac = value;
            }
            break;
            
        case CONFIG_SECTION_RRC:
            if (strcasecmp(key, "registration_timeout") == 0) {
                config->rrc.registration_timeout = value;
            } else if (strcasecmp(key, "establishment_timeout") == 0) {
                config->rrc.establishment_timeout = value;
            } else if (strcasecmp(key, "reestablishment_timeout") == 0) {
                config->rrc.reestablishment_timeout = value;
            } else if (strcasecmp(key, "handover_timeout") == 0) {
                config->rrc.handover_timeout = value;
            } else if (strcasecmp(key, "max_retransmissions") == 0) {
                config->rrc.max_retransmissions = value;
            } else if (strcasecmp(key, "t300_value") == 0) {
                config->rrc.t300_value = value;
            } else if (strcasecmp(key, "t301_value") == 0) {
                config->rrc.t301_value = value;
            } else if (strcasecmp(key, "t302_value") == 0) {
                config->rrc.t302_value = value;
            } else if (strcasecmp(key, "t304_value") == 0) {
                config->rrc.t304_value = value;
            } else if (strcasecmp(key, "t310_value") == 0) {
                config->rrc.t310_value = value;
            } else if (strcasecmp(key, "t311_value") == 0) {
                config->rrc.t311_value = value;
            } else if (strcasecmp(key, "n310_value") == 0) {
                config->rrc.n310_value = value;
            } else if (strcasecmp(key, "n311_value") == 0) {
                config->rrc.n311_value = value;
            }
            break;
            
        case CONFIG_SECTION_PDCP:
            if (strcasecmp(key, "max_pdu_size") == 0) {
                config->pdcp.max_pdu_size = value;
            } else if (strcasecmp(key, "discard_timer") == 0) {
                config->pdcp.discard_timer = value;
            } else if (strcasecmp(key, "status_report_timer") == 0) {
                config->pdcp.status_report_timer = value;
            } else if (strcasecmp(key, "ciphering_algorithm") == 0) {
                config->pdcp.ciphering_algorithm = value;
            } else if (strcasecmp(key, "integrity_algorithm") == 0) {
                config->pdcp.integrity_algorithm = value;
            } else if (strcasecmp(key, "max_retransmissions") == 0) {
                config->pdcp.max_retransmissions = value;
            } else if (strcasecmp(key, "polling_pdu") == 0) {
                config->pdcp.polling_pdu = value;
            } else if (strcasecmp(key, "polling_byte") == 0) {
                config->pdcp.polling_byte = value;
            }
            break;
            
        case CONFIG_SECTION_RLC:
            if (strcasecmp(key, "am_window_size") == 0) {
                config->rlc.am_window_size = value;
            } else if (strcasecmp(key, "um_window_size") == 0) {
                config->rlc.um_window_size = value;
            } else if (strcasecmp(key, "poll_retransmit_timer") == 0) {
                config->rlc.poll_retransmit_timer = value;
            } else if (strcasecmp(key, "reassembly_timer") == 0) {
                config->rlc.reassembly_timer = value;
            } else if (strcasecmp(key, "status_prohibit_timer") == 0) {
                config->rlc.status_prohibit_timer = value;
            } else if (strcasecmp(key, "max_retransmissions") == 0) {
                config->rlc.max_retransmissions = value;
            } else if (strcasecmp(key, "buffer_size") == 0) {
                config->rlc.buffer_size = value;
            } else if (strcasecmp(key, "max_pdu_size") == 0) {
                config->rlc.max_pdu_size = value;
            }
            break;
            
        case CONFIG_SECTION_MAC:
            if (strcasecmp(key, "harq_processes") == 0) {
                config->mac.harq_processes = value;
            } else if (strcasecmp(key, "max_harq_retransmissions") == 0) {
                config->mac.max_harq_retransmissions = value;
            } else if (strcasecmp(key, "tti_length") == 0) {
                config->mac.tti_length = value;
            } else if (strcasecmp(key, "rach_preambles") == 0) {
                config->mac.rach_preambles = value;
            } else if (strcasecmp(key, "rach_response_window") == 0) {
                config->mac.rach_response_window = value;
            } else if (strcasecmp(key, "max_rach_transmissions") == 0) {
                config->mac.max_rach_transmissions = value;
            } else if (strcasecmp(key, "scheduling_requests") == 0) {
                config->mac.scheduling_requests = value;
            } else if (strcasecmp(key, "ul_grants") == 0) {
                config->mac.ul_grants = value;
            } else if (strcasecmp(key, "dl_grants") == 0) {
                config->mac.dl_grants = value;
            } else if (strcasecmp(key, "tb_size") == 0) {
                config->mac.tb_size = value;
            } else if (strcasecmp(key, "mcs") == 0) {
                config->mac.mcs = value;
            }
            break;
            
        case CONFIG_SECTION_NAS:
            if (strcasecmp(key, "registration_timer") == 0) {
                config->nas.registration_timer = value;
            } else if (strcasecmp(key, "authentication_timer") == 0) {
                config->nas.authentication_timer = value;
            } else if (strcasecmp(key, "security_timer") == 0) {
                config->nas.security_timer = value;
            } else if (strcasecmp(key, "pdu_session_timer") == 0) {
                config->nas.pdu_session_timer = value;
            } else if (strcasecmp(key, "periodic_registration_time") == 0) {
                config->nas.periodic_registration_time = value;
            } else if (strcasecmp(key, "max_registration_attempts") == 0) {
                config->nas.max_registration_attempts = value;
            } else if (strcasecmp(key, "max_authentication_attempts") == 0) {
                config->nas.max_authentication_attempts = value;
            } else if (strcasecmp(key, "ciphering_algorithm") == 0) {
                config->nas.ciphering_algorithm = value;
            } else if (strcasecmp(key, "integrity_algorithm") == 0) {
                config->nas.integrity_algorithm = value;
            } else if (strcasecmp(key, "max_pdu_sessions") == 0) {
                config->nas.max_pdu_sessions = value;
            }
            break;
            
        case CONFIG_SECTION_PERFORMANCE:
            if (strcasecmp(key, "thread_pool_size") == 0) {
                config->performance.thread_pool_size = value;
            } else if (strcasecmp(key, "rx_buffer_size") == 0) {
                config->performance.rx_buffer_size = value;
            } else if (strcasecmp(key, "tx_buffer_size") == 0) {
                config->performance.tx_buffer_size = value;
            } else if (strcasecmp(key, "memory_pool_size") == 0) {
                config->performance.memory_pool_size = value;
            } else if (strcasecmp(key, "max_queue_size") == 0) {
                config->performance.max_queue_size = value;
            } else if (strcasecmp(key, "worker_threads") == 0) {
                config->performance.worker_threads = value;
            } else if (strcasecmp(key, "io_threads") == 0) {
                config->performance.io_threads = value;
            } else if (strcasecmp(key, "cpu_affinity_mask") == 0) {
                config->performance.cpu_affinity_mask = value;
            } else if (strcasecmp(key, "io_priority") == 0) {
                config->performance.io_priority = value;
            }
            break;
            
        case CONFIG_SECTION_SECURITY:
            if (strcasecmp(key, "tls_version") == 0) {
                config->security.tls_version = value;
            }
            break;
            
        case CONFIG_SECTION_TEST:
            if (strcasecmp(key, "test_duration") == 0) {
                config->test.test_duration = value;
            } else if (strcasecmp(key, "stress_test_duration") == 0) {
                config->test.stress_test_duration = value;
            } else if (strcasecmp(key, "stress_test_concurrency") == 0) {
                config->test.stress_test_concurrency = value;
            }
            break;
            
        default:
            return UESIM_ERROR_INVALID_PARAM;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t config_set_bool(uesim_config_t* config, config_section_t section,
                             const char* key, bool value) {
    if (config == NULL || key == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Set value based on section and key
    switch (section) {
        case CONFIG_SECTION_GENERAL:
            if (strcasecmp(key, "verbose") == 0) {
                config->general.verbose = value;
            } else if (strcasecmp(key, "debug") == 0) {
                config->general.debug = value;
            } else if (strcasecmp(key, "enable_syslog") == 0) {
                config->general.enable_syslog = value;
            }
            break;
            
        case CONFIG_SECTION_NETWORK:
            if (strcasecmp(key, "enable_ipv6") == 0) {
                config->network.enable_ipv6 = value;
            }
            break;
            
        case CONFIG_SECTION_UE:
            if (strcasecmp(key, "random_imei") == 0) {
                config->ue.random_imei = value;
            }
            break;
            
        case CONFIG_SECTION_RRC:
            if (strcasecmp(key, "enable_registration") == 0) {
                config->rrc.enable_registration = value;
            } else if (strcasecmp(key, "enable_establishment") == 0) {
                config->rrc.enable_establishment = value;
            } else if (strcasecmp(key, "enable_reestablishment") == 0) {
                config->rrc.enable_reestablishment = value;
            } else if (strcasecmp(key, "enable_handover") == 0) {
                config->rrc.enable_handover = value;
            }
            break;
            
        case CONFIG_SECTION_PDCP:
            if (strcasecmp(key, "enable_ciphering") == 0) {
                config->pdcp.enable_ciphering = value;
            } else if (strcasecmp(key, "enable_integrity") == 0) {
                config->pdcp.enable_integrity = value;
            }
            break;
            
        case CONFIG_SECTION_RLC:
            if (strcasecmp(key, "enable_arq") == 0) {
                config->rlc.enable_arq = value;
            } else if (strcasecmp(key, "enable_segmentation") == 0) {
                config->rlc.enable_segmentation = value;
            }
            break;
            
        case CONFIG_SECTION_MAC:
            if (strcasecmp(key, "enable_harq") == 0) {
                config->mac.enable_harq = value;
            } else if (strcasecmp(key, "enable_rach") == 0) {
                config->mac.enable_rach = value;
            }
            break;
            
        case CONFIG_SECTION_NAS:
            if (strcasecmp(key, "enable_periodic_registration") == 0) {
                config->nas.enable_periodic_registration = value;
            } else if (strcasecmp(key, "enable_5g_features") == 0) {
                config->nas.enable_5g_features = value;
            } else if (strcasecmp(key, "enable_sms") == 0) {
                config->nas.enable_sms = value;
            } else if (strcasecmp(key, "enable_voice") == 0) {
                config->nas.enable_voice = value;
            }
            break;
            
        case CONFIG_SECTION_PERFORMANCE:
            if (strcasecmp(key, "use_memory_pool") == 0) {
                config->performance.use_memory_pool = value;
            } else if (strcasecmp(key, "enable_thread_affinity") == 0) {
                config->performance.enable_thread_affinity = value;
            } else if (strcasecmp(key, "enable_async_io") == 0) {
                config->performance.enable_async_io = value;
            }
            break;
            
        case CONFIG_SECTION_SECURITY:
            if (strcasecmp(key, "stack_protection") == 0) {
                config->security.stack_protection = value;
            } else if (strcasecmp(key, "aslr") == 0) {
                config->security.aslr = value;
            } else if (strcasecmp(key, "secure_flags") == 0) {
                config->security.secure_flags = value;
            } else if (strcasecmp(key, "enable_encryption") == 0) {
                config->security.enable_encryption = value;
            } else if (strcasecmp(key, "enable_integrity_protection") == 0) {
                config->security.enable_integrity_protection = value;
            } else if (strcasecmp(key, "enable_certificate_validation") == 0) {
                config->security.enable_certificate_validation = value;
            } else if (strcasecmp(key, "enable_tls") == 0) {
                config->security.enable_tls = value;
            } else if (strcasecmp(key, "enable_pki") == 0) {
                config->security.enable_pki = value;
            }
            break;
            
        case CONFIG_SECTION_TEST:
            if (strcasecmp(key, "enable_tests") == 0) {
                config->test.enable_tests = value;
            } else if (strcasecmp(key, "enable_performance_tests") == 0) {
                config->test.enable_performance_tests = value;
            } else if (strcasecmp(key, "enable_stress_tests") == 0) {
                config->test.enable_stress_tests = value;
            } else if (strcasecmp(key, "enable_compliance_tests") == 0) {
                config->test.enable_compliance_tests = value;
            } else if (strcasecmp(key, "generate_test_report") == 0) {
                config->test.generate_test_report = value;
            } else if (strcasecmp(key, "enable_coverage") == 0) {
                config->test.enable_coverage = value;
            }
            break;
            
        default:
            return UESIM_ERROR_INVALID_PARAM;
    }
    
    return UESIM_SUCCESS;
}

// Configuration section access
config_general_t* config_get_general(uesim_config_t* config) {
    return config ? &config->general : NULL;
}

config_network_t* config_get_network(uesim_config_t* config) {
    return config ? &config->network : NULL;
}

config_ue_t* config_get_ue(uesim_config_t* config) {
    return config ? &config->ue : NULL;
}

config_rrc_t* config_get_rrc(uesim_config_t* config) {
    return config ? &config->rrc : NULL;
}

config_pdcp_t* config_get_pdcp(uesim_config_t* config) {
    return config ? &config->pdcp : NULL;
}

config_rlc_t* config_get_rlc(uesim_config_t* config) {
    return config ? &config->rlc : NULL;
}

config_mac_t* config_get_mac(uesim_config_t* config) {
    return config ? &config->mac : NULL;
}

config_nas_t* config_get_nas(uesim_config_t* config) {
    return config ? &config->nas : NULL;
}

config_performance_t* config_get_performance(uesim_config_t* config) {
    return config ? &config->performance : NULL;
}

config_security_t* config_get_security(uesim_config_t* config) {
    return config ? &config->security : NULL;
}

config_test_t* config_get_test(uesim_config_t* config) {
    return config ? &config->test : NULL;
}

// Configuration validation functions
uesim_error_t config_validate_general(config_general_t* general) {
    if (general == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate log level
    if (general->log_level < 0 || general->log_level > CONFIG_MAX_LOG_LEVEL) {
        fprintf(stderr, "Invalid log level: %d\n", general->log_level);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate number of instances
    if (general->num_instances <= 0 || general->num_instances > CONFIG_MAX_UE_INSTANCES) {
        fprintf(stderr, "Invalid number of instances: %d\n", general->num_instances);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate log file size
    if (general->max_log_file_size <= 0) {
        fprintf(stderr, "Invalid max log file size: %d\n", general->max_log_file_size);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate log file count
    if (general->log_file_count <= 0) {
        fprintf(stderr, "Invalid log file count: %d\n", general->log_file_count);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t config_validate_network(config_network_t* network) {
    if (network == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate ports
    if (network->gnb_ngap_port <= 0 || network->gnb_ngap_port > 65535) {
        fprintf(stderr, "Invalid gNB NGAP port: %d\n", network->gnb_ngap_port);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (network->gnb_gtpu_port <= 0 || network->gnb_gtpu_port > 65535) {
        fprintf(stderr, "Invalid gNB GTP-U port: %d\n", network->gnb_gtpu_port);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (network->local_ngap_port < 0 || network->local_ngap_port > 65535) {
        fprintf(stderr, "Invalid local NGAP port: %d\n", network->local_ngap_port);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (network->local_gtpu_port < 0 || network->local_gtpu_port > 65535) {
        fprintf(stderr, "Invalid local GTP-U port: %d\n", network->local_gtpu_port);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate timeouts
    if (network->connection_timeout <= 0) {
        fprintf(stderr, "Invalid connection timeout: %d\n", network->connection_timeout);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (network->keepalive_interval <= 0) {
        fprintf(stderr, "Invalid keepalive interval: %d\n", network->keepalive_interval);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate max connections
    if (network->max_connections <= 0) {
        fprintf(stderr, "Invalid max connections: %d\n", network->max_connections);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t config_validate_ue(config_ue_t* ue) {
    if (ue == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate MCC (3 digits)
    if (strlen(ue->mcc) != 3) {
        fprintf(stderr, "Invalid MCC length: %s\n", ue->mcc);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate MNC (2 or 3 digits)
    size_t mnc_len = strlen(ue->mnc);
    if (mnc_len != 2 && mnc_len != 3) {
        fprintf(stderr, "Invalid MNC length: %s\n", ue->mnc);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate TAC
    if (ue->tac == 0) {
        fprintf(stderr, "Invalid TAC: %u\n", ue->tac);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t config_validate_rrc(config_rrc_t* rrc) {
    if (rrc == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate timeouts
    if (rrc->registration_timeout <= 0) {
        fprintf(stderr, "Invalid registration timeout: %d\n", rrc->registration_timeout);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (rrc->establishment_timeout <= 0) {
        fprintf(stderr, "Invalid establishment timeout: %d\n", rrc->establishment_timeout);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (rrc->reestablishment_timeout <= 0) {
        fprintf(stderr, "Invalid re-establishment timeout: %d\n", rrc->reestablishment_timeout);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (rrc->handover_timeout <= 0) {
        fprintf(stderr, "Invalid handover timeout: %d\n", rrc->handover_timeout);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate max retransmissions
    if (rrc->max_retransmissions <= 0) {
        fprintf(stderr, "Invalid max retransmissions: %d\n", rrc->max_retransmissions);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate timer values
    if (rrc->t300_value <= 0) {
        fprintf(stderr, "Invalid T300 value: %d\n", rrc->t300_value);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (rrc->t301_value <= 0) {
        fprintf(stderr, "Invalid T301 value: %d\n", rrc->t301_value);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (rrc->t302_value <= 0) {
        fprintf(stderr, "Invalid T302 value: %d\n", rrc->t302_value);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (rrc->t304_value <= 0) {
        fprintf(stderr, "Invalid T304 value: %d\n", rrc->t304_value);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (rrc->t310_value <= 0) {
        fprintf(stderr, "Invalid T310 value: %d\n", rrc->t310_value);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (rrc->t311_value <= 0) {
        fprintf(stderr, "Invalid T311 value: %d\n", rrc->t311_value);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate counter values
    if (rrc->n310_value <= 0) {
        fprintf(stderr, "Invalid N310 value: %d\n", rrc->n310_value);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (rrc->n311_value <= 0) {
        fprintf(stderr, "Invalid N311 value: %d\n", rrc->n311_value);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t config_validate_pdcp(config_pdcp_t* pdcp) {
    if (pdcp == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate PDU size
    if (pdcp->max_pdu_size <= 0) {
        fprintf(stderr, "Invalid max PDU size: %d\n", pdcp->max_pdu_size);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate timer values
    if (pdcp->discard_timer <= 0) {
        fprintf(stderr, "Invalid discard timer: %d\n", pdcp->discard_timer);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pdcp->status_report_timer <= 0) {
        fprintf(stderr, "Invalid status report timer: %d\n", pdcp->status_report_timer);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate algorithm values
    if (pdcp->ciphering_algorithm < 0 || pdcp->ciphering_algorithm > 3) {
        fprintf(stderr, "Invalid ciphering algorithm: %d\n", pdcp->ciphering_algorithm);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pdcp->integrity_algorithm < 0 || pdcp->integrity_algorithm > 3) {
        fprintf(stderr, "Invalid integrity algorithm: %d\n", pdcp->integrity_algorithm);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate max retransmissions
    if (pdcp->max_retransmissions <= 0) {
        fprintf(stderr, "Invalid max retransmissions: %d\n", pdcp->max_retransmissions);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate polling values
    if (pdcp->polling_pdu <= 0) {
        fprintf(stderr, "Invalid polling PDU: %d\n", pdcp->polling_pdu);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (pdcp->polling_byte <= 0) {
        fprintf(stderr, "Invalid polling byte: %d\n", pdcp->polling_byte);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t config_validate_rlc(config_rlc_t* rlc) {
    if (rlc == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate window sizes
    if (rlc->am_window_size <= 0) {
        fprintf(stderr, "Invalid AM window size: %d\n", rlc->am_window_size);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (rlc->um_window_size <= 0) {
        fprintf(stderr, "Invalid UM window size: %d\n", rlc->um_window_size);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate timer values
    if (rlc->poll_retransmit_timer <= 0) {
        fprintf(stderr, "Invalid poll retransmit timer: %d\n", rlc->poll_retransmit_timer);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (rlc->reassembly_timer <= 0) {
        fprintf(stderr, "Invalid reassembly timer: %d\n", rlc->reassembly_timer);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (rlc->status_prohibit_timer <= 0) {
        fprintf(stderr, "Invalid status prohibit timer: %d\n", rlc->status_prohibit_timer);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate max retransmissions
    if (rlc->max_retransmissions <= 0) {
        fprintf(stderr, "Invalid max retransmissions: %d\n", rlc->max_retransmissions);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate buffer size
    if (rlc->buffer_size <= 0) {
        fprintf(stderr, "Invalid buffer size: %d\n", rlc->buffer_size);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate PDU size
    if (rlc->max_pdu_size <= 0) {
        fprintf(stderr, "Invalid max PDU size: %d\n", rlc->max_pdu_size);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t config_validate_mac(config_mac_t* mac) {
    if (mac == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate HARQ processes
    if (mac->harq_processes <= 0 || mac->harq_processes > 16) {
        fprintf(stderr, "Invalid HARQ processes: %d\n", mac->harq_processes);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate max HARQ retransmissions
    if (mac->max_harq_retransmissions <= 0) {
        fprintf(stderr, "Invalid max HARQ retransmissions: %d\n", mac->max_harq_retransmissions);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate TTI length
    if (mac->tti_length <= 0) {
        fprintf(stderr, "Invalid TTI length: %d\n", mac->tti_length);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate RACH parameters
    if (mac->rach_preambles <= 0) {
        fprintf(stderr, "Invalid RACH preambles: %d\n", mac->rach_preambles);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (mac->rach_response_window <= 0) {
        fprintf(stderr, "Invalid RACH response window: %d\n", mac->rach_response_window);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (mac->max_rach_transmissions <= 0) {
        fprintf(stderr, "Invalid max RACH transmissions: %d\n", mac->max_rach_transmissions);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate scheduling parameters
    if (mac->scheduling_requests <= 0) {
        fprintf(stderr, "Invalid scheduling requests: %d\n", mac->scheduling_requests);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (mac->ul_grants <= 0) {
        fprintf(stderr, "Invalid UL grants: %d\n", mac->ul_grants);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (mac->dl_grants <= 0) {
        fprintf(stderr, "Invalid DL grants: %d\n", mac->dl_grants);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate TB size and MCS
    if (mac->tb_size <= 0) {
        fprintf(stderr, "Invalid TB size: %d\n", mac->tb_size);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (mac->mcs < 0 || mac->mcs > 28) { // 5G MCS range
        fprintf(stderr, "Invalid MCS: %d\n", mac->mcs);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t config_validate_nas(config_nas_t* nas) {
    if (nas == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate timers
    if (nas->registration_timer <= 0) {
        fprintf(stderr, "Invalid registration timer: %d\n", nas->registration_timer);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (nas->authentication_timer <= 0) {
        fprintf(stderr, "Invalid authentication timer: %d\n", nas->authentication_timer);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (nas->security_timer <= 0) {
        fprintf(stderr, "Invalid security timer: %d\n", nas->security_timer);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (nas->pdu_session_timer <= 0) {
        fprintf(stderr, "Invalid PDU session timer: %d\n", nas->pdu_session_timer);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate periodic registration time
    if (nas->periodic_registration_time <= 0) {
        fprintf(stderr, "Invalid periodic registration time: %d\n", nas->periodic_registration_time);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate max attempts
    if (nas->max_registration_attempts <= 0) {
        fprintf(stderr, "Invalid max registration attempts: %d\n", nas->max_registration_attempts);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (nas->max_authentication_attempts <= 0) {
        fprintf(stderr, "Invalid max authentication attempts: %d\n", nas->max_authentication_attempts);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate algorithm values
    if (nas->ciphering_algorithm < 0 || nas->ciphering_algorithm > 3) {
        fprintf(stderr, "Invalid ciphering algorithm: %d\n", nas->ciphering_algorithm);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (nas->integrity_algorithm < 0 || nas->integrity_algorithm > 3) {
        fprintf(stderr, "Invalid integrity algorithm: %d\n", nas->integrity_algorithm);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate max PDU sessions
    if (nas->max_pdu_sessions <= 0 || nas->max_pdu_sessions > 16) {
        fprintf(stderr, "Invalid max PDU sessions: %d\n", nas->max_pdu_sessions);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t config_validate_performance(config_performance_t* performance) {
    if (performance == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate buffer sizes
    if (performance->rx_buffer_size <= 0) {
        fprintf(stderr, "Invalid RX buffer size: %d\n", performance->rx_buffer_size);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (performance->tx_buffer_size <= 0) {
        fprintf(stderr, "Invalid TX buffer size: %d\n", performance->tx_buffer_size);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate memory pool size
    if (performance->memory_pool_size <= 0) {
        fprintf(stderr, "Invalid memory pool size: %d\n", performance->memory_pool_size);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate max queue size
    if (performance->max_queue_size <= 0) {
        fprintf(stderr, "Invalid max queue size: %d\n", performance->max_queue_size);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate thread counts
    if (performance->worker_threads < 0) {
        fprintf(stderr, "Invalid worker threads: %d\n", performance->worker_threads);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (performance->io_threads < 0) {
        fprintf(stderr, "Invalid I/O threads: %d\n", performance->io_threads);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate CPU affinity mask
    if (performance->cpu_affinity_mask < 0) {
        fprintf(stderr, "Invalid CPU affinity mask: %d\n", performance->cpu_affinity_mask);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate I/O priority
    if (performance->io_priority < 0 || performance->io_priority > 7) {
        fprintf(stderr, "Invalid I/O priority: %d\n", performance->io_priority);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t config_validate_security(config_security_t* security) {
    if (security == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate TLS version
    if (security->tls_version != 12 && security->tls_version != 13) {
        fprintf(stderr, "Invalid TLS version: %d\n", security->tls_version);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t config_validate_test(config_test_t* test) {
    if (test == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate test duration
    if (test->test_duration <= 0) {
        fprintf(stderr, "Invalid test duration: %d\n", test->test_duration);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Validate stress test parameters
    if (test->stress_test_duration <= 0) {
        fprintf(stderr, "Invalid stress test duration: %d\n", test->stress_test_duration);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (test->stress_test_concurrency <= 0) {
        fprintf(stderr, "Invalid stress test concurrency: %d\n", test->stress_test_concurrency);
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    return UESIM_SUCCESS;
}

// Configuration utilities
uesim_error_t config_parse_file(uesim_config_t* config, const char* filename) {
    if (config == NULL || filename == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Failed to open configuration file: %s\n", filename);
        return UESIM_ERROR_FILE;
    }
    
    char line[1024];
    config_section_t current_section = CONFIG_SECTION_GENERAL;
    
    while (fgets(line, sizeof(line), file)) {
        // Remove comments and whitespace
        char* comment = strchr(line, '#');
        if (comment) {
            *comment = '\0';
        }
        
        // Trim whitespace
        char* start = line;
        while (*start == ' ' || *start == '\t') {
            start++;
        }
        
        char* end = start + strlen(start) - 1;
        while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
            *end = '\0';
            end--;
        }
        
        if (strlen(start) == 0) {
            continue; // Empty line
        }
        
        // Check for section header
        if (start[0] == '[') {
            char* section_end = strchr(start, ']');
            if (section_end) {
                *section_end = '\0';
                char* section_name = start + 1;
                
                // Map section name to enum
                for (int i = 0; i < CONFIG_SECTION_MAX; i++) {
                    // This is a simplified mapping - in real implementation, 
                    // you would have a proper section name mapping
                    if (strcasecmp(section_name, "general") == 0) {
                        current_section = CONFIG_SECTION_GENERAL;
                        break;
                    } else if (strcasecmp(section_name, "network") == 0) {
                        current_section = CONFIG_SECTION_NETWORK;
                        break;
                    } else if (strcasecmp(section_name, "ue") == 0) {
                        current_section = CONFIG_SECTION_UE;
                        break;
                    } else if (strcasecmp(section_name, "rrc") == 0) {
                        current_section = CONFIG_SECTION_RRC;
                        break;
                    } else if (strcasecmp(section_name, "pdcp") == 0) {
                        current_section = CONFIG_SECTION_PDCP;
                        break;
                    } else if (strcasecmp(section_name, "rlc") == 0) {
                        current_section = CONFIG_SECTION_RLC;
                        break;
                    } else if (strcasecmp(section_name, "mac") == 0) {
                        current_section = CONFIG_SECTION_MAC;
                        break;
                    } else if (strcasecmp(section_name, "nas") == 0) {
                        current_section = CONFIG_SECTION_NAS;
                        break;
                    } else if (strcasecmp(section_name, "performance") == 0) {
                        current_section = CONFIG_SECTION_PERFORMANCE;
                        break;
                    } else if (strcasecmp(section_name, "security") == 0) {
                        current_section = CONFIG_SECTION_SECURITY;
                        break;
                    } else if (strcasecmp(section_name, "test") == 0) {
                        current_section = CONFIG_SECTION_TEST;
                        break;
                    }
                }
            }
            continue;
        }
        
        // Parse key=value pairs
        char* equals = strchr(start, '=');
        if (equals) {
            *equals = '\0';
            char* key = start;
            char* value = equals + 1;
            
            // Trim key
            char* key_end = key + strlen(key) - 1;
            while (key_end > key && (*key_end == ' ' || *key_end == '\t')) {
                *key_end = '\0';
                key_end--;
            }
            
            // Trim value
            while (*value == ' ' || *value == '\t') {
                value++;
            }
            
            // Set configuration value based on type
            if (strcasecmp(value, "true") == 0 || strcasecmp(value, "false") == 0) {
                bool bool_value = (strcasecmp(value, "true") == 0);
                config_set_bool(config, current_section, key, bool_value);
            } else if (strspn(value, "0123456789") == strlen(value)) {
                int int_value = atoi(value);
                config_set_int(config, current_section, key, int_value);
            } else {
                config_set_string(config, current_section, key, value);
            }
        }
    }
    
    fclose(file);
    return UESIM_SUCCESS;
}

uesim_error_t config_write_file(uesim_config_t* config, const char* filename) {
    if (config == NULL || filename == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        fprintf(stderr, "Failed to create configuration file: %s\n", filename);
        return UESIM_ERROR_FILE;
    }
    
    // Write configuration in INI format
    fprintf(file, "# 5G UE Simulation Configuration File\n\n");
    
    // General section
    fprintf(file, "[general]\n");
    fprintf(file, "num_instances = %d\n", config->general.num_instances);
    fprintf(file, "log_level = %d\n", config->general.log_level);
    fprintf(file, "verbose = %s\n", config->general.verbose ? "true" : "false");
    fprintf(file, "debug = %s\n", config->general.debug ? "true" : "false");
    fprintf(file, "log_file = %s\n", config->general.log_file);
    fprintf(file, "enable_syslog = %s\n", config->general.enable_syslog ? "true" : "false");
    fprintf(file, "max_log_file_size = %d\n", config->general.max_log_file_size);
    fprintf(file, "log_file_count = %d\n\n", config->general.log_file_count);
    
    // Network section
    fprintf(file, "[network]\n");
    fprintf(file, "gnb_ip = %s\n", config->network.gnb_ip);
    fprintf(file, "gnb_ngap_port = %d\n", config->network.gnb_ngap_port);
    fprintf(file, "gnb_gtpu_port = %d\n", config->network.gnb_gtpu_port);
    fprintf(file, "local_ip = %s\n", config->network.local_ip);
    fprintf(file, "local_ngap_port = %d\n", config->network.local_ngap_port);
    fprintf(file, "local_gtpu_port = %d\n", config->network.local_gtpu_port);
    fprintf(file, "connection_timeout = %d\n", config->network.connection_timeout);
    fprintf(file, "keepalive_interval = %d\n", config->network.keepalive_interval);
    fprintf(file, "enable_ipv6 = %s\n", config->network.enable_ipv6 ? "true" : "false");
    fprintf(file, "max_connections = %d\n\n", config->network.max_connections);
    
    // UE section
    fprintf(file, "[ue]\n");
    fprintf(file, "imsi_prefix = %s\n", config->ue.imsi_prefix);
    fprintf(file, "imsi_start = %u\n", config->ue.imsi_start);
    fprintf(file, "msisdn_prefix = %s\n", config->ue.msisdn_prefix);
    fprintf(file, "msisdn_start = %u\n", config->ue.msisdn_start);
    fprintf(file, "tac = %u\n", config->ue.tac);
    fprintf(file, "mcc = %s\n", config->ue.mcc);
    fprintf(file, "mnc = %s\n", config->ue.mnc);
    fprintf(file, "imei = %s\n", config->ue.imei);
    fprintf(file, "random_imei = %s\n", config->ue.random_imei ? "true" : "false");
    fprintf(file, "apn = %s\n", config->ue.apn);
    fprintf(file, "apn_type = %s\n\n", config->ue.apn_type);
    
    // RRC section
    fprintf(file, "[rrc]\n");
    fprintf(file, "registration_timeout = %d\n", config->rrc.registration_timeout);
    fprintf(file, "establishment_timeout = %d\n", config->rrc.establishment_timeout);
    fprintf(file, "reestablishment_timeout = %d\n", config->rrc.reestablishment_timeout);
    fprintf(file, "handover_timeout = %d\n", config->rrc.handover_timeout);
    fprintf(file, "enable_registration = %s\n", config->rrc.enable_registration ? "true" : "false");
    fprintf(file, "enable_establishment = %s\n", config->rrc.enable_establishment ? "true" : "false");
    fprintf(file, "enable_reestablishment = %s\n", config->rrc.enable_reestablishment ? "true" : "false");
    fprintf(file, "enable_handover = %s\n", config->rrc.enable_handover ? "true" : "false");
    fprintf(file, "max_retransmissions = %d\n", config->rrc.max_retransmissions);
    fprintf(file, "t300_value = %d\n", config->rrc.t300_value);
    fprintf(file, "t301_value = %d\n", config->rrc.t301_value);
    fprintf(file, "t302_value = %d\n", config->rrc.t302_value);
    fprintf(file, "t304_value = %d\n", config->rrc.t304_value);
    fprintf(file, "t310_value = %d\n", config->rrc.t310_value);
    fprintf(file, "t311_value = %d\n", config->rrc.t311_value);
    fprintf(file, "n310_value = %d\n", config->rrc.n310_value);
    fprintf(file, "n311_value = %d\n\n", config->rrc.n311_value);
    
    // PDCP section
    fprintf(file, "[pdcp]\n");
    fprintf(file, "max_pdu_size = %d\n", config->pdcp.max_pdu_size);
    fprintf(file, "discard_timer = %d\n", config->pdcp.discard_timer);
    fprintf(file, "status_report_timer = %d\n", config->pdcp.status_report_timer);
    fprintf(file, "ciphering_algorithm = %d\n", config->pdcp.ciphering_algorithm);
    fprintf(file, "integrity_algorithm = %d\n", config->pdcp.integrity_algorithm);
    fprintf(file, "enable_ciphering = %s\n", config->pdcp.enable_ciphering ? "true" : "false");
    fprintf(file, "enable_integrity = %s\n", config->pdcp.enable_integrity ? "true" : "false");
    fprintf(file, "max_retransmissions = %d\n", config->pdcp.max_retransmissions);
    fprintf(file, "polling_pdu = %d\n", config->pdcp.polling_pdu);
    fprintf(file, "polling_byte = %d\n\n", config->pdcp.polling_byte);
    
    // RLC section
    fprintf(file, "[rlc]\n");
    fprintf(file, "am_window_size = %d\n", config->rlc.am_window_size);
    fprintf(file, "um_window_size = %d\n", config->rlc.um_window_size);
    fprintf(file, "poll_retransmit_timer = %d\n", config->rlc.poll_retransmit_timer);
    fprintf(file, "reassembly_timer = %d\n", config->rlc.reassembly_timer);
    fprintf(file, "status_prohibit_timer = %d\n", config->rlc.status_prohibit_timer);
    fprintf(file, "max_retransmissions = %d\n", config->rlc.max_retransmissions);
    fprintf(file, "enable_arq = %s\n", config->rlc.enable_arq ? "true" : "false");
    fprintf(file, "buffer_size = %d\n", config->rlc.buffer_size);
    fprintf(file, "max_pdu_size = %d\n", config->rlc.max_pdu_size);
    fprintf(file, "enable_segmentation = %s\n", config->rlc.enable_segmentation ? "true" : "false");
    
    // MAC section
    fprintf(file, "\n[mac]\n");
    fprintf(file, "harq_processes = %d\n", config->mac.harq_processes);
    fprintf(file, "max_harq_retransmissions = %d\n", config->mac.max_harq_retransmissions);
    fprintf(file, "tti_length = %d\n", config->mac.tti_length);
    fprintf(file, "rach_preambles = %d\n", config->mac.rach_preambles);
    fprintf(file, "rach_response_window = %d\n", config->mac.rach_response_window);
    fprintf(file, "max_rach_transmissions = %d\n", config->mac.max_rach_transmissions);
    fprintf(file, "scheduling_requests = %d\n", config->mac.scheduling_requests);
    fprintf(file, "ul_grants = %d\n", config->mac.ul_grants);
    fprintf(file, "dl_grants = %d\n", config->mac.dl_grants);
    fprintf(file, "enable_harq = %s\n", config->mac.enable_harq ? "true" : "false");
    fprintf(file, "enable_rach = %s\n", config->mac.enable_rach ? "true" : "false");
    fprintf(file, "tb_size = %d\n", config->mac.tb_size);
    fprintf(file, "mcs = %d\n\n", config->mac.mcs);
    
    // NAS section
    fprintf(file, "[nas]\n");
    fprintf(file, "registration_timer = %d\n", config->nas.registration_timer);
    fprintf(file, "authentication_timer = %d\n", config->nas.authentication_timer);
    fprintf(file, "security_timer = %d\n", config->nas.security_timer);
    fprintf(file, "pdu_session_timer = %d\n", config->nas.pdu_session_timer);
    fprintf(file, "enable_periodic_registration = %s\n", config->nas.enable_periodic_registration ? "true" : "false");
    fprintf(file, "periodic_registration_time = %d\n", config->nas.periodic_registration_time);
    fprintf(file, "max_registration_attempts = %d\n", config->nas.max_registration_attempts);
    fprintf(file, "max_authentication_attempts = %d\n", config->nas.max_authentication_attempts);
    fprintf(file, "ciphering_algorithm = %d\n", config->nas.ciphering_algorithm);
    fprintf(file, "integrity_algorithm = %d\n", config->nas.integrity_algorithm);
    fprintf(file, "enable_5g_features = %s\n", config->nas.enable_5g_features ? "true" : "false");
    fprintf(file, "max_pdu_sessions = %d\n", config->nas.max_pdu_sessions);
    fprintf(file, "enable_sms = %s\n", config->nas.enable_sms ? "true" : "false");
    fprintf(file, "enable_voice = %s\n\n", config->nas.enable_voice ? "true" : "false");
    
    // Performance section
    fprintf(file, "[performance]\n");
    fprintf(file, "thread_pool_size = %d\n", config->performance.thread_pool_size);
    fprintf(file, "rx_buffer_size = %d\n", config->performance.rx_buffer_size);
    fprintf(file, "tx_buffer_size = %d\n", config->performance.tx_buffer_size);
    fprintf(file, "memory_pool_size = %d\n", config->performance.memory_pool_size);
    fprintf(file, "use_memory_pool = %s\n", config->performance.use_memory_pool ? "true" : "false");
    fprintf(file, "max_queue_size = %d\n", config->performance.max_queue_size);
    fprintf(file, "worker_threads = %d\n", config->performance.worker_threads);
    fprintf(file, "io_threads = %d\n", config->performance.io_threads);
    fprintf(file, "enable_thread_affinity = %s\n", config->performance.enable_thread_affinity ? "true" : "false");
    fprintf(file, "cpu_affinity_mask = %d\n", config->performance.cpu_affinity_mask);
    fprintf(file, "enable_async_io = %s\n", config->performance.enable_async_io ? "true" : "false");
    fprintf(file, "io_priority = %d\n\n", config->performance.io_priority);
    
    // Security section
    fprintf(file, "[security]\n");
    fprintf(file, "stack_protection = %s\n", config->security.stack_protection ? "true" : "false");
    fprintf(file, "aslr = %s\n", config->security.aslr ? "true" : "false");
    fprintf(file, "secure_flags = %s\n", config->security.secure_flags ? "true" : "false");
    fprintf(file, "enable_encryption = %s\n", config->security.enable_encryption ? "true" : "false");
    fprintf(file, "enable_integrity_protection = %s\n", config->security.enable_integrity_protection ? "true" : "false");
    fprintf(file, "key_file = %s\n", config->security.key_file);
    fprintf(file, "enable_certificate_validation = %s\n", config->security.enable_certificate_validation ? "true" : "false");
    fprintf(file, "cert_file = %s\n", config->security.cert_file);
    fprintf(file, "ca_file = %s\n", config->security.ca_file);
    fprintf(file, "enable_tls = %s\n", config->security.enable_tls ? "true" : "false");
    fprintf(file, "tls_version = %d\n", config->security.tls_version);
    fprintf(file, "enable_pki = %s\n\n", config->security.enable_pki ? "true" : "false");
    
    // Test section
    fprintf(file, "[test]\n");
    fprintf(file, "enable_tests = %s\n", config->test.enable_tests ? "true" : "false");
    fprintf(file, "test_duration = %d\n", config->test.test_duration);
    fprintf(file, "report_file = %s\n", config->test.report_file);
    fprintf(file, "enable_performance_tests = %s\n", config->test.enable_performance_tests ? "true" : "false");
    fprintf(file, "enable_stress_tests = %s\n", config->test.enable_stress_tests ? "true" : "false");
    fprintf(file, "stress_test_duration = %d\n", config->test.stress_test_duration);
    fprintf(file, "stress_test_concurrency = %d\n", config->test.stress_test_concurrency);
    fprintf(file, "enable_compliance_tests = %s\n", config->test.enable_compliance_tests ? "true" : "false");
    fprintf(file, "test_suite = %s\n", config->test.test_suite);
    fprintf(file, "generate_test_report = %s\n", config->test.generate_test_report ? "true" : "false");
    fprintf(file, "enable_coverage = %s\n", config->test.enable_coverage ? "true" : "false");
    
    fclose(file);
    return UESIM_SUCCESS;
}

uesim_error_t config_merge(uesim_config_t* dest, const uesim_config_t* src) {
    if (dest == NULL || src == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Copy non-default values from src to dest
    // This is a simplified merge - in real implementation, you would check
    // each field and only copy non-default values
    
    // For now, we'll do a simple copy
    return config_copy(dest, src);
}

uesim_error_t config_copy(uesim_config_t* dest, const uesim_config_t* src) {
    if (dest == NULL || src == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Copy entire configuration structure
    memcpy(dest, src, sizeof(uesim_config_t));
    
    return UESIM_SUCCESS;
}

uesim_error_t config_reset(uesim_config_t* config) {
    if (config == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Reset to default configuration
    return config_load_default(config);
}

bool config_is_loaded(uesim_config_t* config) {
    return config ? config->loaded : false;
}

time_t config_get_load_time(uesim_config_t* config) {
    return config ? config->load_time : 0;
}
