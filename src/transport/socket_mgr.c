/*
 * 5G UE Simulation Application
 * Socket management implementation
 */

#include "../uesim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/epoll.h>

// Socket manager structure
typedef struct {
    int epoll_fd;
    pthread_t io_thread;
    atomic_bool running;
    pthread_mutex_t socket_mutex;
} socket_manager_t;

// Global socket manager
static socket_manager_t g_socket_mgr = {0};

// Forward declarations
static void* io_thread_function(void* arg);
static uesim_error_t handle_socket_event(struct epoll_event* event);

uesim_error_t socket_manager_init(void) {
    // Create epoll instance
    g_socket_mgr.epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (g_socket_mgr.epoll_fd == -1) {
        perror("epoll_create1");
        return UESIM_ERROR_SOCKET;
    }
    
    // Initialize mutex
    if (pthread_mutex_init(&g_socket_mgr.socket_mutex, NULL) != 0) {
        close(g_socket_mgr.epoll_fd);
        return UESIM_ERROR_THREAD;
    }
    
    // Mark as running
    atomic_store(&g_socket_mgr.running, true);
    
    // Create I/O thread
    if (pthread_create(&g_socket_mgr.io_thread, NULL, io_thread_function, NULL) != 0) {
        atomic_store(&g_socket_mgr.running, false);
        pthread_mutex_destroy(&g_socket_mgr.socket_mutex);
        close(g_socket_mgr.epoll_fd);
        return UESIM_ERROR_THREAD;
    }
    
    printf("Socket manager initialized successfully\n");
    return UESIM_SUCCESS;
}

void socket_manager_cleanup(void) {
    // Stop I/O thread
    atomic_store(&g_socket_mgr.running, false);
    
    // Wake up the I/O thread
    if (g_socket_mgr.epoll_fd >= 0) {
        uint64_t dummy = 1;
        // In a real implementation, we might use eventfd here
    }
    
    // Wait for I/O thread to finish
    if (g_socket_mgr.io_thread != 0) {
        pthread_join(g_socket_mgr.io_thread, NULL);
    }
    
    // Close epoll
    if (g_socket_mgr.epoll_fd >= 0) {
        close(g_socket_mgr.epoll_fd);
        g_socket_mgr.epoll_fd = -1;
    }
    
    // Destroy mutex
    pthread_mutex_destroy(&g_socket_mgr.socket_mutex);
    
    printf("Socket manager cleanup completed\n");
}

uesim_error_t create_ngap_socket(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    int sock = -1;
    struct sockaddr_in addr;
    
    // Create SCTP socket for NGAP
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
    if (sock == -1) {
        // Fallback to TCP if SCTP not available
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == -1) {
            perror("socket");
            return UESIM_ERROR_SOCKET;
        }
    }
    
    // Set socket to non-blocking
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) {
        flags = 0;
    }
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl");
        close(sock);
        return UESIM_ERROR_SOCKET;
    }
    
    // Configure address
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ue_ctx->gnb_port);
    addr.sin_addr.s_addr = ue_ctx->gnb_ip;
    
    // Connect to gNB
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        if (errno != EINPROGRESS) {
            perror("connect");
            close(sock);
            return UESIM_ERROR_SOCKET;
        }
    }
    
    // Add to epoll
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLOUT | EPOLLET;
    event.data.ptr = ue_ctx;
    
    if (epoll_ctl(g_socket_mgr.epoll_fd, EPOLL_CTL_ADD, sock, &event) == -1) {
        perror("epoll_ctl");
        close(sock);
        return UESIM_ERROR_SOCKET;
    }
    
    // Store socket in UE context
    ue_ctx->ngap_socket = sock;
    
    printf("NGAP socket created for UE %u\n", ue_ctx->ue_id);
    return UESIM_SUCCESS;
}

