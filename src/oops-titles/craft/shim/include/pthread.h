/*
 * POSIX threads, as much of them as tinycthread asks for - over oops-sdk's own threads.
 *
 * # Why this exists at all
 *
 * Craft's threading goes through `deps/tinycthread`, which is C11 `<threads.h>` implemented on
 * top of either pthreads or the Win32 API. There is no third branch, so the POSIX one is what
 * runs here - and without a `<pthread.h>` on the include path, clang finds the **host's** glibc
 * header instead and the compile dies inside `bits/wordsize.h`. Three of Craft's sources fail
 * that way and none of them is about threads: `db.c` and `client.c` get there through
 * `tinycthread.h`.
 *
 * # Why it is a shim and not a port
 *
 * oops-sdk already has threads, mutexes and condition variables (`oops/thread.h`), and they are
 * the same three objects with the same lifetimes. What pthreads adds on top is mostly *attribute
 * objects* - detach state, stack size, scheduling policy - and tinycthread uses almost none of
 * it. So the mapping is one call to one call, and the interesting part is the handful of places
 * where it is not:
 *
 *   - **`pthread_create` takes its attribute argument and ignores it.** tinycthread always
 *     passes NULL. A non-NULL one would silently not be honoured, so the parameter is named
 *     `attr` and documented rather than quietly dropped.
 *   - **`pthread_t` is a pointer, not an integer.** oops-sdk's `oops_thread_t` is `void *`, and
 *     `pthread_equal` exists precisely because POSIX does not promise the type is comparable
 *     with `==`. tinycthread calls it, so it is here and it forwards.
 *   - **`pthread_mutex_t` carries oops-sdk's mutex by value**, so a `PTHREAD_MUTEX_INITIALIZER`
 *     would be a lie - oops-sdk's mutexes need `oops_mutex_init`. tinycthread always calls
 *     `pthread_mutex_init`, so the static initialiser is deliberately **not** defined: a file
 *     that used one would fail to compile here rather than run with an uninitialised lock.
 *
 * # Thread-local storage, which is declared and not defined
 *
 * tinycthread typedefs `tss_t` from `pthread_key_t` **unconditionally**, so the type has to
 * exist or nothing including `tinycthread.h` compiles - which is `db.c` and `main.c`, neither of
 * which is about threads. The four functions are therefore *declared* here and deliberately not
 * defined: Craft calls none of them, so nothing links against them, and a future caller gets an
 * undefined symbol **naming the function** rather than a mystery inside a system header. That
 * is the failure this whole file exists to produce.
 *
 * Cancellation, read-write locks, barriers and spinlocks are absent entirely; tinycthread does
 * not reference them.
 */
#ifndef OOPS_CRAFT_PTHREAD_H
#define OOPS_CRAFT_PTHREAD_H

#include "oops/thread.h"
#include "oops/time.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef oops_thread_t pthread_t;
typedef oops_mutex_t pthread_mutex_t;
typedef oops_cond_t pthread_cond_t;

/* Attribute objects exist so that a caller can pass NULL, which is all tinycthread does. */
typedef struct { int unused; } pthread_attr_t;
typedef struct { int unused; } pthread_mutexattr_t;
typedef struct { int unused; } pthread_condattr_t;

/* The one attribute constant tinycthread names, for `pthread_attr_setdetachstate`. */
#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1

/* ---------------------------------------------------------------------------
 * Threads
 * ------------------------------------------------------------------------- */

/* oops-sdk's thread defaults, which pthreads has no way to express and tinycthread never asks
 * about. The priority is the platform's middle; the stack is 256 KiB, which is what Craft's
 * worker threads need - they build chunk meshes into heap buffers and recurse nowhere. */
#define OOPS_CRAFT_THREAD_PRIORITY 700
#define OOPS_CRAFT_THREAD_STACK    (256u * 1024u)

/* `attr` is accepted and ignored; tinycthread passes NULL. A name is required by oops-sdk and
 * every thread this creates is one of Craft's, so they share one. */
