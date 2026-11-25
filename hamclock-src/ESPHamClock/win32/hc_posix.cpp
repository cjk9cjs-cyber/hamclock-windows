/* POSIX helpers used by the Windows build of HamClock. */
#include "hc_posix.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <io.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/resource.h>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <ifaddrs.h>
#include <net/if.h>

#include <sys/statvfs.h>
#include <termios.h>

/* Unix code opens maps and images with "r"/"w". On Windows that is text mode,
 * which stops at Ctrl-Z and rewrites newlines, so the map comes out blank. */
static struct ForceBinaryFiles {
    ForceBinaryFiles() { _set_fmode(_O_BINARY); }
} force_binary_files;

extern "C" int fcntl(int fd, int cmd, ...)
{
    if (cmd == F_GETFL)
        return 0;

    if (cmd == F_SETFL) {
        va_list ap;
        va_start(ap, cmd);
        int flags = va_arg(ap, int);
        va_end(ap);
        u_long nb = (flags & O_NONBLOCK) ? 1 : 0;
        if (ioctlsocket((SOCKET)fd, FIONBIO, &nb) == 0)
            return 0;
        if (WSAGetLastError() == WSAENOTSOCK)
            return 0;
        errno = EINVAL;
        return -1;
    }

    errno = EINVAL;
    return -1;
}

extern "C" int flock(int fd, int op)
{
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        return -1;
    }

    OVERLAPPED ov;
    memset(&ov, 0, sizeof(ov));

    if (op & LOCK_UN) {
        UnlockFileEx(h, 0, 1, 0, &ov);
        return 0;
    }

    DWORD flags = 0;
    if (op & LOCK_EX)
        flags |= LOCKFILE_EXCLUSIVE_LOCK;
    if (op & LOCK_NB)
        flags |= LOCKFILE_FAIL_IMMEDIATELY;
    if (!LockFileEx(h, flags, 0, 1, 0, &ov)) {
        errno = EWOULDBLOCK;
        return -1;
    }
    return 0;
}

extern "C" void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    (void)addr;
    (void)prot;
    (void)flags;
    if (fd < 0 || length == 0) {
        errno = EINVAL;
        return MAP_FAILED;
    }

    char *buf = (char *)malloc(length);
    if (!buf)
        return MAP_FAILED;

    off_t was = _lseek(fd, 0, SEEK_CUR);
    if (_lseek(fd, (long)offset, SEEK_SET) < 0) {
        free(buf);
        return MAP_FAILED;
    }

    size_t got = 0;
    while (got < length) {
        unsigned chunk = (unsigned)((length - got) > 1 << 20 ? 1 << 20 : (length - got));
        int n = _read(fd, buf + got, chunk);
        if (n < 0) {
            free(buf);
            if (was >= 0)
                _lseek(fd, (long)was, SEEK_SET);
            return MAP_FAILED;
        }
        if (n == 0)
            break;
        got += (size_t)n;
    }
    if (was >= 0)
        _lseek(fd, (long)was, SEEK_SET);
    if (got < length)
        memset(buf + got, 0, length - got);
    return buf;
}

extern "C" int munmap(void *addr, size_t length)
{
    (void)length;
    free(addr);
    return 0;
}

extern "C" int fork(void)
{
    errno = ENOSYS;
    return -1;
}

extern "C" int pipe(int pipefd[2])
{
    (void)pipefd;
    errno = ENOSYS;
    return -1;
}

extern "C" int waitpid(int pid, int *status, int options)
{
    (void)pid;
    (void)options;
    if (status)
        *status = 0;
    errno = ECHILD;
    return -1;
}

extern "C" int wait(int *status)
{
    return waitpid(-1, status, 0);
}

