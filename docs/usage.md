# 5G UE Simulation Application - Usage Guide

## Quick Start

### Configure and Build

The `configure` script auto-detects system capabilities and generates optimal build settings.

**Linux (RHEL 8.5+):**
```bash
# Install dependencies
sudo yum install -y gcc make libsctp-devel

# Run configure (auto-detects SCTP, GCC version, etc.)
./configure

# Build
make                    # Release build
make BUILD_TYPE=debug   # Debug build
```

**Debian/Ubuntu:**
```bash
sudo apt-get install -y build-essential libsctp-dev pkg-config
./configure
make
```

**macOS:**
```bash
xcode-select --install
brew install libsctp pkg-config
./configure
make
```

**Windows (MinGW/MSYS2):**
```cmd
make                    # Release build (produces uesim.exe)
make BUILD_TYPE=debug   # Debug build (produces uesim-debug.exe)
```

### Configure Script Options

```bash
./configure --help

# Common options:
./configure --prefix=/opt/uesim      # Custom installation path
./configure --enable-debug           # Debug build
./configure --disable-sctp           # Disable SCTP (use TCP fallback)
./configure --cross-compile=aarch64  # Cross-compile for ARM64
./configure --with-sctp=/usr/local   # Specify SCTP library path
```

### Manual Build (without configure)

If configure script is not available or you prefer manual configuration:

```bash
make                    # Uses default settings
make BUILD_TYPE=debug   # Debug build
make HAVE_SCTP=0        # Disable SCTP support
```

### Run

```bash
./uesim                          # Run with defaults (1 UE instance)
./uesim -c etc/uesim.conf       # Run with config file
./uesim -i 10                   # Simulate 10 UE instances
./uesim -I                      # Interactive mode
./uesim -v -d                   # Verbose + debug mode
./uesim --help                  # Show usage
```

## Command-Line Options

| Option | Long Form | Argument | Description |
|--------|-----------|----------|-------------|
| `-c` | `--config` | FILE | Path to configuration file |
| `-i` | `--instances` | N | Number of UE instances (1-1024) |
| `-v` | `--verbose` | - | Enable verbose logging |
| `-d` | `--debug` | - | Enable debug mode |
| `-I` | `--interactive` | - | Start interactive CLI |
| `-M` | `--with-mock` | - | Start mock components with interactive mode |
| `-h` | `--help` | - | Show help message |

## Test Mode Options

| Option | Long Form | Argument | Description |
|--------|-----------|----------|-------------|
| `-t` | `--test-mode` | - | Enable test mode with mock core/gNB |
| `-s` | `--test-scenario` | FILE | Test scenario file (JSON) |
| `-u` | `--test-ues` | N | Number of test UEs (default: 1) |
| `-r` | `--test-report` | FILE | Test report output file |

## Component Overview

```
┌─────────────────────────────────────────────┐
│  CLI (src/cli/)     - Interactive commands   │
├─────────────────────────────────────────────┤
│  Config (src/config/) - INI-style config     │
├─────────────────────────────────────────────┤
│  NAS (src/nas/)     - Non-Access Stratum    │
├─────────────────────────────────────────────┤
│  RRC (src/protocol/) - Radio Resource Ctrl  │
│  PDCP               - Packet Data Converge  │
│  RLC                - Radio Link Control     │
│  MAC                - Medium Access Control   │
├─────────────────────────────────────────────┤
│  Crypto (src/protocol/)                      │
│    AES, SNOW-3G, ZUC - 5G NEA/NIA algos     │
├─────────────────────────────────────────────┤
│  Transport (src/transport/)                  │
│    Socket Manager - NGAP/SCTP, GTP-U/UDP    │
├─────────────────────────────────────────────┤
│  Core (src/core/)                            │
│    Memory Pool, UE Instance Management        │
├─────────────────────────────────────────────┤
│  Utils (src/utils/)                          │
│    Ring Buffer - Thread-safe IPC             │
├─────────────────────────────────────────────┤
│  Benchmark (src/benchmark/)                  │
│    Performance measurement framework          │
└─────────────────────────────────────────────┘
```

## Component Usage

### 1. Core Framework (`src/core/`)

**Memory Management** - Custom pool allocator for high-frequency allocations:

```c
#include "core/memory.h"

// Initialize (called by uesim_init)
uesim_error_t result = memory_init(64 * 1024 * 1024);  // 64MB pool

// Allocate
void* buf = uesim_malloc(1024);
void* buf = uesim_calloc(10, sizeof(my_struct_t));
void* buf = uesim_realloc(old_ptr, new_size);

// Free
uesim_free(buf);
```

**UE Instance Management:**

