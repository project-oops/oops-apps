/*
 * `sys/uio.h` - `struct iovec`, one buffer of a scatter-gather list, FreeBSD's layout. The
 * socket calls that take a list (`sendmsg`, `recvmsg`) are in `sys/socket.h`.
 */
#ifndef OOPS_POSIX_SYS_UIO_H
#define OOPS_POSIX_SYS_UIO_H

#include <stddef.h>

struct iovec {
    void *iov_base;
    size_t iov_len;
};

#endif /* OOPS_POSIX_SYS_UIO_H */
