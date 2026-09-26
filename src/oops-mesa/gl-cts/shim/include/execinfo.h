/*
 * `<execinfo.h>` for a platform that has no `backtrace()`.
 *
 * dEQP's `qpCrashHandler.c` includes this to print a stack trace when a test signals.
 * It is not an optional file: `tcu::App`'s constructor calls `qpCrashHandler_create`
 * unconditionally, so leaving the translation unit out of the build leaves three
 * undefined symbols at the link.
 *
 * # Why `backtrace` returns nothing rather than walking the stack
 *
 * The title *does* carry an unwinder - `oops-deps/libcxx` builds libunwind, and
 * `oops-apps#D006` records it working on hardware. So a real `backtrace()` over
 * `_Unwind_Backtrace` is possible and would be a genuine improvement.
 *
 * It is not what this is, and the distinction is worth stating rather than leaving to
 * be discovered from a silent zero. A crash handler runs *after* a fatal signal, on a
 * platform where the kernel has already printed its own register dump and backtrace to
 * the system log
 * (`oops-apps#D006` reads one). Walking the stack a second time from inside a signal
 * handler, with libunwind's allocator and locks in whatever state the fault left them,
 * is a good way to turn a diagnosable crash into a hang - and the information is
 * already in the log.
 *
 * So: zero frames, and `qpCrashHandler` prints its message without a trace. The
 * platform's own report is the better one and it is already being written.
 *
 * **If this is ever revisited**, `_Unwind_Backtrace` is the entry point and the thing
 * to check first is whether it is safe to call from the signal handler dEQP installs.
 */
#ifndef OOPS_GL_CTS_EXECINFO_H
#define OOPS_GL_CTS_EXECINFO_H

#ifdef __cplusplus
extern "C" {
#endif

/* Always 0: no frames captured. Callers loop over the count, so zero is handled
 * everywhere. */
static inline int backtrace(void **buffer, int size) {
    (void)buffer;
    (void)size;
    return 0;
}

/* Null rather than an empty allocation: callers `free()` the result and check it first,
   and a zero-length array they must remember to free is the worse of the two contracts.
 */
static inline char **backtrace_symbols(void *const *buffer, int size) {
    (void)buffer;
    (void)size;
    return (char **)0;
}

static inline void backtrace_symbols_fd(void *const *buffer, int size, int fd) {
    (void)buffer;
    (void)size;
    (void)fd;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* OOPS_GL_CTS_EXECINFO_H */
