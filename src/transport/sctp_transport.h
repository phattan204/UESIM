/*
 * 5G UE Simulation Application
 * SCTP Transport Layer - Cross-platform abstraction
 * 
 * 3GPP TS 38.412 (NGAP), TS 38.472 (F1AP) use SCTP transport
 * This module provides cross-platform SCTP socket handling
 */

#ifndef SCTP_TRANSPORT_H
#define SCTP_TRANSPORT_H

#include "../uesim.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET sctp_socket_t;
#define SCTP_INVALID_SOCKET INVALID_SOCKET
#else
#include <sys/socket.h>
#include <netinet/in.h>
typedef int sctp_socket_t;
#define SCTP_INVALID_SOCKET (-1)
#endif

/* ============== Constants ============== */

#define SCTP_DEFAULT_BACKLOG        10
#define SCTP_MAX_STREAMS            16
#define SCTP_MAX_MSG_SIZE           65536

/* PPID values for 3GPP protocols */
#define SCTP_PPID_NGAP              60      /* NG Application Protocol */
#define SCTP_PPID_F1AP              61      /* F1 Application Protocol */
#define SCTP_PPID_XNAP              62      /* Xn Application Protocol */
#define SCTP_PPID_E1AP              63      /* E1 Application Protocol */
#define SCTP_PPID_S1AP              18      /* S1 Application Protocol (4G) */

/* ============== Error Codes ============== */

typedef enum {
    SCTP_SUCCESS = 0,
    SCTP_ERROR_INVALID_PARAM = -1,
    SCTP_ERROR_SOCKET = -2,
    SCTP_ERROR_BIND = -3,
    SCTP_ERROR_LISTEN = -4,
    SCTP_ERROR_CONNECT = -5,
    SCTP_ERROR_ACCEPT = -6,
    SCTP_ERROR_SEND = -7,
    SCTP_ERROR_RECV = -8,
    SCTP_ERROR_TIMEOUT = -9,
    SCTP_ERROR_NOT_SUPPORTED = -10,
    SCTP_ERROR_NO_MEMORY = -11,
    SCTP_ERROR_PROTOCOL = -12
} sctp_error_t;

/* ============== SCTP Configuration ============== */

typedef struct {
    char bind_ip[46];
    uint16_t port;
    
    /* Stream configuration */
    uint16_t num_in_streams;
    uint16_t num_out_streams;
    
    /* Timeout settings */
    uint32_t connect_timeout_ms;
    uint32_t recv_timeout_ms;
    uint32_t send_timeout_ms;
    
    /* Behavior flags */
    bool reuse_addr;
    bool nodelay;
    bool log_messages;
} sctp_config_t;

/* ============== SCTP Message Info ============== */

typedef struct {
    uint32_t ppid;              /* Protocol Payload ID */
    uint16_t stream;            /* Stream number */
    uint32_t context;           /* Association context */
    bool unordered;             /* Unordered delivery */
    bool complete;              /* Message complete flag */
} sctp_msg_info_t;

/* ============== SCTP Association Info ============== */

typedef struct {
    uint32_t assoc_id;
    uint16_t in_streams;
    uint16_t out_streams;
    struct sockaddr_in primary_addr;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint32_t rx_messages;
    uint32_t tx_messages;
} sctp_assoc_info_t;

/* ============== Core API Functions ============== */

/**
 * Initialize SCTP subsystem
 * @return SCTP_SUCCESS or error code
 */
sctp_error_t sctp_init(void);

/**
 * Cleanup SCTP subsystem
 */
void sctp_cleanup(void);

/**
 * Check if SCTP is natively supported
 * @return true if native SCTP, false if TCP fallback
 */
bool sctp_is_native_supported(void);

/**
 * Get SCTP implementation name
 * @return "native", "usrsctp", or "tcp-fallback"
 */
const char* sctp_get_implementation(void);

/* ============== Socket Lifecycle ============== */

/**
 * Create SCTP socket
 * @param config Socket configuration (NULL for defaults)
 * @param socket Output socket handle
 * @return SCTP_SUCCESS or error code
 */
sctp_error_t sctp_create_socket(const sctp_config_t* config, sctp_socket_t* socket);

/**
 * Bind SCTP socket to address
 * @param socket Socket handle
 * @param ip IP address to bind (NULL = any)
 * @param port Port number
 * @return SCTP_SUCCESS or error code
 */
sctp_error_t sctp_bind(sctp_socket_t socket, const char* ip, uint16_t port);

/**
 * Listen for incoming connections
 * @param socket Socket handle
 * @param backlog Maximum pending connections
 * @return SCTP_SUCCESS or error code
 */
sctp_error_t sctp_listen(sctp_socket_t socket, int backlog);

/**
 * Accept incoming connection
 * @param socket Listening socket handle
 * @param client_addr Output client address
 * @param client_socket Output client socket handle
 * @return SCTP_SUCCESS or error code
 */
sctp_error_t sctp_accept(sctp_socket_t socket, struct sockaddr_in* client_addr, 
                         sctp_socket_t* client_socket);

/**
 * Connect to remote peer
 * @param socket Socket handle
 * @param ip Remote IP address
 * @param port Remote port
 * @return SCTP_SUCCESS or error code
 */
sctp_error_t sctp_connect(sctp_socket_t socket, const char* ip, uint16_t port);

/**
 * Close SCTP socket
 * @param socket Socket handle
 */
void sctp_close(sctp_socket_t socket);

/* ============== Data Transfer ============== */

/**
 * Send message over SCTP
 * @param socket Socket handle
 * @param data Message data
 * @param len Message length
 * @param info Message info (PPID, stream, etc.)
 * @return SCTP_SUCCESS or error code
 */
