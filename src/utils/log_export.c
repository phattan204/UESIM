/*
 * 5G UE Simulation Application
 * Log Schematic Export Module Implementation
 * 
 * Provides structured log export in JSON, Text, and CSV formats
 */

#include "log_export.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#define PATH_SEP '\\'

/* Windows compatibility */
#ifndef strcasecmp
#define strcasecmp _stricmp
#endif
#ifndef strdup
#define strdup _strdup
#endif

/* clock_gettime replacement for Windows */
#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
static int clock_gettime(int clk_id, struct timespec *tp) {
    LARGE_INTEGER frequency, counter;
    (void)clk_id;
    
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    
    tp->tv_sec = (long)(counter.QuadPart / frequency.QuadPart);
    tp->tv_nsec = (long)((counter.QuadPart % frequency.QuadPart) * 1000000000LL / frequency.QuadPart);
    return 0;
}
#endif

#else
#include <sys/stat.h>
#include <dirent.h>
#include <strings.h>
#define PATH_SEP '/'
#endif

/* ============== Constants ============== */

#define MAX_MESSAGE_BUFFER  10000
#define MAX_LINE_SIZE       65536
#define MAX_PAYLOAD_DISPLAY 256

/* ============== Module State ============== */

static struct {
    bool initialized;
    log_export_config_t config;
    FILE* output_file;
    uint64_t sequence_number;
    
    /* Message buffer */
    log_message_entry_t* message_buffer;
    size_t buffer_count;
    size_t buffer_capacity;
    
    /* Statistics */
    log_export_stats_t stats;
    
    /* Current file size */
    size_t current_file_size;
} g_export = {0};

/* ============== Protocol Names ============== */

static const char* g_protocol_names[] = {
    "NGAP", "F1AP", "E1AP", "XnAP", "RRC", "NAS", "PFCP", "SCTP", "UNKNOWN"
};

/* ============== Utility Functions ============== */

const char* log_protocol_to_string(log_protocol_t protocol) {
    if (protocol < 0 || protocol > LOG_PROTO_UNKNOWN) {
        return "UNKNOWN";
    }
    return g_protocol_names[protocol];
}

log_protocol_t log_protocol_from_string(const char* str) {
    if (!str) return LOG_PROTO_UNKNOWN;
    
    for (int i = 0; i <= LOG_PROTO_UNKNOWN; i++) {
        if (strcasecmp(str, g_protocol_names[i]) == 0) {
            return (log_protocol_t)i;
        }
    }
    return LOG_PROTO_UNKNOWN;
}

const char* log_direction_to_string(log_msg_direction_t direction) {
    switch (direction) {
        case LOG_MSG_DIR_UL: return "UL";
        case LOG_MSG_DIR_DL: return "DL";
        default: return "UNKNOWN";
    }
}

const char* log_format_to_extension(log_export_format_t format) {
    switch (format) {
        case LOG_EXPORT_FORMAT_JSON: return ".json";
        case LOG_EXPORT_FORMAT_TEXT: return ".log";
        case LOG_EXPORT_FORMAT_CSV:  return ".csv";
        case LOG_EXPORT_FORMAT_HTML: return ".html";
        default: return ".log";
    }
}

char* log_export_timestamp_iso(char* buffer, size_t size) {
    if (!buffer || size == 0) return NULL;
    
    time_t now = time(NULL);
    struct tm* tm_info = gmtime(&now);
    
    snprintf(buffer, size, "%04d-%02d-%02dT%02d:%02d:%02dZ",
             tm_info->tm_year + 1900,
             tm_info->tm_mon + 1,
             tm_info->tm_mday,
             tm_info->tm_hour,
             tm_info->tm_min,
             tm_info->tm_sec);
    
    return buffer;
}

/* ============== Hex Encoding ============== */