uesim_error_t create_gtpu_socket(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    int sock = -1;
    struct sockaddr_in addr;
    
    // Create UDP socket for GTP-U
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == -1) {
        perror("socket");
        return UESIM_ERROR_SOCKET;
    }
    
    // Set socket to non-blocking
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) {
        flags = 0;
    }
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl");
        close(sock);
        return UESIM_ERROR_SOCKET;
    }
    
    // Bind to local address (optional, for receiving GTP-U packets)
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(2152); // GTP-U default port
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(sock);
        return UESIM_ERROR_SOCKET;
    }
    
    // Add to epoll
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.ptr = ue_ctx;
    
    if (epoll_ctl(g_socket_mgr.epoll_fd, EPOLL_CTL_ADD, sock, &event) == -1) {
        perror("epoll_ctl");
        close(sock);
        return UESIM_ERROR_SOCKET;
    }
    
    // Store socket in UE context
    ue_ctx->gtpu_socket = sock;
    
    printf("GTP-U socket created for UE %u\n", ue_ctx->ue_id);
    return UESIM_SUCCESS;
}

uesim_error_t send_ngap_message(ue_context_t* ue_ctx, const void* data, size_t length) {
    if (ue_ctx == NULL || data == NULL || length == 0) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (ue_ctx->ngap_socket < 0) {
        return UESIM_ERROR_SOCKET;
    }
    
    ssize_t sent = send(ue_ctx->ngap_socket, data, length, MSG_NOSIGNAL);
    if (sent == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Would block, handle in I/O thread
            return UESIM_SUCCESS;
        }
        perror("send");
        return UESIM_ERROR_SOCKET;
    }
    
    if ((size_t)sent != length) {
        printf("Partial send: %zd/%zu bytes\n", sent, length);
        // Handle partial send
    }
    
    return UESIM_SUCCESS;
}

uesim_error_t send_gtpu_packet(ue_context_t* ue_ctx, const void* data, size_t length) {
    if (ue_ctx == NULL || data == NULL || length == 0) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    if (ue_ctx->gtpu_socket < 0) {
        return UESIM_ERROR_SOCKET;
    }
    
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(2152);
    dest_addr.sin_addr.s_addr = ue_ctx->gnb_ip;
    
    ssize_t sent = sendto(ue_ctx->gtpu_socket, data, length, 0,
                         (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if (sent == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Would block, handle in I/O thread
            return UESIM_SUCCESS;
        }
        perror("sendto");
        return UESIM_ERROR_SOCKET;
    }
    
    if ((size_t)sent != length) {
        printf("Partial send: %zd/%zu bytes\n", sent, length);
        // Handle partial send
    }
    
    return UESIM_SUCCESS;
}

// I/O thread function
static void* io_thread_function(void* arg) {
    const int max_events = 64;
    struct epoll_event events[max_events];
    
    printf("I/O thread started\n");
    
    while (atomic_load(&g_socket_mgr.running)) {
        int nfds = epoll_wait(g_socket_mgr.epoll_fd, events, max_events, 1000);
        if (nfds == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("epoll_wait");
            break;
        }
        
        for (int i = 0; i < nfds; i++) {
            handle_socket_event(&events[i]);
        }
    }
    
    printf("I/O thread stopped\n");
    return NULL;
}

// Handle socket events
static uesim_error_t handle_socket_event(struct epoll_event* event) {
    if (event == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    ue_context_t* ue_ctx = (ue_context_t*)event->data.ptr;
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Handle different event types
    if (event->events & EPOLLIN) {
        // Data available for reading
        // TODO: Implement receive logic
    }
    
    if (event->events & EPOLLOUT) {
        // Socket ready for writing
        // TODO: Implement send completion logic
    }
    
    if (event->events & (EPOLLERR | EPOLLHUP)) {
        // Socket error or hangup
        printf("Socket error for UE %u\n", ue_ctx->ue_id);
        // TODO: Handle socket error
    }
    
    return UESIM_SUCCESS;
}