```c
#include "uesim.h"

// Initialize core
uesim_init();

// Create UE instance
ue_context_t* ue = NULL;
uesim_create_ue_instance(&ue);

// Start UE (activates connections)
uesim_start_ue(ue);

// Execute RRC procedure
uesim_execute_procedure(ue, RRC_PROC_REGISTRATION);

// Stop and cleanup
uesim_stop_ue(ue);
uesim_free(ue);
uesim_cleanup();
```

### 2. Protocol Stack (`src/protocol/`)

**RRC Layer** - State machine with 3 states and 4 procedures:

| State | Description |
|-------|-------------|
| `RRC_STATE_IDLE` | No RRC connection |
| `RRC_STATE_CONNECTED` | Active RRC connection |
| `RRC_STATE_INACTIVE` | Suspended connection |

| Procedure | Description |
|-----------|-------------|
| `RRC_PROC_REGISTRATION` | Initial registration to network |
| `RRC_PROC_ESTABLISHMENT` | RRC connection establishment |
| `RRC_PROC_REESTABLISHMENT` | Connection re-establishment after failure |
| `RRC_PROC_HANDOVER` | Inter-cell handover |

**PDCP Layer** - Security processing with 5G algorithms:

| Algorithm | Ciphering | Integrity |
|-----------|-----------|-----------|
| NEA0/NIA0 | NULL (no ciphering) | NULL |
| NEA1/NIA1 | SNOW-3G | SNOW-3G |
| NEA2/NIA2 | AES-128-CTR | AES-CMAC |
| NEA3/NIA3 | ZUC | ZUC |

```c
#include "protocol/pdcp.h"

// Create security context
pdcp_security_context_t* sec_ctx = NULL;
pdcp_create_security_context(
    PDCP_CIPHERING_ALG_NEA2,    // AES ciphering
    PDCP_INTEGRITY_ALG_NIA2,   // AES integrity
    cipher_key, integrity_key, &sec_ctx);

// Process TX data (encrypt + integrity protect)
pdcp_pdu_t* pdu = NULL;
pdcp_process_tx_data(entity, sdu_data, sdu_length, &pdu);

// Process RX data (verify + decrypt)
void* sdu_data = NULL;
size_t sdu_length = 0;
pdcp_process_rx_data(entity, pdu, &sdu_data, &sdu_length);
```

**RLC Layer** - Three modes of operation:

| Mode | Description | Use Case |
|------|-------------|----------|
| TM | Transparent Mode | BCCH, PCCH, CCCH |
| UM | Unacknowledged Mode | VoIP, video streaming |
| AM | Acknowledged Mode | TCP, file transfer |

**MAC Layer** - HARQ and scheduling:

```c
#include "protocol/mac.h"

mac_entity_t* mac = NULL;
mac_create_entity(&mac, MAC_BEARER_DRB1);
mac_activate_entity(mac);
```

### 3. NAS Layer (`src/nas/`)

5G NAS protocol for registration, authentication, and session management:

```c
#include "nas/nas.h"

nas_context_t* nas_ctx = NULL;
nas_init(&nas_ctx, ue_ctx);

// Registration procedure
nas_handle_registration_request(nas_ctx, ...);

// PDU session establishment
nas_handle_pdu_session_establishment_request(nas_ctx, ...);
```

### 4. Transport Layer (`src/transport/`)

Socket management for NGAP (SCTP) and GTP-U (UDP):

```c
#include "transport/socket_mgr.h"

// Create NGAP signaling socket
create_ngap_socket(ue_ctx);

// Create GTP-U data socket
create_gtpu_socket(ue_ctx);

// Send messages
send_ngap_message(ue_ctx, data, length);
send_gtpu_packet(ue_ctx, data, length);
```

### 5. CLI Interface (`src/cli/`)

Interactive command mode with hierarchical commands:

```bash
./uesim -I
```

**Command Format:**
```
<verb> <noun> [arguments]
<noun> <verb> [arguments]
```

**UE Management:**

| Command | Description |
|---------|-------------|
| `ue start <n>` | Start n UE instances (default: 1) |
| `ue stop <id|all>` | Stop specific UE or all UEs |
| `ue status [id]` | Show UE status |
| `ue list` | List all UEs |
| `ue select <id>` | Select UE for context mode |

**gNB Management:**

| Command | Description |
|---------|-------------|
| `gnb add <id> <ip> <port> [type]` | Add gNB |
| `gnb remove <id>` | Remove gNB |
| `gnb connect <ue> <gnb>` | Connect UE to gNB |
| `gnb disconnect <ue>` | Disconnect UE |
| `gnb list` | List all gNBs |

**PDU Session:**

| Command | Description |
|---------|-------------|
| `session create <ue> [type]` | Create PDU session |
| `session release <ue> <id>` | Release session |
| `session list <ue>` | List sessions |