static void hex_encode(const uint8_t* data, size_t len, char* buffer, size_t buffer_size) {
    static const char hex_chars[] = "0123456789ABCDEF";
    size_t i;
    size_t write_len = (len * 2 < buffer_size - 1) ? len : (buffer_size - 1) / 2;
    
    for (i = 0; i < write_len; i++) {
        buffer[i * 2] = hex_chars[(data[i] >> 4) & 0x0F];
        buffer[i * 2 + 1] = hex_chars[data[i] & 0x0F];
    }
    buffer[write_len * 2] = '\0';
}

/* ============== JSON Escape ============== */

static void json_escape_string(const char* input, char* output, size_t output_size) {
    size_t i = 0, j = 0;
    
    while (input[i] && j < output_size - 2) {
        char c = input[i++];
        
        switch (c) {
            case '"':  output[j++] = '\\'; output[j++] = '"'; break;
            case '\\': output[j++] = '\\'; output[j++] = '\\'; break;
            case '\b': output[j++] = '\\'; output[j++] = 'b'; break;
            case '\f': output[j++] = '\\'; output[j++] = 'f'; break;
            case '\n': output[j++] = '\\'; output[j++] = 'n'; break;
            case '\r': output[j++] = '\\'; output[j++] = 'r'; break;
            case '\t': output[j++] = '\\'; output[j++] = 't'; break;
            default:
                if ((unsigned char)c >= 0x20) {
                    output[j++] = c;
                }
                break;
        }
    }
    output[j] = '\0';
}

/* ============== CSV Escape ============== */

static void csv_escape_field(const char* input, char* output, size_t output_size) {
    bool needs_quote = false;
    size_t i = 0, j = 0;
    
    /* Check if quoting needed */
    for (i = 0; input[i]; i++) {
        if (input[i] == ',' || input[i] == '"' || input[i] == '\n' || input[i] == '\r') {
            needs_quote = true;
            break;
        }
    }
    
    if (!needs_quote) {
        strncpy(output, input, output_size - 1);
        output[output_size - 1] = '\0';
        return;
    }
    
    /* Quote and escape */
    output[j++] = '"';
    for (i = 0; input[i] && j < output_size - 2; i++) {
        if (input[i] == '"') {
            output[j++] = '"';
        }
        if (j < output_size - 2) {
            output[j++] = input[i];
        }
    }
    output[j++] = '"';
    output[j] = '\0';
}

/* ============== Format Writers ============== */

