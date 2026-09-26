/*
 * POSIX threads, as much of them as tinycthread uses, over oops-sdk's `oops/thread.h`.
 *
 * tinycthread implements C11 threads over pthreads or Win32, so the POSIX branch runs
 * here, and this header keeps clang from finding the host's glibc one. The mapping is
 * one call to one call. `pthread_create` ignores its attribute (tinycthread passes
 * NULL). `pthread_mutex_t` holds oops-sdk's mutex by value, which needs
 * `oops_mutex_init`, so `PTHREAD_MUTEX_INITIALIZER` is deliberately not defined.
 * Cancellation, read-write locks, barriers and spinlocks are absent; tinycthread does
 * not reference them.
 */
#ifndef OOPS_CRAFT_PTHREAD_H
#define OOPS_CRAFT_PTHREAD_H

#include "oops/thread.h"
#include "oops/time.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>  /* fprintf, for the pthread_exit message below */
#include <stdlib.h> /* abort, likewise */

#ifdef __cplusplus
extern "C" {
#endif

typedef oops_thread_t pthread_t;
typedef oops_mutex_t pthread_mutex_t;
typedef oops_cond_t pthread_cond_t;

/* Thread and condition attributes exist so that a caller can pass NULL, which is all
 * tinycthread does with those two. */
typedef struct {
    int unused;
} pthread_attr_t;
typedef struct {
    int unused;
} pthread_condattr_t;

/*
 * The mutex attribute carries its type, which tinycthread sets for `mtx_recursive`
 * (`tinycthread.c:64-71`); an ordinary mutex would deadlock on re-entry. The values
 * are FreeBSD's, where `PTHREAD_MUTEX_RECURSIVE` is 2 (glibc uses 1).
 */
#define PTHREAD_MUTEX_NORMAL 0
#define PTHREAD_MUTEX_ERRORCHECK 1
#define PTHREAD_MUTEX_RECURSIVE 2
#define PTHREAD_MUTEX_DEFAULT PTHREAD_MUTEX_NORMAL

typedef struct {
    int type;
} pthread_mutexattr_t;

/* The one attribute constant tinycthread names, for `pthread_attr_setdetachstate`. */
#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1

/* ---------------------------------------------------------------------------
 * Threads
 * ------------------------------------------------------------------------- */

/* oops-sdk thread parameters that pthreads does not express. The priority is the
 * platform's middle; 256 KiB suffices for Craft's workers, which build chunk meshes
 * into heap buffers and do not recurse. */
#define OOPS_CRAFT_THREAD_PRIORITY 700
#define OOPS_CRAFT_THREAD_STACK (256u * 1024u)

/* `attr` is accepted and ignored; tinycthread passes NULL. A name is required by
 * oops-sdk and every thread this creates is one of Craft's, so they share one. */
static inline int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                                 void *(*start_routine)(void *), void *arg) {
    (void)attr;
    const oops_thread_t t =
        oops_thread_create("craft", start_routine, arg, OOPS_CRAFT_THREAD_PRIORITY,
                           OOPS_CRAFT_THREAD_STACK);
    if (!t)
        return -1;
    if (thread)
        *thread = t;
    return 0;
}

static inline int pthread_join(pthread_t thread, void **retval) {
    return oops_thread_join(thread, retval);
}

static inline int pthread_detach(pthread_t thread) {
    return oops_thread_detach(thread);
}

static inline pthread_t pthread_self(void) {
    return oops_thread_self();
}

static inline int pthread_equal(pthread_t a, pthread_t b) {
    return oops_thread_equal(a, b);
}

static inline void pthread_yield(void) {
    oops_thread_yield();
}

/*
 * `pthread_exit` aborts the process with a message. oops-sdk threads end only by
 * returning from their entry function, so ending one thread from the middle is not
 * possible, and returning normally would continue the caller's code. Its only caller
 * is `thrd_exit`, which Craft does not call.
 */