**QoS Flow:**

| Command | Description |
|---------|-------------|
| `qos add <ue> <5qi> [gbr]` | Add QoS flow |
| `qos remove <ue> <qfi>` | Remove QoS flow |
| `qos list <ue>` | List QoS flows |

**Scenario Execution:**

| Command | Description |
|---------|-------------|
| `scenario run <type> [ue]` | Execute RRC scenario |
| `scenario list` | List available scenarios |

**Scenarios:** `registration`, `establishment`, `reestablishment`, `handover`

**Load Testing:**

| Command | Description |
|---------|-------------|
| `loadtest start <scenario> <ues> <dur>` | Start load test |
| `loadtest stop` | Stop load test |
| `loadtest status` | Show status |

**Configuration:**

| Command | Description |
|---------|-------------|
| `config show [section]` | Show configuration |
| `config set <section.key> <value>` | Set value |
| `config load <file>` | Load from file |
| `config save <file>` | Save to file |

**System:**

| Command | Description |
|---------|-------------|
| `system status` | Show system status |
| `system info` | Show system information |

**Measurement Events (Handover Triggers):**

| Command | Description |
|---------|-------------|
| `meas add <A3\|A4\|A5> [ue]` | Add measurement event |
| `meas remove <event> [ue]` | Remove measurement event |
| `meas list [ue]` | List configured events |
| `meas show [ue]` | Show measurement results |
| `meas run [ue]` | Perform measurement and evaluate |
| `meas set <event> <param> <value> [ue]` | Configure event parameter |

**Measurement Event Types (3GPP TS 38.331):**

| Event | Description | Condition |
|-------|-------------|-----------|
| **A3** | Neighbor better than serving | Mn - Hys > Mp + Off |
| **A4** | Neighbor better than threshold | Mn - Hys > Thresh |
| **A5** | Serving worse + neighbor better | Mp < Thresh1 AND Mn > Thresh2 |

**Measurement Parameters:**

| Parameter | Description | Default |
|-----------|-------------|---------|
| `enable` | Enable/disable event | on |
| `hysteresis` | Hysteresis in dB | 0.5 |
| `ttt` | Time-to-trigger in ms | 640 |
| `offset` | A3 offset in dB | 3 |
| `threshold` | A4 threshold in dBm | -100 |
| `threshold1` | A5 serving threshold in dBm | -110 |
| `threshold2` | A5 neighbor threshold in dBm | -100 |

**Context Mode:**
```bash
uesim> ue select 0
uesim(ue:0)> status        # Shows UE 0 status
uesim(ue:0)> scenario run registration   # Runs on UE 0
uesim(ue:0)> exit          # Exit context mode
uesim>
```

**Measurement Event Example:**
```bash
uesim> ue start 1
uesim> gnb add gnb1 192.168.1.100 38412
uesim> gnb add gnb2 192.168.1.101 38412
uesim> gnb connect 0 gnb1
uesim> ue select 0

# Configure handover triggers
uesim(ue:0)> meas add A3           # Handover when neighbor 3dB better
uesim(ue:0)> meas add A4           # Handover when neighbor > -100dBm
uesim(ue:0)> meas add A5           # Handover when serving bad + neighbor good

# Customize thresholds
uesim(ue:0)> meas set A3 offset 5
uesim(ue:0)> meas set A4 threshold -95
uesim(ue:0)> meas set A5 threshold1 -115
uesim(ue:0)> meas set A5 threshold2 -105

# View configuration
uesim(ue:0)> meas list

# Execute measurement
uesim(ue:0)> meas run

# View results
uesim(ue:0)> meas show
```

**Legacy Commands (Backward Compatible):**

| Command | Description |
|---------|-------------|
| `start` | Start UE simulation |
| `stop` | Stop UE simulation |
| `status` | Show current status |
| `help` | Show help |
| `exit` | Exit interactive mode |

### 6. Configuration (`src/config/`)

INI-style configuration file (`etc/uesim.conf`):

```ini
general {
    num_instances = 10;
    log_level = 2;        # 0=ERROR, 1=WARN, 2=INFO, 3=DEBUG, 4=TRACE
    verbose = false;
    debug = false;
}

network {
    gnb_ip = "192.168.1.100";
    gnb_ngap_port = 38412;
    gnb_gtpu_port = 2152;
    local_ip = "0.0.0.0";
}

ue {
    imsi_prefix = "00101";
    imsi_start = 1000000000;
    tac = 1;
    mcc = "001";
    mnc = "01";
}

rrc {
    registration_timeout = 30;
    enable_registration = true;
    enable_handover = true;
}

pdcp {
    ciphering_algorithm = 2;   # NEA2 (AES)
    integrity_algorithm = 2;   # NIA2 (AES)
    enable_ciphering = true;
    enable_integrity = true;
}

performance {
    thread_pool_size = 0;      # Auto-detect
    rx_buffer_size = 65536;
    tx_buffer_size = 65536;
    use_memory_pool = true;
}
```

