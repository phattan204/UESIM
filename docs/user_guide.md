# 5G UE Simulation - User Guide

## Quick Start

```bash
# Build the application
make clean && make

# Run with default configuration
./uesim

# Run with custom configuration
./uesim -c etc/uesim.conf

# Run in interactive mode
./uesim -I

# Run with multiple UE instances
./uesim -i 100
```

## Command Line Options

| Option | Description | Example |
|--------|-------------|---------|
| `-c, --config FILE` | Load configuration from file | `./uesim -c etc/uesim.conf` |
| `-i, --instances N` | Number of UE instances | `./uesim -i 100` |
| `-v, --verbose` | Enable verbose logging | `./uesim -v` |
| `-d, --debug` | Enable debug mode | `./uesim -d` |
| `-I, --interactive` | Start in interactive mode | `./uesim -I` |
| `-M, --with-mock` | Start with mock core network | `./uesim -I --with-mock` |
| `-t, --test` | Run in test mode | `./uesim --test` |
| `-s, --scenario FILE` | Test scenario file | `./uesim --test --scenario scenarios/registration.json` |
| `-h, --help` | Show help message | `./uesim -h` |

---

## Interactive CLI Commands

### Basic Commands

| Command | Description | Example |
|---------|-------------|---------|
| `start` | Start UE simulation | `uesim> start` |
| `stop` | Stop UE simulation | `uesim> stop` |
| `status` | Show UE status | `uesim> status` |
| `help` | Show available commands | `uesim> help` |
| `exit` | Exit the application | `uesim> exit` |

### Scenario Commands

| Command | Description | Example |
|---------|-------------|---------|
| `scenario <type> [ue_id]` | Execute RRC scenario | `uesim> scenario registration 0` |

**Available Scenarios:**
- `registration` - RRC registration procedure
- `establishment` - RRC establishment procedure
- `reestablishment` - RRC re-establishment procedure
- `handover` - RRC handover procedure

### QoS Commands

| Command | Description | Example |
|---------|-------------|---------|
| `qos create <ue_id> <5qi>` | Create QoS flow | `uesim> qos create 0 9` |
| `qos list <ue_id>` | List QoS flows | `uesim> qos list 0` |

### Session Commands

| Command | Description | Example |
|---------|-------------|---------|
| `session create <ue_id>` | Create PDU session | `uesim> session create 0` |
| `session list <ue_id>` | List sessions | `uesim> session list 0` |

### LoadTest Commands

| Command | Description | Example |
|---------|-------------|---------|
| `loadtest start <scenario> <ues> <dur>` | Start load test | `uesim> loadtest start burst 100 60` |
| `loadtest status` | Show status | `uesim> loadtest status` |

### gNB Commands

| Command | Description | Example |
|---------|-------------|---------|
| `gnb add <type> <ip> <port>` | Add gNB | `uesim> gnb add oai 192.168.1.2 38412` |
| `gnb list` | List gNBs | `uesim> gnb list` |

### Configuration Commands

| Command | Description | Example |
|---------|-------------|---------|
| `show <section> [key]` | Show configuration values | `uesim> show network` |
| `set <section> <key> <value>` | Set configuration value | `uesim> set network gnb_ip 192.168.1.10` |
| `save <file>` | Save configuration to file | `uesim> save my_config.conf` |
| `load <file>` | Load configuration from file | `uesim> load etc/uesim.conf` |
| `config <file>` | Load configuration file | `uesim> config etc/uesim.conf` |

**Configuration Sections:**
- `general` - General settings
- `network` - Network settings
- `ue` - UE settings
- `rrc` - RRC settings
- `pdcp` - PDCP settings
- `rlc` - RLC settings
- `mac` - MAC settings
- `nas` - NAS settings
- `performance` - Performance settings
- `security` - Security settings
- `test` - Test settings

---

## Scenario Examples

### Scenario 1: Basic Registration

