/*
 * `signal.h` - the numbers, `raise`, a `signal` that is real for faults, and a `kill` that only
 * answers whether a process exists. Each of those three has its own note below.
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
#include <sys/types.h> /* pid_t, for the kill below */

#define SIGHUP   1
#define SIGINT   2
#define SIGQUIT  3
#define SIGILL   4
#define SIGTRAP  5
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
 * `sigaction` is `signal` with a structure around it, and refuses everything `signal` cannot
 * keep - see its own note below.
 */
#define SIGBUS  10

typedef void (*sighandler_t)(int);

/* The type a handler can write atomically; FreeBSD's on amd64. */
typedef long sig_atomic_t;

#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_ERR ((sighandler_t)-1)

#ifdef __cplusplus
extern "C" {
#endif

/* The previous handler, or `SIG_ERR` if this signal cannot be delivered here. See above. */
sighandler_t signal(int sig, sighandler_t handler);

/*
 * **`sigaction` does exactly what `signal` does, and refuses the rest out loud.**
 *
 * The structure carries more than this platform can honour: a three-argument handler
 * (`SA_SIGINFO`), a mask of signals blocked while the handler runs, and flags. The plain
 * handler maps onto `signal` above and is installed the same way - so a fault handler
 * fires. Everything else fails with `EINVAL` rather than being quietly dropped: a
 * `SA_SIGINFO` handler, and any signal `signal` itself cannot deliver. `sa_mask` is not
 * applied; the signals that can fire here are synchronous faults, raised by the thread
 * they interrupt, so there is nothing for a mask to hold off.
 *
 * `oact`, when given, receives the previous plain handler.
 *
 * Xash3D's `Posix_SetupSigtermHandling` asks for `SIGTERM`, which no one can send a payload,
 * and is told so.
 */
typedef unsigned long sigset_t;

struct sigaction {
    sighandler_t sa_handler;
    void (*sa_sigaction)(int, void *, void *);
    sigset_t sa_mask;
    int sa_flags;
};

#define SA_RESTART 0x0002
#define SA_SIGINFO 0x0040

int sigaction(int sig, const struct sigaction *act, struct sigaction *oact);
int sigemptyset(sigset_t *set);
int sigaddset(sigset_t *set, int sig);
int sigfillset(sigset_t *set);
int sigdelset(sigset_t *set, int sig);
int sigismember(const sigset_t *set, int sig);

/*
 * **A signal mask holds nothing off, and says so.** The signals that can fire here are
 * synchronous faults, raised by the thread they interrupt - blocking one would not defer it,
 * it would lose the fault - so there is no asynchronous delivery for a mask to apply to. Both
 * calls succeed, and the previous mask they report is the empty one, which is the truth.
 * LÖVE masks every signal around creating a thread (`modules/thread/threads.cpp`), so that no
 * asynchronous signal lands on it; there is none to land.
 */
#define SIG_BLOCK 1
#define SIG_UNBLOCK 2
#define SIG_SETMASK 3
int sigprocmask(int how, const sigset_t *set, sigset_t *oset);
int pthread_sigmask(int how, const sigset_t *set, sigset_t *oset);

/*
 * **`kill` is an existence test and nothing more.**
 *
 * `kill(pid, 0)` is POSIX's way to ask "is this process alive" without touching it, and that
 * question has an exact answer here: there is one process, so `pid` is alive if and only if it is
 * `getpid()`. This answers it, with `ESRCH` for anything else.
 *
 * Any non-zero `sig` fails with `ENOSYS`, because there is no process to send it to but this one and
 * no delivery mechanism if there were - `signal` above explains which handlers can fire and why
 * these are not among them.
 *
 * The caller that matters: ioquake3 keeps a lock file holding the pid of the running copy, and
 * `Sys_PIDIsRunning` calls `kill(pid, 0)` to decide whether a lock file left behind by a crash is
 * stale. Answering `ESRCH` for a pid that is not ours is what lets it take the lock over, which is
 * the right outcome every time on a machine that runs one payload.
 */
int kill(pid_t pid, int sig);

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