static int write_json_entry(FILE* fp, const log_message_entry_t* entry, 
                            const log_export_config_t* config) {
    char escaped[1024];
    char timestamp[64];
    char* payload_hex = NULL;
    time_t ts_sec;
    
    /* Format timestamp */
    ts_sec = (time_t)entry->timestamp.tv_sec;
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S.", gmtime(&ts_sec));
    snprintf(timestamp + 20, sizeof(timestamp) - 20, "%03dZ", 
             (int)(entry->timestamp.tv_nsec / 1000000));
    
    /* Build JSON object */
    int written = 0;
    
    if (config->pretty_print) {
        written = fprintf(fp,
            "{\n"
            "  \"sequence\": %lu,\n"
            "  \"timestamp\": \"%s\",\n"
            "  \"protocol\": \"%s\",\n"
            "  \"direction\": \"%s\",\n"
            "  \"procedureCode\": %u,\n"
            "  \"messageType\": %u,\n"
            "  \"messageName\": \"%s\",\n",
            (unsigned long)entry->sequence_number,
            timestamp,
            log_protocol_to_string(entry->protocol),
            log_direction_to_string(entry->direction),
            entry->procedure_code,
            entry->message_type,
            entry->message_name ? entry->message_name : "Unknown");
    } else {
        written = fprintf(fp,
            "{\"seq\":%lu,\"ts\":\"%s\",\"proto\":\"%s\",\"dir\":\"%s\","
            "\"proc\":%u,\"msgType\":%u,\"msgName\":\"%s\",",
            (unsigned long)entry->sequence_number,
            timestamp,
            log_protocol_to_string(entry->protocol),
            log_direction_to_string(entry->direction),
            entry->procedure_code,
            entry->message_type,
            entry->message_name ? entry->message_name : "Unknown");
    }
    
    /* Add endpoints */
    if (config->include_endpoints) {
        json_escape_string(entry->source_addr, escaped, sizeof(escaped));
        if (config->pretty_print) {
            written += fprintf(fp, "  \"source\": \"%s:%u\",\n", escaped, entry->source_port);
        } else {
            written += fprintf(fp, "\"src\":\"%s:%u\",", escaped, entry->source_port);
        }
        
        json_escape_string(entry->dest_addr, escaped, sizeof(escaped));
        if (config->pretty_print) {
            written += fprintf(fp, "  \"destination\": \"%s:%u\",\n", escaped, entry->dest_port);
        } else {
            written += fprintf(fp, "\"dst\":\"%s:%u\",", escaped, entry->dest_port);
        }
    }
    
    /* Add UE IDs */
    if (config->include_ue_ids) {
        if (config->pretty_print) {
            written += fprintf(fp,
                "  \"ranUeId\": %lu,\n"
                "  \"amfUeId\": %lu,\n",
                (unsigned long)entry->ran_ue_id,
                (unsigned long)entry->amf_ue_id);
        } else {
            written += fprintf(fp,
                "\"ranUe\":%lu,\"amfUe\":%lu,",
                (unsigned long)entry->ran_ue_id,
                (unsigned long)entry->amf_ue_id);
        }
    }
    
    /* Add payload */
    if (config->include_payload && entry->payload && entry->payload_len > 0) {
        size_t display_len = entry->payload_len;
        if (config->max_payload_display > 0 && display_len > config->max_payload_display) {
            display_len = config->max_payload_display;
        }
        
        payload_hex = malloc(display_len * 2 + 1);
        if (payload_hex) {
            hex_encode(entry->payload, display_len, payload_hex, display_len * 2 + 1);
            if (config->pretty_print) {
                written += fprintf(fp, "  \"payload\": \"%s\",\n", payload_hex);
            } else {
                written += fprintf(fp, "\"payload\":\"%s\",", payload_hex);
            }
            free(payload_hex);
        }
    }
    
    /* Add result */
    if (config->pretty_print) {
        written += fprintf(fp,
            "  \"result\": %d,\n"
            "  \"resultStr\": \"%s\"\n"
            "}\n",
            entry->result_code,
            entry->result_string ? entry->result_string : "OK");
    } else {
        written += fprintf(fp,
            "\"res\":%d,\"resStr\":\"%s\"}\n",
            entry->result_code,
            entry->result_string ? entry->result_string : "OK");
    }
    
    return written;
}

static int write_text_entry(FILE* fp, const log_message_entry_t* entry,
                            const log_export_config_t* config) {
    char timestamp[64];
    int written = 0;
    time_t ts_sec;
    
    /* Format timestamp */
    ts_sec = (time_t)entry->timestamp.tv_sec;
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", 
             gmtime(&ts_sec));
    
    /* Header line */
    written = fprintf(fp,
        "[%s.%03d] #%lu %s %s %s (proc=%u, type=%u)\n",
        timestamp,
        (int)(entry->timestamp.tv_nsec / 1000000),
        (unsigned long)entry->sequence_number,
        log_protocol_to_string(entry->protocol),
        log_direction_to_string(entry->direction),
        entry->message_name ? entry->message_name : "Unknown",
        entry->procedure_code,
        entry->message_type);
    
    /* Endpoints */
    if (config->include_endpoints) {
        written += fprintf(fp, "  %s:%u -> %s:%u\n",
            entry->source_addr, entry->source_port,
            entry->dest_addr, entry->dest_port);
    }
    
    /* UE IDs */
    if (config->include_ue_ids && (entry->ran_ue_id || entry->amf_ue_id)) {
        written += fprintf(fp, "  RAN-UE-ID: %lu, AMF-UE-ID: %lu\n",
            (unsigned long)entry->ran_ue_id,
            (unsigned long)entry->amf_ue_id);
    }
    
    /* Payload */
    if (config->include_payload && entry->payload && entry->payload_len > 0) {
        size_t display_len = entry->payload_len;
        if (config->max_payload_display > 0 && display_len > config->max_payload_display) {
            display_len = config->max_payload_display;
        }
        
        written += fprintf(fp, "  Payload (%zu bytes): ", entry->payload_len);
        for (size_t i = 0; i < display_len && i < 64; i++) {
            written += fprintf(fp, "%02X ", entry->payload[i]);
        }
        if (display_len > 64 || entry->payload_len > display_len) {
            written += fprintf(fp, "...");
        }
        written += fprintf(fp, "\n");
    }
    
    written += fprintf(fp, "\n");
    return written;
}

