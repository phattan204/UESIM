/*
 * 5G UE Simulation Application
 * Windows inet_pton/inet_ntop implementation for MinGW-GCC
 * This file provides a single implementation that all mock_core files can link against
 */

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WINVER
#define WINVER 0x0A00
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <string.h>
#include <stdio.h>

/* inet_pton implementation for MinGW-GCC on Windows */
int inet_pton(int af, const char *src, void *dst) {
    struct sockaddr_storage ss;
    int size = sizeof(ss);
    char src_copy[INET6_ADDRSTRLEN + 1];
    
    ZeroMemory(&ss, sizeof(ss));
    strncpy(src_copy, src, sizeof(src_copy) - 1);
    src_copy[sizeof(src_copy) - 1] = '\0';
    
    if (WSAStringToAddressA(src_copy, af, NULL, (struct sockaddr*)&ss, &size) == 0) {
        switch (af) {
            case AF_INET:
                *(struct in_addr*)dst = ((struct sockaddr_in*)&ss)->sin_addr;
                return 1;
            case AF_INET6:
                *(struct in6_addr*)dst = ((struct sockaddr_in6*)&ss)->sin6_addr;
                return 1;
        }
    }
    return 0;
}

/* inet_ntop implementation for MinGW-GCC on Windows */
const char* inet_ntop(int af, const void *src, char *dst, size_t size) {
    struct sockaddr_storage ss;
    DWORD size_result;
    
    ZeroMemory(&ss, sizeof(ss));
    
    switch (af) {
        case AF_INET:
            {
                struct sockaddr_in *ss4 = (struct sockaddr_in*)&ss;
                ss4->sin_family = AF_INET;
                memcpy(&ss4->sin_addr, src, sizeof(struct in_addr));
                size_result = sizeof(ss4->sin_addr);
            }
            break;
        case AF_INET6:
            {
                struct sockaddr_in6 *ss6 = (struct sockaddr_in6*)&ss;
                ss6->sin6_family = AF_INET6;
                memcpy(&ss6->sin6_addr, src, sizeof(struct in6_addr));
                size_result = sizeof(ss6->sin6_addr);
            }
            break;
        default:
            return NULL;
    }
    
    if (WSAAddressToStringA((struct sockaddr*)&ss, sizeof(ss), NULL, dst, &size_result) == 0) {
        return dst;
    }
    
    return NULL;
}

#endif /* _WIN32 */
