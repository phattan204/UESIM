# ASN.1 Schema Files for 3GPP Protocol Validation

This directory contains ASN.1 schema files extracted from 3GPP specifications for formal message validation.

## Files

| File | Protocol | 3GPP Spec | Version |
|------|----------|-----------|---------|
| `NR-RRC-38331.asn` | NR Radio Resource Control | TS 38.331 | 17.4.0 |
| `NGAP-38413.asn` | NG Application Protocol | TS 38.413 | 17.4.0 |
| `F1AP-38473.asn` | F1 Application Protocol | TS 38.473 | 17.4.0 |
| `E1AP-38463.asn` | E1 Application Protocol | TS 38.463 | 17.4.0 |
| `XnAP-38423.asn` | Xn Application Protocol | TS 38.423 | 17.4.0 |

## Usage

### 1. ASN.1 Compiler (asn1c)

The open-source ASN.1 compiler can generate C code from these schemas:

```bash
# Install asn1c
sudo apt-get install asn1c  # Ubuntu/Debian
brew install asn1c         # macOS

# Compile schema
asn1c -fcompound-names -gen-PER NGAP-38413.asn

# This generates C encoder/decoder functions
```

### 2. Online Validators

- [ASN.1 Playground](https://asn1.io/asn1playground/) - Online ASN.1 compiler and validator
- [ObjSys](https://www.obj-sys.com/products-asn1.php) - Commercial ASN.1 tools

### 3. Wireshark Integration

Wireshark can decode NGAP, F1AP, E1AP, XnAP, and RRC messages if the schemas are available:

1. Place `.asn` files in Wireshark's asn1 directory
2. Enable protocol dissectors in preferences

### 4. Manual Validation

Compare encoded messages against schema definitions:

```c
// Example: Validate RRC Setup message
#include "asn1/per_encoder.h"
#include "asn1/per_decoder.h"

// Encode
RRCSetup_t rrc_setup;
memset(&rrc_setup, 0, sizeof(rrc_setup));
rrc_setup.rrc_TransactionIdentifier = 0;
rrc_setup.criticalExtensions.present = RRCSetup__criticalExtensions_PR_rrcSetup;

// PER encode
ssize_t encoded = per_encode_to_buffer(NULL, &asn_DEF_RRCSetup, &rrc_setup, buffer, buffer_size);
```

## Schema Coverage

### NGAP (TS 38.413)
- NG Setup (Request/Response/Failure)
- UE Context Management (Initial/Release/Modification)
- PDU Session Resource Management
- Handover Preparation/Execution
- Paging

### F1AP (TS 38.473)
- F1 Setup (Request/Response/Failure)
- UE Context Management
- RRC Message Transfer (DL/UL)
- DRB/SRB Configuration
- QoS Flow Management

### E1AP (TS 38.463)
- E1 Setup (Request/Response/Failure)
- Bearer Context Management
- PDU Session Resource Management
- DRB Configuration

### XnAP (TS 38.423)
- Xn Setup (Request/Response/Failure)
- Handover Preparation/Execution
- Paging
- UE Context Transfer

### NR-RRC (TS 38.331)
- RRC Setup/Reconfiguration/Release
- Security Mode Command
- Measurement Configuration/Report
- Handover Command
- UE Capability Enquiry

## Encoding Rules

All 3GPP protocols use **ASN.1 PER (Packed Encoding Rules)**:

- **Aligned PER**: Used for most AP messages
- **Unaligned PER**: Used for RRC messages

Key characteristics:
- No padding between fields (unaligned)
- Variable-length fields use length determinants
- CHOICE types use index encoding
- SEQUENCE uses bitmap for optional fields

## Procedure Code Reference

| Protocol | Setup | Reset | UE Context | Handover | Paging |
|----------|-------|-------|------------|----------|--------|
| NGAP | 21 | 22 | 25-28 | 34-38 | 45 |
| F1AP | 1 | 2 | 7-10 | - | - |
| E1AP | 1 | 2 | 7-10 | - | - |
| XnAP | 1 | 2 | 8-9 | 6-7 | 5 |

## SCTP PPID Values

| Protocol | PPID |
|----------|------|
| NGAP | 60 |
| F1AP | 61 |
| XnAP | 62 |
| E1AP | 63 |
| S1AP | 18 |

## Validation Tools Integration

### Build Integration

```makefile
# Add to Makefile for validation
ASN1_VALIDATE = asn1c -check NGAP-38413.asn

validate-asn1:
	$(ASN1_VALIDATE)
```

### CI/CD Integration

```yaml
# .github/workflows/validate.yml
- name: Validate ASN.1 Schemas
  run: |
    for file in asn1/*.asn; do
      echo "Validating $file"
      asn1c -check "$file" || exit 1
    done
```

## References

- [3GPP TS 38.331](https://www.3gpp.org/DynaReport/38331.htm) - NR RRC
- [3GPP TS 38.413](https://www.3gpp.org/DynaReport/38413.htm) - NGAP
- [3GPP TS 38.473](https://www.3gpp.org/DynaReport/38473.htm) - F1AP
- [3GPP TS 38.463](https://www.3gpp.org/DynaReport/38463.htm) - E1AP
- [3GPP TS 38.423](https://www.3gpp.org/DynaReport/38423.htm) - XnAP
- [ITU-T X.691](https://www.itu.int/rec/T-REC-X.691) - ASN.1 PER

## License

These schemas are derived from 3GPP specifications. Use subject to 3GPP terms and conditions.