sctp_error_t sctp_send_msg(sctp_socket_t socket, const void* data, size_t len,
                           const sctp_msg_info_t* info);

/**
 * Receive message from SCTP
 * @param socket Socket handle
 * @param buf Output buffer
 * @param buf_size Buffer size
 * @param bytes_received Output bytes received
 * @param info Output message info
 * @return SCTP_SUCCESS or error code
 */
sctp_error_t sctp_recv_msg(sctp_socket_t socket, void* buf, size_t buf_size,
                           size_t* bytes_received, sctp_msg_info_t* info);

/**
 * Send message with specific PPID (convenience function)
 * @param socket Socket handle
 * @param data Message data
 * @param len Message length
 * @param ppid Protocol Payload ID
 * @return SCTP_SUCCESS or error code
 */
sctp_error_t sctp_send(sctp_socket_t socket, const void* data, size_t len, uint32_t ppid);

/**
 * Receive message (convenience function)
 * @param socket Socket handle
 * @param buf Output buffer
 * @param buf_size Buffer size
 * @param bytes_received Output bytes received
 * @param ppid Output PPID
 * @return SCTP_SUCCESS or error code
 */
sctp_error_t sctp_recv(sctp_socket_t socket, void* buf, size_t buf_size,
                       size_t* bytes_received, uint32_t* ppid);

/* ============== Socket Options ============== */

/**
 * Set socket to non-blocking mode
 * @param socket Socket handle
 * @param nonblock Enable non-blocking mode
 * @return SCTP_SUCCESS or error code
 */
sctp_error_t sctp_set_nonblocking(sctp_socket_t socket, bool nonblock);

/**
 * Set receive timeout
 * @param socket Socket handle
 * @param timeout_ms Timeout in milliseconds
 * @return SCTP_SUCCESS or error code
 */
sctp_error_t sctp_set_recv_timeout(sctp_socket_t socket, uint32_t timeout_ms);

/**
 * Set send timeout
 * @param socket Socket handle
 * @param timeout_ms Timeout in milliseconds
 * @return SCTP_SUCCESS or error code
 */
sctp_error_t sctp_set_send_timeout(sctp_socket_t socket, uint32_t timeout_ms);

/**
 * Enable/disable SCTP stream reset
 * @param socket Socket handle
 * @param enable Enable stream reset
 * @return SCTP_SUCCESS or error code
 */
sctp_error_t sctp_set_stream_reset(sctp_socket_t socket, bool enable);

/* ============== Association Management ============== */

/**
 * Get association information
 * @param socket Socket handle
 * @param info Output association info
 * @return SCTP_SUCCESS or error code
 */
sctp_error_t sctp_get_assoc_info(sctp_socket_t socket, sctp_assoc_info_t* info);

/**
 * Get number of active streams
 * @param socket Socket handle
 * @param in_streams Output number of inbound streams
 * @param out_streams Output number of outbound streams
 * @return SCTP_SUCCESS or error code
 */
sctp_error_t sctp_get_stream_count(sctp_socket_t socket, uint16_t* in_streams, 
                                   uint16_t* out_streams);

/* ============== Utility Functions ============== */

/**
 * Get default SCTP configuration
 * @param config Output configuration
 */
void sctp_get_default_config(sctp_config_t* config);

/**
 * Convert error code to string
 * @param error Error code
 * @return Error string
 */
const char* sctp_error_to_string(sctp_error_t error);

/**
 * Convert PPID to protocol name
 * @param ppid PPID value
 * @return Protocol name string
 */
const char* sctp_ppid_to_string(uint32_t ppid);

/**
 * Check if socket is valid
 * @param socket Socket handle
 * @return true if valid
 */
bool sctp_socket_is_valid(sctp_socket_t socket);

/**
 * Wait for socket to be readable
 * @param socket Socket handle
 * @param timeout_ms Timeout in milliseconds (-1 for infinite)
 * @return SCTP_SUCCESS if readable, SCTP_ERROR_TIMEOUT on timeout
 */
sctp_error_t sctp_wait_readable(sctp_socket_t socket, int timeout_ms);

/**
 * Wait for socket to be writable
 * @param socket Socket handle
 * @param timeout_ms Timeout in milliseconds (-1 for infinite)
 * @return SCTP_SUCCESS if writable, SCTP_ERROR_TIMEOUT on timeout
 */
sctp_error_t sctp_wait_writable(sctp_socket_t socket, int timeout_ms);

/* ============== TCP Fallback Functions (Internal) ============== */

/**
 * Create TCP socket for fallback mode
 * @param socket Output socket handle
 * @return SCTP_SUCCESS or error code
 */
sctp_error_t sctp_tcp_fallback_create(sctp_socket_t* socket);

/**
 * Send message over TCP fallback
 * @param socket Socket handle
 * @param data Message data
 * @param len Message length
 * @param ppid PPID (embedded in header)
 * @return SCTP_SUCCESS or error code
 */
sctp_error_t sctp_tcp_fallback_send(sctp_socket_t socket, const void* data, 
                                    size_t len, uint32_t ppid);

/**
 * Receive message from TCP fallback
 * @param socket Socket handle
 * @param buf Output buffer
 * @param buf_size Buffer size
 * @param bytes_received Output bytes received
 * @param ppid Output PPID
 * @return SCTP_SUCCESS or error code
 */
sctp_error_t sctp_tcp_fallback_recv(sctp_socket_t socket, void* buf, size_t buf_size,
                                    size_t* bytes_received, uint32_t* ppid);

#endif /* SCTP_TRANSPORT_H */