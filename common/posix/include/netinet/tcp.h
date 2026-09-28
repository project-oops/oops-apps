/*
 * `netinet/tcp.h` - TCP socket options, FreeBSD's values. They reach the kernel unchanged
 * through `setsockopt` (`sys/socket.h`), so a caller turning Nagle off gets exactly that.
 * luasocket and ENet include it for `TCP_NODELAY`.
 */
#ifndef OOPS_POSIX_NETINET_TCP_H
#define OOPS_POSIX_NETINET_TCP_H

#define TCP_NODELAY   1
#define TCP_MAXSEG    2
#define TCP_NOPUSH    4
#define TCP_KEEPIDLE  256
#define TCP_KEEPINTVL 512
#define TCP_KEEPCNT   1024

#endif /* OOPS_POSIX_NETINET_TCP_H */
