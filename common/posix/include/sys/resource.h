/*
 * `sys/resource.h` - resource usage and limits, which a payload cannot ask the kernel for
 * through the SDK. The types are FreeBSD's, so code that declares them compiles; every
 * call fails with `ENOSYS`, and a caller reading a limit is told it could not.
 *
 * glslang includes it for a memory counter it prints only when built with
 * `DUMP_COUNTERS`, which it is not.
 */
#ifndef OOPS_POSIX_SYS_RESOURCE_H
#define OOPS_POSIX_SYS_RESOURCE_H

#include <sys/time.h> /* struct timeval */

#define RUSAGE_SELF 0
#define RUSAGE_CHILDREN (-1)
#define RUSAGE_THREAD 1

#define RLIMIT_CPU 0
#define RLIMIT_FSIZE 1
#define RLIMIT_DATA 2
#define RLIMIT_STACK 3
#define RLIMIT_CORE 4
#define RLIMIT_NOFILE 8
#define RLIM_INFINITY ((rlim_t)(((unsigned long long)1 << 63) - 1))

typedef long long rlim_t;

struct rlimit {
    rlim_t rlim_cur;
    rlim_t rlim_max;
};

struct rusage {
    struct timeval ru_utime;
    struct timeval ru_stime;
    long ru_maxrss;
    long ru_ixrss;
    long ru_idrss;
    long ru_isrss;
    long ru_minflt;
    long ru_majflt;
    long ru_nswap;
    long ru_inblock;
    long ru_oublock;
    long ru_msgsnd;
    long ru_msgrcv;
    long ru_nsignals;
    long ru_nvcsw;
    long ru_nivcsw;
};

#ifdef __cplusplus
extern "C" {
#endif

/* All three fail with `ENOSYS`: see above. */
int getrusage(int who, struct rusage *usage);
int getrlimit(int resource, struct rlimit *rlp);
int setrlimit(int resource, const struct rlimit *rlp);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_POSIX_SYS_RESOURCE_H */
