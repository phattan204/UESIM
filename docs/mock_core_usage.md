# Mock Core Usage for Testing UESim

## Overview

The mock core components provide a simulated 5G core network for testing the UESim application without requiring a real 5GC deployment. This enables rapid development, automated testing, and protocol validation.

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              Test Environment                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   ┌─────────────┐      ┌──────────────────────────────────────────────┐    │
│   │   UESim     │      │              Mock Core Network               │    │
│   │   (UE)      │      │                                              │    │
│   │             │      │  ┌─────────┐  ┌─────────┐  ┌─────────┐      │    │
│   │  - NAS      │◄────►│  │   AMF   │  │   SMF   │  │   UPF   │      │    │
│   │  - RRC     │      │  │ (NGAP)  │  │ (PFCP)  │  │ (GTP-U) │      │    │
│   │  - PDCP    │      │  └─────────┘  └─────────┘  └─────────┘      │    │
│   │  - RLC     │      │       │            │            │             │    │
│   │  - MAC     │      │       │            │            │             │    │
│   └─────────────┘      │       ▼            ▼            ▼             │    │
│                        │  ┌─────────────────────────────────────┐     │    │
│                        │  │    Mock gNB (CU-CP/DU/CU-UP)       │     │    │
│                        │  │                                     │     │    │
│                        │  │  ┌────────┐  ┌────────┐  ┌───────┐ │     │    │
│                        │  │  │ CU-CP  │  │   DU   │  │ CU-UP │ │     │    │
│                        │  │  │ (F1AP) │  │ (F1AP) │  │ (E1AP)│ │     │    │
│                        │  │  └────────┘  └────────┘  └───────┘ │     │    │
│                        │  │                                     │     │    │
│                        │  │  ┌────────────────────────────────┐│     │    │
│                        │  │  │    XnAP (gNB-to-gNB Handover)   ││     │    │
│                        │  │  └────────────────────────────────┘│     │    │
│                        │  └─────────────────────────────────────┘     │    │
│                        └──────────────────────────────────────────────┘    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Component Usage

### 1. AMF (Access and Mobility Management Function)

**Purpose:** Handles UE registration, authentication, and connection management.

**Testing Usage:**

```c
#include "mock_core/mock_core.h"

void test_ue_registration(void) {
    // 1. Create AMF server
    amf_config_t amf_config;
    amf_get_default_config(&amf_config);
    amf_config.log_messages = true;
    
    amf_server_t* amf = amf_create(&amf_config);
    amf_start(amf);
    
    // 2. Simulate NG Setup from gNB
    ngap_message_t ng_setup;
    memset(&ng_setup, 0, sizeof(ng_setup));
    ng_setup.message_type = NGAP_MSG_NG_SETUP_REQUEST;
    ng_setup.payload.ng_setup_request.gnb_id = 1;
    
    ngap_message_t response;
    amf_handle_ng_setup(amf, &ng_setup, -1, &response);
    
    // 3. Simulate Initial UE Message (Registration Request)
    ngap_message_t initial_ue;
    memset(&initial_ue, 0, sizeof(initial_ue));
    initial_ue.message_type = NGAP_MSG_INITIAL_UE_MESSAGE;
    initial_ue.payload.initial_ue_message.ran_ue_ngap_id = 1;
    
    // NAS Registration Request
    uint8_t nas_pdu[] = { 0x7e, 0x02, 0x01, 0x03 };  // Simplified
    amf_handle_initial_ue(amf, &initial_ue, -1, &response);
    
    // 4. Verify UE context created
    amf_ue_context_t* ue = amf_find_ue_by_ran_id(amf, 1);
    assert(ue != NULL);
    
    // 5. Cleanup
    amf_stop(amf);
    amf_destroy(amf);
}
```

### 2. SMF (Session Management Function)

**Purpose:** Manages PDU sessions and IP address allocation.

**Testing Usage:**

