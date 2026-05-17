/*
 * 5G UE Simulation Application
 * SCTP Transport Layer Implementation
 * 
 * Cross-platform SCTP support:
 * - Linux: Native SCTP via libsctp
 * - Windows: TCP fallback with PPID embedding
 */

#include "sctp_transport.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <ws2tcpip.h>
#define close closesocket
#define SHUT_RDWR SD_BOTH
#ifndef MSG_WAITALL
#define MSG_WAITALL 0
#endif

/* inet_pton implementation for Windows/MinGW if not available */
#ifndef INET_PTON_IMPLEMENTED
static int inet_pton_impl(int af, const char* src, void* dst) {
    struct sockaddr_in sa;
    int result = -1;
    
    if (af == AF_INET) {
        result = inet_addr(src);
        if (result != INADDR_NONE) {
            memcpy(dst, &result, sizeof(struct in_addr));
            return 1;
        }
    }
    return 0;
}
#define inet_pton inet_pton_impl
#define INET_PTON_IMPLEMENTED
#endif

#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>

/* Try to include native SCTP headers */
#ifdef HAVE_SCTP
#include <netinet/sctp.h>
#endif
#endif

/* ============== Module State ============== */

static struct {
    bool initialized;
    bool native_sctp;
    char implementation[32];
} g_sctp_state = {false, false, "none"};

/* ============== TCP Fallback Header ============== */

/* For TCP fallback, we embed PPID in a simple header */
#define TCP_FALLBACK_MAGIC  0x53545450  /* "STTP" */
typedef struct {
    uint32_t magic;
    uint32_t ppid;
    uint32_t length;
    uint32_t reserved;
} tcp_fallback_header_t;

/* ============== Utility Functions ============== */

void sctp_get_default_config(sctp_config_t* config) {
    if (config == NULL) return;
    
    memset(config, 0, sizeof(sctp_config_t));
    strncpy(config->bind_ip, "0.0.0.0", sizeof(config->bind_ip) - 1);
    config->port = 0;
    config->num_in_streams = 2;
    config->num_out_streams = 2;
    config->connect_timeout_ms = 5000;
    config->recv_timeout_ms = 10000;
    config->send_timeout_ms = 5000;
    config->reuse_addr = true;
    config->nodelay = true;
    config->log_messages = false;
}

const char* sctp_error_to_string(sctp_error_t error) {
    static const char* error_strings[] = {
        "Success",
        "Invalid parameter",
        "Socket error",
        "Bind error",
        "Listen error",
        "Connect error",
        "Accept error",
        "Send error",
        "Receive error",
        "Timeout",
        "Not supported",
        "No memory"
    };
    
    if (error >= 0 || error < -11) return "Unknown error";
    return error_strings[-error];
}

const char* sctp_ppid_to_string(uint32_t ppid) {
    switch (ppid) {
        case SCTP_PPID_NGAP: return "NGAP";
        case SCTP_PPID_F1AP: return "F1AP";
        case SCTP_PPID_XNAP: return "XnAP";
        case SCTP_PPID_E1AP: return "E1AP";
        case SCTP_PPID_S1AP: return "S1AP";
        default: return "Unknown";
    }
}

bool sctp_socket_is_valid(sctp_socket_t socket) {
#ifdef _WIN32
    return socket != INVALID_SOCKET;
#else
    return socket >= 0;
#endif
}

/* ============== Initialization ============== */

sctp_error_t sctp_init(void) {
    if (g_sctp_state.initialized) {
        return SCTP_SUCCESS;
    }
    
#ifdef _WIN32
    /* Initialize Winsock */
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        fprintf(stderr, "[SCTP] WSAStartup failed\n");
        return SCTP_ERROR_SOCKET;
    }
    g_sctp_state.native_sctp = false;
    strncpy(g_sctp_state.implementation, "tcp-fallback", 
            sizeof(g_sctp_state.implementation) - 1);
#else
    /* Check for native SCTP support */
#ifdef HAVE_SCTP
    /* Try to create an SCTP socket to verify support */
    int test_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
    if (test_sock >= 0) {
        close(test_sock);
        g_sctp_state.native_sctp = true;
        strncpy(g_sctp_state.implementation, "native",
                sizeof(g_sctp_state.implementation) - 1);
    } else {
        g_sctp_state.native_sctp = false;
        strncpy(g_sctp_state.implementation, "tcp-fallback",
                sizeof(g_sctp_state.implementation) - 1);
    }
