# 5G UE Simulation gNB Integration Testing

## Overview

This document describes the cross-component testing framework for integrating the 5G UE Simulation application with O-RAN compliant gNB implementations. The testing methodology focuses on capturing real 5G protocol traffic using Wireshark for analysis and validation of RRC procedures including establishment, re-establishment, and handover.

## Testing Architecture

### 1. Test Environment Setup

#### Network Configuration
```
Test Environment Layout:
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   5G UE Sim     │    │     gNB          │    │     5GC         │
│   (uesim)       │◄──►│   (OAI/srsRAN/   │◄──►│   (Free5GC/     │
│                 │    │    Commercial)   │    │    OAI 5GC)     │
└─────────────────┘    └──────────────────┘    └─────────────────┘
         │                       │                       │
         └───────────────────────┼───────────────────────┘
                                 │
                         ┌───────▼───────┐
                         │  Wireshark    │
                         │  Capture      │
                         │  Interface    │
                         └───────────────┘
```

#### RHEL 8.5 Environment Requirements
- **Kernel**: 4.18.0-348 or higher
- **Libraries**: libsctp-devel, libconfig-devel, wireshark-cli
- **Network Tools**: tcpdump, netstat, ss
- **Development Tools**: gcc 8.5+, make, cmake 3.18+

### 2. O-RAN Integration Architecture

#### OAI with O-RAN Support Integration
```
OAI O-RAN Integration Points:
┌─────────────────────────────────────────────────────────────┐
│                    OAI gNB (O-RAN Mode)                     │
├─────────────────────────────────────────────────────────────┤
│  CU-CP (Centralized Unit - Control Plane)                   │
│  • NGAP/SCTP interface for 5GC connection                   │
│  • F1-C interface for DU communication                      │
│  • RRC processing and UE state management                   │
├─────────────────────────────────────────────────────────────┤
│  CU-UP (Centralized Unit - User Plane)                      │
│  • GTP-U/UDP interface for user plane traffic               │
│  • PDCP/RLC/MAC processing                                  │
├─────────────────────────────────────────────────────────────┤
│  DU (Distributed Unit)                                      │
│  • F1-U interface for CU-UP communication                   │
│  • Real-time PHY processing                                 │
│  • Radio resource scheduling                                │
└─────────────────────────────────────────────────────────────┘
```

#### Commercial O-RAN gNB Integration
```
Commercial O-RAN Integration Points:
┌─────────────────────────────────────────────────────────────┐
│                 Commercial O-RAN gNB                       │
├─────────────────────────────────────────────────────────────┤
│  Near-RT RIC (Non-Real-Time RAN Intelligent Controller)     │
│  • A1/E2 interface for policy control                       │
│  • O1 interface for configuration management                │
├─────────────────────────────────────────────────────────────┤
│  Non-RT RIC (Non-Real-Time RAN Intelligent Controller)      │
│  • A1 interface for service management                      │
│  • O1 interface for network orchestration                   │
├─────────────────────────────────────────────────────────────┤
│  O-DU (O-RAN Distributed Unit)                              │
│  • E2 interface for real-time control                       │
│  • F1 interface for O-CU communication                      │
├─────────────────────────────────────────────────────────────┤
│  O-CU (O-RAN Centralized Unit)                              │
│  • F1/E1 interfaces for O-DU/O-DU communication             │
│  • NGAP/SCTP for 5GC connection                             │
└─────────────────────────────────────────────────────────────┘
```

## Wireshark Capture and Analysis

### 1. Capture Configuration

#### NGAP/SCTP Capture Filters
```bash
# Capture NGAP messages on SCTP port 38412
wireshark -i any -f "port 38412" -w ngap_capture.pcap

# Capture with specific UE IP filtering
wireshark -i any -f "host 192.168.1.100 and port 38412" -w ue_ngap_capture.pcap

# Command line capture for automation
tcpdump -i any -s 0 -w ngap_full_capture.pcap 'port 38412'
```

#### GTP-U/UDP Capture Filters
```bash
# Capture GTP-U user plane traffic
wireshark -i any -f "port 2152" -w gtpu_capture.pcap

# Capture with TEID filtering
wireshark -i any -f "port 2152 and udp[8:4] = 0x12345678" -w specific_teid_capture.pcap
```

#### RRC Message Capture Filters
```bash
# Wireshark display filters for RRC analysis
# Filter for specific RRC procedures
ngap.procedureCode == 12  # InitialUEMessage
ngap.procedureCode == 13  # DownlinkNASTransport
rrc.UECapabilityEnquiry
rrc.RRCSetup
rrc.RRCReestablishment
rrc.HandoverCommand

# Filter for message direction
ngap.direction == "Originating message"
ngap.direction == "Successful outcome"
```