static int write_csv_entry(FILE* fp, const log_message_entry_t* entry,
                           const log_export_config_t* config) {
    char buffer[2048];
    char field[512];
    int written = 0;
    time_t ts_sec;
    
    /* Timestamp */
    char timestamp[64];
    ts_sec = (time_t)entry->timestamp.tv_sec;
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", 
             gmtime(&ts_sec));
    snprintf(timestamp + 19, sizeof(timestamp) - 19, ".%03dZ",
             (int)(entry->timestamp.tv_nsec / 1000000));
    
    csv_escape_field(timestamp, field, sizeof(field));
    written += fprintf(fp, "%s,", field);
    
    /* Sequence */
    written += fprintf(fp, "%lu,", (unsigned long)entry->sequence_number);
    
    /* Protocol */
    csv_escape_field(log_protocol_to_string(entry->protocol), field, sizeof(field));
    written += fprintf(fp, "%s,", field);
    
    /* Direction */
    csv_escape_field(log_direction_to_string(entry->direction), field, sizeof(field));
    written += fprintf(fp, "%s,", field);
    
    /* Procedure code */
    written += fprintf(fp, "%u,", entry->procedure_code);
    
    /* Message type */
    written += fprintf(fp, "%u,", entry->message_type);
    
    /* Message name */
    csv_escape_field(entry->message_name ? entry->message_name : "Unknown", field, sizeof(field));
    written += fprintf(fp, "%s,", field);
    
    /* Source */
    if (config->include_endpoints) {
        snprintf(buffer, sizeof(buffer), "%s:%u", entry->source_addr, entry->source_port);
        csv_escape_field(buffer, field, sizeof(field));
        written += fprintf(fp, "%s,", field);
    }
    
    /* Result */
    written += fprintf(fp, "%d\n", entry->result_code);
    
    return written;
}

static int write_csv_header(FILE* fp) {
    return fprintf(fp,
        "timestamp,sequence,protocol,direction,procedure_code,message_type,"
        "message_name,source,result\n");
}

/* ============== Initialization ============== */

int log_export_init(const log_export_config_t* config) {
    if (g_export.initialized) {
        return -1;  /* Already initialized */
    }
    
    memset(&g_export, 0, sizeof(g_export));
    
    if (config) {
        memcpy(&g_export.config, config, sizeof(log_export_config_t));
    } else {
        log_export_get_default_config(&g_export.config);
    }
    
    /* Allocate message buffer */
    g_export.buffer_capacity = MAX_MESSAGE_BUFFER;
    g_export.message_buffer = calloc(g_export.buffer_capacity, sizeof(log_message_entry_t));
    if (!g_export.message_buffer) {
        return -1;
    }
    
    /* Initialize statistics */
    g_export.stats.start_time = time(NULL);
    g_export.sequence_number = 1;
    
    g_export.initialized = true;
    
    /* Open output file if path specified */
    if (g_export.config.output_path[0] != '\0') {
        int ret = log_export_open(g_export.config.output_path);
        if (ret != 0) {
            return ret;
        }
    }
    
    return 0;
}

