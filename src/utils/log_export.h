/*
 * 5G UE Simulation Application
 * Log Schematic Export Module Header
 * 
 * Provides structured log export in JSON, Text, and CSV formats
 * for protocol message analysis and debugging.
 */

#ifndef UESIM_LOG_EXPORT_H
#define UESIM_LOG_EXPORT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

/* ============== Export Formats ============== */

typedef enum {
    LOG_EXPORT_FORMAT_JSON,     /* JSON format (structured) */
    LOG_EXPORT_FORMAT_TEXT,     /* Human-readable text */
    LOG_EXPORT_FORMAT_CSV,      /* Comma-separated values */
    LOG_EXPORT_FORMAT_HTML      /* HTML report with styling */
} log_export_format_t;

/* ============== Message Direction ============== */

typedef enum {
    LOG_MSG_DIR_UL,             /* Uplink */
    LOG_MSG_DIR_DL,             /* Downlink */
    LOG_MSG_DIR_UNKNOWN         /* Unknown direction */
} log_msg_direction_t;

/* ============== Protocol Types ============== */

typedef enum {
    LOG_PROTO_NGAP,
    LOG_PROTO_F1AP,
    LOG_PROTO_E1AP,
    LOG_PROTO_XNAP,
    LOG_PROTO_RRC,
    LOG_PROTO_NAS,
    LOG_PROTO_PFCP,
    LOG_PROTO_SCTP,
    LOG_PROTO_UNKNOWN
} log_protocol_t;

/* ============== Message Log Entry ============== */

typedef struct {
    /* Timestamp */
    struct timespec timestamp;
    uint64_t sequence_number;
    
    /* Message identification */
    log_protocol_t protocol;
    uint8_t procedure_code;
    uint16_t message_type;
    const char* message_name;
    
    /* Direction and endpoints */
    log_msg_direction_t direction;
    char source_addr[64];
    char dest_addr[64];
    uint16_t source_port;
    uint16_t dest_port;
    
    /* UE identification */
    uint64_t ran_ue_id;
    uint64_t amf_ue_id;
    uint32_t gnb_cu_ue_id;
    uint32_t gnb_du_ue_id;
    
    /* Message content */
    const uint8_t* payload;
    size_t payload_len;
    
    /* Decoded fields (optional) */
    const char* decoded_summary;
    
    /* Result */
    int result_code;
    const char* result_string;
} log_message_entry_t;

/* ============== Export Configuration ============== */

typedef struct {
    log_export_format_t format;
    char output_path[256];
    bool include_payload;           /* Include raw payload (hex) */
    bool include_decoded;           /* Include decoded fields */
    bool include_timestamps;        /* Include timestamps */
    bool include_endpoints;         /* Include source/dest addresses */
    bool include_ue_ids;            /* Include UE identifiers */
    bool pretty_print;              /* Pretty print (JSON/HTML) */
    size_t max_payload_display;     /* Max payload bytes to display (0 = all) */
    bool rotate_files;              /* Enable file rotation */
    size_t max_file_size;           /* Max file size before rotation (bytes) */
    int max_file_count;             /* Max number of rotated files */
} log_export_config_t;

/* ============== Export Statistics ============== */

typedef struct {
    uint64_t total_messages;
    uint64_t messages_by_protocol[LOG_PROTO_UNKNOWN + 1];
    uint64_t messages_ul;
    uint64_t messages_dl;
    uint64_t bytes_total;
    uint64_t errors;
    time_t start_time;
    time_t end_time;
} log_export_stats_t;

/* ============== Initialization & Cleanup ============== */

/**
 * Initialize log export module
 * @param config Export configuration
 * @return 0 on success, negative on error
 */
int log_export_init(const log_export_config_t* config);

/**
 * Cleanup log export module
 */
void log_export_cleanup(void);

/**
 * Get default export configuration
 * @param config Output configuration
 */
void log_export_get_default_config(log_export_config_t* config);

/* ============== Message Logging ============== */

/**
 * Log a protocol message
 * @param entry Message entry to log
 * @return 0 on success, negative on error
 */
int log_export_message(const log_message_entry_t* entry);

/**
 * Log a message with simplified parameters
 * @param protocol Protocol type
 * @param direction Message direction
 * @param payload Raw message payload
 * @param payload_len Payload length
 * @param fmt Format string for message description
 * @param ... Format arguments
 */
