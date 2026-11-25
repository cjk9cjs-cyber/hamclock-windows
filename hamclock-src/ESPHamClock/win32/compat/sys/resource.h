#ifndef HC_SYS_RESOURCE_H
#define HC_SYS_RESOURCE_H
#include <sys/time.h>
#include "hc_posix.h"
#ifdef __cplusplus
extern "C" {
#endif
struct rusage {
    struct timeval ru_utime;
    struct timeval ru_stime;
};
#ifndef RUSAGE_SELF
#define RUSAGE_SELF 0
#endif
int getrusage(int who, struct rusage *usage);
#ifdef __cplusplus
}
#endif
#endif
