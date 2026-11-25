/* Force-included on the Windows build. Declares the POSIX pieces HamClock
 * uses that MinGW does not provide, and keeps Windows headers from renaming
 * identifiers before the rest of the sources see them.
 */
#ifndef HC_POSIX_H
#define HC_POSIX_H

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <sys/types.h>
#include <direct.h>
#include <io.h>

#ifndef F_GETFL
#define F_GETFL 3
#endif
#ifndef F_SETFL
#define F_SETFL 4
#endif
#ifndef O_NONBLOCK
#define O_NONBLOCK 04000
#endif
#ifndef O_NOCTTY
#define O_NOCTTY 0
#endif

/* MinGW mkdir() takes only a path. HamClock passes a POSIX mode too. */
#define mkdir(path, mode) _mkdir(path)

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef SIGPIPE
#define SIGPIPE 13
#endif

#ifndef LOCK_SH
#define LOCK_SH 1
#endif
#ifndef LOCK_EX
#define LOCK_EX 2
#endif
#ifndef LOCK_NB
#define LOCK_NB 4
#endif
#ifndef LOCK_UN
#define LOCK_UN 8
#endif

#ifndef PROT_READ
#define PROT_READ 1
#endif
#ifndef PROT_WRITE
#define PROT_WRITE 2
#endif
#ifndef MAP_PRIVATE
#define MAP_PRIVATE 2
#endif
#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)
#endif

#ifndef WNOHANG
#define WNOHANG 1
#endif
#ifndef WUNTRACED
#define WUNTRACED 2
#endif
#ifndef WIFEXITED
#define WIFEXITED(s) (((s) & 0x7f) == 0)
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(s) (((s) >> 8) & 0xff)
#endif
#ifndef WIFSIGNALED
#define WIFSIGNALED(s) (((s) & 0x7f) > 0 && ((s) & 0x7f) < 0x7f)
#endif
#ifndef WTERMSIG
#define WTERMSIG(s) ((s) & 0x7f)
#endif

#ifdef __cplusplus
extern "C" {
#endif

int fcntl(int fd, int cmd, ...);
int flock(int fd, int op);
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int munmap(void *addr, size_t length);
int fork(void);
int pipe(int pipefd[2]);
int waitpid(int pid, int *status, int options);
int wait(int *status);
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);
long random(void);
void srandom(unsigned int seed);
int chown(const char *path, int uid, int gid);
int fchown(int fd, int uid, int gid);
int getuid(void);
int geteuid(void);
int getgid(void);
int setuid(int uid);
int seteuid(int uid);
int readlink(const char *path, char *buf, size_t bufsiz);
int faccessat(int dirfd, const char *path, int mode, int flags);
#ifndef AT_EACCESS
#define AT_EACCESS 0
#endif
long sysconf(int name);
#ifndef _SC_NPROCESSORS_ONLN
#define _SC_NPROCESSORS_ONLN 1
#endif
int truncate(const char *path, off_t length);
char *strsep(char **stringp, const char *delim);
int ioctl(int fd, unsigned long request, ...);
struct stat;
int lstat(const char *path, struct stat *st);
void sync(void);
struct tm;
struct tm *gmtime_r(const time_t *t, struct tm *result);
time_t timegm(struct tm *tm);
char *strcasestr(const char *haystack, const char *needle);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */
#endif /* HC_POSIX_H */