static inline int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                                 void *(*start_routine)(void *), void *arg) {
    (void)attr;
    const oops_thread_t t = oops_thread_create("craft", start_routine, arg,
                                               OOPS_CRAFT_THREAD_PRIORITY,
                                               OOPS_CRAFT_THREAD_STACK);
    if (!t) return -1;
    if (thread) *thread = t;
    return 0;
}

static inline int pthread_join(pthread_t thread, void **retval) {
    return oops_thread_join(thread, retval);
}

static inline int pthread_detach(pthread_t thread) { return oops_thread_detach(thread); }

static inline pthread_t pthread_self(void) { return oops_thread_self(); }

static inline int pthread_equal(pthread_t a, pthread_t b) { return oops_thread_equal(a, b); }

/* POSIX's `pthread_exit` does not return. oops-sdk has no such call, so this returns from the
 * thread's entry the ordinary way - which is what tinycthread's `thrd_exit` wants anyway. */
static inline void pthread_yield(void) { oops_thread_yield(); }

/* ---------------------------------------------------------------------------
 * Thread-local storage: the type and the declarations, no definitions.
 * See the note at the top of the file.
 * ------------------------------------------------------------------------- */

typedef unsigned int pthread_key_t;

int pthread_key_create(pthread_key_t *key, void (*destructor)(void *));
int pthread_key_delete(pthread_key_t key);
void *pthread_getspecific(pthread_key_t key);
int pthread_setspecific(pthread_key_t key, const void *value);

/* ---------------------------------------------------------------------------
 * Mutexes
 * ------------------------------------------------------------------------- */

static inline int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *attr) {
    (void)attr;
    return oops_mutex_init(m, "craft");
}
static inline int pthread_mutex_destroy(pthread_mutex_t *m) { return oops_mutex_destroy(m); }
static inline int pthread_mutex_lock(pthread_mutex_t *m) { return oops_mutex_lock(m); }
static inline int pthread_mutex_trylock(pthread_mutex_t *m) { return oops_mutex_trylock(m); }
static inline int pthread_mutex_unlock(pthread_mutex_t *m) { return oops_mutex_unlock(m); }

/* ---------------------------------------------------------------------------
 * Condition variables
 * ------------------------------------------------------------------------- */

static inline int pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *attr) {
    (void)attr;
    return oops_cond_init(c, "craft");
}
static inline int pthread_cond_destroy(pthread_cond_t *c) { return oops_cond_destroy(c); }
static inline int pthread_cond_signal(pthread_cond_t *c) { return oops_cond_signal(c); }
static inline int pthread_cond_broadcast(pthread_cond_t *c) { return oops_cond_broadcast(c); }
static inline int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m) {
    return oops_cond_wait(c, m);
}

/*
 * **An absolute deadline becomes a relative timeout**, and this is the one conversion in the
 * file that can be wrong in a way nothing catches. Three things about it:
 *
 *   - POSIX's `abstime` is a wall-clock *instant*; `oops_cond_timedwait` takes **microseconds
 *     from now**. Not milliseconds - the parameter is `timeout_us`, and reading it as ms makes
 *     every timed wait a thousand times too short, which on a loading thread looks like a busy
 *     loop and not like a bug.
 *   - "Now" comes from `oops_time_get_ns` rather than `clock_gettime`, which this target does
 *     not have. That makes the comparison monotonic-against-wall-clock, which is wrong in
 *     principle; it is what there is, and tinycthread only uses this to bound a wait that a
 *     signal normally ends first.
 *   - A deadline already past gives a negative difference, clamped to zero rather than allowed
 *     to wrap into a very long wait.
 *
 * `struct timespec` is whatever is in scope at the point of inclusion - tinycthread emulates its
 * own when the C library has no `TIME_UTC`, and includes this header before doing so, so the
 * two agree on layout by both being `{ seconds, nanoseconds }`.
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

#define pthread_cond_timedwait(c, m, abstime) \
    pthread_cond_timedwait_us((c), (m), (long long)(abstime)->tv_sec, \
                              (long long)(abstime)->tv_nsec)

#ifdef __cplusplus
}
#endif

#endif /* OOPS_CRAFT_PTHREAD_H */