### 2. Analysis Procedures

#### RRC Procedure Validation
```bash
# Analyze RRC Setup procedure
tshark -r capture.pcap -Y "rrc.RRCSetup" -T fields -e frame.number -e ip.src -e ip.dst -e rrc.message

# Analyze RRC Reestablishment procedure
tshark -r capture.pcap -Y "rrc.RRCReestablishment" -T fields -e frame.number -e rrc.ue-Identity

# Analyze Handover procedure
tshark -r capture.pcap -Y "rrc.HandoverCommand" -T fields -e frame.number -e rrc.targetPhysCellId
```

## Testing Procedures

### 1. OAI-O-RAN Integration Testing

#### Setup Procedure
```bash
# 1. Start OAI 5GC
cd openairinterface5g
sudo ./build/scripts/oai-cn5g-fed.sh --start

# 2. Configure OAI gNB for O-RAN mode
sudo vim /usr/local/etc/oai/gnb.conf
# Set: tr_s_preference = "f1"

# 3. Start OAI gNB
sudo ./cmake_targets/ran_build/build/nr-softmodem -O /usr/local/etc/oai/gnb.conf --sa --O-RAN

# 4. Configure uesim for OAI connection
vim etc/uesim.conf
# Set: gnb_ip = "192.168.1.2"
# Set: gnb_port = 38412

# 5. Start Wireshark capture
wireshark -i any -f "host 192.168.1.2 and port 38412" -w oai_registration_test.pcap &

# 6. Run uesim registration test
./uesim scenario registration
```

#### Registration/Initial Access Test
```c
// Test case: OAI Registration Procedure
void test_oai_registration_procedure(void) {
    // Initialize UE context
    ue_context_t* ue_ctx = NULL;
    uesim_create_ue_instance(&ue_ctx);
    
    // Configure for OAI gNB
    ue_ctx->gnb_ip = inet_addr("192.168.1.2");
    ue_ctx->gnb_port = 38412;
    
    // Start Wireshark capture (external process)
    system("tshark -i any -f 'host 192.168.1.2 and port 38412' -w registration_capture.pcap &");
    
    // Execute registration procedure
    uesim_execute_procedure(ue_ctx, RRC_PROC_REGISTRATION);
    
    // Wait for completion
    sleep(5);
    
    // Stop capture
    system("pkill tshark");
    
    // Analyze captured traffic
    system("tshark -r registration_capture.pcap -Y 'ngap.procedureCode == 12' > registration_analysis.txt");
    
    // Validate results
    validate_registration_success("registration_analysis.txt");
}
```

### 2. Commercial O-RAN gNB Integration Testing

#### Setup Procedure
```bash
# 1. Configure commercial gNB O-RAN interfaces
# This typically involves:
# - O-DU configuration via NETCONF/YANG
# - O-CU configuration via O1 interface
# - Near-RT RIC policy configuration via A1/E2

# 2. Configure uesim for commercial gNB
vim etc/uesim.conf
# Set: gnb_ip = "192.168.10.100"
# Set: gnb_port = 38412
# Set: o_ran_mode = true

# 3. Start Wireshark capture for O-RAN interfaces
wireshark -i any -f "host 192.168.10.100 and (port 38412 or port 2152)" -w oran_test.pcap &

# 4. Run uesim with O-RAN specific procedures
./uesim --oran-mode scenario establishment
```

## Enhanced RRC Procedure Testing

### 1. Registration/Initial Access Testing

#### Test Implementation
```c
// Enhanced RRC Registration with Wireshark integration
uesim_error_t rrc_execute_registration_with_capture(ue_context_t* ue_ctx, bool capture_enabled) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("Executing RRC registration for UE %u with Wireshark capture: %s\n", 
           ue_ctx->ue_id, capture_enabled ? "ENABLED" : "DISABLED");
    
    // Start capture if enabled
    if (capture_enabled) {
        char capture_cmd[256];
        snprintf(capture_cmd, sizeof(capture_cmd), 
                "tshark -i any -f 'host %s and port %d' -w registration_ue_%u.pcap &",
                inet_ntoa(ue_ctx->gnb_addr.sin_addr), 
                ntohs(ue_ctx->gnb_addr.sin_port),
                ue_ctx->ue_id);
        system(capture_cmd);
    }
    
    // Execute standard registration
    uesim_error_t result = rrc_execute_registration(ue_ctx);
    
    // Stop capture and analyze
    if (capture_enabled) {
        system("pkill tshark");
        analyze_registration_capture(ue_ctx->ue_id);
    }
    
    return result;
}
```

