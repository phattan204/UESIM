/*
 * 5G UE Simulation Application
 * Cross-platform inet_pton/inet_ntop compatibility layer
 * 
 * Linux/Unix: Uses native <arpa/inet.h> implementations
 * Windows: Provides fallback implementations for MinGW
 */

#ifndef INET_COMPAT_H
#define INET_COMPAT_H

#include <stdint.h>

#ifdef _WIN32
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0A00
    #endif
    #ifndef WINVER
        #define WINVER 0x0A00
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <sys/socket.h>
#endif

/*
 * Convert network address from presentation format to binary format
 * @param af Address family (AF_INET or AF_INET6)
 * @param src Null-terminated string representation
 * @param dst Output buffer for binary address
 * @return 1 on success, 0 on invalid format, -1 on error
 */
#ifndef HAVE_INET_PTON
int inet_pton(int af, const char *src, void *dst);
#endif

/*
 * Convert network address from binary format to presentation format
 * @param af Address family (AF_INET or AF_INET6)
 * @param src Binary address buffer
 * @param dst Output buffer for string representation
 * @param size Size of output buffer
 * @return Pointer to dst on success, NULL on error
 */
#ifndef HAVE_INET_NTOP
const char* inet_ntop(int af, const void *src, char *dst, size_t size);
#endif

#endif /* INET_COMPAT_H */