void log_export_cleanup(void) {
    if (!g_export.initialized) return;
    
    log_export_close();
    
    if (g_export.message_buffer) {
        /* Free any allocated payload copies */
        for (size_t i = 0; i < g_export.buffer_count; i++) {
            if (g_export.message_buffer[i].payload) {
                free((void*)g_export.message_buffer[i].payload);
            }
            if (g_export.message_buffer[i].decoded_summary) {
                free((void*)g_export.message_buffer[i].decoded_summary);
            }
        }
        free(g_export.message_buffer);
        g_export.message_buffer = NULL;
    }
    
    g_export.initialized = false;
}

void log_export_get_default_config(log_export_config_t* config) {
    if (!config) return;
    
    memset(config, 0, sizeof(log_export_config_t));
    config->format = LOG_EXPORT_FORMAT_JSON;
    config->include_payload = true;
    config->include_decoded = true;
    config->include_timestamps = true;
    config->include_endpoints = true;
    config->include_ue_ids = true;
    config->pretty_print = true;
    config->max_payload_display = MAX_PAYLOAD_DISPLAY;
    config->rotate_files = false;
    config->max_file_size = 10 * 1024 * 1024;  /* 10 MB */
    config->max_file_count = 5;
}

/* ============== File Operations ============== */

int log_export_open(const char* path) {
    if (!g_export.initialized) return -1;
    
    if (g_export.output_file) {
        log_export_close();
    }
    
    const char* filepath = path ? path : g_export.config.output_path;
    if (!filepath || filepath[0] == '\0') {
        return -1;
    }
    
    g_export.output_file = fopen(filepath, "a");
    if (!g_export.output_file) {
        return -1;
    }
    
    g_export.current_file_size = 0;
    
    /* Write CSV header if needed */
    if (g_export.config.format == LOG_EXPORT_FORMAT_CSV) {
        int written = write_csv_header(g_export.output_file);
        g_export.current_file_size += written;
    }
    
    return 0;
}

void log_export_close(void) {
    if (g_export.output_file) {
        fflush(g_export.output_file);
        fclose(g_export.output_file);
        g_export.output_file = NULL;
    }
}

void log_export_flush(void) {
    if (g_export.output_file) {
        fflush(g_export.output_file);
    }
}

bool log_export_rotate(void) {
    if (!g_export.config.rotate_files || g_export.config.output_path[0] == '\0') {
        return false;
    }
    
    if (g_export.current_file_size < g_export.config.max_file_size) {
        return false;
    }
    
    log_export_close();
    
    /* Generate rotated filename */
    char old_path[512];
    char new_path[512];
    int i;
    
    /* Remove oldest file if at max count */
    snprintf(old_path, sizeof(old_path), "%s.%d", 
             g_export.config.output_path, g_export.config.max_file_count);
    remove(old_path);
    
    /* Rotate existing files */
    for (i = g_export.config.max_file_count - 1; i >= 1; i--) {
        snprintf(old_path, sizeof(old_path), "%s.%d", g_export.config.output_path, i);
        snprintf(new_path, sizeof(new_path), "%s.%d", g_export.config.output_path, i + 1);
        rename(old_path, new_path);
    }
    
    /* Rename current to .1 */
    snprintf(new_path, sizeof(new_path), "%s.1", g_export.config.output_path);
    rename(g_export.config.output_path, new_path);
    
    /* Reopen new file */
    log_export_open(g_export.config.output_path);
    
    return true;
}

/* ============== Message Logging ============== */

int log_export_message(const log_message_entry_t* entry) {
    if (!g_export.initialized || !entry) return -1;
    
    /* Update statistics */
    g_export.stats.total_messages++;
    g_export.stats.messages_by_protocol[entry->protocol]++;
    
    if (entry->direction == LOG_MSG_DIR_UL) {
        g_export.stats.messages_ul++;
    } else if (entry->direction == LOG_MSG_DIR_DL) {
        g_export.stats.messages_dl++;
    }
    
    g_export.stats.bytes_total += entry->payload_len;
    g_export.stats.end_time = time(NULL);
    
    /* Write to file if open */
    if (g_export.output_file) {
        int written = 0;
        
        switch (g_export.config.format) {
            case LOG_EXPORT_FORMAT_JSON:
                written = write_json_entry(g_export.output_file, entry, &g_export.config);
                break;
            case LOG_EXPORT_FORMAT_TEXT:
                written = write_text_entry(g_export.output_file, entry, &g_export.config);
                break;
            case LOG_EXPORT_FORMAT_CSV:
                written = write_csv_entry(g_export.output_file, entry, &g_export.config);
                break;
            default:
                break;
        }
        
        if (written > 0) {
            g_export.current_file_size += written;
        }
        
        /* Check for rotation */
        log_export_rotate();
    }
    
    return 0;
}