### 2. RRC Establishment Testing

#### Test Implementation
```c
// Enhanced RRC Establishment with real packet generation
uesim_error_t rrc_execute_establishment_with_real_packets(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("Executing RRC establishment for UE %u with real packet generation\n", ue_ctx->ue_id);
    
    // Create realistic RRC Setup Request with proper ASN.1 encoding
    rrc_message_t setup_request = {0};
    setup_request.message_type = RRC_MESSAGE_TYPE_SETUP_REQUEST;
    setup_request.message_id = atomic_fetch_add(&g_transaction_id_counter, 1);
    setup_request.transaction_id = setup_request.message_id;
    
    // Add realistic UE capabilities
    rrc_ue_capabilities_t ue_caps = {0};
    ue_caps.access_stratum_release = 15; // 5G Release 15
    ue_caps.pdcp_parameters.maxNumberROHC_ContextSessions = 16;
    ue_caps.rf_Parameters.supportedBandListNR.count = 2;
    
    // Encode with proper 3GPP format
    size_t encoded_caps_len = 0;
    void* encoded_caps = encode_rrc_ue_capabilities(&ue_caps, &encoded_caps_len);
    if (encoded_caps != NULL) {
        setup_request.data = encoded_caps;
        setup_request.data_length = encoded_caps_len;
    }
    
    // Send with proper NGAP wrapper
    uesim_error_t result = rrc_send_message_with_ngap_wrapper(ue_ctx, &setup_request);
    
    // Cleanup
    if (encoded_caps) {
        uesim_free(encoded_caps);
    }
    
    return result;
}
```

### 3. RRC Re-establishment Testing

#### Test Implementation
```c
// Enhanced RRC Re-establishment with failure simulation
uesim_error_t rrc_execute_reestablishment_with_capture(ue_context_t* ue_ctx, 
                                                      rrc_reest_cause_t cause) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("Executing RRC re-establishment for UE %u, cause: %d\n", ue_ctx->ue_id, cause);
    
    // Start detailed capture
    char capture_cmd[256];
    snprintf(capture_cmd, sizeof(capture_cmd), 
            "tshark -i any -f 'host %s and port %d' -w reest_ue_%u_cause_%d.pcap &",
            inet_ntoa(ue_ctx->gnb_addr.sin_addr), 
            ntohs(ue_ctx->gnb_addr.sin_port),
            ue_ctx->ue_id, cause);
    system(capture_cmd);
    
    // Create RRC Reestablishment Request with specific cause
    rrc_message_t reest_request = {0};
    reest_request.message_type = RRC_MESSAGE_TYPE_REESTABLISHMENT_REQUEST;
    reest_request.message_id = atomic_fetch_add(&g_transaction_id_counter, 1);
    reest_request.transaction_id = reest_request.message_id;
    
    // Add re-establishment cause
    rrc_reest_request_ies_t reest_ies = {0};
    reest_ies.ue_Identity.c_RNTI = ue_ctx->c_rnti;
    reest_ies.ue_Identity.physCellId = ue_ctx->current_cell_id;
    reest_ies.reestablishmentCause = cause;
    reest_ies.spare = 0;
    
    // Encode re-establishment request
    size_t encoded_ies_len = 0;
    void* encoded_ies = encode_rrc_reest_request_ies(&reest_ies, &encoded_ies_len);
    if (encoded_ies != NULL) {
        reest_request.data = encoded_ies;
        reest_request.data_length = encoded_ies_len;
    }
    
    // Send reestablishment request
    uesim_error_t result = rrc_send_message(ue_ctx, &reest_request);
    
    // Stop capture
    system("pkill tshark");
    
    // Analyze capture for re-establishment procedure
    analyze_reestablishment_capture(ue_ctx->ue_id, cause);
    
    // Cleanup
    if (encoded_ies) {
        uesim_free(encoded_ies);
    }
    
    return result;
}
```

### 4. Handover Procedure Testing

