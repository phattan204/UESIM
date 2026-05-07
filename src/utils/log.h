/*
 * 5G UE Simulation Application
 * Structured Logging Module Header
 * 
 * Log Format: [DD-MM-YYYY HH.MM.SS.mmm]   elapsed L/CAT(P pid, T tid): file: func(line) > message
 */

#ifndef UESIM_LOG_H
#define UESIM_LOG_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/* Log Levels */
typedef enum {
    LOG_LEVEL_TRACE = 'T',   /* Detailed function entry/exit */
    LOG_LEVEL_DEBUG = 'D',   /* Debug information */
    LOG_LEVEL_INFO  = 'I',   /* Normal operation (default) */
    LOG_LEVEL_WARN  = 'W',   /* Warning conditions */
    LOG_LEVEL_ERROR = 'E',   /* Error conditions */
    LOG_LEVEL_FATAL = 'F'    /* System unusable */
} log_level_t;

/* Log Categories */
typedef enum {
    LOG_CAT_CORE      = 0,   /* Core framework */
    LOG_CAT_PHY       = 1,   /* Physical layer */
    LOG_CAT_MAC       = 2,   /* MAC layer */
    LOG_CAT_RLC       = 3,   /* RLC layer */
    LOG_CAT_PDCP      = 4,   /* PDCP layer */
    LOG_CAT_SDAP      = 5,   /* SDAP layer */
    LOG_CAT_RRC       = 6,   /* RRC layer */
    LOG_CAT_NAS       = 7,   /* NAS layer */
    LOG_CAT_RLF       = 8,   /* RLF recovery */
    LOG_CAT_TRANSPORT = 9,   /* Socket/transport */
    LOG_CAT_CLI       = 10,  /* CLI interface */
    LOG_CAT_QOS       = 11,  /* QoS flow */
    LOG_CAT_MAX       = 12
} log_category_t;

/* Category names for output */
extern const char* g_log_category_names[LOG_CAT_MAX];

/* Output backends */
typedef enum {
    LOG_BACKEND_NONE   = 0,
    LOG_BACKEND_CONSOLE = (1 << 0),  /* stdout/stderr */
    LOG_BACKEND_FILE    = (1 << 1),  /* File output */
    LOG_BACKEND_CALLBACK = (1 << 2)  /* Custom callback */
} log_backend_t;

/* Log configuration */
typedef struct {
    log_level_t min_level;          /* Minimum level to output */
    uint32_t enabled_categories;    /* Bitmask of enabled categories */
    log_backend_t backends;         /* Active output backends */
    FILE* file_handle;              /* File handle for LOG_BACKEND_FILE */
    bool include_file;              /* Include source file in output */
    bool include_function;           /* Include function name in output */
    bool include_line;              /* Include line number in output */
    bool use_colors;                /* Use ANSI colors in console */
} log_config_t;

/* Callback type for custom log handling */
typedef void (*log_callback_t)(log_level_t level, const char* category,
                               const char* file, const char* func, int line,
                               const char* message, void* user_data);

/* ============== Initialization & Cleanup ============== */

/**
 * Initialize the logging subsystem
 * @return UESIM_SUCCESS on success, error code otherwise
 */
int log_init(void);

/**
 * Initialize logging with custom configuration
 * @param config Configuration settings
 * @return UESIM_SUCCESS on success, error code otherwise
 */
int log_init_with_config(const log_config_t* config);

/**
 * Cleanup logging subsystem
 */
void log_cleanup(void);

/**
 * Get default configuration
 * @param config Output configuration structure
 */
void log_get_default_config(log_config_t* config);

/* ============== Configuration ============== */

/**
 * Set minimum log level
 * @param level Minimum level to output
 */
void log_set_level(log_level_t level);

/**
 * Get current minimum log level
 * @return Current minimum level
 */
log_level_t log_get_level(void);

/**
 * Enable specific categories
 * @param categories Bitmask of categories to enable
 */
void log_set_categories(uint32_t categories);

/**
 * Enable a single category
 * @param category Category to enable
 */
void log_enable_category(log_category_t category);

/**
 * Disable a single category
 * @param category Category to disable
 */
void log_disable_category(log_category_t category);

/**
 * Check if a category is enabled
 * @param category Category to check
 * @return true if enabled
 */
bool log_is_category_enabled(log_category_t category);

/**
 * Set output file
 * @param filepath Path to log file (NULL to close)
 * @return UESIM_SUCCESS on success
 */
int log_set_file(const char* filepath);

/**
 * Set custom callback
 * @param callback Callback function
 * @param user_data User data passed to callback
 */
void log_set_callback(log_callback_t callback, void* user_data);

/**
 * Enable/disable console output
 * @param enable true to enable
 */
void log_set_console(bool enable);

/**
 * Enable/disable ANSI colors
 * @param enable true to enable
 */
void log_set_colors(bool enable);

/* ============== Core Logging Function ============== */

/**
 * Write a log entry
 * @param level Log level
 * @param category Category name string
 * @param file Source file name
 * @param func Function name
 * @param line Line number
 * @param fmt Printf-style format string
 * @param ... Format arguments
 */
void log_write(log_level_t level, const char* category,
               const char* file, const char* func, int line,
               const char* fmt, ...);

/* ============== Convenience Macros ============== */

#define LOG_TRACE(cat, fmt, ...) \
    log_write(LOG_LEVEL_TRACE, cat, __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_DEBUG(cat, fmt, ...) \
    log_write(LOG_LEVEL_DEBUG, cat, __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_INFO(cat, fmt, ...) \
    log_write(LOG_LEVEL_INFO, cat, __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_WARN(cat, fmt, ...) \
    log_write(LOG_LEVEL_WARN, cat, __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_ERROR(cat, fmt, ...) \
    log_write(LOG_LEVEL_ERROR, cat, __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_FATAL(cat, fmt, ...) \
    log_write(LOG_LEVEL_FATAL, cat, __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)

/* Category name macros for convenience */
#define LOG_CAT_NAME_CORE      "CORE"
#define LOG_CAT_NAME_PHY       "PHY"
#define LOG_CAT_NAME_MAC       "MAC"
#define LOG_CAT_NAME_RLC       "RLC"
#define LOG_CAT_NAME_PDCP      "PDCP"
#define LOG_CAT_NAME_SDAP      "SDAP"
#define LOG_CAT_NAME_RRC       "RRC"
#define LOG_CAT_NAME_NAS       "NAS"
#define LOG_CAT_NAME_RLF       "RLF"
#define LOG_CAT_NAME_TRANSPORT "SOCKET"
#define LOG_CAT_NAME_CLI       "CLI"
#define LOG_CAT_NAME_QOS       "QOS"

/* ============== Utility Functions ============== */

/**
 * Get current timestamp as formatted string
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Pointer to buffer
 */
char* log_get_timestamp(char* buffer, size_t size);

/**
 * Get elapsed time since log init in seconds
 * @return Elapsed time in seconds with millisecond precision
 */
double log_get_elapsed(void);

/**
 * Get process ID
 * @return Process ID
 */
uint32_t log_get_pid(void);

/**
 * Get thread ID
 * @return Thread ID
 */
uint32_t log_get_tid(void);

/**
 * Convert log level to string
 * @param level Log level
 * @return Level string
 */
const char* log_level_to_string(log_level_t level);

/**
 * Parse log level from string
 * @param str Level string (T/D/I/W/E/F or TRACE/DEBUG/INFO/WARN/ERROR/FATAL)
 * @return Log level, LOG_LEVEL_INFO if invalid
 */
log_level_t log_level_from_string(const char* str);

#endif /* UESIM_LOG_H */