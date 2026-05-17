/*
 * 5G UE Simulation Application
 * PCAP Capture Module
 * tcpdump-compatible PCAP/PCAPNG format
 */

#ifndef PCAP_CAPTURE_H
#define PCAP_CAPTURE_H

#include "../uesim.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

/* ============== PCAP Constants ============== */

#define PCAP_MAGIC_NUMBER       0xA1B2C3D4
#define PCAP_MAGIC_SWAPPED      0xD4C3B2A1
#define PCAPNG_MAGIC_NUMBER     0x0A0D0D0A

#define PCAP_VERSION_MAJOR      2
#define PCAP_VERSION_MINOR      4

#define PCAP_MAX_SNAPLEN        65535
#define PCAP_DEFAULT_SNAPLEN    65535

/* Link Types */
#define PCAP_LINKTYPE_ETHERNET  1
#define PCAP_LINKTYPE_RAW       101
#define PCAP_LINKTYPE_SCTP      248

/* Protocol ports */
#define PCAP_PORT_NGAP          38412
#define PCAP_PORT_GTPU          2152
#define PCAP_PORT_XNAP          38422

/* ============== PCAP File Header ============== */

typedef struct __attribute__((packed)) {
    uint32_t magic_number;
    uint16_t version_major;
    uint16_t version_minor;
    int32_t thiszone;       /* GMT to local correction */
    uint32_t sigfigs;       /* Accuracy of timestamps */
    uint32_t snaplen;       /* Max length of captured packets */
    uint32_t network;       /* Data link type */
} pcap_file_header_t;

/* ============== PCAP Packet Header ============== */

typedef struct __attribute__((packed)) {
    uint32_t ts_sec;        /* Timestamp seconds */
    uint32_t ts_usec;       /* Timestamp microseconds */
    uint32_t incl_len;      /* Number of octets of packet saved */
    uint32_t orig_len;      /* Actual length of packet */
} pcap_packet_header_t;

/* ============== PCAPNG Block Types ============== */

#define PCAPNG_SECTION_HEADER   0x0A0D0D0A
#define PCAPNG_INTERFACE_DESC   0x00000001
#define PCAPNG_ENHANCED_PACKET  0x00000006
#define PCAPNG_INTERFACE_STATS  0x00000005

/* ============== PCAPNG Section Header Block ============== */

typedef struct __attribute__((packed)) {
    uint32_t block_type;
    uint32_t block_total_length;
    uint32_t byte_order_magic;
    uint16_t major_version;
    uint16_t minor_version;
    uint64_t section_length;
} pcapng_section_header_t;

/* ============== PCAPNG Interface Description Block ============== */

typedef struct __attribute__((packed)) {
    uint32_t block_type;
    uint32_t block_total_length;
    uint16_t link_type;
    uint16_t reserved;
    uint32_t snap_len;
} pcapng_interface_desc_t;

/* ============== PCAPNG Enhanced Packet Block ============== */

typedef struct __attribute__((packed)) {
    uint32_t block_type;
    uint32_t block_total_length;
    uint32_t interface_id;
    uint32_t timestamp_high;
    uint32_t timestamp_low;
    uint32_t captured_len;
    uint32_t original_len;
} pcapng_enhanced_packet_t;

/* ============== Capture Interface Configuration ============== */

typedef struct {
    char name[64];
    char description[128];
    uint16_t link_type;
    uint32_t snap_len;
    uint32_t interface_id;
} pcap_interface_t;

/* ============== PCAP Capture Context ============== */

typedef struct {
    /* File handle */
    FILE* file;
    char filename[256];
    bool is_open;
    bool use_pcapng;
    
    /* Configuration */
    uint32_t snap_len;
    bool include_ethernet_header;
    bool include_ip_header;
    
    /* Interfaces (for PCAPNG) */
    pcap_interface_t interfaces[16];
    uint8_t num_interfaces;
    
    /* Statistics */
    uint64_t packets_captured;
    uint64_t bytes_captured;
    uint64_t ngap_packets;
    uint64_t gtpu_packets;
    uint64_t xn_packets;
    uint64_t nas_packets;
    uint64_t rrc_packets;
    
    /* Timestamp */
    uint64_t start_time_ns;
    
    /* Buffer */
    uint8_t* packet_buffer;
    size_t buffer_size;
    
} pcap_capture_t;

/* ============== PCAP Capture API Functions ============== */

pcap_capture_t* pcap_capture_create(const char* filename, bool use_pcapng);
void pcap_capture_destroy(pcap_capture_t* pcap);