extern "C" int setenv(const char *name, const char *value, int overwrite)
{
    if (!overwrite && getenv(name))
        return 0;
    if (!name || strchr(name, '=')) {
        errno = EINVAL;
        return -1;
    }
    size_t n = strlen(name) + strlen(value ? value : "") + 2;
    char *buf = (char *)malloc(n);
    if (!buf)
        return -1;
    snprintf(buf, n, "%s=%s", name, value ? value : "");
    int rc = _putenv(buf);
    /* UCRT copies the string. */
    free(buf);
    if (rc != 0) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

extern "C" int unsetenv(const char *name)
{
    return setenv(name, "", 1);
}

extern "C" long random(void)
{
    return rand();
}

extern "C" void srandom(unsigned int seed)
{
    srand(seed);
}

extern "C" int chown(const char *path, int uid, int gid)
{
    (void)path;
    (void)uid;
    (void)gid;
    return 0;
}

extern "C" int fchown(int fd, int uid, int gid)
{
    (void)fd;
    (void)uid;
    (void)gid;
    return 0;
}

extern "C" int getuid(void)
{
    return 0;
}

extern "C" int geteuid(void)
{
    return 0;
}

extern "C" int getgid(void)
{
    return 0;
}

extern "C" int setuid(int uid)
{
    (void)uid;
    return 0;
}

extern "C" int seteuid(int uid)
{
    (void)uid;
    return 0;
}

extern "C" int readlink(const char *path, char *buf, size_t bufsiz)
{
    (void)path;
    (void)buf;
    (void)bufsiz;
    errno = EINVAL;
    return -1;
}

extern "C" int faccessat(int dirfd, const char *path, int mode, int flags)
{
    (void)dirfd;
    (void)flags;
    return _access(path, mode);
}

extern "C" long sysconf(int name)
{
    (void)name;
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwNumberOfProcessors > 0 ? (long)info.dwNumberOfProcessors : 1;
}

struct hc_ifaddr_node {
    struct ifaddrs ifa;
    struct sockaddr_in addr;
    struct sockaddr_in mask;
    char name[64];
};

extern "C" int getifaddrs(struct ifaddrs **ifap)
{
    if (!ifap) {
        errno = EINVAL;
        return -1;
    }
    *ifap = NULL;

    ULONG bytes = 0;
    GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
            NULL, NULL, &bytes);
    if (bytes == 0)
        return 0;

    IP_ADAPTER_ADDRESSES *list = (IP_ADAPTER_ADDRESSES *)malloc(bytes);
    if (!list) {
        errno = ENOMEM;
        return -1;
    }
    if (GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
            NULL, list, &bytes) != NO_ERROR) {
        free(list);
        errno = EIO;
        return -1;
    }

    struct ifaddrs *head = NULL;
    for (IP_ADAPTER_ADDRESSES *a = list; a; a = a->Next) {
        for (IP_ADAPTER_UNICAST_ADDRESS *u = a->FirstUnicastAddress; u; u = u->Next) {
            if (!u->Address.lpSockaddr || u->Address.lpSockaddr->sa_family != AF_INET)
                continue;
            hc_ifaddr_node *node = (hc_ifaddr_node *)calloc(1, sizeof(*node));
            if (!node)
                continue;
            node->addr = *(struct sockaddr_in *)u->Address.lpSockaddr;
            unsigned prefix = u->OnLinkPrefixLength;
            if (prefix > 32)
                prefix = 32;
            unsigned long mask = prefix == 0 ? 0 : htonl(prefix == 32 ? 0xFFFFFFFFUL : (0xFFFFFFFFUL << (32 - prefix)));
            node->mask.sin_family = AF_INET;
            node->mask.sin_addr.s_addr = mask;
            snprintf(node->name, sizeof(node->name), "%s", a->AdapterName);
            node->ifa.ifa_next = head;
            node->ifa.ifa_name = node->name;
            node->ifa.ifa_flags = (a->OperStatus == IfOperStatusUp) ? IFF_UP : 0;
            node->ifa.ifa_addr = (struct sockaddr *)&node->addr;
            node->ifa.ifa_netmask = (struct sockaddr *)&node->mask;
            head = &node->ifa;
        }
    }
    free(list);
    *ifap = head;
    return 0;
}

extern "C" void freeifaddrs(struct ifaddrs *ifp)
{
    while (ifp) {
        struct ifaddrs *next = ifp->ifa_next;
        free(ifp);
        ifp = next;
    }
}

extern "C" int truncate(const char *path, off_t length)
{
    int fd = _open(path, _O_RDWR);
    if (fd < 0)
        return -1;
    int rc = _chsize(fd, (long)length);
    _close(fd);
    return rc;
}