#### Test Implementation
```c
// Enhanced RRC Handover with measurement reporting
uesim_error_t rrc_execute_handover_with_capture(ue_context_t* ue_ctx, 
                                               uint16_t target_cell_id) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    printf("Executing RRC handover for UE %u to cell %u\n", ue_ctx->ue_id, target_cell_id);
    
    // Start handover-specific capture
    char capture_cmd[256];
    snprintf(capture_cmd, sizeof(capture_cmd), 
            "tshark -i any -f 'host %s and port %d' -w handover_ue_%u.pcap &",
            inet_ntoa(ue_ctx->gnb_addr.sin_addr), 
            ntohs(ue_ctx->gnb_addr.sin_port),
            ue_ctx->ue_id);
    system(capture_cmd);
    
    // Send measurement report to trigger handover
    rrc_send_measurement_report(ue_ctx, target_cell_id);
    
    // Wait for handover command
    rrc_message_t* ho_command = wait_for_handover_command(ue_ctx, 5000); // 5 second timeout
    
    if (ho_command != NULL) {
        // Process handover command
        uesim_error_t result = rrc_process_handover_command(ue_ctx, ho_command);
        
        // Send handover confirmation
        rrc_send_handover_confirmation(ue_ctx);
        
        // Cleanup
        rrc_destroy_message(ho_command);
        
        // Stop capture
        system("pkill tshark");
        
        // Analyze handover procedure
        analyze_handover_capture(ue_ctx->ue_id, target_cell_id);
        
        return result;
    } else {
        // Stop capture
        system("pkill tshark");
        return UESIM_ERROR_TIMEOUT;
    }
}
```

## Cross-Component Test Framework

### 1. Test Automation for OAI-O-RAN

#### Test Suite Implementation
```c
// OAI-O-RAN Integration Test Suite
typedef struct {
    char name[64];
    char description[256];
    uesim_error_t (*test_function)(ue_context_t* ue_ctx);
    bool requires_capture;
    bool requires_oran;
    int timeout_seconds;
} oai_test_case_t;

static oai_test_case_t g_oai_test_cases[] = {
    {
        .name = "registration_procedure",
        .description = "RRC Registration with Initial Access",
        .test_function = test_oai_registration_procedure,
        .requires_capture = true,
        .requires_oran = false,
        .timeout_seconds = 10
    },
    {
        .name = "establishment_procedure",
        .description = "RRC Connection Establishment",
        .test_function = test_oai_establishment_procedure,
        .requires_capture = true,
        .requires_oran = false,
        .timeout_seconds = 15
    },
    {
        .name = "reestablishment_procedure",
        .description = "RRC Connection Re-establishment",
        .test_function = test_oai_reestablishment_procedure,
        .requires_capture = true,
        .requires_oran = false,
        .timeout_seconds = 20
    },
    {
        .name = "handover_procedure",
        .description = "RRC Handover Procedure",
        .test_function = test_oai_handover_procedure,
        .requires_capture = true,
        .requires_oran = false,
        .timeout_seconds = 25
    }
};

uesim_error_t run_oai_integration_tests(ue_context_t* ue_ctx) {
    int num_tests = sizeof(g_oai_test_cases) / sizeof(g_oai_test_cases[0]);
    int passed = 0;
    int failed = 0;
    
    printf("Running OAI Integration Test Suite (%d tests)\n", num_tests);
    printf("==========================================\n");
    
    for (int i = 0; i < num_tests; i++) {
        printf("Test %d/%d: %s\n", i+1, num_tests, g_oai_test_cases[i].name);
        printf("  Description: %s\n", g_oai_test_cases[i].description);
        
        // Run test with timeout
        uesim_error_t result = run_test_with_timeout(g_oai_test_cases[i].test_function, 
                                                    ue_ctx, 
                                                    g_oai_test_cases[i].timeout_seconds);
        
        if (result == UESIM_SUCCESS) {
            printf("  Result: PASSED ✓\n");
            passed++;
        } else {
            printf("  Result: FAILED ✗ (Error: %d)\n", result);
            failed++;
        }
        
        printf("\n");
    }
    
    printf("Test Suite Results:\n");
    printf("  Passed: %d\n", passed);
    printf("  Failed: %d\n", failed);
    printf("  Success Rate: %.2f%%\n", (float)passed / num_tests * 100);
    
    return (failed == 0) ? UESIM_SUCCESS : UESIM_ERROR_PROTOCOL;
}
```

### 2. Test Automation for Commercial O-RAN

