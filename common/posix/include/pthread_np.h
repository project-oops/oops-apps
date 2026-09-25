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
 */
#ifndef OOPS_POSIX_PTHREAD_NP_H
#define OOPS_POSIX_PTHREAD_NP_H

#ifdef __cplusplus
extern "C" {
#endif

int pthread_getthreadid_np(void);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_POSIX_PTHREAD_NP_H */
