/*
 * `sched.h` - `sched_yield`, and the scheduling-policy names as queries that fail.
 *
 * tinycthread's `thrd_yield` calls `sched_yield`, which maps onto `oops_thread_yield`: the same
 * operation, and the only one here that does what its name says.
 *
 * **Nothing here sets a policy.** A payload does not choose its own scheduling, and a program told
 * it had succeeded would be relying on a priority it does not have. What the rest of this header
 * gives is the shape the names have, so that a third-party source can be compiled, and an answer
 * that says the policy is not available:
 *
 *   - `sched_getscheduler` returns -1 with ENOSYS. There is no policy to report.
 *   - `sched_get_priority_min` and `sched_get_priority_max` both return 0, which is a one-point
 *     range: a caller that scales a priority across it lands on the only value there is.
 *   - `struct sched_param` exists so it can be declared. `pthread_attr_setschedparam` in
 *     `<pthread.h>` fails, so filling one in reaches nothing.
 *
 * This was the absent half of the header until miniaudio, whose `ma_thread_create` sets a thread
 * priority through all of the above inside `if (... == 0)` and says in its own comment that failure
 * is not critical. That code is in somebody else's header, where a build error cannot be acted on,
 * which is the one case where an honest failure beats a missing declaration.
 */
#ifndef OOPS_POSIX_SCHED_H
#define OOPS_POSIX_SCHED_H

#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Always 0. See `posix.c`. */
int sched_yield(void);

/* The policy names, at their FreeBSD values (`sys/sched.h`), so that a source comparing a number
 * against one of them means the same thing here as on the platform this target is built for. */
#define SCHED_FIFO 1
#define SCHED_OTHER 2
#define SCHED_RR 3
/* Linux's, which has no FreeBSD equivalent; miniaudio names it for a background thread. */
#define SCHED_IDLE 5

#ifndef _SCHED_PARAM_DECLARED
#define _SCHED_PARAM_DECLARED
struct sched_param {
    int sched_priority;
};
#endif

/* No policy to report, so this reports none rather than guessing at one. */
static __inline int sched_getscheduler(int pid) {
    (void)pid;
    errno = ENOSYS;
    return -1;
}

/* One point, so a caller scaling a priority across the range lands on the only value there is. */
static __inline int sched_get_priority_min(int policy) {
    (void)policy;
    return 0;
}
static __inline int sched_get_priority_max(int policy) {
    (void)policy;
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* OOPS_POSIX_SCHED_H */
