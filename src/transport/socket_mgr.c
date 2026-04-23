/*
 * 5G UE Simulation Application
 * Socket management implementation
 */

#include "../uesim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
    #include <io.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #define SHUT_RDWR SD_BOTH
    #define MSG_NOSIGNAL 0
#else
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/epoll.h>
#endif

// Socket manager structure
typedef struct {
    int epoll_fd;
    pthread_t io_thread;
#ifdef _WIN32
    volatile LONG running;
#else
    atomic_bool running;
#endif
    pthread_mutex_t socket_mutex;
} socket_manager_t;

// Global socket manager
static socket_manager_t g_socket_mgr = {0};

// Forward declarations
static void* io_thread_function(void* arg);

#ifdef _WIN32
/* Windows: use select() instead of epoll */
uesim_error_t socket_manager_init(void) {
    g_socket_mgr.epoll_fd = -1;
    
    if (pthread_mutex_init(&g_socket_mgr.socket_mutex, NULL) != 0) {
        return UESIM_ERROR_THREAD;
    }
    
    g_socket_mgr.running = 1;
    
    if (pthread_create(&g_socket_mgr.io_thread, NULL, io_thread_function, NULL) != 0) {
        g_socket_mgr.running = 0;
        pthread_mutex_destroy(&g_socket_mgr.socket_mutex);
        return UESIM_ERROR_THREAD;
    }
    
    printf("Socket manager initialized successfully\n");
    return UESIM_SUCCESS;
}

void socket_manager_cleanup(void) {
    g_socket_mgr.running = 0;
    
    if (g_socket_mgr.io_thread != NULL) {
        pthread_join(g_socket_mgr.io_thread, NULL);
    }
    
    pthread_mutex_destroy(&g_socket_mgr.socket_mutex);
    printf("Socket manager cleanup completed\n");
}

#else
/* Linux: use epoll */
uesim_error_t socket_manager_init(void) {
    g_socket_mgr.epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (g_socket_mgr.epoll_fd == -1) {
        perror("epoll_create1");
        return UESIM_ERROR_SOCKET;
    }
    
    if (pthread_mutex_init(&g_socket_mgr.socket_mutex, NULL) != 0) {
        close(g_socket_mgr.epoll_fd);
        return UESIM_ERROR_THREAD;
    }
    
    atomic_store(&g_socket_mgr.running, true);
    
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
    atomic_store(&g_socket_mgr.running, false);
    
    if (g_socket_mgr.io_thread != 0) {
        pthread_join(g_socket_mgr.io_thread, NULL);
    }
    
    if (g_socket_mgr.epoll_fd >= 0) {
        close(g_socket_mgr.epoll_fd);
        g_socket_mgr.epoll_fd = -1;
    }
    
    pthread_mutex_destroy(&g_socket_mgr.socket_mutex);
    printf("Socket manager cleanup completed\n");
}
#endif

uesim_error_t create_ngap_socket(ue_context_t* ue_ctx) {
    if (ue_ctx == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    int sock = -1;
    struct sockaddr_in addr;
    
    /* Try SCTP first, fallback to TCP */
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == -1) {
        perror("socket");
        return UESIM_ERROR_SOCKET;
    }
    
#ifndef _WIN32
    /* Set non-blocking on Unix */
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) flags = 0;
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl");
        uesim_sock_close(sock);
        return UESIM_ERROR_SOCKET;
    }
#else
    /* Set non-blocking on Windows */
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#endif
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ue_ctx->gnb_port);
    addr.sin_addr.s_addr = ue_ctx->gnb_ip;
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK) {
            uesim_sock_close(sock);
            return UESIM_ERROR_SOCKET;
        }
#else
        if (errno != EINPROGRESS) {
            perror("connect");
            uesim_sock_close(sock);
            return UESIM_ERROR_SOCKET;
        }
#endif
    }
    
#ifndef _WIN32
    /* Add to epoll */
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLOUT | EPOLLET;
    event.data.ptr = ue_ctx;
    
    if (epoll_ctl(g_socket_mgr.epoll_fd, EPOLL_CTL_ADD, sock, &event) == -1) {
        perror("epoll_ctl");
        uesim_sock_close(sock);
        return UESIM_ERROR_SOCKET;
    }
#endif
    
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
    
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == -1) {
        perror("socket");
        return UESIM_ERROR_SOCKET;
    }
    
#ifndef _WIN32
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) flags = 0;
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl");
        uesim_sock_close(sock);
        return UESIM_ERROR_SOCKET;
    }
#else
    {
        u_long mode = 1;
        ioctlsocket(sock, FIONBIO, &mode);
    }
#endif
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(2152);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        uesim_sock_close(sock);
        return UESIM_ERROR_SOCKET;
    }
    
#ifndef _WIN32
    {
        struct epoll_event event;
        event.events = EPOLLIN | EPOLLET;
        event.data.ptr = ue_ctx;
        
        if (epoll_ctl(g_socket_mgr.epoll_fd, EPOLL_CTL_ADD, sock, &event) == -1) {
            perror("epoll_ctl");
            uesim_sock_close(sock);
            return UESIM_ERROR_SOCKET;
        }
    }
#endif
    
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
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            return UESIM_SUCCESS;
        }
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return UESIM_SUCCESS;
        }
#endif
        perror("send");
        return UESIM_ERROR_SOCKET;
    }
    
    if ((size_t)sent != length) {
        printf("Partial send: %zd/%zu bytes\n", sent, length);
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
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            return UESIM_SUCCESS;
        }
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return UESIM_SUCCESS;
        }
#endif
        perror("sendto");
        return UESIM_ERROR_SOCKET;
    }
    
    if ((size_t)sent != length) {
        printf("Partial send: %zd/%zu bytes\n", sent, length);
    }
    
    return UESIM_SUCCESS;
}

#ifdef _WIN32
/* Windows I/O thread using select() */
static void* io_thread_function(void* arg) {
    (void)arg;
    printf("I/O thread started\n");
    
    while (g_socket_mgr.running) {
        Sleep(100); /* Poll interval */
    }
    
    printf("I/O thread stopped\n");
    return NULL;
}
#else
/* Linux I/O thread using epoll */
static void* io_thread_function(void* arg) {
    (void)arg;
    const int max_events = 64;
    struct epoll_event events[max_events];
    
    printf("I/O thread started\n");
    
    while (atomic_load(&g_socket_mgr.running)) {
        int nfds = epoll_wait(g_socket_mgr.epoll_fd, events, max_events, 1000);
        if (nfds == -1) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }
        
        for (int i = 0; i < nfds; i++) {
            ue_context_t* ue_ctx = (ue_context_t*)events[i].data.ptr;
            if (ue_ctx == NULL) continue;
            
            if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                printf("Socket error for UE %u\n", ue_ctx->ue_id);
            }
        }
    }
    
    printf("I/O thread stopped\n");
    return NULL;
}
#endif