int log_export_message_simple(log_protocol_t protocol, log_msg_direction_t direction,
                               const uint8_t* payload, size_t payload_len,
                               const char* fmt, ...) {
    log_message_entry_t entry = {0};
    char description[256];
    va_list args;
    
    /* Fill entry */
    clock_gettime(CLOCK_REALTIME, &entry.timestamp);
    entry.sequence_number = g_export.sequence_number++;
    entry.protocol = protocol;
    entry.direction = direction;
    entry.payload = payload;
    entry.payload_len = payload_len;
    entry.result_code = 0;
    
    /* Format description */
    va_start(args, fmt);
    vsnprintf(description, sizeof(description), fmt, args);
    va_end(args);
    entry.message_name = strdup(description);
    
    /* Log the message */
    int ret = log_export_message(&entry);
    
    if (entry.message_name) {
        free((void*)entry.message_name);
    }
    
    return ret;
}

/* ============== Buffer Management ============== */

void log_export_set_buffer_size(size_t max_messages) {
    if (max_messages == 0) {
        max_messages = MAX_MESSAGE_BUFFER;
    }
    
    g_export.buffer_capacity = max_messages;
    
    if (g_export.message_buffer) {
        g_export.message_buffer = realloc(g_export.message_buffer,
            max_messages * sizeof(log_message_entry_t));
    }
}

void log_export_clear_buffer(void) {
    if (!g_export.initialized) return;
    
    for (size_t i = 0; i < g_export.buffer_count; i++) {
        if (g_export.message_buffer[i].payload) {
            free((void*)g_export.message_buffer[i].payload);
        }
    }
    
    g_export.buffer_count = 0;
    memset(g_export.message_buffer, 0, 
           g_export.buffer_capacity * sizeof(log_message_entry_t));
}

void log_export_get_stats(log_export_stats_t* stats) {
    if (stats) {
        memcpy(stats, &g_export.stats, sizeof(log_export_stats_t));
    }
}

/* ============== Export Functions ============== */

int log_export_to_file(log_export_format_t format, const char* path) {
    if (!g_export.initialized || !path) return -1;
    
    FILE* fp = fopen(path, "w");
    if (!fp) return -1;
    
    int ret = 0;
    
    /* Write header for CSV */
    if (format == LOG_EXPORT_FORMAT_CSV) {
        write_csv_header(fp);
    }
    
    /* Write all buffered messages */
    for (size_t i = 0; i < g_export.buffer_count; i++) {
        log_message_entry_t* entry = &g_export.message_buffer[i];
        
        switch (format) {
            case LOG_EXPORT_FORMAT_JSON:
                ret = write_json_entry(fp, entry, &g_export.config);
                break;
            case LOG_EXPORT_FORMAT_TEXT:
                ret = write_text_entry(fp, entry, &g_export.config);
                break;
            case LOG_EXPORT_FORMAT_CSV:
                ret = write_csv_entry(fp, entry, &g_export.config);
                break;
            default:
                break;
        }
        
        if (ret < 0) break;
    }
    
    fclose(fp);
    return (ret >= 0) ? 0 : -1;
}