#else
    g_sctp_state.native_sctp = false;
    strncpy(g_sctp_state.implementation, "tcp-fallback",
            sizeof(g_sctp_state.implementation) - 1);
#endif
#endif
    
    g_sctp_state.initialized = true;
    
    printf("[SCTP] Initialized: %s mode (native SCTP: %s)\n",
           g_sctp_state.implementation,
           g_sctp_state.native_sctp ? "yes" : "no");
    
    return SCTP_SUCCESS;
}

void sctp_cleanup(void) {
    if (!g_sctp_state.initialized) return;
    
#ifdef _WIN32
    WSACleanup();
#endif
    
    g_sctp_state.initialized = false;
    g_sctp_state.native_sctp = false;
}

bool sctp_is_native_supported(void) {
    return g_sctp_state.native_sctp;
}

const char* sctp_get_implementation(void) {
    return g_sctp_state.implementation;
}

/* ============== Native SCTP Implementation (Linux) ============== */

#ifdef HAVE_SCTP

static sctp_error_t sctp_native_create_socket(const sctp_config_t* config, 
                                               sctp_socket_t* socket) {
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
    if (sock < 0) {
        return SCTP_ERROR_SOCKET;
    }
    
    /* Set socket options */
    if (config && config->reuse_addr) {
        int opt = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    }
    
    if (config && config->nodelay) {
        int opt = 1;
        setsockopt(sock, IPPROTO_SCTP, SCTP_NODELAY, &opt, sizeof(opt));
    }
    
    /* Configure streams */
    if (config) {
        struct sctp_initmsg initmsg;
        memset(&initmsg, 0, sizeof(initmsg));
        initmsg.sinit_num_ostreams = config->num_out_streams;
        initmsg.sinit_max_instreams = config->num_in_streams;
        initmsg.sinit_max_attempts = 3;
        setsockopt(sock, IPPROTO_SCTP, SCTP_INITMSG, &initmsg, sizeof(initmsg));
    }
    
    *socket = sock;
    return SCTP_SUCCESS;
}

static sctp_error_t sctp_native_send(sctp_socket_t socket, const void* data, 
                                      size_t len, uint32_t ppid) {
    struct sctp_sndrcvinfo sinfo;
    memset(&sinfo, 0, sizeof(sinfo));
    sinfo.sinfo_ppid = htonl(ppid);
    sinfo.sinfo_stream = 0;
    sinfo.sinfo_flags = SCTP_UNORDERED;
    
    ssize_t sent = sctp_send(socket, data, len, &sinfo, 0);
    if (sent < 0 || (size_t)sent != len) {
        return SCTP_ERROR_SEND;
    }
    
    return SCTP_SUCCESS;
}

static sctp_error_t sctp_native_recv(sctp_socket_t socket, void* buf, 
                                      size_t buf_size, size_t* bytes_received,
                                      uint32_t* ppid) {
    struct sctp_sndrcvinfo rinfo;
    int flags = 0;
    socklen_t infolen = sizeof(rinfo);
    
    ssize_t received = sctp_recvmsg(socket, buf, buf_size, NULL, 0, &rinfo, &flags);
    if (received < 0) {
        return SCTP_ERROR_RECV;
    }
    
    *bytes_received = (size_t)received;
    *ppid = ntohl(rinfo.sinfo_ppid);
    
    return SCTP_SUCCESS;
}

#endif /* HAVE_SCTP */

/* ============== TCP Fallback Implementation ============== */

sctp_error_t sctp_tcp_fallback_create(sctp_socket_t* sock) {
    *sock = socket(AF_INET, SOCK_STREAM, 0);
    if (!sctp_socket_is_valid(*sock)) {
        return SCTP_ERROR_SOCKET;
    }
    
    return SCTP_SUCCESS;
}