```c
void test_pdu_session_establishment(void) {
    // 1. Create SMF server
    smf_config_t smf_config;
    smf_get_default_config(&smf_config);
    
    smf_server_t* smf = smf_create(&smf_config);
    
    // 2. Create PDU session
    uint64_t amf_ue_id = 0x123456789ABCDEF0ULL;
    smf_pdu_session_t* session = smf_create_session(smf, amf_ue_id, 
                                                      1,  // PDU Session ID
                                                      1,  // SST
                                                      0); // SD
    
    // 3. Verify session properties
    assert(session != NULL);
    assert(session->pdu_session_id == 1);
    assert(session->ue_ip_addr != 0);  // IP allocated from pool
    
    printf("PDU Session Created:\n");
    printf("  Session ID: %u\n", session->pdu_session_id);
    printf("  UE IP: %u.%u.%u.%u\n",
           (session->ue_ip_addr >> 24) & 0xFF,
           (session->ue_ip_addr >> 16) & 0xFF,
           (session->ue_ip_addr >> 8) & 0xFF,
           session->ue_ip_addr & 0xFF);
    printf("  UPF TEID: %u\n", session->upf_dl_teid);
    
    // 4. Cleanup
    smf_release_session(smf, 1, amf_ue_id);
    smf_destroy(smf);
}
```

### 3. UPF (User Plane Function)

**Purpose:** Handles GTP-U tunneling and data forwarding.

**Testing Usage:**

```c
void test_gtpu_tunnel(void) {
    // 1. Create UPF server
    upf_config_t upf_config;
    upf_get_default_config(&upf_config);
    upf_config.log_packets = true;
    
    upf_server_t* upf = upf_create(&upf_config);
    upf_start(upf);
    
    // 2. Create GTP-U tunnel
    uint32_t teid = 0x12345678;
    uint32_t ue_ip = 0x0A000001;  // 10.0.0.1
    uint32_t peer_ip = 0x7F000001;  // 127.0.0.1
    
    upf_tunnel_t* tunnel = upf_create_tunnel(upf, teid, ue_ip, 
                                             peer_ip, 2152, true);
    assert(tunnel != NULL);
    
    // 3. Simulate GTP-U packet
    uint8_t gtpu_packet[] = {
        0x30, 0xFF, 0x00, 0x10,  // GTP-U header (v1, G-PDU)
        0x12, 0x34, 0x56, 0x78,  // TEID
        // ... payload
    };
    
    struct sockaddr_in src_addr;
    src_addr.sin_addr.s_addr = peer_ip;
    src_addr.sin_port = htons(2152);
    
    upf_handle_gtpu_packet(upf, gtpu_packet, sizeof(gtpu_packet), &src_addr);
    
    // 4. Verify statistics
    upf_print_statistics(upf);
    
    // 5. Cleanup
    upf_remove_tunnel(upf, teid);
    upf_stop(upf);
    upf_destroy(upf);
}
```

### 4. CU-CP (Central Unit - Control Plane)

**Purpose:** F1AP interface for DU management.

**Testing Usage:**

```c
void test_f1_setup(void) {
    // 1. Create CU-CP server
    cu_cp_config_t config;
    cu_cp_get_default_config(&config);
    config.log_messages = true;
    
    cu_cp_server_t* cu_cp = cu_cp_create(&config);
    cu_cp_start(cu_cp);
    
    // 2. Simulate F1 Setup Request from DU
    f1ap_message_t f1_setup;
    f1ap_init_f1_setup_request(&f1_setup);
    
    f1ap_f1_setup_request_t* req = &f1_setup.payload.f1_setup_request;
    req->gnb_du_id.gnb_du_id = 0x00000001;
    strncpy((char*)req->gnb_du_id.gnb_du_name, "Test-DU-01", 
            sizeof(req->gnb_du_id.gnb_du_name) - 1);
    req->served_cells.num_cells = 1;
    req->served_cells.cells[0].nr_cell_id.nr_cell_id = 0x123456789ABULL;
    
    // 3. Encode and process
    uint8_t* buffer = NULL;
    size_t length = 0;
    f1ap_encode_message(&f1_setup, &buffer, &length);
    
    cu_cp_process_f1ap_message(cu_cp, buffer, length, -1);
    
    // 4. Verify DU connection
    cu_cp_print_statistics(cu_cp);
    
    // 5. Cleanup
    free(buffer);
    cu_cp_stop(cu_cp);
    cu_cp_destroy(cu_cp);
}
```

### 5. DU (Distributed Unit)

**Purpose:** F1AP interface for CU-CP connection.

**Testing Usage:**

```c
void test_du_connection(void) {
    // 1. Create DU server
    du_config_t config;
    du_get_default_config(&config);
    strncpy(config.cu_cp_ip, "127.0.0.1", sizeof(config.cu_cp_ip) - 1);
    
    du_server_t* du = du_create(&config);
    du_start(du);
    
    // 2. Connect to CU-CP
    mock_core_error_t err = du_connect_cu(du, "127.0.0.1", 38472);
    if (err == MOCK_CORE_SUCCESS) {
        // 3. Send F1 Setup Request
        du_send_f1_setup_request(du);
    }
    
    // 4. Verify connection
    du_print_statistics(du);
    
    // 5. Cleanup
    du_stop(du);
    du_destroy(du);
}
```

