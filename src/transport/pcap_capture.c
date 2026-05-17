/*
 * 5G UE Simulation Application
 * PCAP Capture Module Implementation
 * tcpdump-compatible PCAP/PCAPNG format
 */

#include "pcap_capture.h"
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#endif

/* ============== Utility Functions ============== */

uint64_t pcap_get_timestamp_ns(void) {
#ifdef _WIN32
    FILETIME ft;
    ULARGE_INTEGER uli;
    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    /* Convert from Windows epoch (1601) to Unix epoch (1970) */
    return (uli.QuadPart - 116444736000000000ULL) * 100;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

void pcap_format_timestamp(uint64_t ns, uint32_t* sec, uint32_t* usec) {
    *sec = (uint32_t)(ns / 1000000000ULL);
    *usec = (uint32_t)((ns % 1000000000ULL) / 1000ULL);
}

uint16_t pcap_calc_checksum(const uint8_t* data, size_t len) {
    uint32_t sum = 0;
    while (len > 1) {
        sum += ((uint16_t)*data << 8) | *(data + 1);
        data += 2;
        len -= 2;
    }
    if (len > 0) {
        sum += (uint16_t)*data << 8;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)~sum;
}

/* CRC32c lookup table for SCTP checksum (RFC 4960) */
static uint32_t crc32c_table[256];
static int crc32c_table_initialized = 0;

static void init_crc32c_table(void) {
    if (crc32c_table_initialized) return;
    
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0x82F63B78; /* CRC32c polynomial */
            } else {
                crc >>= 1;
            }
        }
        crc32c_table[i] = crc;
    }
    crc32c_table_initialized = 1;
}