sctp_error_t sctp_tcp_fallback_send(sctp_socket_t socket, const void* data, 
                                    size_t len, uint32_t ppid) {
    /* Build TCP fallback header */
    tcp_fallback_header_t header;
    header.magic = htonl(TCP_FALLBACK_MAGIC);
    header.ppid = htonl(ppid);
    header.length = htonl((uint32_t)len);
    header.reserved = 0;
    
    /* Send header */
    ssize_t sent = send(socket, (const char*)&header, sizeof(header), 0);
    if (sent != sizeof(header)) {
        return SCTP_ERROR_SEND;
    }
    
    /* Send data */
    if (len > 0 && data != NULL) {
        sent = send(socket, (const char*)data, len, 0);
        if (sent != (ssize_t)len) {
            return SCTP_ERROR_SEND;
        }
    }
    
    return SCTP_SUCCESS;
}

sctp_error_t sctp_tcp_fallback_recv(sctp_socket_t socket, void* buf, 
                                    size_t buf_size, size_t* bytes_received,
                                    uint32_t* ppid) {
    /* Receive TCP fallback header - read byte by byte for cross-platform safety */
    tcp_fallback_header_t header;
    size_t total_received = 0;
    
    while (total_received < sizeof(header)) {
        ssize_t received = recv(socket, (char*)&header + total_received, 
                                sizeof(header) - total_received, 0);
        if (received <= 0) {
            if (received == 0) {
                /* Connection closed */
                return SCTP_ERROR_RECV;
            }
#ifdef _WIN32
            if (WSAGetLastError() == WSAEWOULDBLOCK) {
                /* Would block, try again */
                continue;
            }
#endif
            return SCTP_ERROR_RECV;
        }
        total_received += received;
    }
    
    /* Validate magic */
    if (ntohl(header.magic) != TCP_FALLBACK_MAGIC) {
        fprintf(stderr, "[SCTP] TCP fallback: Invalid magic 0x%08X\n", 
                ntohl(header.magic));
        return SCTP_ERROR_PROTOCOL;
    }
    
    /* Extract PPID and length */
    *ppid = ntohl(header.ppid);
    uint32_t length = ntohl(header.length);
    
    if (length > buf_size) {
        fprintf(stderr, "[SCTP] TCP fallback: Buffer too small (%u > %zu)\n",
                length, buf_size);
        return SCTP_ERROR_RECV;
    }
    
    /* Receive data - read in loop for cross-platform safety */
    if (length > 0) {
        total_received = 0;
        while (total_received < length) {
            ssize_t received = recv(socket, (char*)buf + total_received,
                                    length - total_received, 0);
            if (received <= 0) {
#ifdef _WIN32
                if (received < 0 && WSAGetLastError() == WSAEWOULDBLOCK) {
                    continue;
                }
#endif
                return SCTP_ERROR_RECV;
            }
            total_received += received;
        }
    }
    
    *bytes_received = length;
    return SCTP_SUCCESS;
}

/* ============== Socket Lifecycle ============== */

sctp_error_t sctp_create_socket(const sctp_config_t* config, sctp_socket_t* socket) {
    if (!g_sctp_state.initialized) {
        sctp_error_t err = sctp_init();
        if (err != SCTP_SUCCESS) return err;
    }
    
    if (socket == NULL) return SCTP_ERROR_INVALID_PARAM;
    
    sctp_config_t default_config;
    if (config == NULL) {
        sctp_get_default_config(&default_config);
        config = &default_config;
    }
    
#ifdef HAVE_SCTP
    if (g_sctp_state.native_sctp) {
        return sctp_native_create_socket(config, socket);
    }
#endif
    
    /* TCP fallback */
    return sctp_tcp_fallback_create(socket);
}

sctp_error_t sctp_bind(sctp_socket_t socket, const char* ip, uint16_t port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (ip == NULL || strlen(ip) == 0) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, ip, &addr.sin_addr);
    }
    
    if (bind(socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        return SCTP_ERROR_BIND;
    }
    
    return SCTP_SUCCESS;
}

sctp_error_t sctp_listen(sctp_socket_t socket, int backlog) {
    if (backlog <= 0) backlog = SCTP_DEFAULT_BACKLOG;
    
    if (listen(socket, backlog) < 0) {
        return SCTP_ERROR_LISTEN;
    }
    
    return SCTP_SUCCESS;
}

sctp_error_t sctp_accept(sctp_socket_t socket, struct sockaddr_in* client_addr, 
                         sctp_socket_t* client_socket) {
    socklen_t addr_len = sizeof(struct sockaddr_in);
    sctp_socket_t sock = accept(socket, (struct sockaddr*)client_addr, &addr_len);
    
    if (!sctp_socket_is_valid(sock)) {
        return SCTP_ERROR_ACCEPT;
    }
    
    *client_socket = sock;
    return SCTP_SUCCESS;
}