int log_export_entry_to_buffer(const log_message_entry_t* entry,
                                log_export_format_t format,
                                char* buffer, size_t buffer_size) {
    if (!entry || !buffer || buffer_size == 0) return -1;
    
    int written = 0;
    size_t offset = 0;
    
    /* Write directly to buffer using snprintf for each format */
    char timestamp[64];
    time_t ts_sec = (time_t)entry->timestamp.tv_sec;
    char escaped[512];
    
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S.", gmtime(&ts_sec));
    snprintf(timestamp + 20, sizeof(timestamp) - 20, "%03dZ", 
             (int)(entry->timestamp.tv_nsec / 1000000));
    
    switch (format) {
        case LOG_EXPORT_FORMAT_JSON:
            written = snprintf(buffer + offset, buffer_size - offset,
                "{\"seq\":%lu,\"ts\":\"%s\",\"proto\":\"%s\",\"dir\":\"%s\","
                "\"proc\":%u,\"msgType\":%u,\"msgName\":\"%s\"}\n",
                (unsigned long)entry->sequence_number,
                timestamp,
                log_protocol_to_string(entry->protocol),
                log_direction_to_string(entry->direction),
                entry->procedure_code,
                entry->message_type,
                entry->message_name ? entry->message_name : "Unknown");
            break;
        case LOG_EXPORT_FORMAT_TEXT:
            written = snprintf(buffer + offset, buffer_size - offset,
                "[%s] #%lu %s %s %s\n",
                timestamp,
                (unsigned long)entry->sequence_number,
                log_protocol_to_string(entry->protocol),
                log_direction_to_string(entry->direction),
                entry->message_name ? entry->message_name : "Unknown");
            break;
        case LOG_EXPORT_FORMAT_CSV:
            csv_escape_field(timestamp, escaped, sizeof(escaped));
            written = snprintf(buffer + offset, buffer_size - offset,
                "%s,%lu,%s,%s,%u,%u,%s,%d\n",
                escaped,
                (unsigned long)entry->sequence_number,
                log_protocol_to_string(entry->protocol),
                log_direction_to_string(entry->direction),
                entry->procedure_code,
                entry->message_type,
                entry->message_name ? entry->message_name : "Unknown",
                entry->result_code);
            break;
        default:
            break;
    }
    
    return (written > 0 && (size_t)written < buffer_size) ? written : -1;
}

int log_export_stats(const char* path) {
    if (!path) return -1;
    
    FILE* fp = fopen(path, "w");
    if (!fp) return -1;
    
    char start_ts[64], end_ts[64];
    time_t now = time(NULL);
    
    strftime(start_ts, sizeof(start_ts), "%Y-%m-%d %H:%M:%S",
             gmtime(&g_export.stats.start_time));
    strftime(end_ts, sizeof(end_ts), "%Y-%m-%d %H:%M:%S",
             g_export.stats.end_time ? gmtime(&g_export.stats.end_time) : gmtime(&now));
    
    fprintf(fp, "{\n");
    fprintf(fp, "  \"totalMessages\": %lu,\n", (unsigned long)g_export.stats.total_messages);
    fprintf(fp, "  \"uplinkMessages\": %lu,\n", (unsigned long)g_export.stats.messages_ul);
    fprintf(fp, "  \"downlinkMessages\": %lu,\n", (unsigned long)g_export.stats.messages_dl);
    fprintf(fp, "  \"totalBytes\": %lu,\n", (unsigned long)g_export.stats.bytes_total);
    fprintf(fp, "  \"errors\": %lu,\n", (unsigned long)g_export.stats.errors);
    fprintf(fp, "  \"startTime\": \"%s\",\n", start_ts);
    fprintf(fp, "  \"endTime\": \"%s\",\n", end_ts);
    fprintf(fp, "  \"protocolBreakdown\": {\n");
    
    for (int i = 0; i <= LOG_PROTO_UNKNOWN; i++) {
        fprintf(fp, "    \"%s\": %lu%s\n",
                g_protocol_names[i],
                (unsigned long)g_export.stats.messages_by_protocol[i],
                (i < LOG_PROTO_UNKNOWN) ? "," : "");
    }
    
    fprintf(fp, "  }\n");
    fprintf(fp, "}\n");
    
    fclose(fp);
    return 0;
}