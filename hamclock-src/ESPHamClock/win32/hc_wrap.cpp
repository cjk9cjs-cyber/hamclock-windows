/* Route POSIX socket calls onto Winsock and set errno.
 * File descriptors and sockets share close/read/write in HamClock, so sockets
 * created here are remembered and everything else stays a CRT file handle.
 */
#include "hc_posix.h"

#include <errno.h>
#include <string.h>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#ifndef SO_REUSEPORT
#define SO_REUSEPORT 0x700B
#endif

static CRITICAL_SECTION sock_mu;
struct SockMuInit {
    SockMuInit()
    {
        InitializeCriticalSection(&sock_mu);
        WSADATA data;
        WSAStartup(MAKEWORD(2, 2), &data);
    }
};
static SockMuInit sock_mu_init;

static SOCKET known_socks[1024];
static int n_known;

static int wsa_errno(int err)
{
    switch (err) {
    case WSAEINTR: return EINTR;
    case WSAEWOULDBLOCK: return EWOULDBLOCK;
    case WSAEINPROGRESS: return EINPROGRESS;
    case WSAEALREADY: return EALREADY;
    case WSAENOTSOCK: return ENOTSOCK;
    case WSAEDESTADDRREQ: return EDESTADDRREQ;
    case WSAEMSGSIZE: return EMSGSIZE;
    case WSAEPROTOTYPE: return EPROTOTYPE;
    case WSAENOPROTOOPT: return ENOPROTOOPT;
    case WSAEPROTONOSUPPORT: return EPROTONOSUPPORT;
    case WSAEOPNOTSUPP: return EOPNOTSUPP;
    case WSAEAFNOSUPPORT: return EAFNOSUPPORT;
    case WSAEADDRINUSE: return EADDRINUSE;
    case WSAEADDRNOTAVAIL: return EADDRNOTAVAIL;
    case WSAENETDOWN: return ENETDOWN;
    case WSAENETUNREACH: return ENETUNREACH;
    case WSAECONNABORTED: return ECONNABORTED;
    case WSAECONNRESET: return ECONNRESET;
    case WSAENOBUFS: return ENOBUFS;
    case WSAEISCONN: return EISCONN;
    case WSAENOTCONN: return ENOTCONN;
    case WSAETIMEDOUT: return ETIMEDOUT;
    case WSAECONNREFUSED: return ECONNREFUSED;
    case WSAEHOSTUNREACH: return EHOSTUNREACH;
    case WSAEINVAL: return EINVAL;
    default: return EIO;
    }
}

static void remember_socket(SOCKET s)
{
    if (s == INVALID_SOCKET)
        return;
    EnterCriticalSection(&sock_mu);
    for (int i = 0; i < n_known; i++) {
        if (known_socks[i] == s) {
            LeaveCriticalSection(&sock_mu);
            return;
        }
    }
    if (n_known < (int)(sizeof(known_socks) / sizeof(known_socks[0])))
        known_socks[n_known++] = s;
    LeaveCriticalSection(&sock_mu);
}

static bool forget_socket(SOCKET s)
{
    bool found = false;
    EnterCriticalSection(&sock_mu);
    for (int i = 0; i < n_known; i++) {
        if (known_socks[i] == s) {
            known_socks[i] = known_socks[--n_known];
            found = true;
            break;
        }
    }
    LeaveCriticalSection(&sock_mu);
    return found;
}

static bool is_socket(SOCKET s)
{
    bool found = false;
    EnterCriticalSection(&sock_mu);
    for (int i = 0; i < n_known; i++) {
        if (known_socks[i] == s) {
            found = true;
            break;
        }
    }
    LeaveCriticalSection(&sock_mu);
    return found;
}

extern "C" int __real_read(int, void *, unsigned int);
extern "C" int __real_write(int, const void *, unsigned int);
extern "C" int __real_close(int);
extern "C" SOCKET WSAAPI __real_socket(int, int, int);
extern "C" int WSAAPI __real_connect(SOCKET, const struct sockaddr *, int);
extern "C" int WSAAPI __real_bind(SOCKET, const struct sockaddr *, int);
extern "C" int WSAAPI __real_listen(SOCKET, int);
extern "C" SOCKET WSAAPI __real_accept(SOCKET, struct sockaddr *, int *);
extern "C" int WSAAPI __real_shutdown(SOCKET, int);
extern "C" int WSAAPI __real_send(SOCKET, const char *, int, int);
extern "C" int WSAAPI __real_recv(SOCKET, char *, int, int);
extern "C" int WSAAPI __real_sendto(SOCKET, const char *, int, int, const struct sockaddr *, int);
extern "C" int WSAAPI __real_recvfrom(SOCKET, char *, int, int, struct sockaddr *, int *);
extern "C" int WSAAPI __real_setsockopt(SOCKET, int, int, const char *, int);
extern "C" int WSAAPI __real_getsockopt(SOCKET, int, int, char *, int *);
extern "C" int WSAAPI __real_select(int, fd_set *, fd_set *, fd_set *, const struct timeval *);

extern "C" int __wrap_read(int fd, void *buf, unsigned int n)
{
    if (!is_socket((SOCKET)fd)) {
        int r = __real_read(fd, buf, n);
        /* Winsock handles are not CRT fds. A failed read is the socket path. */
        if (r >= 0 || errno != EBADF)
            return r;
        remember_socket((SOCKET)fd);
    }
    int r = __real_recv((SOCKET)fd, (char *)buf, (int)n, 0);
    if (r == SOCKET_ERROR) {
        errno = wsa_errno(WSAGetLastError());
        return -1;
    }
    return r;
}

