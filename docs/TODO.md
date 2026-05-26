# UESim TODO List and Known Issues

This document tracks all known issues, stub implementations, incomplete features, and technical debt in the UESim project.

**Last Updated:** 2026-05-21

---

## Table of Contents

1. [Critical Issues](#critical-issues)
2. [High Priority](#high-priority)
3. [Medium Priority](#medium-priority)
4. [Low Priority](#low-priority)
5. [Known Stubs](#known-stubs)
6. [Technical Debt](#technical-debt)
7. [Future Enhancements](#future-enhancements)

---

## Critical Issues

### 1. PHY Layer - Placeholder Implementation

**File:** `src/protocol/phy.c`

**Issue:** PHY layer functions return placeholder values for power control and signal measurements.

```c
// Current implementation
int16_t phy_get_current_power(phy_context_t* ctx) {
    if (ctx == NULL) return 0;
    return ctx->power.p_pusch;  // Returns configured value, not actual measurement
}
```

**Impact:** Cannot perform realistic PHY layer simulations.

**Fix Required:** Implement actual power control algorithms per 3GPP TS 38.213.

---

### 2. NAS Network Slicing - Incomplete Integration

**File:** `src/nas/network_slicing.c`

**Issue:** S-NSSAI selection and slice authentication not fully integrated with AMF.

**Impact:** Network slicing simulation limited to basic S-NSSAI storage.

**Fix Required:** 
- Implement slice selection during registration
- Add NSSAA (Network Slice-Specific Authentication and Authorization)
- Integrate with mock AMF slice handling

---

## High Priority

### 3. SDAP Layer - Not Integrated in Data Path

**Files:** `src/protocol/sdap.c`, `src/protocol/sdap.h`

**Issue:** SDAP layer is fully implemented per 3GPP TS 37.324 but not integrated into the main UE data path.

**Status:** Implementation complete, integration pending.

**Fix Required:**
- Add SDAP entity creation during PDU session establishment
- Integrate SDAP between application IP layer and PDCP
- Add SDAP header to protocol stack diagram

---

### 4. Mock UPF - Data Forwarding Not Implemented

**File:** `src/mock_core/mock_upf.c`

**Issue:** UPF mock does not forward data to external network.

```c
/* Forward to peer (not implemented in mock) */
```

**Impact:** Cannot test end-to-end data path beyond UPF.

**Workaround:** Use external UPF (e.g., UERANSIM) for full data path testing.

---

### 5. Test Flow Controller - Custom Steps

**File:** `src/mock_integration/test_flow_controller.c`

**Issue:** Custom test step handler returns "not implemented".

```c
strncpy(step->result_message, "Custom step not implemented", sizeof(step->result_message) - 1);
```

**Impact:** Limited test scenario flexibility.

**Fix Required:** Implement custom step callback mechanism.

---

## Medium Priority

### 6. SCTP Native Support - Auto-Detection

**File:** `configure`, `src/transport/sctp_transport.c`

**Issue:** SCTP native support depends on `HAVE_SCTP` define which requires manual configuration on some systems.

**Status:** Configure script auto-detects, but may fail on non-standard installations.

**Improvement:** Add runtime SCTP capability check and fallback to TCP mode.

---

### 7. PFCP Messages - Limited IE Support

**File:** `src/protocol/pfcp_messages.c`

**Issue:** Only basic IEs (Information Elements) implemented. Missing:
- QoS Enforcement Rules
- Packet Detection Rules with full matching
- Usage Reporting Rules

**Impact:** Limited PFCP session management capabilities.

---

### 8. E1AP/F1AP/XnAP - Basic Implementation

**Files:** 
- `src/protocol/e1ap_messages.c`
- `src/protocol/f1ap_messages.c`
- `src/protocol/xnap_messages.c`

**Issue:** Only basic message types implemented. Missing:
- Resource management procedures
- Error handling for invalid messages
- Full IE encoding/decoding

---

## Low Priority

### 9. Checksum Placeholders

**File:** `src/transport/pcap_capture.c`

**Issue:** Checksum calculation uses placeholder values.

```c
/* Checksum placeholder - will be filled after payload */
buf[8] = 0x00;
```

**Impact:** Captured packets have invalid checksums (Wireshark shows checksum errors).

**Fix Required:** Implement proper IP/UDP/TCP checksum calculation.

---

### 10. RNTI Placeholder

**File:** `src/mock_gnb/mock_gnb_server.c`

**Issue:** RNTI assignment uses placeholder value.

```c
rrc_data[1] = htonl(0);  /* RNTI placeholder */
```

**Impact:** RNTI not properly assigned in RRC messages.

---

### 11. Log Export Module - Limited Formats

**File:** `src/utils/log_export.c`

**Issue:** Limited export format support.

**Enhancement:** Add JSON, XML export formats.

---

## Known Stubs

### Functions Returning Early (Intentional)

These functions return `NULL`, `0`, or `-1` as proper error handling, not as stubs:

| File | Function | Reason |
|------|----------|--------|
| `src/utils/ring_buffer.c` | `ring_buffer_available_write()` | Returns 0 when buffer full |
| `src/protocol/zuc.c` | `zuc_init()` | Returns 0 on invalid params |
| `src/protocol/snow3g.c` | `snow3g_init()` | Returns 0 on invalid params |
| `src/nas/qos_flow.c` | `qos_flow_find_by_qfi()` | Returns NULL when not found |
| `src/nas/pdu_session.c` | `pdu_session_find_by_id()` | Returns NULL when not found |

### Intentionally Unused Functions

These functions are declared but may not be called in current implementation:

| File | Function | Purpose |
|------|----------|---------|
| `src/protocol/rrc_si.c` | `rrc_get_sfn_increment()` | SFN calculation utility |
| `src/core/gnb_manager.c` | `gnb_manager_get_history()` | Historical gNB data |

---

## Technical Debt

### 1. Memory Management

**Issue:** Mixed use of memory pool and system malloc.

**Files:** `src/core/memory.c`

**Debt:** 
- Pool allocator doesn't track individual allocations
- `uesim_free()` doesn't actually free to pool

**Fix Required:** Implement proper pool deallocation or document pool-based lifetime management.

---

### 2. Thread Safety Audits

**Issue:** Not all shared data structures have proper mutex protection.

**Files:** Multiple

**Audit Required:**
- [ ] `ue_context_t` - All fields protected?
- [ ] `gnb_context_t` - All fields protected?
- [ ] Global state variables

---

### 3. Error Message Quality

**Issue:** Many error returns don't provide context.

**Example:**
```c
return UESIM_ERROR_MEMORY;  // No context about what allocation failed
```

**Improvement:** Add error context or use structured error reporting.

---

### 4. Configuration File Validation

**File:** `src/config/config.c`

**Issue:** Limited validation of configuration values.

**Improvement:** Add range checking, dependency validation between config options.

---

## Future Enhancements

### Protocol Enhancements

1. **RRC Layer:**
   - [ ] Full RRC Reconfiguration support
   - [ ] RRC Inactive state handling
   - [ ] Connected Mode DRX (C-DRX)

2. **NAS Layer:**
   - [ ] 5G AKA authentication full implementation
   - [ ] EAP-AKA' support
   - [ ] UE Policy Container handling

3. **User Plane:**
   - [ ] Full SDAP integration
   - [ ] GTP-U encapsulation/decapsulation
   - [ ] QoS flow enforcement

### Testing Enhancements

1. **Unit Tests:**
   - [ ] Increase code coverage to >80%
   - [ ] Add fuzzing tests for ASN.1 decoders
   - [ ] Add performance benchmarks

2. **Integration Tests:**
   - [ ] Test with commercial 5G core
   - [ ] Test with OAI gNB
   - [ ] Test with srsRAN

### Platform Enhancements

1. **Cross-Platform:**
   - [ ] macOS native build support
   - [ ] Docker container for easy deployment
   - [ ] Kubernetes operator for scale testing

2. **Performance:**
   - [ ] DPDK integration for high-performance packet processing
   - [ ] Multi-process architecture for >1000 UEs
   - [ ] NUMA-aware memory allocation

---

## Compiler Warnings Configuration

To detect unused code and potential issues, the following warnings should be enabled:

```makefile
# In config.mk or rules.mk
WARNINGS = -Wall -Wextra
WARNINGS += -Wunused-function
WARNINGS += -Wunused-variable
WARNINGS += -Wunused-parameter
WARNINGS += -Wmissing-prototypes
WARNINGS += -Wstrict-prototypes
WARNINGS += -Wold-style-definition
WARNINGS += -Wno-unused-function  # For intentionally unused static functions
```

---

## Action Items

> **Note:** For the complete implementation roadmap with detailed phases, timelines, and milestones, see [Roadmap](roadmap.md).

### Immediate (Next Release)

- [ ] Add compiler warnings to build system
- [ ] Document SDAP integration status
- [ ] Fix RNTI placeholder in mock gNB
- [ ] Add checksum calculation in pcap_capture

### Short Term (1-3 Months)

- [ ] Integrate SDAP into data path
- [ ] Implement custom test step callbacks
- [ ] Add SCTP runtime detection fallback
- [ ] Audit thread safety

### Long Term (3-6 Months)

- [ ] Complete PHY layer implementation
- [ ] Full NAS authentication support
- [ ] Increase test coverage to 80%
- [ ] Docker container support

---

## References

- 3GPP TS 37.324 - SDAP specification
- 3GPP TS 38.331 - RRC specification
- 3GPP TS 24.501 - NAS specification
- 3GPP TS 38.213 - PHY procedures

---

*This document should be updated whenever new issues are discovered or existing issues are resolved.*