#### Test Suite Implementation
```c
// Commercial O-RAN Integration Test Suite
typedef struct {
    char name[64];
    char description[256];
    uesim_error_t (*test_function)(ue_context_t* ue_ctx);
    bool requires_capture;
    bool requires_oran;
    int timeout_seconds;
    char* oran_interface; // "O1", "E2", "F1", etc.
} oran_test_case_t;

static oran_test_case_t g_oran_test_cases[] = {
    {
        .name = "oran_registration",
        .description = "O-RAN Registration with O1 Configuration",
        .test_function = test_oran_registration_procedure,
        .requires_capture = true,
        .requires_oran = true,
        .timeout_seconds = 15,
        .oran_interface = "O1"
    },
    {
        .name = "oran_establishment",
        .description = "O-RAN Connection Establishment with E2 Control",
        .test_function = test_oran_establishment_procedure,
        .requires_capture = true,
        .requires_oran = true,
        .timeout_seconds = 20,
        .oran_interface = "E2"
    },
    {
        .name = "oran_reestablishment",
        .description = "O-RAN Connection Re-establishment",
        .test_function = test_oran_reestablishment_procedure,
        .requires_capture = true,
        .requires_oran = true,
        .timeout_seconds = 25,
        .oran_interface = "F1"
    },
    {
        .name = "oran_handover",
        .description = "O-RAN Handover with Near-RT RIC Coordination",
        .test_function = test_oran_handover_procedure,
        .requires_capture = true,
        .requires_oran = true,
        .timeout_seconds = 30,
        .oran_interface = "E2"
    }
};

uesim_error_t run_oran_integration_tests(ue_context_t* ue_ctx) {
    int num_tests = sizeof(g_oran_test_cases) / sizeof(g_oran_test_cases[0]);
    int passed = 0;
    int failed = 0;
    
    printf("Running Commercial O-RAN Integration Test Suite (%d tests)\n", num_tests);
    printf("=====================================================\n");
    
    for (int i = 0; i < num_tests; i++) {
        printf("Test %d/%d: %s (Interface: %s)\n", 
               i+1, num_tests, g_oran_test_cases[i].name, g_oran_test_cases[i].oran_interface);
        printf("  Description: %s\n", g_oran_test_cases[i].description);
        
        // Run test with timeout
        uesim_error_t result = run_test_with_timeout(g_oran_test_cases[i].test_function, 
                                                    ue_ctx, 
                                                    g_oran_test_cases[i].timeout_seconds);
        
        if (result == UESIM_SUCCESS) {
            printf("  Result: PASSED ✓\n");
            passed++;
        } else {
            printf("  Result: FAILED ✗ (Error: %d)\n", result);
            failed++;
        }
        
        printf("\n");
    }
    
    printf("O-RAN Test Suite Results:\n");
    printf("  Passed: %d\n", passed);
    printf("  Failed: %d\n", failed);
    printf("  Success Rate: %.2f%%\n", (float)passed / num_tests * 100);
    
    return (failed == 0) ? UESIM_SUCCESS : UESIM_ERROR_PROTOCOL;
}
```

## Test Execution and Validation

### 1. Automated Test Execution

#### Main Test Runner
```bash
#!/bin/bash
# run_gnb_integration_tests.sh

echo "5G UE Simulation gNB Integration Testing"
echo "========================================"

# Check prerequisites
if ! command -v wireshark &> /dev/null; then
    echo "Error: Wireshark not found. Please install wireshark-cli"
    exit 1
fi

if ! command -v tshark &> /dev/null; then
    echo "Error: tshark not found. Please install wireshark-cli"
    exit 1
fi

# Configuration
TEST_ENV="RHEL_8.5"
CAPTURE_DIR="./captures"
REPORT_DIR="./reports"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# Create directories
mkdir -p $CAPTURE_DIR
mkdir -p $REPORT_DIR

# Run tests based on target gNB
case $1 in
    "oai")
        echo "Running OAI-O-RAN Integration Tests..."
        ./uesim --test-mode --target-gnb=oai --capture-dir=$CAPTURE_DIR
        ;;
    "oran")
        echo "Running Commercial O-RAN Integration Tests..."
        ./uesim --test-mode --target-gnb=oran --capture-dir=$CAPTURE_DIR
        ;;
    "all")
        echo "Running All Integration Tests..."
        ./uesim --test-mode --target-gnb=all --capture-dir=$CAPTURE_DIR
        ;;
    *)
        echo "Usage: $0 {oai|oran|all}"
        echo "  oai   - Run OAI-O-RAN integration tests"
        echo "  oran  - Run Commercial O-RAN integration tests"
        echo "  all   - Run all integration tests"
        exit 1
        ;;
esac

# Generate reports
echo "Generating test reports..."
./generate_test_report.sh $CAPTURE_DIR $REPORT_DIR $TIMESTAMP

echo "Test execution completed. Reports available in $REPORT_DIR"
```

### 2. Result Analysis and Reporting

