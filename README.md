# 5G UE Simulation Application

A C-based User Equipment (UE) simulator for 5G NR networks in NFV environments, designed for RHEL 8.5.

## Overview

This application simulates 5G User Equipment behavior for testing and development of 5G networks. It supports multiple UE instances and implements key RRC procedures including registration, establishment, re-establishment, and handover.

## Features

- **Multi-UE Support**: Simulate multiple UE instances concurrently
- **RRC State Management**: Full RRC state machine implementation
- **5G NR Protocol Stack**: RRC, PDCP, RLC, MAC layer simulation
- **Socket Communication**: NGAP/SCTP and GTP-U/UDP socket handling
- **CLI Interface**: Command-line interface for control and monitoring
- **Thread-Safe Design**: Thread pool, mutexes, condition variables
- **Advanced Memory Management**: Custom memory pool with thread safety
- **IPC Mechanisms**: Ring buffers, shared memory, message queues
- **Mock Core Network**: Built-in mock AMF, SMF, UPF, CU-CP, DU, CU-UP, XnAP for testing
- **Test Mode**: Automated scenario-based testing with JSON test files

## Architecture

```
+-------------------+
|    CLI Interface  |
+-------------------+
|  Scenario Manager |
+-------------------+
|   RRC State Mgr   |
+-------------------+
| Protocol Handlers |
| (RRC/PDCP/RLC/MAC)|
+-------------------+
| Socket Transport  |
+-------------------+
|  Configuration    |
+-------------------+
```

## Build Requirements

### Linux (RHEL 8.5 / CentOS / Rocky Linux)

**Required packages:**
```bash
# Install development tools and dependencies
sudo yum install -y gcc make libsctp-devel

# Optional: For advanced features
sudo yum install -y pkgconfig
```

**Minimum requirements:**
- RHEL 8.5 or compatible distribution (CentOS 8, Rocky Linux 8, AlmaLinux 8)
- GCC 8.5 or higher
- libsctp-devel (for SCTP support, falls back to TCP if not available)
- Development tools (make, gcc)

### Debian / Ubuntu

```bash
# Install development tools and dependencies
sudo apt-get update
sudo apt-get install -y build-essential libsctp-dev pkg-config
```

### macOS

```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install dependencies via Homebrew
brew install libsctp pkg-config
```

### Windows

**Required:**
- MSYS2 or MinGW with GCC
- Make utility

```cmd
# Using MSYS2
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-make
```

## Build Instructions

### Quick Start (Linux)

```bash
# Clone the repository
git clone <repository-url>
cd uesim

# Run configure (auto-detects system capabilities)
./configure

# Build the application
make

# Install (optional)
sudo make install
```

### Configuration Options

The `configure` script auto-detects system capabilities. Available options:

```bash
./configure --help

# Common options:
./configure --prefix=/opt/uesim      # Custom installation path
./configure --enable-debug           # Debug build
./configure --disable-sctp           # Disable SCTP (use TCP fallback)
./configure --cross-compile=aarch64  # Cross-compile for ARM64
```

### Manual Build (without configure)

If you prefer not to use the configure script:

```bash
# RHEL 8.5 / CentOS
make

# With debug symbols
make BUILD_TYPE=debug

# With compression (requires UPX)
make COMPRESS=yes

# Clean build
make clean
```

### Cross-Compilation

For ARM64 (aarch64) targets:

```bash
# Install cross-compiler
sudo yum install -y gcc-aarch64-linux-gnu

# Configure and build
./configure --cross-compile=aarch64
make
```

### Windows

```cmd
# Clone the repository
git clone <repository-url>
cd uesim

# Build using the batch script (recommended)
build.bat

# Or build directly with make
make

# Build with debug symbols
make BUILD_TYPE=debug

# Clean build
make clean
```

**Note**: On Windows, you need to have MSYS2 or MinGW installed with GCC and make utilities. The build.bat script will guide you through the build process and check for required tools.

## Usage

