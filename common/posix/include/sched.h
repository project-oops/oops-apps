/*
 * `sched.h` - `sched_yield` only.
 *
 * tinycthread's `thrd_yield` calls it, and that is the only use in this tree. It maps onto
 * `oops_thread_yield`, which is the same operation.
 *
 * **The scheduling-policy half of this header is deliberately absent** - `sched_setscheduler`,
 * `sched_param`, `SCHED_FIFO` and the rest. A payload does not choose its own scheduling policy,
 * and a program that set one and was told it succeeded would be relying on a priority it does not
 * have. Leaving them undeclared makes that a build error where the caller can see it.
 */
#ifndef OOPS_POSIX_SCHED_H
#define OOPS_POSIX_SCHED_H

#ifdef __cplusplus
extern "C" {
#endif

/* Always 0. See `posix.c`. */
int sched_yield(void);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_POSIX_SCHED_H */
