/*
 * The system's name, as `uname` reports it.
 *
 * Only what is known without asking the kernel: the system is FreeBSD-derived and the machine is
 * `amd64`, which is what `-target x86_64-unknown-freebsd` already asserts. The release and
 * version are empty rather than guessed - this layer's `sysctl` answers nothing, so there is no
 * measured value to give - and the node name is `localhost`. Luanti puts these in its user-agent
 * string; nothing reads them as a capability test.
 */
#ifndef OOPS_POSIX_SYS_UTSNAME_H
#define OOPS_POSIX_SYS_UTSNAME_H

#define SYS_NMLN 256

struct utsname {
    char sysname[SYS_NMLN];
    char nodename[SYS_NMLN];
    char release[SYS_NMLN];
    char version[SYS_NMLN];
    char machine[SYS_NMLN];
};

#ifdef __cplusplus
extern "C" {
#endif
int uname(struct utsname *name);
#ifdef __cplusplus
}
#endif

#endif
