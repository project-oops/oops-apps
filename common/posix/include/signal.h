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

#ifdef __cplusplus
extern "C" {
#endif

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
