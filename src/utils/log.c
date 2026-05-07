/*
 * 5G UE Simulation Application
 * Structured Logging Module Implementation
 * 
 * Log Format: [DD-MM-YYYY HH.MM.SS.mmm]   elapsed L/CAT(P pid, T tid): file: func(line) > message
 */

#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>

/* Case-insensitive string compare for Windows */
static int uesim_strcasecmp(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    return *s1 - *s2;
}
#define strcasecmp uesim_strcasecmp

#else
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <strings.h>
#endif

/* Category names */
const char* g_log_category_names[LOG_CAT_MAX] = {
    "CORE", "PHY", "MAC", "RLC", "PDCP", "SDAP", "RRC", "NAS", "RLF", "SOCKET", "CLI", "QOS"
};


/* ANSI color codes */
#define ANSI_RESET   "\033[0m"
#define ANSI_RED     "\033[31m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_BLUE    "\033[34m"
#define ANSI_MAGENTA "\033[35m"
#define ANSI_CYAN    "\033[36m"
#define ANSI_WHITE   "\033[37m"

/* Level colors */
static const char* g_log_level_colors[] = {
    ANSI_CYAN,    /* TRACE - cyan */
    ANSI_BLUE,    /* DEBUG - blue */
    ANSI_GREEN,   /* INFO - green */
    ANSI_YELLOW,  /* WARN - yellow */
    ANSI_RED,     /* ERROR - red */
    ANSI_MAGENTA  /* FATAL - magenta */
};

/* Global log state */
static struct {
    log_config_t config;
    bool initialized;
    double start_time_ms;
    uint32_t pid;
    log_callback_t callback;
    void* callback_data;
#ifdef _WIN32
    CRITICAL_SECTION mutex;
#else
    pthread_mutex_t mutex;
#endif
} g_log_state = {0};

/* ============== Time Functions ============== */

static double get_time_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart * 1000.0 / (double)frequency.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
#endif
}