uesim_error_t pcap_capture_open(pcap_capture_t* pcap);
void pcap_capture_close(pcap_capture_t* pcap);

/* Add interface for PCAPNG multi-interface capture */
uesim_error_t pcap_capture_add_interface(pcap_capture_t* pcap, const char* name,
                                          const char* description, uint16_t link_type);

/* ============== Packet Capture Functions ============== */

/* Raw packet capture */
uesim_error_t pcap_capture_raw_packet(pcap_capture_t* pcap, const uint8_t* data,
                                       size_t len, uint32_t interface_id);

/* Ethernet frame capture */
uesim_error_t pcap_capture_ethernet(pcap_capture_t* pcap, const uint8_t* payload,
                                     size_t len, uint16_t ethertype,
                                     const uint8_t src_mac[6], const uint8_t dst_mac[6],
                                     uint32_t interface_id);

/* IP packet capture */
uesim_error_t pcap_capture_ip(pcap_capture_t* pcap, const uint8_t* payload,
                               size_t len, uint8_t protocol,
                               uint32_t src_ip, uint32_t dst_ip,
                               uint16_t src_port, uint16_t dst_port,
                               uint32_t interface_id);

/* SCTP packet capture (for NGAP) */
uesim_error_t pcap_capture_sctp(pcap_capture_t* pcap, const uint8_t* payload,
                                 size_t len, uint32_t src_ip, uint32_t dst_ip,
                                 uint16_t src_port, uint16_t dst_port,
                                 uint32_t ppid, uint32_t stream_id,
                                 uint32_t ssn, uint32_t tsn);

/* UDP packet capture (for GTP-U) */
uesim_error_t pcap_capture_udp(pcap_capture_t* pcap, const uint8_t* payload,
                                size_t len, uint32_t src_ip, uint32_t dst_ip,
                                uint16_t src_port, uint16_t dst_port,
                                uint32_t interface_id);

/* GTP-U packet capture */
uesim_error_t pcap_capture_gtpu(pcap_capture_t* pcap, const uint8_t* payload,
                                 size_t len, uint32_t src_ip, uint32_t dst_ip,
                                 uint32_t teid, uint8_t message_type);

/* NGAP packet capture */
uesim_error_t pcap_capture_ngap(pcap_capture_t* pcap, const uint8_t* payload,
                                 size_t len, uint32_t src_ip, uint32_t dst_ip,
                                 bool is_uplink);

/* ============== tcpdump Integration ============== */

typedef struct {
    char tcpdump_command[512];
    char output_file[256];
    char filter[256];
    char net_interface[64];
    bool is_running;
#ifdef _WIN32
    HANDLE process;
#else
    pid_t pid;
#endif
} tcpdump_context_t;

tcpdump_context_t* tcpdump_start(const char* output_file, const char* filter,
                                  const char* net_interface);
void tcpdump_stop(tcpdump_context_t* ctx);

/* Generate tcpdump command for 5G capture */
void tcpdump_generate_command(char* cmd, size_t cmd_size,
                               const char* output_file,
                               const char* net_interface,
                               bool capture_ngap,
                               bool capture_gtpu,
                               bool capture_xnap);

/* ============== Wireshark Dissector Helpers ============== */

/* Create Wireshark-compatible packet hints */
typedef struct {
    char protocol[32];      /* Protocol name: ngap, gtp, nas-5gs, rrc.nr */
    char src_addr[64];      /* Source address */
    char dst_addr[64];      /* Destination address */
    uint32_t src_port;
    uint32_t dst_port;
    char info[256];         /* Packet info string */
} wireshark_hint_t;

void pcap_set_wireshark_hint(wireshark_hint_t* hint, const char* protocol,
                              const char* src, const char* dst,
                              uint32_t src_port, uint32_t dst_port,
                              const char* info);

/* ============== Utility Functions ============== */

uint64_t pcap_get_timestamp_ns(void);
void pcap_format_timestamp(uint64_t ns, uint32_t* sec, uint32_t* usec);

/* Get link type for protocol */
uint16_t pcap_get_link_type(const char* protocol);

/* Calculate checksums */
uint16_t pcap_calc_checksum(const uint8_t* data, size_t len);
uint16_t pcap_calc_ip_checksum(const uint8_t* ip_header, size_t len);

/* Format IP address */
void pcap_format_ip(char* buf, size_t buf_size, uint32_t ip);
void pcap_format_mac(char* buf, size_t buf_size, const uint8_t mac[6]);

#endif /* PCAP_CAPTURE_H */