### 7. Ring Buffer (`src/utils/`)

Thread-safe producer-consumer buffer:

```c
#include "utils/ring_buffer.h"

ring_buffer_t rb;
ring_buffer_init(&rb, 4096);

// Write (producer)
ring_buffer_write(&rb, data, length);

// Read (consumer)
ring_buffer_read(&rb, buffer, length);

// Cleanup
ring_buffer_destroy(&rb);
```

### 8. Benchmark (`src/benchmark/`)

Performance measurement framework:

```c
#include "benchmark/benchmark.h"

benchmark_ctx_t ctx;
benchmark_init(&ctx, BENCHMARK_CATEGORY_PROTOCOL, "RRC Registration");
benchmark_start(&ctx);

// ... operation to measure ...

benchmark_stop(&ctx);
printf("Duration: %s\n", benchmark_format_time(ctx.elapsed_ns));
benchmark_cleanup(&ctx);
```

## Build System

### Build Types

```bash
make BUILD_TYPE=release    # -O2 -DNDEBUG (default)
make BUILD_TYPE=debug      # -g -DDEBUG -O0
make BUILD_TYPE=profile    # -pg -O2
```

### Build Targets

| Target | Description |
|--------|-------------|
| `uesim` (default) | Build executable |
| `clean` | Remove build artifacts |
| `install` | Install to PREFIX |
| `uninstall` | Remove installed files |

### Platform-Specific Notes

**RHEL 8.5:**
- GCC 8.5+ required (`gcc` from `Development Tools` group)
- Security hardening enabled: `-fstack-protector-strong`, `-D_FORTIFY_SOURCE=2`, PIE, full RELRO
- Links: `-lpthread -lrt`
- Install: `yum groupinstall "Development Tools"`

**Windows (MinGW):**
- MinGW-w64 GCC 6.3+ required
- Winsock2 for sockets (`-lws2_32`)
- Windows API for threading (HANDLE-based mutex/cond/thread wrappers)
- Atomic ops via `InterlockedExchange*` family

### Cross-Compilation

```bash
make CROSS_COMPILE=yes CROSS_COMPILER=aarch64-linux-gnu-gcc
```

## Error Codes

| Code | Constant | Description |
|------|----------|-------------|
| 0 | `UESIM_SUCCESS` | Operation succeeded |
| -1 | `UESIM_ERROR_INVALID_PARAM` | Invalid parameter passed |
| -2 | `UESIM_ERROR_MEMORY` | Memory allocation failed |
| -3 | `UESIM_ERROR_SOCKET` | Socket operation failed |
| -4 | `UESIM_ERROR_THREAD` | Thread synchronization error |
| -5 | `UESIM_ERROR_TIMEOUT` | Operation timed out |
| -6 | `UESIM_ERROR_PROTOCOL` | Protocol-level error |
| -7 | `UESIM_ERROR_FILE` | File I/O error |
| -8 | `UESIM_ERROR_NOT_INITIALIZED` | Subsystem not initialized |

## Typical Workflows

### Single UE Registration Test

```bash
# 1. Configure gNB address
cat > /tmp/uesim.conf << EOF
general { num_instances = 1; log_level = 3; }
network { gnb_ip = "10.0.1.100"; gnb_ngap_port = 38412; }
EOF

# 2. Run
./uesim -c /tmp/uesim.conf
```

### Multi-UE Load Test

```bash
# Simulate 100 UEs with verbose logging
./uesim -i 100 -v -c etc/uesim.conf
```

### Interactive Debugging

```bash
./uesim -I -d
# > status
# > show network
# > set general verbose true
# > start
# > stop
# > exit
```

### Interactive Mode with Mock Components

```bash
# Start interactive mode with mock core and gNB
./uesim -I --with-mock

# Or short form
./uesim -I -M

# Inside interactive mode:
uesim> ue start 5
uesim> scenario run registration
uesim> status
uesim> exit
```

### Test Mode with Mock Components

```bash
# Run automated test with 5 UEs
./uesim -t -u 5

# Run test with scenario file
./uesim -t -s scenarios/registration_scenario.json -u 10

# Run test and generate report
./uesim -t -u 5 -r test_report.txt

# Full test with all options
./uesim -t -u 10 -s scenarios/handover_scenario.json -r results.txt
```

### Performance Benchmark

```bash
./uesim -c etc/uesim.conf -i 50
# Benchmark results logged to stdout
