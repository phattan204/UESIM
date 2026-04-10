# 5G UE Simulation Application Architecture

## Overview

The 5G UE Simulation application is designed as a modular, thread-safe C application that simulates User Equipment behavior in 5G NR networks. The architecture follows a layered approach with clear separation of concerns.

## System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    Application Layer                            │
├─────────────────────────────────────────────────────────────────┤
│                    CLI Interface                                │
├─────────────────────────────────────────────────────────────────┤
│                    Scenario Manager                             │
├─────────────────────────────────────────────────────────────────┤
│                    RRC State Manager                            │
├─────────────────────────────────────────────────────────────────┤
│              Protocol Stack (RRC/PDCP/RLC/MAC)                  │
├─────────────────────────────────────────────────────────────────┤
│                    Transport Layer                              │
│              (Socket Management, Buffering)                     │
├─────────────────────────────────────────────────────────────────┤
│                    System Services                              │
│          (Memory Management, Threading, IPC)                    │
└─────────────────────────────────────────────────────────────────┘
```

## Component Architecture

### 1. Core Framework

**Main Application (src/main.c)**
- Entry point and command-line argument parsing
- Application lifecycle management
- Global resource initialization and cleanup

**Memory Management (src/core/memory.*)**
- Custom memory pool implementation
- Thread-safe allocation/deallocation
- Memory layout management (stack, heap, data segment)

**UE Management (src/core/uesim_core.*)**
- UE instance creation and management
- Multi-UE coordination
- Global state management

### 2. Protocol Stack

**RRC Layer (src/protocol/rrc.*)**
- RRC state machine implementation
- RRC procedure handling (Registration, Establishment, Re-establishment, Handover)
- Message encoding/decoding
- State transition management

**PDCP/RLC/MAC Layers (src/protocol/)**
- Protocol layer implementations
- Data processing and forwarding
- Quality of Service (QoS) handling

### 3. Transport Layer

**Socket Manager (src/transport/socket_mgr.*)**
- NGAP/SCTP socket handling
- GTP-U/UDP socket management
- Non-blocking I/O with epoll
- Connection management

**Buffering System (src/utils/ring_buffer.*)**
- Ring buffer implementation for IPC
- Thread-safe data queuing
- Flow control mechanisms

### 4. User Interface

**CLI Interface (src/cli/cli.*)**
- Command parsing and execution
- Interactive mode support
- Status reporting
- Scenario execution

### 5. System Services

**Threading System**
- Thread pool implementation
- Worker thread management
- Task scheduling

**Synchronization**
- Mutexes for resource protection
- Condition variables for signaling
- Atomic operations for lock-free programming

**IPC Mechanisms**
- Shared memory segments
- Message queues
- Ring buffers
- Pipe communication

## Data Flow

### UE Instance Creation Flow
```
main() → uesim_create_ue_instance() → [Memory Allocation] → [Socket Creation] → [RRC Init]
```

### RRC Procedure Execution Flow
```
CLI Command → Scenario Manager → RRC Execute Procedure → Message Encoding → Socket Send → [Wait for Response] → State Update
```

### Data Reception Flow
```
Socket I/O Thread → Message Reception → RRC Decode → Procedure Handler → State Update → CLI Notification
```

## Memory Architecture

### Memory Layout
```
High Address
┌─────────────────┐ ← Stack (8MB per thread)
├─────────────────┤
┌─────────────────┐ ← Heap (64MB custom pool)
│  Memory Pool    │
└─────────────────┘
├─────────────────┤
│  Data Segment   │ ← Global variables (16MB)
├─────────────────┤
│  Text Segment   │ ← Code
Low Address
└─────────────────┘
```

### Memory Management Strategy
- Custom memory pool for frequent allocations
- Thread-local storage for per-thread data
- Reference counting for shared resources
- Automatic cleanup on UE instance destruction

## Threading Model

### Thread Types
1. **Main Thread**: Application control and CLI
2. **I/O Threads**: Socket communication handling
3. **Worker Threads**: UE instance processing
4. **Timer Threads**: Timeout management

### Thread Pool Architecture
```
┌─────────────────┐    ┌─────────────────┐
│   Task Queue    │───→│  Worker Thread  │
├─────────────────┤    ├─────────────────┤
│  Thread Pool    │    │  Worker Thread  │
├─────────────────┤    ├─────────────────┤
│  Load Balancer  │    │  Worker Thread  │
└─────────────────┘    └─────────────────┘
```

## IPC Mechanisms

### Ring Buffers
- Lock-free circular buffers for high-performance data transfer
- Producer-consumer pattern implementation
- Atomic head/tail pointers

### Shared Memory
- Memory-mapped regions for inter-process communication
- Synchronized access with semaphores
- Data consistency guarantees

### Message Queues
- POSIX message queues for structured communication
- Priority-based message handling
- Blocking/non-blocking operations

## Error Handling

### Error Types
- **System Errors**: Memory, socket, thread failures
- **Protocol Errors**: RRC procedure failures, message format errors
- **Configuration Errors**: Invalid parameters, missing files
- **Runtime Errors**: Timeout, connection failures

### Error Propagation
- Error codes returned through function return values
- Detailed error logging with context information
- Graceful degradation when possible

## Build System

### Makefile Architecture
```
Makefile (Main)
├── config.mk (Build configuration)
├── rules.mk (Compilation rules)
├── targets.mk (Build targets)
└── compress.mk (Optimization rules)
```

### Build Features
- Cross-compilation support
- Debug/Release builds
- Binary compression
- Profile-guided optimization
- Dependency tracking

## Security Architecture

### Protection Mechanisms
- Stack protection (`-fstack-protector`)
- Address Space Layout Randomization (ASLR)
- Non-executable stack and heap
- Input validation and bounds checking

### Secure Coding Practices
- Buffer overflow prevention
- Format string protection
- Integer overflow checking
- Race condition avoidance

## Performance Considerations

### Optimization Strategies
- Memory pool allocation to reduce malloc overhead
- Lock-free data structures where possible
- Cache-friendly data layout
- Minimal system call overhead
- Efficient I/O multiplexing with epoll

### Scalability Features
- Thread pool for multi-UE handling
- Connection pooling for socket reuse
- Asynchronous I/O operations
- Load balancing across worker threads

## Testing Architecture

### Test Layers
1. **Unit Tests**: Individual function testing
2. **Integration Tests**: Component interaction testing
3. **System Tests**: End-to-end scenario testing
4. **Performance Tests**: Load and stress testing

### Test Infrastructure
- Mock gNB server for protocol testing
- Automated test execution framework
- Performance monitoring and reporting
- Regression testing suite

## Deployment Architecture

### RHEL 8.5 Optimizations
- Systemd service integration
- SELinux policy compliance
- Log rotation and management
- Package management (RPM)
- System monitoring integration

### Container Support
- Docker containerization
- Kubernetes deployment manifests
- Health check endpoints
- Resource limits and quotas

## Monitoring and Logging

### Logging Levels
- **ERROR**: Critical failures requiring immediate attention
- **WARN**: Non-critical issues that should be investigated
- **INFO**: General operational information
- **DEBUG**: Detailed debugging information
- **TRACE**: Fine-grained trace information

### Monitoring Features
- Performance metrics collection
- Health status reporting
- Resource utilization tracking
- Connection state monitoring