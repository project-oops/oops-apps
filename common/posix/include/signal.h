/*
 * `signal.h` - the numbers and `raise`, which is all anything here has asked for.
 *
 * libtomcrypt's `tomcrypt_argchk.h` includes it to `raise(SIGABRT)` when an argument check fails.
 * That is the only use in the tree, and it is a fatal-error path rather than signal handling.
 *
 * **`raise` ends the process; it does not deliver a signal.** There is no signal delivery on this
 * platform - a payload is not a POSIX process with handlers - so a faithful `raise` is not
 * something this shim can offer. What it can do is honour the one contract that matters for the
 * caller above: `raise(SIGABRT)` must not return. It goes to `abort`, which is what `SIGABRT`
 * means anyway.
 *
 * `signal()` itself is deliberately absent. A program registering a handler would get one that
 * never fires, and silently never firing is the failure mode this collection keeps writing
 * comments about. A port that genuinely needs asynchronous delivery wants
 * `oops_thread_install_exception_handler`, which is real.
 *
 * The numbers are FreeBSD's, because that is the kernel underneath and a program comparing
 * against them should see the platform's values.
 */
#ifndef OOPS_POSIX_SIGNAL_H
#define OOPS_POSIX_SIGNAL_H

#include <stdlib.h>

#define SIGHUP   1
#define SIGINT   2
#define SIGQUIT  3
#define SIGILL   4
#define SIGABRT  6
#define SIGFPE   8
#define SIGKILL  9
#define SIGSEGV 11
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15

/*
 * **`signal()` is here now, and it is real for the signals that can be real.**
 *
 * An earlier version of this header left it out on the reasoning that a handler which never fires
 * is worse than a missing function. That reasoning still holds and the conclusion was wrong:
 * `oops_thread_install_exception_handler` exists, so a handler for a *fault* - `SIGSEGV`, `SIGILL`,
 * `SIGFPE`, `SIGBUS` - does fire. ioquake3's `sys_main.c:864` installs exactly those, plus two that
 * cannot work.
 *
 * So the split is honest rather than uniform:
 *
 *   SIGSEGV, SIGILL, SIGFPE, SIGBUS, SIGABRT   installed through the SDK; they fire
 *   everything else (SIGINT, SIGTERM, SIGHUP, SIGPIPE, SIGALRM, ...)
 *                                              **`SIG_ERR`**, because nothing on this platform
 *                                              can deliver them - there is no shell to interrupt
 *                                              a payload and no pipe to break. A caller that
 *                                              checks the return is told the truth; one that
 *                                              ignores it, as ioquake3 does, is no worse off than
 *                                              on a system where the signal simply never arrives.
 *
 * `sigaction` is still absent. It carries flags, masks and a three-argument handler that this
 * platform has nothing to map onto, and a partial `sigaction` would be the silent kind of wrong.
 */
#define SIGBUS  10

typedef void (*sighandler_t)(int);

#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_ERR ((sighandler_t)-1)

#ifdef __cplusplus
extern "C" {
#endif

/* The previous handler, or `SIG_ERR` if this signal cannot be delivered here. See above. */
sighandler_t signal(int sig, sighandler_t handler);

/* Ends the process. Inline so that nothing has to link a definition for a header this thin, and
 * so the `noreturn` is visible to the caller's flow analysis. */
static inline int raise(int sig) {
    (void)sig;
    abort();
    return 0; /* not reached; abort does not return */
}

#ifdef __cplusplus
}
#endif

#endif /* OOPS_POSIX_SIGNAL_H */