static inline void pthread_exit(void *retval) {
    (void)retval;
    fprintf(stderr,
            "craft: pthread_exit - this target cannot end one thread; aborting\n");
    abort();
}

/* ---------------------------------------------------------------------------
 * Thread-local storage, over oops-sdk's `oops_tls_*`. tinycthread typedefs `tss_t`
 * from `pthread_key_t` and compiles `tss_create` and friends whether or not Craft
 * calls them, so these are defined.
 * ------------------------------------------------------------------------- */

typedef oops_tls_key_t pthread_key_t;

static inline int pthread_key_create(pthread_key_t *key, void (*destructor)(void *)) {
    return oops_tls_create(key, destructor);
}
static inline int pthread_key_delete(pthread_key_t key) {
    return oops_tls_delete(key);
}
static inline void *pthread_getspecific(pthread_key_t key) {
    return oops_tls_get(key);
}
static inline int pthread_setspecific(pthread_key_t key, const void *value) {
    return oops_tls_set(key, value);
}

/* ---------------------------------------------------------------------------
 * Mutexes
 * ------------------------------------------------------------------------- */

static inline int pthread_mutexattr_init(pthread_mutexattr_t *attr) {
    if (attr)
        attr->type = PTHREAD_MUTEX_DEFAULT;
    return 0;
}
static inline int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type) {
    if (!attr)
        return -1;
    attr->type = type;
    return 0;
}
static inline int pthread_mutexattr_destroy(pthread_mutexattr_t *attr) {
    (void)attr; /* it owns nothing */
    return 0;
}

/* Honours `PTHREAD_MUTEX_RECURSIVE`; a NULL attribute gives the default kind. */
static inline int pthread_mutex_init(pthread_mutex_t *m,
                                     const pthread_mutexattr_t *attr) {
    if (attr && attr->type == PTHREAD_MUTEX_RECURSIVE) {
        return oops_mutex_init_recursive(m, "craft");
    }
    return oops_mutex_init(m, "craft");
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

/* ---------------------------------------------------------------------------
 * Condition variables
 * ------------------------------------------------------------------------- */

static inline int pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *attr) {
    (void)attr;
    return oops_cond_init(c, "craft");
}
static inline int pthread_cond_destroy(pthread_cond_t *c) {
    return oops_cond_destroy(c);
}
static inline int pthread_cond_signal(pthread_cond_t *c) {
    return oops_cond_signal(c);
}
static inline int pthread_cond_broadcast(pthread_cond_t *c) {
    return oops_cond_broadcast(c);
}
static inline int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m) {
    return oops_cond_wait(c, m);
}

/*
 * POSIX's absolute `abstime` becomes `oops_cond_timedwait`'s microseconds from now.
 * "Now" is `oops_time_get_ns`, since the target has no `clock_gettime`; a past deadline
 * clamps to zero. `struct timespec` is whichever `{ seconds, nanoseconds }` definition
 * is in scope, tinycthread's own when the C library has no `TIME_UTC`.
 */
struct timespec;

static inline int pthread_cond_timedwait_us(pthread_cond_t *c, pthread_mutex_t *m,
                                            long long abs_sec, long long abs_nsec) {
    const unsigned long long now_ns = oops_time_get_ns();
    const long long want_ns = abs_sec * 1000000000LL + abs_nsec;
    const long long delta_ns = want_ns - (long long)now_ns;
    const long long us = delta_ns > 0 ? delta_ns / 1000LL : 0LL;
    return oops_cond_timedwait(c, m, us > 0xffffffffLL ? 0xffffffffu : (uint32_t)us);
}

#define pthread_cond_timedwait(c, m, abstime)                                          \
    pthread_cond_timedwait_us((c), (m), (long long)(abstime)->tv_sec,                  \
                              (long long)(abstime)->tv_nsec)

#ifdef __cplusplus
}
#endif

#endif /* OOPS_CRAFT_PTHREAD_H */