#### Capture Analysis
```c
// Wireshark capture analysis functions
uesim_error_t analyze_registration_capture(uint32_t ue_id) {
    char capture_file[128];
    char analysis_file[128];
    snprintf(capture_file, sizeof(capture_file), "registration_ue_%u.pcap", ue_id);
    snprintf(analysis_file, sizeof(analysis_file), "registration_analysis_%u.txt", ue_id);
    
    // Analyze NGAP InitialUEMessage
    char cmd[256];
    snprintf(cmd, sizeof(cmd), 
            "tshark -r %s -Y 'ngap.procedureCode == 12' -T fields "
            "-e frame.number -e ngap.rAN_UE_NGAP_ID -e ngap.nAS_PDU > %s",
            capture_file, analysis_file);
    system(cmd);
    
    // Check if registration was successful
    FILE* analysis = fopen(analysis_file, "r");
    if (analysis) {
        char line[256];
        bool found_initial_ue = false;
        while (fgets(line, sizeof(line), analysis)) {
            if (strlen(line) > 1) { // Non-empty line
                found_initial_ue = true;
                break;
            }
        }
        fclose(analysis);
        
        if (found_initial_ue) {
            printf("Registration capture analysis: SUCCESS ✓\n");
            return UESIM_SUCCESS;
        }
    }
    
    printf("Registration capture analysis: FAILED ✗\n");
    return UESIM_ERROR_PROTOCOL;
}

uesim_error_t analyze_reestablishment_capture(uint32_t ue_id, int cause) {
    char capture_file[128];
    char analysis_file[128];
    snprintf(capture_file, sizeof(capture_file), "reest_ue_%u_cause_%d.pcap", ue_id, cause);
    snprintf(analysis_file, sizeof(analysis_file), "reest_analysis_%u_cause_%d.txt", ue_id, cause);
    
    // Analyze RRC Reestablishment procedure
    char cmd[256];
    snprintf(cmd, sizeof(cmd), 
            "tshark -r %s -Y 'rrc.RRCReestablishment' -T fields "
            "-e frame.number -e rrc.ue-Identity.c-RNTI -e rrc.reestablishmentCause > %s",
            capture_file, analysis_file);
    system(cmd);
    
    // Validate reestablishment cause
    return validate_reestablishment_cause(analysis_file, cause);
}

uesim_error_t analyze_handover_capture(uint32_t ue_id, uint16_t target_cell_id) {
    char capture_file[128];
    char analysis_file[128];
    snprintf(capture_file, sizeof(capture_file), "handover_ue_%u.pcap", ue_id);
    snprintf(analysis_file, sizeof(analysis_file), "handover_analysis_%u.txt", ue_id);
    
    // Analyze Handover Command and Confirmation
    char cmd[256];
    snprintf(cmd, sizeof(cmd), 
            "tshark -r %s -Y 'rrc.HandoverCommand or rrc.HandoverConfirm' -T fields "
            "-e frame.number -e rrc.message > %s",
            capture_file, analysis_file);
    system(cmd);
    
    // Validate handover completion
    return validate_handover_completion(analysis_file, target_cell_id);
}
```

## Integration with Existing Test Suite

### 1. Makefile Integration

#### Updated Makefile Targets
```makefile
# gNB Integration Testing Targets
test-gnb-oai: tests/test_gnb_oai.c
	$(CC) $(CFLAGS) tests/test_gnb_oai.c src/protocol/rrc.c src/transport/socket_mgr.c \
	      src/core/memory.c -o test_gnb_oai $(LDFLAGS)

test-gnb-oran: tests/test_gnb_oran.c
	$(CC) $(CFLAGS) tests/test_gnb_oran.c src/protocol/rrc.c src/transport/socket_mgr.c \
	      src/core/memory.c -o test_gnb_oran $(LDFLAGS)

test-gnb-integration: test-gnb-oai test-gnb-oran
	./test_gnb_oai
	./test_gnb_oran

# Wireshark Capture Integration
test-with-capture: test-gnb-integration
	@echo "Running integration tests with Wireshark capture..."
	./run_gnb_integration_tests.sh all

# Test Reports
test-reports:
	@echo "Generating test reports..."
	./generate_test_report.sh ./captures ./reports $(shell date +%Y%m%d_%H%M%S)
```

### 2. CLI Integration