static uint32_t calc_crc32c(const uint8_t* data, size_t len) {
    init_crc32c_table();
    
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crc32c_table[(crc ^ data[i]) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

uint16_t pcap_calc_ip_checksum(const uint8_t* ip_header, size_t len) {
    return pcap_calc_checksum(ip_header, len);
}

void pcap_format_ip(char* buf, size_t buf_size, uint32_t ip) {
    snprintf(buf, buf_size, "%u.%u.%u.%u",
             (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
             (ip >> 8) & 0xFF, ip & 0xFF);
}

void pcap_format_mac(char* buf, size_t buf_size, const uint8_t mac[6]) {
    snprintf(buf, buf_size, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

uint16_t pcap_get_link_type(const char* protocol) {
    if (strcmp(protocol, "ethernet") == 0) return PCAP_LINKTYPE_ETHERNET;
    if (strcmp(protocol, "raw") == 0) return PCAP_LINKTYPE_RAW;
    if (strcmp(protocol, "sctp") == 0) return PCAP_LINKTYPE_SCTP;
    return PCAP_LINKTYPE_RAW;
}

/* ============== PCAP Capture Lifecycle ============== */

pcap_capture_t* pcap_capture_create(const char* filename, bool use_pcapng) {
    pcap_capture_t* pcap = (pcap_capture_t*)uesim_calloc(1, sizeof(pcap_capture_t));
    if (!pcap) return NULL;
    
    strncpy(pcap->filename, filename, sizeof(pcap->filename) - 1);
    pcap->use_pcapng = use_pcapng;
    pcap->snap_len = PCAP_DEFAULT_SNAPLEN;
    pcap->include_ethernet_header = false;
    pcap->include_ip_header = true;
    pcap->is_open = false;
    pcap->num_interfaces = 0;
    pcap->packets_captured = 0;
    pcap->bytes_captured = 0;
    pcap->ngap_packets = 0;
    pcap->gtpu_packets = 0;
    pcap->start_time_ns = pcap_get_timestamp_ns();
    
    pcap->buffer_size = PCAP_MAX_SNAPLEN;
    pcap->packet_buffer = (uint8_t*)uesim_malloc(pcap->buffer_size);
    if (!pcap->packet_buffer) {
        uesim_free(pcap);
        return NULL;
    }
    
    return pcap;
}

void pcap_capture_destroy(pcap_capture_t* pcap) {
    if (!pcap) return;
    
    pcap_capture_close(pcap);
    
    if (pcap->packet_buffer) {
        uesim_free(pcap->packet_buffer);
    }
    
    uesim_free(pcap);
}

uesim_error_t pcap_capture_open(pcap_capture_t* pcap) {
    if (!pcap || pcap->is_open) return UESIM_ERROR_INVALID_PARAM;
    
    pcap->file = fopen(pcap->filename, "wb");
    if (!pcap->file) {
        return UESIM_ERROR_FILE;
    }
    
    if (pcap->use_pcapng) {
        /* Write PCAPNG Section Header */
        pcapng_section_header_t shb;
        memset(&shb, 0, sizeof(shb));
        shb.block_type = PCAPNG_SECTION_HEADER;
        shb.block_total_length = 28;
        shb.byte_order_magic = PCAPNG_MAGIC_NUMBER;
        shb.major_version = 1;
        shb.minor_version = 0;
        shb.section_length = 0xFFFFFFFFFFFFFFFFULL;
        
        fwrite(&shb, sizeof(shb), 1, pcap->file);
        
        /* Write default interface if none added */
        if (pcap->num_interfaces == 0) {
            pcap_capture_add_interface(pcap, "any", "Default interface", PCAP_LINKTYPE_RAW);
        }
    } else {
        /* Write PCAP file header */
        pcap_file_header_t header;
        memset(&header, 0, sizeof(header));
        header.magic_number = PCAP_MAGIC_NUMBER;
        header.version_major = PCAP_VERSION_MAJOR;
        header.version_minor = PCAP_VERSION_MINOR;
        header.thiszone = 0;
        header.sigfigs = 0;
        header.snaplen = pcap->snap_len;
        header.network = PCAP_LINKTYPE_RAW;
        
        fwrite(&header, sizeof(header), 1, pcap->file);
    }
    
    pcap->is_open = true;
    return UESIM_SUCCESS;
}

void pcap_capture_close(pcap_capture_t* pcap) {
    if (!pcap) return;
    
    if (pcap->file) {
        fflush(pcap->file);
        fclose(pcap->file);
        pcap->file = NULL;
    }
    
    pcap->is_open = false;
}

uesim_error_t pcap_capture_add_interface(pcap_capture_t* pcap, const char* name,
                                          const char* description, uint16_t link_type) {
    if (!pcap || pcap->num_interfaces >= 16) return UESIM_ERROR_CAPACITY;
    
    pcap_interface_t* iface = &pcap->interfaces[pcap->num_interfaces];
    strncpy(iface->name, name, sizeof(iface->name) - 1);
    strncpy(iface->description, description, sizeof(iface->description) - 1);
    iface->link_type = link_type;
    iface->snap_len = pcap->snap_len;
    iface->interface_id = pcap->num_interfaces;
    
    if (pcap->use_pcapng && pcap->is_open) {
        /* Write Interface Description Block */
        pcapng_interface_desc_t idb;
        memset(&idb, 0, sizeof(idb));
        idb.block_type = PCAPNG_INTERFACE_DESC;
        idb.block_total_length = 16;
        idb.link_type = link_type;
        idb.reserved = 0;
        idb.snap_len = pcap->snap_len;
        
        fwrite(&idb, sizeof(idb), 1, pcap->file);
    }
    
    pcap->num_interfaces++;
    return UESIM_SUCCESS;
}

/* ============== Raw Packet Capture ============== */

uesim_error_t pcap_capture_raw_packet(pcap_capture_t* pcap, const uint8_t* data,
                                       size_t len, uint32_t interface_id) {
    if (!pcap || !data || !pcap->is_open) return UESIM_ERROR_INVALID_PARAM;
    
    uint64_t ts_ns = pcap_get_timestamp_ns();
    uint32_t ts_sec, ts_usec;
    pcap_format_timestamp(ts_ns, &ts_sec, &ts_usec);
    
    size_t capture_len = (len > pcap->snap_len) ? pcap->snap_len : len;
    
    if (pcap->use_pcapng) {
        /* PCAPNG Enhanced Packet Block */
        pcapng_enhanced_packet_t epb;
        memset(&epb, 0, sizeof(epb));
        epb.block_type = PCAPNG_ENHANCED_PACKET;
        epb.block_total_length = (uint32_t)(32 + ((capture_len + 3) & ~3) + 4);
        epb.interface_id = interface_id;
        epb.timestamp_high = (uint32_t)(ts_ns >> 32);
        epb.timestamp_low = (uint32_t)(ts_ns & 0xFFFFFFFF);
        epb.captured_len = (uint32_t)capture_len;
        epb.original_len = (uint32_t)len;
        
        fwrite(&epb, sizeof(epb), 1, pcap->file);
        fwrite(data, 1, capture_len, pcap->file);
        
        /* Padding to 4-byte boundary */
        size_t padding = (4 - (capture_len % 4)) % 4;
        if (padding > 0) {
            uint8_t pad[4] = {0};
            fwrite(pad, 1, padding, pcap->file);
        }
        
        /* Block total length (trailer) */
        uint32_t trailer = epb.block_total_length;
        fwrite(&trailer, sizeof(trailer), 1, pcap->file);
    } else {
        /* Standard PCAP packet header */
        pcap_packet_header_t header;
        memset(&header, 0, sizeof(header));
        header.ts_sec = ts_sec;
        header.ts_usec = ts_usec;
        header.incl_len = (uint32_t)capture_len;
        header.orig_len = (uint32_t)len;
        
        fwrite(&header, sizeof(header), 1, pcap->file);
        fwrite(data, 1, capture_len, pcap->file);
    }
    
    pcap->packets_captured++;
    pcap->bytes_captured += len;
    
    return UESIM_SUCCESS;
}

/* ============== IP Packet Capture ============== */

uesim_error_t pcap_capture_ip(pcap_capture_t* pcap, const uint8_t* payload,
                               size_t len, uint8_t protocol,
                               uint32_t src_ip, uint32_t dst_ip,
                               uint16_t src_port, uint16_t dst_port,
                               uint32_t interface_id) {
    if (!pcap || !payload || !pcap->is_open) return UESIM_ERROR_INVALID_PARAM;
    
    uint8_t* buf = pcap->packet_buffer;
    size_t total_len = 20 + len; /* IP header only (no options) */
    
    /* IPv4 header */
    buf[0] = 0x45; /* Version 4, IHL 5 (20 bytes) */
    buf[1] = 0x00; /* TOS */
    buf[2] = (total_len >> 8) & 0xFF;
    buf[3] = total_len & 0xFF;
    buf[4] = 0x00; /* Identification */
    buf[5] = 0x00;
    buf[6] = 0x40; /* Flags (Don't Fragment) */
    buf[7] = 0x00; /* Fragment offset */
    buf[8] = 64;   /* TTL */
    buf[9] = protocol; /* Protocol */
    buf[10] = 0x00; /* Checksum (calculated below) */
    buf[11] = 0x00;
    buf[12] = (src_ip >> 24) & 0xFF;
    buf[13] = (src_ip >> 16) & 0xFF;
    buf[14] = (src_ip >> 8) & 0xFF;
    buf[15] = src_ip & 0xFF;
    buf[16] = (dst_ip >> 24) & 0xFF;
    buf[17] = (dst_ip >> 16) & 0xFF;
    buf[18] = (dst_ip >> 8) & 0xFF;
    buf[19] = dst_ip & 0xFF;
    
    /* Calculate IP checksum */
    uint16_t checksum = pcap_calc_ip_checksum(buf, 20);
    buf[10] = (checksum >> 8) & 0xFF;
    buf[11] = checksum & 0xFF;
    
    /* Copy payload */
    memcpy(buf + 20, payload, len);
    
    return pcap_capture_raw_packet(pcap, buf, total_len, interface_id);
}

/* ============== SCTP Packet Capture (for NGAP) ============== */

uesim_error_t pcap_capture_sctp(pcap_capture_t* pcap, const uint8_t* payload,
                                 size_t len, uint32_t src_ip, uint32_t dst_ip,
                                 uint16_t src_port, uint16_t dst_port,
                                 uint32_t ppid, uint32_t stream_id,
                                 uint32_t ssn, uint32_t tsn) {
    if (!pcap || !payload || !pcap->is_open) return UESIM_ERROR_INVALID_PARAM;
    
    /* Build SCTP common header + DATA chunk */
    uint8_t* buf = pcap->packet_buffer;
    size_t sctp_header_len = 12; /* Common header */
    size_t data_chunk_len = 16;  /* DATA chunk header */
    size_t total_sctp_len = sctp_header_len + data_chunk_len + len;
    
    /* Align to 4 bytes */
    size_t padded_len = (total_sctp_len + 3) & ~3;
    
    /* SCTP common header */
    buf[0] = (src_port >> 8) & 0xFF;
    buf[1] = src_port & 0xFF;
    buf[2] = (dst_port >> 8) & 0xFF;
    buf[3] = dst_port & 0xFF;
    buf[4] = 0x00; /* Verification tag (0 for INIT) */
    buf[5] = 0x00;
    buf[6] = 0x00;
    buf[7] = 0x00;
    /* Checksum placeholder - will be filled after payload */
    buf[8] = 0x00;
    buf[9] = 0x00;
    buf[10] = 0x00;
    buf[11] = 0x00;
    
    /* DATA chunk header */
    buf[12] = 0x00; /* Chunk type: DATA */
    buf[13] = 0x00; /* Flags */
    buf[14] = ((data_chunk_len + len + 3) & ~3) >> 8; /* Length */
    buf[15] = ((data_chunk_len + len + 3) & ~3) & 0xFF;
    buf[16] = (tsn >> 24) & 0xFF;
    buf[17] = (tsn >> 16) & 0xFF;
    buf[18] = (tsn >> 8) & 0xFF;
    buf[19] = tsn & 0xFF;
    buf[20] = (stream_id >> 8) & 0xFF;
    buf[21] = stream_id & 0xFF;
    buf[22] = (ssn >> 8) & 0xFF;
    buf[23] = ssn & 0xFF;
    buf[24] = (ppid >> 24) & 0xFF;
    buf[25] = (ppid >> 16) & 0xFF;
    buf[26] = (ppid >> 8) & 0xFF;
    buf[27] = ppid & 0xFF;
    
    /* Copy payload */
    memcpy(buf + 28, payload, len);
    
    /* Pad to 4-byte boundary */
    size_t padding = (4 - ((data_chunk_len + len) % 4)) % 4;
    if (padding > 0) {
        memset(buf + 28 + len, 0, padding);
    }
    
    /* Calculate and insert CRC32c checksum (RFC 4960) */
    uint32_t crc = calc_crc32c(buf, padded_len);
    buf[8] = (crc >> 24) & 0xFF;
    buf[9] = (crc >> 16) & 0xFF;
    buf[10] = (crc >> 8) & 0xFF;
    buf[11] = crc & 0xFF;
    
    /* Capture as IP packet with SCTP protocol (132) */
    pcap->ngap_packets++;
    return pcap_capture_ip(pcap, buf, padded_len, 132, src_ip, dst_ip,
                            src_port, dst_port, 0);
}

/* ============== UDP Packet Capture (for GTP-U) ============== */

uesim_error_t pcap_capture_udp(pcap_capture_t* pcap, const uint8_t* payload,
                                size_t len, uint32_t src_ip, uint32_t dst_ip,
                                uint16_t src_port, uint16_t dst_port,
                                uint32_t interface_id) {
    if (!pcap || !payload || !pcap->is_open) return UESIM_ERROR_INVALID_PARAM;
    
    uint8_t* buf = pcap->packet_buffer;
    size_t udp_len = 8 + len;
    
    /* UDP header */
    buf[0] = (src_port >> 8) & 0xFF;
    buf[1] = src_port & 0xFF;
    buf[2] = (dst_port >> 8) & 0xFF;
    buf[3] = dst_port & 0xFF;
    buf[4] = (udp_len >> 8) & 0xFF;
    buf[5] = udp_len & 0xFF;
    buf[6] = 0x00; /* Checksum (optional for IPv4) */
    buf[7] = 0x00;
    
    /* Copy payload */
    memcpy(buf + 8, payload, len);
    
    return pcap_capture_ip(pcap, buf, udp_len, 17, src_ip, dst_ip,
                            src_port, dst_port, interface_id);
}

/* ============== GTP-U Packet Capture ============== */

uesim_error_t pcap_capture_gtpu(pcap_capture_t* pcap, const uint8_t* payload,
                                 size_t len, uint32_t src_ip, uint32_t dst_ip,
                                 uint32_t teid, uint8_t message_type) {
    if (!pcap || !payload || !pcap->is_open) return UESIM_ERROR_INVALID_PARAM;
    
    uint8_t* buf = pcap->packet_buffer;
    size_t gtp_len = 8 + len; /* GTP-U header (minimum) */
    
    /* GTP-U header */
    buf[0] = 0x30; /* Version 1, Protocol Type 1 */
    if (message_type == 255) {
        /* G-PDU (user data) - include TEID */
        buf[0] |= 0x10; /* E bit set for optional fields if needed */
    }
    buf[1] = message_type; /* Message Type */
    buf[2] = (len >> 8) & 0xFF; /* Length */
    buf[3] = len & 0xFF;
    buf[4] = (teid >> 24) & 0xFF; /* TEID */
    buf[5] = (teid >> 16) & 0xFF;
    buf[6] = (teid >> 8) & 0xFF;
    buf[7] = teid & 0xFF;
    
    /* Copy payload */
    memcpy(buf + 8, payload, len);
    
    pcap->gtpu_packets++;
    return pcap_capture_udp(pcap, buf, gtp_len, src_ip, dst_ip,
                             PCAP_PORT_GTPU, PCAP_PORT_GTPU, 0);
}

/* ============== NGAP Packet Capture ============== */

uesim_error_t pcap_capture_ngap(pcap_capture_t* pcap, const uint8_t* payload,
                                 size_t len, uint32_t src_ip, uint32_t dst_ip,
                                 bool is_uplink) {
    /* NGAP uses SCTP with PPID = 60 (NGAP) */
    uint16_t src_port = is_uplink ? PCAP_PORT_NGAP : PCAP_PORT_NGAP;
    uint16_t dst_port = PCAP_PORT_NGAP;
    
    static uint32_t tsn_counter = 0;
    tsn_counter++;
    
    pcap->ngap_packets++;
    return pcap_capture_sctp(pcap, payload, len, src_ip, dst_ip,
                              src_port, dst_port, 60, /* PPID for NGAP */
                              0,  /* Stream ID */
                              0,  /* SSN */
                              tsn_counter);
}

/* ============== Wireshark Helpers ============== */

void pcap_set_wireshark_hint(wireshark_hint_t* hint, const char* protocol,
                              const char* src, const char* dst,
                              uint32_t src_port, uint32_t dst_port,
                              const char* info) {
    if (!hint) return;
    
    strncpy(hint->protocol, protocol, sizeof(hint->protocol) - 1);
    strncpy(hint->src_addr, src, sizeof(hint->src_addr) - 1);
    strncpy(hint->dst_addr, dst, sizeof(hint->dst_addr) - 1);
    hint->src_port = src_port;
    hint->dst_port = dst_port;
    strncpy(hint->info, info, sizeof(hint->info) - 1);
}

/* ============== tcpdump Integration ============== */

void tcpdump_generate_command(char* cmd, size_t cmd_size,
                               const char* output_file,
                               const char* net_interface,
                               bool capture_ngap,
                               bool capture_gtpu,
                               bool capture_xnap) {
    if (!cmd || cmd_size == 0) return;
    
    char filter[256] = "";
    bool first = true;
    
    if (capture_ngap) {
        strcat(filter, "port 38412");
        first = false;
    }
    
    if (capture_gtpu) {
        if (!first) strcat(filter, " or ");
        strcat(filter, "port 2152");
        first = false;
    }
    
    if (capture_xnap) {
        if (!first) strcat(filter, " or ");
        strcat(filter, "port 38422");
    }
    
    snprintf(cmd, cmd_size,
             "tcpdump -i %s -w %s '%s' -s 0",
             net_interface ? net_interface : "any",
             output_file,
             filter);
}

tcpdump_context_t* tcpdump_start(const char* output_file, const char* filter,
                                  const char* net_interface) {
    tcpdump_context_t* ctx = (tcpdump_context_t*)uesim_calloc(1, sizeof(tcpdump_context_t));
    if (!ctx) return NULL;
    
    strncpy(ctx->output_file, output_file, sizeof(ctx->output_file) - 1);
    strncpy(ctx->filter, filter ? filter : "", sizeof(ctx->filter) - 1);
    strncpy(ctx->net_interface, net_interface ? net_interface : "any", sizeof(ctx->net_interface) - 1);
    
    /* Build tcpdump command */
    snprintf(ctx->tcpdump_command, sizeof(ctx->tcpdump_command),
             "tcpdump -i %s -w %s '%s' -s 0",
             ctx->net_interface, ctx->output_file, ctx->filter);
    
#ifdef _WIN32
    /* Windows: Use CreateProcess */
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));
    
    if (!CreateProcessA(NULL, ctx->tcpdump_command, NULL, NULL, FALSE,
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        uesim_free(ctx);
        return NULL;
    }
    
    ctx->process = pi.hProcess;
    ctx->is_running = true;
#else
    /* Linux/Unix: Use fork/exec */
    ctx->pid = fork();
    if (ctx->pid < 0) {
        uesim_free(ctx);
        return NULL;
    }
    
    if (ctx->pid == 0) {
        /* Child process */
        execlp("tcpdump", "tcpdump", "-i", ctx->net_interface, "-w", 
               ctx->output_file, ctx->filter, "-s", "0", NULL);
        exit(1);
    }
    
    ctx->is_running = true;
#endif
    
    return ctx;
}

void tcpdump_stop(tcpdump_context_t* ctx) {
    if (!ctx || !ctx->is_running) return;
    
#ifdef _WIN32
    /* Windows: Terminate process */
    TerminateProcess(ctx->process, 0);
    CloseHandle(ctx->process);
#else
    /* Linux/Unix: Send SIGTERM */
    kill(ctx->pid, SIGTERM);
    waitpid(ctx->pid, NULL, 0);
#endif
    
    ctx->is_running = false;
    uesim_free(ctx);
}