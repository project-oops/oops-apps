/*
 * `dlfcn.h` - `dladdr` only, and it always fails.
 *
 * # What this is for
 *
 * A program that wants to name a function in a log message calls `dladdr` to ask the dynamic
 * linker which symbol an address belongs to. libultraship's `FileDropMgr.cpp:55` is the one
 * caller here, and it is a warning message.
 *
 * # Why it fails rather than answering
 *
 * A payload has no symbol table at run time to look an address up in - after `make title` the
 * module is a Sony module whose dynlibdata is not mapped - so there is nothing this could read.
 * Returning 0 is what the platform actually offers, and every caller of `dladdr` already has a
 * path for it, because it fails on a static binary everywhere else too.
 *
 * `dlopen`/`dlsym`/`dlclose` are deliberately absent. There is no loading of a second module into
 * a payload, and a program reaching for them wants a design conversation rather than a stub.
 *
 * # `info` is zeroed even on failure, and that is not tidiness
 *
 * POSIX says nothing about `*info` when `dladdr` returns 0, so a caller that reads it first is
 * wrong - and the caller here does exactly that: `FileDropMgr.cpp:56` takes `info.dli_sname`
 * before testing the return value. With an uninitialised struct that is a garbage pointer handed
 * to a log formatter. Zeroing costs four stores and turns it into a null, which the line after
 * already checks for.
 */
#ifndef OOPS_POSIX_DLFCN_H
#define OOPS_POSIX_DLFCN_H

typedef struct {
    const char *dli_fname; /* always NULL here */
    void       *dli_fbase;
    const char *dli_sname;
    void       *dli_saddr;
} Dl_info;

#ifdef __cplusplus
extern "C" {
#endif

/* Always 0, with `*info` zeroed. See above. */
int dladdr(const void *addr, Dl_info *info);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_POSIX_DLFCN_H */
