#ifndef HC_SYS_SOCKET_H
#define HC_SYS_SOCKET_H
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

/* Arduino headers use these names as macros. Windows headers use the same
 * words as types and parameter names, so set them aside across the include. */
#pragma push_macro("String")
#pragma push_macro("INPUT")
#pragma push_macro("OUTPUT")
#pragma push_macro("INPUT_PULLUP")
#pragma push_macro("LOW")
#pragma push_macro("HIGH")
#pragma push_macro("HANDLE")
#pragma push_macro("byte")
#pragma push_macro("word")
#pragma push_macro("boolean")
#pragma push_macro("bit")
#pragma push_macro("constrain")
#undef String
#undef INPUT
#undef OUTPUT
#undef INPUT_PULLUP
#undef LOW
#undef HIGH
#undef HANDLE
#undef byte
#undef word
#undef boolean
#undef bit
#undef constrain

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma pop_macro("constrain")
#pragma pop_macro("bit")
#pragma pop_macro("boolean")
#pragma pop_macro("word")
#pragma pop_macro("byte")
#pragma pop_macro("HANDLE")
#pragma pop_macro("HIGH")
#pragma pop_macro("LOW")
#pragma pop_macro("INPUT_PULLUP")
#pragma pop_macro("OUTPUT")
#pragma pop_macro("INPUT")
#pragma pop_macro("String")

#ifndef SHUT_RD
#define SHUT_RD SD_RECEIVE
#endif
#ifndef SHUT_WR
#define SHUT_WR SD_SEND
#endif
#ifndef SHUT_RDWR
#define SHUT_RDWR SD_BOTH
#endif
#ifndef SO_REUSEPORT
#define SO_REUSEPORT 0x700B
#endif
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#ifdef __cplusplus
inline int hc_wsa_errno(int err)
{
    switch (err) {
    case WSAEINTR: return EINTR;
    case WSAEWOULDBLOCK: return EWOULDBLOCK;
    case WSAEINPROGRESS: return EINPROGRESS;
    case WSAEINVAL: return EINVAL;
    case WSAECONNREFUSED: return ECONNREFUSED;
    case WSAETIMEDOUT: return ETIMEDOUT;
    case WSAECONNRESET: return ECONNRESET;
    case WSAENOTCONN: return ENOTCONN;
    case WSAEADDRINUSE: return EADDRINUSE;
    case WSAEISCONN: return EISCONN;
    default: return EIO;
    }
}
inline int hc_setsockopt(SOCKET s, int level, int opt, const void *val, int len)
{
    /* Windows has no SO_REUSEPORT. Treating it as success keeps the listener up. */
    if (opt == SO_REUSEPORT)
        return 0;
    int r = (::setsockopt)(s, level, opt, (const char *)val, len);
    if (r == 0)
        return 0;
    errno = hc_wsa_errno(WSAGetLastError());
    return -1;
}
inline int hc_connect(SOCKET s, const struct sockaddr *name, int namelen)
{
    int r = (::connect)(s, name, namelen);
    if (r == 0)
        return 0;
    int err = WSAGetLastError();
    if (err == WSAEWOULDBLOCK || err == WSAEINPROGRESS || err == WSAEALREADY)
        errno = EINPROGRESS;
    else
        errno = hc_wsa_errno(err);
    return -1;
}
inline int hc_getsockopt(SOCKET s, int level, int opt, void *val, int *len)
{
    return (::getsockopt)(s, level, opt, (char *)val, len);
}
inline int hc_recv(SOCKET s, void *buf, int len, int flags)
{
    return (::recv)(s, (char *)buf, len, flags);
}
inline int hc_recvfrom(SOCKET s, void *buf, int len, int flags, struct sockaddr *from, int *fromlen)
{
    return (::recvfrom)(s, (char *)buf, len, flags, from, fromlen);
}
inline int hc_send(SOCKET s, const void *buf, int len, int flags)
{
    return (::send)(s, (const char *)buf, len, flags);
}
inline int hc_sendto(SOCKET s, const void *buf, int len, int flags, const struct sockaddr *to, int tolen)
{
    return (::sendto)(s, (const char *)buf, len, flags, to, tolen);
}
#define setsockopt hc_setsockopt
#define getsockopt hc_getsockopt
#define recv hc_recv
#define recvfrom hc_recvfrom
#define send hc_send
#define sendto hc_sendto
#endif

#ifdef far
#undef far
#endif
#ifdef near
#undef near
#endif
#ifdef small
#undef small
#endif
#ifdef interface
#undef interface
#endif
#ifdef CM_NONE
#undef CM_NONE
#endif
#ifdef IN
#undef IN
#endif
#ifdef OUT
#undef OUT
#endif

#include <pthread.h>

#endif
