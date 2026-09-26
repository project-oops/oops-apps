/*
 * `sys/ioctl.h` - `FIONBIO` only, and it is real.
 *
 * ioquake3's `net_ip.c` uses `ioctl(sock, FIONBIO, &one)` to put a socket into non-blocking mode,
 * at four call sites, and nothing else in this tree uses `ioctl` at all.
 *
 * That one request maps exactly onto `oops_set_nonblocking` in `<oops/net.h>`, so this is a real
 * implementation rather than a stub: the socket really does become non-blocking, which matters,
 * because a networking loop that believes it is non-blocking and is not will hang the frame.
 *
 * **Every other request fails with `EINVAL`.** `ioctl` is an open-ended interface - hundreds of
 * requests across terminals, disks and interfaces - and there is no honest general implementation
 * behind it here. A caller asking for something else is told no, which is the truthful answer and
 * distinguishable from success.
 *
 * `FIONREAD` in particular is *not* implemented. It is the other request a networking program
 * commonly wants, and answering 0 ("no data") would be indistinguishable from a real empty socket
 * while being a guess - so it is left to fail rather than to mislead.
 */
#ifndef OOPS_POSIX_SYS_IOCTL_H
#define OOPS_POSIX_SYS_IOCTL_H

/* FreeBSD's values, because that is the kernel underneath. */
#define FIONBIO  0x8004667eu
#define FIONREAD 0x4004667fu
#define FIOASYNC 0x8004667du

#ifdef __cplusplus
extern "C" {
#endif

/* 0 on success, -1 with `errno`. Only `FIONBIO` is honoured - see above. */
int ioctl(int fd, unsigned long request, ...);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_POSIX_SYS_IOCTL_H */
