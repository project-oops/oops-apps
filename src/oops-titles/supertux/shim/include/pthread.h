/*
 * `<pthread.h>` for PhysFS: the thread identity and mutex calls its POSIX platform
 * layer makes, made real over `<oops/thread.h>`.
 *
 * oops-sdk's `<pthread.h>` has stub locks and no `pthread_t` or `pthread_self`, which
 * `physfs_platform_posix.c` needs for the mutex guarding its file table.
 *
 * Only the title's C objects put `shim/include` ahead of the SDK's libc; C++ reaches
 * the SDK's header and uses libc++'s threading over `oops_mutex`. The include guard is
 * the SDK header's own, so whichever is found first is the one used.
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
