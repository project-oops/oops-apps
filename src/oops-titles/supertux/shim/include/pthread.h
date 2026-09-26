/*
 * `<pthread.h>` for PhysFS: the thread identity and mutex calls its POSIX platform
 * layer makes, made real over `<oops/thread.h>`.
 *
 * **Why the title has one when oops-sdk's libc already does.** The SDK's `<pthread.h>`
 * is single-threaded stubs - every lock succeeds without locking - and has no
 * `pthread_t` or `pthread_self`, both of which `physfs_platform_posix.c` needs for its
 * recursive mutex. PhysFS guards its whole file table with that mutex, and has no
 * switch to build without threads. A stub lock is a race the day a second thread opens
 * a file, so this is the real thing.
 *
 * **It answers PhysFS and nothing else.** Only the title's C objects put `shim/include`
 * ahead of the SDK's libc; the C++ side reaches the SDK's header, and C++ threads go
 * through libc++'s external-threading layer, which is already over `oops_mutex`. The
 * include guard is the SDK header's own, so whichever is found first is the only one
 * that counts.
 *
 * The better home for this is oops-sdk's `<pthread.h>` itself; it lives here until that
 * header grows real locks.
 */
#ifndef OOPS_LIBC_PTHREAD_H
#define OOPS_LIBC_PTHREAD_H

#include <oops/thread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef oops_thread_t pthread_t;
typedef oops_mutex_t pthread_mutex_t;
typedef int pthread_mutexattr_t;

static inline pthread_t pthread_self(void) {
    return oops_thread_self();
}
static inline int pthread_equal(pthread_t a, pthread_t b) {
    return oops_thread_equal(a, b);
}

/* A plain (non-recursive) mutex, which is what PhysFS asks for - it counts recursion
 * itself, against the owner it records with `pthread_self`. */
static inline int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a) {
    (void)a;
    return oops_mutex_init(m, "physfs") == 0 ? 0 : -1;
}
static inline int pthread_mutex_destroy(pthread_mutex_t *m) {
    return oops_mutex_destroy(m);
}
static inline int pthread_mutex_lock(pthread_mutex_t *m) {
    return oops_mutex_lock(m);
}
static inline int pthread_mutex_trylock(pthread_mutex_t *m) {
    return oops_mutex_trylock(m);
}
static inline int pthread_mutex_unlock(pthread_mutex_t *m) {
    return oops_mutex_unlock(m);
}

#ifdef __cplusplus
}
#endif

#endif /* OOPS_LIBC_PTHREAD_H */
