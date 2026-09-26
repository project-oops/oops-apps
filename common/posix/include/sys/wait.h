/*
 * `sys/wait.h` - the status macros, and a `waitpid` that always fails.
 *
 * ioquake3's `sys_unix.c` forks an external process (to open a URL, or to show a dialog) and waits
 * for it. There is no `fork` on this platform and no second process to wait for, so `waitpid`
 * answers -1 with `ECHILD`: "no child to wait for", which is exactly true.
 *
 * **The status macros are defined and they are correct**, because a caller that gets -1 may still
 * pass its uninitialised `status` through `WEXITSTATUS` before checking the return - `sys_unix.c`
 * is close to doing that. Correct macros over a garbage value produce a garbage number rather than
 * a crash; absent macros would not compile at all. Neither is a reason to leave them out.
 *
 * `fork` itself is deliberately not declared anywhere in this shim. A port that calls it should fail
 * to compile with `fork` in the error, rather than link against something that pretends.
 */
#ifndef OOPS_POSIX_SYS_WAIT_H
#define OOPS_POSIX_SYS_WAIT_H

#include <sys/types.h>

#define WNOHANG   1
#define WUNTRACED 2

/* FreeBSD's encoding: the low byte is the terminating signal, the next the exit status. */
#define _W_INT(w)      (*(const int *)&(w))
#define WEXITSTATUS(x) (((x) >> 8) & 0xff)
#define WTERMSIG(x)    ((x) & 0x7f)
#define WSTOPSIG(x)    (((x) >> 8) & 0xff)
#define WIFEXITED(x)   (WTERMSIG(x) == 0)
#define WIFSIGNALED(x) (WTERMSIG(x) != 0 && WTERMSIG(x) != 0x7f)
#define WIFSTOPPED(x)  (((x) & 0xff) == 0x7f)

#ifdef __cplusplus
extern "C" {
#endif

/* Always -1 with `errno = ECHILD`: there are no child processes here. */
pid_t waitpid(pid_t pid, int *status, int options);
/* Likewise. */
pid_t wait(int *status);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_POSIX_SYS_WAIT_H */
