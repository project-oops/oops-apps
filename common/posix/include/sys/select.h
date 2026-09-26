/*
 * `select`, and it is a real one.
 *
 * # Why this is not a stub
 *
 * ioquake3's `NET_Sleep` is the caller, and it does not use `select` to sleep - it uses it to find
 * out which sockets have a packet waiting, then hands the result set to `NET_GetPacket`, which
 * reads a socket only if `FD_ISSET` says so. A `select` that reported "nothing ready" would
 * therefore not slow the game down; it would mean **the game never receives a packet**, with no
 * error anywhere. That is the failure this collection keeps writing comments about, so this one
 * does the work.
 *
 * # How it works, and what that costs
 *
 * The SDK has no readiness call - no `select`, no `poll`, no `kqueue`. What it has is a
 * non-destructive read: `oops_recvfrom` with `OOPS_MSG_PEEK | OOPS_MSG_DONTWAIT`, both of which
 * `oops/net.h` records as measured on this console. So readiness is asked one socket at a time, by
 * peeking at a single byte and putting it back, and a timeout is waited out by peeking again every
 * millisecond.
 *
 * The consequences are worth stating rather than discovering:
 *
 *   - **Resolution is a millisecond, not a microsecond.** A `timeval` asking for 200us waits up to
 *     1ms. Nothing here polls that finely; `NET_Sleep` passes whatever is left of a frame.
 *   - **It costs one syscall per socket per millisecond of waiting.** ioquake3 watches at most
 *     three, so a 10ms wait is about 30 peeks. That is the price of not having a readiness call;
 *     it is small here and would not be for a program watching a hundred descriptors.
 *   - **Only the read set is answered.** `writefds` and `exceptfds` are refused outright if
 *     anything is set in them (see below) rather than reported ready, because peeking cannot tell
 *     whether a socket would accept a write, and "always writable" is the kind of guess that turns
 *     into a busy loop in somebody else's event loop. Both callers in this tree pass NULL.
 *   - **A descriptor that is not a socket reads as ready.** The probe cannot distinguish "this is
 *     a file, not a socket" from "this socket has an error pending", and select's own answer to a
 *     pending error is to report the descriptor readable so the caller's `read` reports it. So a
 *     file descriptor in the read set comes back set, immediately. ioquake3 puts `STDIN_FILENO`
 *     in one, on a branch guarded by `isatty`, which is 0 here - so that branch never runs.
 */
#ifndef OOPS_POSIX_SYS_SELECT_H
#define OOPS_POSIX_SYS_SELECT_H

#include <stdint.h>
#include <string.h>   /* memset, for FD_ZERO */
#include <sys/time.h> /* struct timeval */

/*
 * **1024, which is FreeBSD's, and the descriptors here fit inside it.** `oops_socket` returns a
 * kernel file descriptor straight from `SYS_socket` (`oops-sdk/src/net/net.c:361`), so they are
 * small integers counting up from a handful - not the large opaque handles some platforms hand
 * back, which would not fit a bitmask at all.
 */
#define FD_SETSIZE 1024

typedef uint64_t __fd_mask;
#define __NFDBITS ((int)(sizeof(__fd_mask) * 8))

typedef struct fd_set {
    __fd_mask __fds_bits[(FD_SETSIZE + __NFDBITS - 1) / __NFDBITS];
} fd_set;

/*
 * **The macros bounds-check, which the real ones do not.** POSIX says passing a descriptor at or
 * above `FD_SETSIZE` is undefined, and on a desktop that means a write past the end of the caller's
 * `fd_set` - four bytes of somebody else's stack. Here it is silently ignored instead: a dropped
 * descriptor makes a socket look permanently idle, which is bad, and corrupting the stack frame of
 * whatever called `NET_Sleep` is worse and far harder to find. The condition cannot arise with the
 * descriptors this platform produces; the guard is for the day it can.
 */
#define FD_ZERO(p)     memset((void *)(p), 0, sizeof(fd_set))
#define FD_SET(n, p)                                                           \
    do {                                                                       \
        int __n = (int)(n);                                                    \
        if (__n >= 0 && __n < FD_SETSIZE)                                      \
            (p)->__fds_bits[__n / __NFDBITS] |= ((__fd_mask)1                  \
                                                 << (__n % __NFDBITS));        \
    } while (0)
#define FD_CLR(n, p)                                                           \
    do {                                                                       \
        int __n = (int)(n);                                                    \
        if (__n >= 0 && __n < FD_SETSIZE)                                      \
            (p)->__fds_bits[__n / __NFDBITS] &= ~((__fd_mask)1                 \
                                                  << (__n % __NFDBITS));       \
    } while (0)
#define FD_ISSET(n, p)                                                         \
    (((int)(n) >= 0 && (int)(n) < FD_SETSIZE)                                  \
         ? (((p)->__fds_bits[(int)(n) / __NFDBITS] &                           \
             ((__fd_mask)1 << ((int)(n) % __NFDBITS))) != 0)                    \
         : 0)
#define FD_COPY(f, t) (*(t) = *(f))

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The number of ready descriptors, 0 on timeout, or -1 with `errno`.
 *
 * `EINVAL` for a negative `nfds`, and **also for a non-empty `writefds` or `exceptfds`** - see the
 * header comment. A non-NULL but empty set is accepted and left empty, because that is what a
 * caller zeroing three sets and filling one means.
 *
 * `timeout` NULL waits indefinitely, as POSIX says. `select(0, NULL, NULL, NULL, &tv)` sleeps,
 * which is a use the interface has always had and this honours.
 */
int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
           struct timeval *timeout);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_POSIX_SYS_SELECT_H */