extern "C" char *strsep(char **stringp, const char *delim)
{
    if (!stringp || !*stringp)
        return NULL;
    char *start = *stringp;
    char *p = start + strcspn(start, delim);
    if (*p) {
        *p = '\0';
        *stringp = p + 1;
    } else {
        *stringp = NULL;
    }
    return start;
}

extern "C" int lstat(const char *path, struct stat *st)
{
    return stat(path, st);
}

extern "C" void sync(void)
{
}

extern "C" struct tm *gmtime_r(const time_t *t, struct tm *result)
{
    if (!t || !result)
        return NULL;
    if (gmtime_s(result, t) != 0)
        return NULL;
    return result;
}

extern "C" time_t timegm(struct tm *tm)
{
    return _mkgmtime(tm);
}

extern "C" char *strcasestr(const char *haystack, const char *needle)
{
    if (!haystack || !needle)
        return NULL;
    if (!*needle)
        return (char *)haystack;
    size_t nlen = strlen(needle);
    for (const char *p = haystack; *p; p++) {
        if (_strnicmp(p, needle, nlen) == 0)
            return (char *)p;
    }
    return NULL;
}

extern "C" int ioctl(int fd, unsigned long request, ...)
{
    (void)fd;
    (void)request;
    errno = ENOTTY;
    return -1;
}

extern "C" int statvfs(const char *path, struct statvfs *buf)
{
    ULARGE_INTEGER avail, total, freeb;
    memset(buf, 0, sizeof(*buf));
    if (!GetDiskFreeSpaceExA(path, &avail, &total, &freeb)) {
        errno = EIO;
        return -1;
    }
    buf->f_bsize = 1024UL * 1024UL;
    buf->f_frsize = buf->f_bsize;
    buf->f_blocks = (unsigned long)(total.QuadPart / buf->f_bsize);
    buf->f_bfree = (unsigned long)(freeb.QuadPart / buf->f_bsize);
    buf->f_bavail = (unsigned long)(avail.QuadPart / buf->f_bsize);
    if (buf->f_blocks == 0)
        buf->f_blocks = 1;
    return 0;
}

extern "C" int getrusage(int who, struct rusage *usage)
{
    (void)who;
    FILETIME create, exit_t, kernel, user;
    memset(usage, 0, sizeof(*usage));
    if (!GetProcessTimes(GetCurrentProcess(), &create, &exit_t, &kernel, &user))
        return 0;
    ULARGE_INTEGER k, u;
    k.LowPart = kernel.dwLowDateTime;
    k.HighPart = kernel.dwHighDateTime;
    u.LowPart = user.dwLowDateTime;
    u.HighPart = user.dwHighDateTime;
    unsigned long long ticks = u.QuadPart + k.QuadPart;
    usage->ru_utime.tv_sec = (long)(ticks / 10000000ULL);
    usage->ru_utime.tv_usec = (long)((ticks % 10000000ULL) / 10ULL);
    return 0;
}

extern "C" int tcgetattr(int fd, struct termios *t)
{
    (void)fd;
    (void)t;
    errno = ENOTTY;
    return -1;
}

extern "C" int tcsetattr(int fd, int actions, const struct termios *t)
{
    (void)fd;
    (void)actions;
    (void)t;
    errno = ENOTTY;
    return -1;
}

extern "C" int tcflush(int fd, int queue)
{
    (void)fd;
    (void)queue;
    errno = ENOTTY;
    return -1;
}

extern "C" int cfsetspeed(struct termios *t, speed_t speed)
{
    if (!t) {
        errno = EINVAL;
        return -1;
    }
    t->c_ispeed = t->c_ospeed = speed;
    return 0;
}

extern "C" int cfsetispeed(struct termios *t, speed_t speed)
{
    return cfsetspeed(t, speed);
}

extern "C" int cfsetospeed(struct termios *t, speed_t speed)
{
    return cfsetspeed(t, speed);
}

extern "C" int cfmakeraw(struct termios *t)
{
    if (!t) {
        errno = EINVAL;
        return -1;
    }
    t->c_iflag = 0;
    t->c_oflag = 0;
    t->c_lflag = 0;
    return 0;
}

extern "C" int tcdrain(int fd)
{
    (void)fd;
    return 0;
}