```bash
# Start interactive mode
./uesim -I

# CLI commands
uesim> start
uesim> scenario registration
uesim> status
uesim> stop
uesim> exit
```

### Scenario 2: Load and Run with Custom Config

```bash
# Load custom configuration
uesim> config etc/uesim.conf
uesim> show network
uesim> start
uesim> scenario registration
uesim> status
uesim> stop
```

### Scenario 3: Modify Configuration at Runtime

```bash
uesim> show network gnb_ip
uesim> set network gnb_ip 192.168.1.100
uesim> set network gnb_ngap_port 38412
uesim> show network
uesim> start
uesim> scenario establishment
```

### Scenario 4: Handover Test

```bash
uesim> start
uesim> scenario registration
uesim> status
uesim> scenario handover
uesim> status
uesim> stop
```

---

## Mock Core Network Testing

### Interactive Mode with Mock Components

Start interactive mode with mock core network auto-started:

```bash
# Start with mock core network
./uesim -I --with-mock

# CLI commands
uesim> mock status
uesim> start
uesim> scenario registration
uesim> status
uesim> stop
uesim> mock stop
uesim> exit
```

### Mock Control Commands

| Command | Description | Example |
|---------|-------------|---------|
| `mock start` | Start mock core network | `uesim> mock start` |
| `mock stop` | Stop mock components | `uesim> mock stop` |
| `mock status` | Show mock component status | `uesim> mock status` |

### Mock Components

The mock core network includes:

| Component | Description | Port |
|-----------|-------------|------|
| AMF | Access and Mobility Management Function | 38412 |
| SMF | Session Management Function | 38413 |
| UPF | User Plane Function | 2152 |
| CU-CP | Central Unit - Control Plane | 38472 |
| DU | Distributed Unit | 38473 |
| CU-UP | Central Unit - User Plane | 38474 |
| XnAP | Inter-gNB Communication | 38423 |

---

## Test Mode (Automated Testing)

### Running Test Scenarios

Run automated tests with scenario files:

```bash
# Run registration test
./uesim --test --scenario scenarios/registration_scenario.json

# Run PDU session test
./uesim --test --scenario scenarios/pdu_session_scenario.json

# Run handover test
./uesim --test --scenario scenarios/handover_scenario.json

# Run deregistration test
./uesim --test --scenario scenarios/deregistration_scenario.json
```

### Test Scenario File Format

Test scenarios are defined in JSON format:

```json
{
  "name": "Registration Test",
  "description": "Test UE registration procedure",
  "steps": [
    {
      "action": "start",
      "params": {}
    },
    {
      "action": "scenario",
      "params": {
        "type": "registration"
      }
    },
    {
      "action": "verify",
      "params": {
        "state": "registered"
      }
    }
  ],
  "expected_result": "success"
}
```

### Test Output

Test mode provides detailed output:

```
=== UESim Test Mode ===
Scenario: scenarios/registration_scenario.json
Name: Registration Test
Steps: 3

[1/3] start... PASSED
[2/3] scenario registration... PASSED
[3/3] verify state=registered... PASSED

=== Test Results ===
Total: 3 | Passed: 3 | Failed: 0
Result: PASS
```

---

## Configuration Quick Reference

### General Settings
```
general {
    num_instances = 1;
    log_level = 2;          # 0=ERROR, 1=WARN, 2=INFO, 3=DEBUG, 4=TRACE
    verbose = false;
    debug = false;
}
```

### Network Settings
```
network {
    gnb_ip = "127.0.0.1";
    gnb_ngap_port = 38412;
    gnb_gtpu_port = 2152;
    multi_gnb_enabled = false;
    gnb_type = "oai";
}
```

---

## Troubleshooting

### Common Issues

**Configuration file not found:**
```bash
ls etc/uesim.conf
./uesim -c /full/path/to/uesim.conf
```

**Unknown command:**
```
uesim> test
Unknown command: test
Type 'help' for available commands