extern "C" int __wrap_write(int fd, const void *buf, unsigned int n)
{
    if (!is_socket((SOCKET)fd)) {
        int r = __real_write(fd, buf, n);
        if (r >= 0 || errno != EBADF)
            return r;
        remember_socket((SOCKET)fd);
    }
    int r = __real_send((SOCKET)fd, (const char *)buf, (int)n, 0);
    if (r == SOCKET_ERROR) {
        errno = wsa_errno(WSAGetLastError());
        return -1;
    }
    return r;
}

extern "C" int __wrap_close(int fd)
{
    if (!forget_socket((SOCKET)fd)) {
        int r = __real_close(fd);
        if (r == 0 || errno != EBADF)
            return r;
    }
    if (__real_shutdown((SOCKET)fd, SD_BOTH) != 0) {
        /* already disconnected is fine */
    }
    if (closesocket((SOCKET)fd) == SOCKET_ERROR) {
        errno = wsa_errno(WSAGetLastError());
        return -1;
    }
    return 0;
}

extern "C" SOCKET WSAAPI __wrap_socket(int af, int type, int protocol)
{
    SOCKET s = __real_socket(af, type, protocol);
    if (s == INVALID_SOCKET) {
        errno = wsa_errno(WSAGetLastError());
        return INVALID_SOCKET;
    }
    remember_socket(s);
    return s;
}

extern "C" int WSAAPI __wrap_connect(SOCKET s, const struct sockaddr *name, int namelen)
{
    int r = __real_connect(s, name, namelen);
    if (r == 0)
        return 0;
    int err = WSAGetLastError();
    if (err == WSAEWOULDBLOCK || err == WSAEINPROGRESS || err == WSAEALREADY)
        errno = EINPROGRESS;
    else
        errno = wsa_errno(err);
    return -1;
}

extern "C" int WSAAPI __wrap_bind(SOCKET s, const struct sockaddr *name, int namelen)
{
    int r = __real_bind(s, name, namelen);
    if (r == 0)
        return 0;
    errno = wsa_errno(WSAGetLastError());
    return -1;
}

extern "C" int WSAAPI __wrap_listen(SOCKET s, int backlog)
{
    int r = __real_listen(s, backlog);
    if (r == 0)
        return 0;
    errno = wsa_errno(WSAGetLastError());
    return -1;
}

extern "C" SOCKET WSAAPI __wrap_accept(SOCKET s, struct sockaddr *addr, int *len)
{
    SOCKET c = __real_accept(s, addr, len);
    if (c == INVALID_SOCKET) {
        errno = wsa_errno(WSAGetLastError());
        return INVALID_SOCKET;
    }
    remember_socket(c);
    return c;
}

extern "C" int WSAAPI __wrap_shutdown(SOCKET s, int how)
{
    int r = __real_shutdown(s, how);
    if (r == 0)
        return 0;
    errno = wsa_errno(WSAGetLastError());
    return -1;
}

extern "C" int WSAAPI __wrap_send(SOCKET s, const char *buf, int len, int flags)
{
    int r = __real_send(s, buf, len, flags);
    if (r == SOCKET_ERROR) {
        errno = wsa_errno(WSAGetLastError());
        return -1;
    }
    return r;
}

extern "C" int WSAAPI __wrap_recv(SOCKET s, char *buf, int len, int flags)
{
    int r = __real_recv(s, buf, len, flags);
    if (r == SOCKET_ERROR) {
        errno = wsa_errno(WSAGetLastError());
        return -1;
    }
    return r;
}

extern "C" int WSAAPI __wrap_sendto(SOCKET s, const char *buf, int len, int flags,
        const struct sockaddr *to, int tolen)
{
    int r = __real_sendto(s, buf, len, flags, to, tolen);
    if (r == SOCKET_ERROR) {
        errno = wsa_errno(WSAGetLastError());
        return -1;
    }
    return r;
}

extern "C" int WSAAPI __wrap_recvfrom(SOCKET s, char *buf, int len, int flags,
        struct sockaddr *from, int *fromlen)
{
    int r = __real_recvfrom(s, buf, len, flags, from, fromlen);
    if (r == SOCKET_ERROR) {
        errno = wsa_errno(WSAGetLastError());
        return -1;
    }
    return r;
}

extern "C" int WSAAPI __wrap_setsockopt(SOCKET s, int level, int opt, const char *val, int len)
{
    if (opt == SO_REUSEPORT)
        return 0;
    int r = __real_setsockopt(s, level, opt, val, len);
    if (r == 0)
        return 0;
    errno = wsa_errno(WSAGetLastError());
    return -1;
}

extern "C" int WSAAPI __wrap_getsockopt(SOCKET s, int level, int opt, char *val, int *len)
{
    int r = __real_getsockopt(s, level, opt, val, len);
    if (r == 0)
        return 0;
    errno = wsa_errno(WSAGetLastError());
    return -1;
}

extern "C" int WSAAPI __wrap_select(int nfds, fd_set *r, fd_set *w, fd_set *e, const struct timeval *t)
{
    int n = __real_select(nfds, r, w, e, t);
    if (n == SOCKET_ERROR) {
        errno = wsa_errno(WSAGetLastError());
        return -1;
    }
    return n;
}