#### Enhanced CLI Commands
```c
// New CLI commands for gNB integration testing
uesim_error_t cli_handle_gnb_test(cli_command_t* command) {
    if (command->arg_count == 0) {
        printf("Usage: gnb-test <target> [options]\n");
        printf("Targets:\n");
        printf("  oai    - Test with OAI-O-RAN gNB\n");
        printf("  oran   - Test with Commercial O-RAN gNB\n");
        printf("  all    - Test with all gNB types\n");
        printf("\nOptions:\n");
        printf("  --capture    Enable Wireshark capture\n");
        printf("  --analyze    Analyze captured traffic\n");
        printf("  --report     Generate test report\n");
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    const char* target = command->arg_values[0];
    bool capture_enabled = false;
    bool analyze_enabled = false;
    bool report_enabled = false;
    
    // Parse options
    for (int i = 1; i < command->arg_count; i++) {
        if (strcmp(command->arg_values[i], "--capture") == 0) {
            capture_enabled = true;
        } else if (strcmp(command->arg_values[i], "--analyze") == 0) {
            analyze_enabled = true;
        } else if (strcmp(command->arg_values[i], "--report") == 0) {
            report_enabled = true;
        }
    }
    
    printf("Executing gNB integration test: %s\n", target);
    if (capture_enabled) printf("Wireshark capture: ENABLED\n");
    if (analyze_enabled) printf("Traffic analysis: ENABLED\n");
    if (report_enabled) printf("Report generation: ENABLED\n");
    
    // Execute appropriate test
    if (strcmp(target, "oai") == 0) {
        return run_oai_integration_tests_with_options(capture_enabled, analyze_enabled, report_enabled);
    } else if (strcmp(target, "oran") == 0) {
        return run_oran_integration_tests_with_options(capture_enabled, analyze_enabled, report_enabled);
    } else if (strcmp(target, "all") == 0) {
        uesim_error_t oai_result = run_oai_integration_tests_with_options(capture_enabled, analyze_enabled, report_enabled);
        uesim_error_t oran_result = run_oran_integration_tests_with_options(capture_enabled, analyze_enabled, report_enabled);
        return (oai_result == UESIM_SUCCESS && oran_result == UESIM_SUCCESS) ? UESIM_SUCCESS : UESIM_ERROR_PROTOCOL;
    } else {
        printf("Unknown target: %s\n", target);
        return UESIM_ERROR_INVALID_PARAM;
    }
}

// Register new CLI command
static const char* g_extended_command_strings[] = {
    "start", "stop", "status", "config", "scenario", "help", "exit",
    "show", "set", "save", "load", "gnb-test"  // Added gnb-test
};

// Command handler mapping
static uesim_error_t (*g_command_handlers[])(cli_command_t*) = {
    cli_handle_start, cli_handle_stop, cli_handle_status, cli_handle_config,
    cli_handle_scenario, cli_handle_help, cli_handle_exit,
    cli_handle_show, cli_handle_set, cli_handle_save, cli_handle_load,
    cli_handle_gnb_test  // Added gnb-test handler
};
```

## Performance and Validation

### 1. Performance Metrics Collection

#### Test Performance Monitoring
```c
// Performance metrics for gNB integration tests
typedef struct {
    uint64_t registration_latency_us;      // Registration procedure latency
    uint64_t establishment_latency_us;     // Establishment procedure latency
    uint64_t reestablishment_latency_us;   // Re-establishment procedure latency
    uint64_t handover_latency_us;          // Handover procedure latency
    uint64_t packets_sent;                 // Total packets sent
    uint64_t packets_received;             // Total packets received
    uint64_t bytes_sent;                   // Total bytes sent
    uint64_t bytes_received;               // Total bytes received
    double packet_loss_rate;               // Packet loss percentage
    double throughput_mbps;                // Average throughput
} gnb_test_metrics_t;

// Collect metrics during testing
uesim_error_t collect_gnb_test_metrics(ue_context_t* ue_ctx, gnb_test_metrics_t* metrics) {
    if (ue_ctx == NULL || metrics == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Collect socket statistics
    struct tcp_info tcp_info;
    socklen_t tcp_info_len = sizeof(tcp_info);
    
    if (getsockopt(ue_ctx->ngap_socket, IPPROTO_TCP, TCP_INFO, &tcp_info, &tcp_info_len) == 0) {
        metrics->packet_loss_rate = (double)tcp_info.tcpi_lost / (tcp_info.tcpi_segs_out + tcp_info.tcpi_segs_in) * 100;
    }
    
    // Collect timing information
    // This would be populated during procedure execution
    printf("gNB Test Metrics:\n");
    printf("  Registration Latency: %lu μs\n", metrics->registration_latency_us);
    printf("  Establishment Latency: %lu μs\n", metrics->establishment_latency_us);
    printf("  Re-establishment Latency: %lu μs\n", metrics->reestablishment_latency_us);
    printf("  Handover Latency: %lu μs\n", metrics->handover_latency_us);
    printf("  Packet Loss Rate: %.2f%%\n", metrics->packet_loss_rate);
    printf("  Throughput: %.2f Mbps\n", metrics->throughput_mbps);
    
    return UESIM_SUCCESS;
}
```