sctp_error_t sctp_connect(sctp_socket_t socket, const char* ip, uint16_t port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    
    if (connect(socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        return SCTP_ERROR_CONNECT;
    }
    
    return SCTP_SUCCESS;
}

void sctp_close(sctp_socket_t socket) {
    if (sctp_socket_is_valid(socket)) {
        shutdown(socket, SHUT_RDWR);
        close(socket);
    }
}

/* ============== Data Transfer ============== */

sctp_error_t sctp_send_msg(sctp_socket_t socket, const void* data, size_t len,
                           const sctp_msg_info_t* info) {
    if (info == NULL) {
        return sctp_send(socket, data, len, 0);
    }
    
#ifdef HAVE_SCTP
    if (g_sctp_state.native_sctp) {
        struct sctp_sndrcvinfo sinfo;
        memset(&sinfo, 0, sizeof(sinfo));
        sinfo.sinfo_ppid = htonl(info->ppid);
        sinfo.sinfo_stream = info->stream;
        sinfo.sinfo_flags = info->unordered ? SCTP_UNORDERED : 0;
        
        ssize_t sent = sctp_send(socket, data, len, &sinfo, 0);
        if (sent < 0 || (size_t)sent != len) {
            return SCTP_ERROR_SEND;
        }
        return SCTP_SUCCESS;
    }
#endif
    
    /* TCP fallback */
    return sctp_tcp_fallback_send(socket, data, len, info->ppid);
}

sctp_error_t sctp_recv_msg(sctp_socket_t socket, void* buf, size_t buf_size,
                            size_t* bytes_received, sctp_msg_info_t* info) {
    if (bytes_received == NULL) return SCTP_ERROR_INVALID_PARAM;
    
#ifdef HAVE_SCTP
    if (g_sctp_state.native_sctp) {
        struct sctp_sndrcvinfo rinfo;
        int flags = 0;
        
        ssize_t received = sctp_recvmsg(socket, buf, buf_size, NULL, 0, &rinfo, &flags);
        if (received < 0) {
            return SCTP_ERROR_RECV;
        }
        
        *bytes_received = (size_t)received;
        if (info) {
            info->ppid = ntohl(rinfo.sinfo_ppid);
            info->stream = rinfo.sinfo_stream;
            info->context = rinfo.sinfo_context;
            info->unordered = (rinfo.sinfo_flags & SCTP_UNORDERED) != 0;
            info->complete = true;
        }
        
        return SCTP_SUCCESS;
    }
#endif
    
    /* TCP fallback */
    uint32_t ppid = 0;
    sctp_error_t err = sctp_tcp_fallback_recv(socket, buf, buf_size, bytes_received, &ppid);
    if (err == SCTP_SUCCESS && info) {
        info->ppid = ppid;
        info->stream = 0;
        info->context = 0;
        info->unordered = false;
        info->complete = true;
    }
    
    return err;
}

sctp_error_t sctp_send(sctp_socket_t socket, const void* data, size_t len, uint32_t ppid) {
#ifdef HAVE_SCTP
    if (g_sctp_state.native_sctp) {
        return sctp_native_send(socket, data, len, ppid);
    }
#endif
    
    return sctp_tcp_fallback_send(socket, data, len, ppid);
}

sctp_error_t sctp_recv(sctp_socket_t socket, void* buf, size_t buf_size,
                       size_t* bytes_received, uint32_t* ppid) {
    if (ppid == NULL) {
        uint32_t dummy_ppid;
        return sctp_recv(socket, buf, buf_size, bytes_received, &dummy_ppid);
    }
    
    sctp_msg_info_t info;
    sctp_error_t err = sctp_recv_msg(socket, buf, buf_size, bytes_received, &info);
    if (err == SCTP_SUCCESS) {
        *ppid = info.ppid;
    }
    
    return err;
}

/* ============== Socket Options ============== */

sctp_error_t sctp_set_nonblocking(sctp_socket_t socket, bool nonblock) {
#ifdef _WIN32
    u_long mode = nonblock ? 1 : 0;
    if (ioctlsocket(socket, FIONBIO, &mode) != 0) {
        return SCTP_ERROR_SOCKET;
    }
#else
    int flags = fcntl(socket, F_GETFL, 0);
    if (flags < 0) return SCTP_ERROR_SOCKET;
    
    if (nonblock) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    
    if (fcntl(socket, F_SETFL, flags) < 0) {
        return SCTP_ERROR_SOCKET;
    }
#endif
    
    return SCTP_SUCCESS;
}

sctp_error_t sctp_set_recv_timeout(sctp_socket_t socket, uint32_t timeout_ms) {
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, 
                   (const char*)&tv, sizeof(tv)) < 0) {
        return SCTP_ERROR_SOCKET;
    }
    
    return SCTP_SUCCESS;
}

