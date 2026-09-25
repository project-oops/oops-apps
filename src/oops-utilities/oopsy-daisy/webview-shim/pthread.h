/*
 * Freestanding <pthread.h> for the webview build's C side (QuickJS, CONFIG_ATOMICS path).
 *
 * QuickJS guards one process-wide mutex (the class-id allocator) behind CONFIG_ATOMICS. The payload
 * is single-threaded, so these are no-ops. If a build ever enables JS Atomics.wait (which needs real
 * threads) the link will name the missing pthread_create etc.; it does not today. Part of the
 * webview freestanding-libc workaround (REQ-20260925T0110Z-e1d9).
 */
#ifndef OOPSY_SHIM_PTHREAD_H
#define OOPSY_SHIM_PTHREAD_H

typedef int pthread_mutex_t;
typedef int pthread_mutexattr_t;
typedef int pthread_cond_t;
typedef int pthread_condattr_t;

#define PTHREAD_MUTEX_INITIALIZER 0
#define PTHREAD_COND_INITIALIZER  0

static inline int pthread_mutex_init(pthread_mutex_t *__m, const pthread_mutexattr_t *__a) {
    (void)__m; (void)__a; return 0;
}
static inline int pthread_mutex_destroy(pthread_mutex_t *__m) { (void)__m; return 0; }
static inline int pthread_mutex_lock(pthread_mutex_t *__m)    { (void)__m; return 0; }
static inline int pthread_mutex_unlock(pthread_mutex_t *__m)  { (void)__m; return 0; }

static inline int pthread_cond_init(pthread_cond_t *__c, const pthread_condattr_t *__a) {
    (void)__c; (void)__a; return 0;
}
static inline int pthread_cond_destroy(pthread_cond_t *__c)   { (void)__c; return 0; }
static inline int pthread_cond_signal(pthread_cond_t *__c)    { (void)__c; return 0; }
static inline int pthread_cond_broadcast(pthread_cond_t *__c) { (void)__c; return 0; }
static inline int pthread_cond_wait(pthread_cond_t *__c, pthread_mutex_t *__m) {
    (void)__c; (void)__m; return 0;
}
struct timespec;
static inline int pthread_cond_timedwait(pthread_cond_t *__c, pthread_mutex_t *__m,
                                         const struct timespec *__t) {
    (void)__c; (void)__m; (void)__t; return 0;
}

#endif /* OOPSY_SHIM_PTHREAD_H */