### 2. Validation Criteria

#### Test Success/Failure Conditions
```c
// Validation functions for test results
bool validate_registration_success(const char* analysis_file) {
    FILE* file = fopen(analysis_file, "r");
    if (!file) return false;
    
    char line[512];
    bool found_initial_ue = false;
    bool found_setup = false;
    
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, "ngap.procedureCode == 12")) {
            found_initial_ue = true;
        }
        if (strstr(line, "rrc.RRCSetup")) {
            found_setup = true;
        }
    }
    
    fclose(file);
    return found_initial_ue && found_setup;
}

bool validate_reestablishment_cause(const char* analysis_file, int expected_cause) {
    FILE* file = fopen(analysis_file, "r");
    if (!file) return false;
    
    char line[512];
    bool cause_matched = false;
    
    while (fgets(line, sizeof(line), file)) {
        // Parse cause from analysis
        int actual_cause = -1;
        if (sscanf(line, "%*d %*s %d", &actual_cause) == 1) {
            if (actual_cause == expected_cause) {
                cause_matched = true;
                break;
            }
        }
    }
    
    fclose(file);
    return cause_matched;
}

bool validate_handover_completion(const char* analysis_file, uint16_t target_cell_id) {
    FILE* file = fopen(analysis_file, "r");
    if (!file) return false;
    
    char line[512];
    bool found_command = false;
    bool found_confirm = false;
    bool target_cell_matched = false;
    
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, "rrc.HandoverCommand")) {
            found_command = true;
        }
        if (strstr(line, "rrc.HandoverConfirm")) {
            found_confirm = true;
        }
        // Check target cell ID match
        uint16_t cell_id = 0;
        if (sscanf(line, "%*d %*s %hu", &cell_id) == 1) {
            if (cell_id == target_cell_id) {
                target_cell_matched = true;
            }
        }
    }
    
    fclose(file);
    return found_command && found_confirm && target_cell_matched;
}
```

## RHEL 8.5 Optimization and Validation

### 1. System Configuration

#### RHEL 8.5 Network Optimization
```bash
#!/bin/bash
# rhel85_network_optimization.sh

echo "Optimizing RHEL 8.5 for 5G UE Simulation testing..."

# Increase network buffer sizes
echo 'net.core.rmem_max = 134217728' >> /etc/sysctl.conf
echo 'net.core.wmem_max = 134217728' >> /etc/sysctl.conf
echo 'net.core.rmem_default = 131072' >> /etc/sysctl.conf
echo 'net.core.wmem_default = 131072' >> /etc/sysctl.conf

# TCP optimizations
echo 'net.ipv4.tcp_rmem = 4096 87380 134217728' >> /etc/sysctl.conf
echo 'net.ipv4.tcp_wmem = 4096 65536 134217728' >> /etc/sysctl.conf
echo 'net.ipv4.tcp_congestion_control = bbr' >> /etc/sysctl.conf

# Apply settings
sysctl -p

# Configure CPU affinity for network processing
echo "Configuring CPU affinity for network processing..."
echo "0-3" > /sys/class/net/eth0/device/numa_node

# Enable kernel network debugging
echo 1 > /proc/sys/net/core/netdev_max_backlog
echo 1 > /proc/sys/net/core/netdev_budget

echo "RHEL 8.5 network optimization completed."
```

### 2. Security Considerations

#### SELinux Configuration for Testing
```bash
#!/bin/bash
# rhel85_selinux_testing.sh

echo "Configuring SELinux for 5G UE Simulation testing..."

# Allow network capture
setsebool -P allow_zebra_exporter 1

# Create custom SELinux policy for uesim
cat > uesim_testing.te << 'EOF'
module uesim_testing 1.0;

require {
    type unconfined_t;
    type wireshark_t;
    type sctp_socket_t;
    type tcp_socket_t;
    type udp_socket_t;
    class packet { recv send };
    class tcp_socket { accept bind connect create getattr ioctl listen read setopt write };
    class udp_socket { bind connect create getattr ioctl read setopt write };
    class sctp_socket { accept bind connect create getattr ioctl listen read setopt write };
}

# Allow uesim to capture network traffic
allow unconfined_t wireshark_t:packet { recv send };
allow unconfined_t sctp_socket_t:packet { recv send };
EOF

# Compile and install policy
checkmodule -M -m -o