int log_export_message_simple(log_protocol_t protocol, log_msg_direction_t direction,
                               const uint8_t* payload, size_t payload_len,
                               const char* fmt, ...);

/* ============== File Operations ============== */

/**
 * Open export file
 * @param path File path (NULL to use config path)
 * @return 0 on success, negative on error
 */
int log_export_open(const char* path);

/**
 * Close export file
 */
void log_export_close(void);

/**
 * Flush buffered data to file
 */
void log_export_flush(void);

/**
 * Rotate log file if size exceeded
 * @return true if rotation occurred
 */
bool log_export_rotate(void);

/* ============== Export Functions ============== */

/**
 * Export all logged messages to file
 * @param format Export format
 * @param path Output file path
 * @return 0 on success, negative on error
 */
int log_export_to_file(log_export_format_t format, const char* path);

/**
 * Export message entry to buffer
 * @param entry Message entry
 * @param format Export format
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written, negative on error
 */
int log_export_entry_to_buffer(const log_message_entry_t* entry,
                                log_export_format_t format,
                                char* buffer, size_t buffer_size);

/**
 * Export statistics to file
 * @param path Output file path
 * @return 0 on success, negative on error
 */
int log_export_stats(const char* path);

/* ============== Buffer Management ============== */

/**
 * Set message buffer size
 * @param max_messages Maximum messages to buffer (0 = unlimited)
 */
void log_export_set_buffer_size(size_t max_messages);

/**
 * Clear message buffer
 */
void log_export_clear_buffer(void);

/**
 * Get current buffer statistics
 * @param stats Output statistics
 */
void log_export_get_stats(log_export_stats_t* stats);

/* ============== Utility Functions ============== */

/**
 * Convert protocol type to string
 * @param protocol Protocol type
 * @return Protocol name string
 */
const char* log_protocol_to_string(log_protocol_t protocol);

/**
 * Parse protocol type from string
 * @param str Protocol name
 * @return Protocol type
 */
log_protocol_t log_protocol_from_string(const char* str);

/**
 * Convert message direction to string
 * @param direction Message direction
 * @return Direction string ("UL", "DL", "UNKNOWN")
 */
const char* log_direction_to_string(log_msg_direction_t direction);

/**
 * Convert format to file extension
 * @param format Export format
 * @return File extension (e.g., ".json", ".csv")
 */
const char* log_format_to_extension(log_export_format_t format);

/**
 * Get current timestamp as ISO 8601 string
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Pointer to buffer
 */
char* log_export_timestamp_iso(char* buffer, size_t size);

/* ============== Convenience Macros ============== */

#define LOG_MSG_NGAP_UL(payload, len, fmt, ...) \
    log_export_message_simple(LOG_PROTO_NGAP, LOG_MSG_DIR_UL, payload, len, fmt, ##__VA_ARGS__)

#define LOG_MSG_NGAP_DL(payload, len, fmt, ...) \
    log_export_message_simple(LOG_PROTO_NGAP, LOG_MSG_DIR_DL, payload, len, fmt, ##__VA_ARGS__)

#define LOG_MSG_F1AP_UL(payload, len, fmt, ...) \
    log_export_message_simple(LOG_PROTO_F1AP, LOG_MSG_DIR_UL, payload, len, fmt, ##__VA_ARGS__)

#define LOG_MSG_F1AP_DL(payload, len, fmt, ...) \
    log_export_message_simple(LOG_PROTO_F1AP, LOG_MSG_DIR_DL, payload, len, fmt, ##__VA_ARGS__)

#define LOG_MSG_RRC_UL(payload, len, fmt, ...) \
    log_export_message_simple(LOG_PROTO_RRC, LOG_MSG_DIR_UL, payload, len, fmt, ##__VA_ARGS__)

#define LOG_MSG_RRC_DL(payload, len, fmt, ...) \
    log_export_message_simple(LOG_PROTO_RRC, LOG_MSG_DIR_DL, payload, len, fmt, ##__VA_ARGS__)

#define LOG_MSG_NAS_UL(payload, len, fmt, ...) \
    log_export_message_simple(LOG_PROTO_NAS, LOG_MSG_DIR_UL, payload, len, fmt, ##__VA_ARGS__)

#define LOG_MSG_NAS_DL(payload, len, fmt, ...) \
    log_export_message_simple(LOG_PROTO_NAS, LOG_MSG_DIR_DL, payload, len, fmt, ##__VA_ARGS__)

#endif /* UESIM_LOG_EXPORT_H */