### 6. CU-UP (Central Unit - User Plane)

**Purpose:** E1AP interface for bearer context management.

**Testing Usage:**

```c
void test_bearer_context(void) {
    // 1. Create CU-UP server
    cu_up_server_t* cu_up = cu_up_create(NULL);
    cu_up_start(cu_up);
    
    // 2. Simulate E1 Setup Request
    e1ap_message_t e1_setup;
    e1ap_init_e1_setup_request(&e1_setup);
    
    e1ap_e1_setup_request_t* req = &e1_setup.payload.e1_setup_request;
    req->gnb_cu_up_id.gnb_cu_up_id = 0x00000001;
    
    // 3. Process message
    uint8_t* buffer = NULL;
    size_t length = 0;
    e1ap_encode_message(&e1_setup, &buffer, &length);
    
    cu_up_process_e1ap_message(cu_up, buffer, length);
    
    // 4. Verify
    cu_up_print_statistics(cu_up);
    
    // 5. Cleanup
    free(buffer);
    cu_up_stop(cu_up);
    cu_up_destroy(cu_up);
}
```

### 7. XnAP (gNB-to-gNB Interface)

**Purpose:** Handover between gNBs.

**Testing Usage:**

```c
void test_xn_handover(void) {
    // 1. Create XnAP server
    xnap_server_t* xnap = xnap_create(NULL);
    xnap_start(xnap);
    
    // 2. Simulate Xn Setup from neighbor gNB
    xnap_message_t xn_setup;
    xnap_init_xn_setup_request(&xn_setup);
    
    xnap_xn_setup_request_t* req = &xn_setup.payload.xn_setup_request;
    req->gnb_id.gnb_id = 0x00000002;
    strncpy(req->gnb_id.gnb_name, "Neighbor-gNB-02", 
            sizeof(req->gnb_id.gnb_name) - 1);
    
    // 3. Process message
    uint8_t* buffer = NULL;
    size_t length = 0;
    xnap_encode_message(&xn_setup, &buffer, &length);
    
    xnap_process_message(xnap, buffer, length, 0);
    
    // 4. Initiate handover
    xnap_initiate_handover(xnap, 1,  // Source UE ID
                            0x00000002,  // Target gNB ID
                            0x123456789ABULL);  // Target Cell ID
    
    // 5. Verify
    xnap_print_statistics(xnap);
    
    // 6. Cleanup
    free(buffer);
    xnap_stop(xnap);
    xnap_destroy(xnap);
}
```

## Integration Testing

### Complete Registration Flow Test

```c
void test_complete_registration_flow(void) {
    printf("\n=== Complete Registration Flow Test ===\n\n");
    
    // 1. Start all mock core components
    amf_server_t* amf = amf_create(NULL);
    amf_start(amf);
    
    smf_server_t* smf = smf_create(NULL);
    
    upf_server_t* upf = upf_create(NULL);
    upf_start(upf);
    
    cu_cp_server_t* cu_cp = cu_cp_create(NULL);
    cu_cp_start(cu_cp);
    
    du_server_t* du = du_create(NULL);
    du_start(du);
    
    // 2. Establish F1 connection (DU -> CU-CP)
    du_connect_cu(du, "127.0.0.1", 38472);
    du_send_f1_setup_request(du);
    
    // 3. Simulate UE Registration
    ngap_message_t ng_setup;
    memset(&ng_setup, 0, sizeof(ng_setup));
    ng_setup.message_type = NGAP_MSG_NG_SETUP_REQUEST;
    
    ngap_message_t response;
    amf_handle_ng_setup(amf, &ng_setup, -1, &response);
    
    // 4. Create UE context
    amf_ue_context_t* ue = amf_create_ue_context(amf, 1);
    assert(ue != NULL);
    
    // 5. Create PDU session
    smf_pdu_session_t* session = smf_create_session(smf, ue->amf_ue_ngap_id,
                                                      1, 1, 0);
    assert(session != NULL);
    
    // 6. Create GTP-U tunnel
    upf_tunnel_t* tunnel = upf_create_tunnel(upf, session->upf_dl_teid,
                                              session->ue_ip_addr,
                                              0x7F000001, 2152, true);
    assert(tunnel != NULL);
    
    // 7. Print all statistics
    printf("\n--- AMF Statistics ---\n");
    amf_print_stats(amf);
    
    printf("\n--- SMF Statistics ---\n");
    smf_print_statistics(smf);
    
    printf("\n--- UPF Statistics ---\n");
    upf_print_statistics(upf);
    
    printf("\n--- CU-CP Statistics ---\n");
    cu_cp_print_statistics(cu_cp);
    
    printf("\n--- DU Statistics ---\n");
    du_print_statistics(du);
    
    // 8. Cleanup
    du_stop(du);
    du_destroy(du);
    
    cu_cp_stop(cu_cp);
    cu_cp_destroy(cu_cp);
    
    upf_stop(upf);
    upf_destroy(upf);
    
    smf_destroy(smf);
    
    amf_stop(amf);
    amf_destroy(amf);
    
    printf("\n=== Test Complete ===\n");
}
```

