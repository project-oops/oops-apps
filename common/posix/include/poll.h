/*
 * `poll.h` - `poll`, over the same readiness check `select` uses (`sys/select.h` says how it
 * works and what it costs). FreeBSD's values.
 *
 * As with `select`, only readability can be asked: the SDK has no way to learn whether a
 * send would block, so a descriptor asking for `POLLOUT` (or any event but `POLLIN` and
 * `POLLRDNORM`) fails the call with `EINVAL` rather than being answered with a guess. Luanti
 * polls one UDP socket for `POLLIN` (`src/network/socket.cpp`).
 */
#ifndef OOPS_POSIX_POLL_H
#define OOPS_POSIX_POLL_H

#define POLLIN     0x0001
#define POLLPRI    0x0002
#define POLLOUT    0x0004
#define POLLRDNORM 0x0040
#define POLLWRNORM POLLOUT
#define POLLRDBAND 0x0080
#define POLLWRBAND 0x0100
#define POLLERR    0x0008
#define POLLHUP    0x0010
#define POLLNVAL   0x0020

typedef unsigned int nfds_t;

struct pollfd {
    int fd;
    short events;
    short revents;
};

#ifdef __cplusplus
extern "C" {
#endif

/* Descriptors ready to read, 0 on timeout, or -1. `timeout_ms` below zero waits forever. */
int poll(struct pollfd *fds, nfds_t nfds, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_POSIX_POLL_H */
