/*
 * `pthread_np.h` - FreeBSD's non-portable pthread extensions, or the one of them a port here
 * has asked for.
 *
 * **This header is needed because the target really is FreeBSD.** `-target
 * x86_64-unknown-freebsd` defines `__FreeBSD__`, so a portable program's `#elif
 * defined(__FreeBSD__)` branch is the one that compiles - spdlog's `details/os-inl.h:52` is the
 * first to take it. That is the correct branch for this platform and not something to work
 * around; what was missing is simply the header it names.
 *
 * `pthread_getthreadid_np` answers a small integer identifying the calling thread, used for log
 * lines and nothing else. `oops_thread_self` is the handle the platform already gives, and its
 * low bits are what this returns - stable for the life of a thread, distinct between live
 * threads, and not meaningful beyond that. It is defined in `common/posix/posix.c` rather than
 * here so there is one copy per link rather than one per translation unit.
 *
 * `pthread_set_name_np` changes nothing: the platform names a thread when it is created
 * (`oops_thread_create`'s first argument) and has no call to rename one. FreeBSD's returns
 * `void`, so there is no failure to report, and the name is only ever read by a debugger.
 *
 * FreeBSD's copy includes `<pthread.h>`, which includes `<sched.h>`, and Luanti's thread code
 * reaches both through this header alone.
 */
#ifndef OOPS_POSIX_PTHREAD_NP_H
#define OOPS_POSIX_PTHREAD_NP_H

#include <pthread.h>
#include <sched.h>

#ifdef __cplusplus
extern "C" {
#endif

int pthread_getthreadid_np(void);

static inline void pthread_set_name_np(pthread_t thread, const char *name) {
    (void)thread;
    (void)name;
}

#ifdef __cplusplus
}
#endif

#endif /* OOPS_POSIX_PTHREAD_NP_H */