## Test Integration with Makefile

Add to your Makefile:

```makefile
# Mock Core Test Targets
test-mock-core: test-mock-amf test-mock-smf test-mock-upf test-mock-cu-cp test-mock-du test-mock-cu-up test-mock-xnap

test-mock-amf:
	$(CC) $(CFLAGS) tests/test_mock_amf.c src/mock_core/mock_amf.c \
	      src/protocol/ngap_messages.c -o test_mock_amf $(LDFLAGS)
	./test_mock_amf

test-mock-smf:
	$(CC) $(CFLAGS) tests/test_mock_smf.c src/mock_core/mock_smf.c \
	      -o test_mock_smf $(LDFLAGS)
	./test_mock_smf

test-mock-upf:
	$(CC) $(CFLAGS) tests/test_mock_upf.c src/mock_core/mock_upf.c \
	      -o test_mock_upf $(LDFLAGS)
	./test_mock_upf

test-mock-integration:
	$(CC) $(CFLAGS) tests/test_mock_integration.c \
	      src/mock_core/mock_amf.c \
	      src/mock_core/mock_smf.c \
	      src/mock_core/mock_upf.c \
	      src/mock_core/mock_cu_cp.c \
	      src/mock_core/mock_du.c \
	      src/protocol/ngap_messages.c \
	      src/protocol/f1ap_messages.c \
	      -o test_mock_integration $(LDFLAGS)
	./test_mock_integration
```

## Best Practices

### 1. Component Lifecycle

```c
// Always follow this order:
// 1. Create
amf_server_t* amf = amf_create(&config);

// 2. Start
amf_start(amf);

// 3. Use
amf_handle_ng_setup(amf, &msg, socket, &response);

// 4. Stop
amf_stop(amf);

// 5. Destroy
amf_destroy(amf);
```

### 2. Error Handling

```c
mock_core_error_t err = amf_start(amf);
if (err != MOCK_CORE_SUCCESS) {
    fprintf(stderr, "AMF start failed: %s\n", 
            mock_core_error_to_string(err));
    amf_destroy(amf);
    return err;
}
```

### 3. Logging Control

```c
// Enable for debugging
config.log_messages = true;

// Disable for performance testing
config.log_messages = false;
```

### 4. Resource Cleanup

```c
// Always clean up in reverse order of creation
void cleanup_test_environment(test_env_t* env) {
    if (env->upf) { upf_stop(env->upf); upf_destroy(env->upf); }
    if (env->smf) { smf_destroy(env->smf); }
    if (env->amf) { amf_stop(env->amf); amf_destroy(env->amf); }
    if (env->du) { du_stop(env->du); du_destroy(env->du); }
    if (env->cu_cp) { cu_cp_stop(env->cu_cp); cu_cp_destroy(env->cu_cp); }
}
```

## Troubleshooting

### Common Issues

1. **Port Already in Use**
   ```
   Error: MOCK_CORE_ERROR_SOCKET
   Solution: Check if another process is using the port (netstat -tlnp | grep 38412)
   ```

2. **Memory Leaks**
   ```
   Always pair create/destroy and start/stop calls
   Use valgrind to detect leaks: valgrind --leak-check=full ./test_mock_core
   ```

3. **Thread Issues**
   ```
   Ensure proper cleanup with stop() before destroy()
   Check for hanging threads with timeout in pthread_join
   ```

## Summary

The mock core components enable comprehensive testing of UESim without requiring:
- Real 5GC hardware
- Commercial gNB equipment
- Complex network setup

This allows for:
- Rapid development iteration
- Automated CI/CD testing
- Protocol compliance validation
- Performance benchmarking
- Regression testing