sctp_error_t sctp_set_send_timeout(sctp_socket_t socket, uint32_t timeout_ms) {
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    if (setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, 
                   (const char*)&tv, sizeof(tv)) < 0) {
        return SCTP_ERROR_SOCKET;
    }
    
    return SCTP_SUCCESS;
}

sctp_error_t sctp_set_stream_reset(sctp_socket_t socket, bool enable) {
#ifdef HAVE_SCTP
    if (g_sctp_state.native_sctp) {
        struct sctp_assoc_value value;
        value.assoc_id = 0;  /* Current association */
        value.assoc_value = enable ? 1 : 0;
        
        if (setsockopt(socket, IPPROTO_SCTP, SCTP_ENABLE_STREAM_RESET,
                       &value, sizeof(value)) < 0) {
            return SCTP_ERROR_SOCKET;
        }
    }
#else
    (void)socket;
    (void)enable;
#endif
    
    return SCTP_SUCCESS;
}

/* ============== Association Management ============== */

sctp_error_t sctp_get_assoc_info(sctp_socket_t socket, sctp_assoc_info_t* info) {
    if (info == NULL) return SCTP_ERROR_INVALID_PARAM;
    
    memset(info, 0, sizeof(sctp_assoc_info_t));
    
#ifdef HAVE_SCTP
    if (g_sctp_state.native_sctp) {
        struct sctp_status status;
        socklen_t len = sizeof(status);
        memset(&status, 0, sizeof(status));
        
        if (getsockopt(socket, IPPROTO_SCTP, SCTP_STATUS, &status, &len) == 0) {
            info->assoc_id = status.sstat_assoc_id;
            info->in_streams = status.sstat_instrms;
            info->out_streams = status.sstat_outstrms;
        }
    }
#else
    (void)socket;
#endif
    
    return SCTP_SUCCESS;
}

sctp_error_t sctp_get_stream_count(sctp_socket_t socket, uint16_t* in_streams, 
                                   uint16_t* out_streams) {
    sctp_assoc_info_t info;
    sctp_error_t err = sctp_get_assoc_info(socket, &info);
    
    if (err != SCTP_SUCCESS) return err;
    
    if (in_streams) *in_streams = info.in_streams;
    if (out_streams) *out_streams = info.out_streams;
    
    return SCTP_SUCCESS;
}

/* ============== Wait Functions ============== */

sctp_error_t sctp_wait_readable(sctp_socket_t socket, int timeout_ms) {
    fd_set read_fds;
    struct timeval tv;
    
    FD_ZERO(&read_fds);
    FD_SET(socket, &read_fds);
    
    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
    }
    
    int result = select((int)socket + 1, &read_fds, NULL, NULL, 
                        timeout_ms >= 0 ? &tv : NULL);
    
    if (result < 0) {
        return SCTP_ERROR_SOCKET;
    } else if (result == 0) {
        return SCTP_ERROR_TIMEOUT;
    }
    
    return SCTP_SUCCESS;
}

sctp_error_t sctp_wait_writable(sctp_socket_t socket, int timeout_ms) {
    fd_set write_fds;
    struct timeval tv;
    
    FD_ZERO(&write_fds);
    FD_SET(socket, &write_fds);
    
    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
    }
    
    int result = select((int)socket + 1, NULL, &write_fds, NULL, 
                        timeout_ms >= 0 ? &tv : NULL);
    
    if (result < 0) {
        return SCTP_ERROR_SOCKET;
    } else if (result == 0) {
        return SCTP_ERROR_TIMEOUT;
    }
    
    return SCTP_SUCCESS;
}