```bash
# Start with default settings
./uesim

# Start with specific number of UE instances
./uesim --instances 10

# Start with configuration file
./uesim --config uesim.conf

# Verbose mode
./uesim --verbose

# Debug mode
./uesim --debug

# Interactive mode with mock core network
./uesim -I --with-mock

# Run automated test with scenario file
./uesim --test --scenario scenarios/registration_scenario.json
```

## Mock Core Network

The built-in mock core network allows testing without external network components:

| Component | Description | Default Port |
|-----------|-------------|--------------|
| AMF | Access and Mobility Management Function | 38412 |
| SMF | Session Management Function | 38413 |
| UPF | User Plane Function | 2152 |
| CU-CP | Central Unit - Control Plane | 38472 |
| DU | Distributed Unit | 38473 |
| CU-UP | Central Unit - User Plane | 38474 |
| XnAP | Inter-gNB Communication | 38423 |

### Mock CLI Commands

- `mock start` - Start mock core network components
- `mock stop` - Stop mock components
- `mock status` - Show mock component status

## Test Mode

Run automated tests with JSON scenario files:

```bash
# Available test scenarios
./uesim --test --scenario scenarios/registration_scenario.json
./uesim --test --scenario scenarios/pdu_session_scenario.json
./uesim --test --scenario scenarios/handover_scenario.json
./uesim --test --scenario scenarios/deregistration_scenario.json
```

## CLI Commands

- `start` - Start UE simulation
- `stop` - Stop UE simulation
- `status` - Show UE status
- `config` - Configure UE parameters
- `scenario` - Execute RRC scenario
- `help` - Show help message
- `exit` - Exit the application

## RRC Scenarios

- `registration` - RRC registration procedure
- `establishment` - RRC establishment procedure
- `reestablishment` - RRC re-establishment procedure
- `handover` - RRC handover procedure

## Directory Structure

```
uesim/
├── src/                 # Source code
│   ├── core/           # Core framework
│   ├── protocol/       # Protocol stack
│   ├── transport/      # Socket transport
│   ├── cli/            # Command line interface
│   ├── utils/          # Utility functions
│   ├── main.c          # Main entry point
│   └── uesim.h         # Main header
├── etc/                # Configuration files
├── tests/              # Unit tests
├── docs/               # Documentation
├── Makefile           # Main makefile
├── config.mk          # Build configuration
├── rules.mk           # Build rules
├── targets.mk         # Build targets
├── compress.mk        # Compression rules
└── README.md          # This file
```

## Advanced C Features

The application demonstrates expert C programming techniques:

- **Pointer Manipulation**: Function pointers, void pointers, pointer arithmetic
- **Memory Management**: Custom memory pool, stack/heap/data segment layout
- **Bitwise Operations**: Bit manipulation for protocol field handling
- **Macro Usage**: Preprocessor macros for configuration and optimization
- **Thread Safety**: Mutexes, condition variables, atomic operations
- **IPC Mechanisms**: Shared memory, message queues, ring buffers
- **Socket Programming**: Advanced socket handling with epoll
- **Build System**: Advanced Makefile with compression and flag management

## Configuration

The application can be configured through command-line options or configuration files. Key parameters include:

- Number of UE instances
- gNB IP address and port
- IMSI and MSISDN ranges
- TAC (Tracking Area Code)
- Logging levels

## Testing

Unit tests are provided in the `tests/` directory. Run tests with:

```bash
make test
```

## Performance Considerations

- Optimized for RHEL 8.5
- Memory pool allocation for reduced malloc overhead
- Thread pool for efficient multi-UE handling
- Non-blocking I/O with epoll
- Lock-free data structures where possible

## Security

- Stack protection enabled
- Address space layout randomization (ASLR) support
- Secure compilation flags
- Input validation and bounds checking

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Push to the branch
5. Create a Pull Request

## Authors

- Telecom Software Developer - Nguyen Van Tan Phat

## Acknowledgments

- Based on 3GPP specifications for 5G NR
- Inspired by UERANSIM project
- Designed for O-RAN compatible 5G networks