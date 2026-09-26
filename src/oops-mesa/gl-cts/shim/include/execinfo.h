/*
 * `<execinfo.h>` for a platform that has no `backtrace()`. dEQP's `qpCrashHandler.c`
 * includes it, and `tcu::App` always creates the crash handler.
 *
 * `backtrace` captures no frames. The kernel already logs a register dump and backtrace
 * on a fatal signal, and walking the stack with libunwind from inside the signal
 * handler, with its allocator and locks in an unknown state, risks a hang.
 */
#ifndef OOPS_GL_CTS_EXECINFO_H
#define OOPS_GL_CTS_EXECINFO_H

#ifdef __cplusplus
extern "C" {
#endif

/* Always 0: no frames captured. */
static inline int backtrace(void **buffer, int size) {
    (void)buffer;
    (void)size;
    return 0;
}

/* Null rather than an empty allocation; callers check before `free()`. */
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