char* log_get_timestamp(char* buffer, size_t size) {
    if (buffer == NULL || size < 24) {
        return NULL;
    }
    
#ifdef _WIN32
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(buffer, size, "%02d-%02d-%04d %02d.%02d.%02d.%03d",
             st.wDay, st.wMonth, st.wYear,
             st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
#else
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    snprintf(buffer, size, "%02d-%02d-%04d %02d.%02d.%02d.%03ld",
             tm_info->tm_mday, tm_info->tm_mon + 1, tm_info->tm_year + 1900,
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, tv.tv_usec / 1000);
#endif
    
    return buffer;
}

double log_get_elapsed(void) {
    if (!g_log_state.initialized) {
        return 0.0;
    }
    return (get_time_ms() - g_log_state.start_time_ms) / 1000.0;
}

uint32_t log_get_pid(void) {
#ifdef _WIN32
    return (uint32_t)GetCurrentProcessId();
#else
    return (uint32_t)getpid();
#endif
}

uint32_t log_get_tid(void) {
#ifdef _WIN32
    return (uint32_t)GetCurrentThreadId();
#else
    return (uint32_t)syscall(SYS_gettid);
#endif
}

/* ============== Lock Functions ============== */

static void log_lock(void) {
#ifdef _WIN32
    EnterCriticalSection(&g_log_state.mutex);
#else
    pthread_mutex_lock(&g_log_state.mutex);
#endif
}

static void log_unlock(void) {
#ifdef _WIN32
    LeaveCriticalSection(&g_log_state.mutex);
#else
    pthread_mutex_unlock(&g_log_state.mutex);
#endif
}

/* ============== Initialization & Cleanup ============== */

void log_get_default_config(log_config_t* config) {
    if (config == NULL) {
        return;
    }
    
    memset(config, 0, sizeof(log_config_t));
    config->min_level = LOG_LEVEL_INFO;
    config->enabled_categories = 0xFFFFFFFF;  /* All categories enabled */
    config->backends = LOG_BACKEND_CONSOLE;
    config->file_handle = NULL;
    config->include_file = true;
    config->include_function = true;
    config->include_line = true;
    config->use_colors = true;
}

int log_init(void) {
    log_config_t config;
    log_get_default_config(&config);
    return log_init_with_config(&config);
}

int log_init_with_config(const log_config_t* config) {
    if (config == NULL) {
        return -1;
    }
    
    if (g_log_state.initialized) {
        log_cleanup();
    }
    
    /* Initialize mutex */
#ifdef _WIN32
    InitializeCriticalSection(&g_log_state.mutex);
#else
    pthread_mutex_init(&g_log_state.mutex, NULL);
#endif
    
    /* Store configuration */
    memcpy(&g_log_state.config, config, sizeof(log_config_t));
    g_log_state.start_time_ms = get_time_ms();
    g_log_state.pid = log_get_pid();
    g_log_state.callback = NULL;
    g_log_state.callback_data = NULL;
    g_log_state.initialized = true;
    
    return 0;
}

void log_cleanup(void) {
    if (!g_log_state.initialized) {
        return;
    }
    
    log_lock();
    
    /* Close file if open */
    if (g_log_state.config.file_handle != NULL) {
        fclose(g_log_state.config.file_handle);
        g_log_state.config.file_handle = NULL;
    }
    
    g_log_state.initialized = false;
    
    log_unlock();
    
    /* Destroy mutex */
#ifdef _WIN32
    DeleteCriticalSection(&g_log_state.mutex);
#else
    pthread_mutex_destroy(&g_log_state.mutex);
#endif
}

/* ============== Configuration ============== */

void log_set_level(log_level_t level) {
    if (!g_log_state.initialized) {
        return;
    }
    log_lock();
    g_log_state.config.min_level = level;
    log_unlock();
}

log_level_t log_get_level(void) {
    if (!g_log_state.initialized) {
        return LOG_LEVEL_INFO;
    }
    return g_log_state.config.min_level;
}

void log_set_categories(uint32_t categories) {
    if (!g_log_state.initialized) {
        return;
    }
    log_lock();
    g_log_state.config.enabled_categories = categories;
    log_unlock();
}

void log_enable_category(log_category_t category) {
    if (!g_log_state.initialized || category >= LOG_CAT_MAX) {
        return;
    }
    log_lock();
    g_log_state.config.enabled_categories |= (1 << category);
    log_unlock();
}

void log_disable_category(log_category_t category) {
    if (!g_log_state.initialized || category >= LOG_CAT_MAX) {
        return;
    }
    log_lock();
    g_log_state.config.enabled_categories &= ~(1 << category);
    log_unlock();
}

bool log_is_category_enabled(log_category_t category) {
    if (!g_log_state.initialized || category >= LOG_CAT_MAX) {
        return false;
    }
    return (g_log_state.config.enabled_categories & (1 << category)) != 0;
}

int log_set_file(const char* filepath) {
    if (!g_log_state.initialized) {
        return -1;
    }
    
    log_lock();
    
    /* Close existing file */
    if (g_log_state.config.file_handle != NULL) {
        fclose(g_log_state.config.file_handle);
        g_log_state.config.file_handle = NULL;
        g_log_state.config.backends &= ~LOG_BACKEND_FILE;
    }
    
    /* Open new file if specified */
    if (filepath != NULL) {
        g_log_state.config.file_handle = fopen(filepath, "a");
        if (g_log_state.config.file_handle == NULL) {
            log_unlock();
            return -1;
        }
        g_log_state.config.backends |= LOG_BACKEND_FILE;
    }
    
    log_unlock();
    return 0;
}

void log_set_callback(log_callback_t callback, void* user_data) {
    if (!g_log_state.initialized) {
        return;
    }
    log_lock();
    g_log_state.callback = callback;
    g_log_state.callback_data = user_data;
    if (callback != NULL) {
        g_log_state.config.backends |= LOG_BACKEND_CALLBACK;
    } else {
        g_log_state.config.backends &= ~LOG_BACKEND_CALLBACK;
    }
    log_unlock();
}

void log_set_console(bool enable) {
    if (!g_log_state.initialized) {
        return;
    }
    log_lock();
    if (enable) {
        g_log_state.config.backends |= LOG_BACKEND_CONSOLE;
    } else {
        g_log_state.config.backends &= ~LOG_BACKEND_CONSOLE;
    }
    log_unlock();
}

void log_set_colors(bool enable) {
    if (!g_log_state.initialized) {
        return;
    }
    log_lock();
    g_log_state.config.use_colors = enable;
    log_unlock();
}

/* ============== Level Conversion ============== */

const char* log_level_to_string(log_level_t level) {
    switch (level) {
        case LOG_LEVEL_TRACE: return "TRACE";
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

log_level_t log_level_from_string(const char* str) {
    if (str == NULL) {
        return LOG_LEVEL_INFO;
    }
    
    /* Single character */
    if (strlen(str) == 1) {
        switch (str[0]) {
            case 'T': case 't': return LOG_LEVEL_TRACE;
            case 'D': case 'd': return LOG_LEVEL_DEBUG;
            case 'I': case 'i': return LOG_LEVEL_INFO;
            case 'W': case 'w': return LOG_LEVEL_WARN;
            case 'E': case 'e': return LOG_LEVEL_ERROR;
            case 'F': case 'f': return LOG_LEVEL_FATAL;
            default: return LOG_LEVEL_INFO;
        }
    }
    
    /* Full name */
    if (strcasecmp(str, "TRACE") == 0) return LOG_LEVEL_TRACE;
    if (strcasecmp(str, "DEBUG") == 0) return LOG_LEVEL_DEBUG;
    if (strcasecmp(str, "INFO") == 0) return LOG_LEVEL_INFO;
    if (strcasecmp(str, "WARN") == 0 || strcasecmp(str, "WARNING") == 0) return LOG_LEVEL_WARN;
    if (strcasecmp(str, "ERROR") == 0) return LOG_LEVEL_ERROR;
    if (strcasecmp(str, "FATAL") == 0) return LOG_LEVEL_FATAL;
    
    return LOG_LEVEL_INFO;
}

/* ============== Core Logging Function ============== */

void log_write(log_level_t level, const char* category,
               const char* file, const char* func, int line,
               const char* fmt, ...) {
    if (!g_log_state.initialized) {
        return;
    }
    
    /* Check level filter */
    if (level < g_log_state.config.min_level) {
        return;
    }
    
    /* Format the message */
    char message[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    
    /* Extract filename from path */
    const char* filename = file;
    if (filename != NULL) {
        const char* last_sep = strrchr(filename, '/');
        if (last_sep == NULL) {
            last_sep = strrchr(filename, '\\');
        }
        if (last_sep != NULL) {
            filename = last_sep + 1;
        }
    }
    
    log_lock();
    
    /* Get timestamp and elapsed time */
    char timestamp[32];
    log_get_timestamp(timestamp, sizeof(timestamp));
    double elapsed = log_get_elapsed();
    uint32_t tid = log_get_tid();
    
    /* Build the log line */
    char log_line[2048];
    char* ptr = log_line;
    char* end = log_line + sizeof(log_line);
    
    /* [DD-MM-YYYY HH.MM.SS.mmm] */
    ptr += snprintf(ptr, end - ptr, "[%s]", timestamp);
    
    /* Elapsed time (right-aligned, 10 chars) */
    ptr += snprintf(ptr, end - ptr, "%10.3f ", elapsed);
    
    /* Level/Category(P pid, T tid) */
    ptr += snprintf(ptr, end - ptr, "%c/%s(P %u, T %u): ",
                    (char)level, category ? category : "GENERIC",
                    g_log_state.pid, tid);
    
    /* File: function(line) > message */
    if (g_log_state.config.include_file && filename) {
        ptr += snprintf(ptr, end - ptr, "%s: ", filename);
    }
    if (g_log_state.config.include_function && func) {
        ptr += snprintf(ptr, end - ptr, "%s(", func);
        if (g_log_state.config.include_line) {
            ptr += snprintf(ptr, end - ptr, "%d)", line);
        } else {
            ptr += snprintf(ptr, end - ptr, ")");
        }
        ptr += snprintf(ptr, end - ptr, " > ");
    }
    
    /* Message */
    ptr += snprintf(ptr, end - ptr, "%s", message);
    
    /* Console output */
    if (g_log_state.config.backends & LOG_BACKEND_CONSOLE) {
        FILE* out = (level >= LOG_LEVEL_ERROR) ? stderr : stdout;
        
        if (g_log_state.config.use_colors) {
            int level_idx = 0;
            switch (level) {
                case LOG_LEVEL_TRACE: level_idx = 0; break;
                case LOG_LEVEL_DEBUG: level_idx = 1; break;
                case LOG_LEVEL_INFO:  level_idx = 2; break;
                case LOG_LEVEL_WARN:  level_idx = 3; break;
                case LOG_LEVEL_ERROR: level_idx = 4; break;
                case LOG_LEVEL_FATAL: level_idx = 5; break;
            }
            fprintf(out, "%s%s%s\n", g_log_level_colors[level_idx], log_line, ANSI_RESET);
        } else {
            fprintf(out, "%s\n", log_line);
        }
        fflush(out);
    }
    
    /* File output */
    if ((g_log_state.config.backends & LOG_BACKEND_FILE) && g_log_state.config.file_handle) {
        fprintf(g_log_state.config.file_handle, "%s\n", log_line);
        fflush(g_log_state.config.file_handle);
    }
    
    /* Callback */
    if ((g_log_state.config.backends & LOG_BACKEND_CALLBACK) && g_log_state.callback) {
        g_log_state.callback(level, category, filename, func, line, message, g_log_state.callback_data);
    }
    
    log_unlock();
}

