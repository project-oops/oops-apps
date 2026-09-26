/*
 * `libgen.h` - `basename` and `dirname`.
 *
 * ioquake3's `sys_unix.c` uses both to find its own directory from argv[0].
 *
 * **These are the non-modifying variants, which is FreeBSD's behaviour and not glibc's.** POSIX
 * permits `basename` to modify the string it is given, and glibc's does; FreeBSD's has returned a
 * pointer to internal storage since 12. Since this target is FreeBSD, that is the contract to
 * match - and it is also the safer of the two, because a caller passing a string literal or a
 * shared buffer does not get it rewritten underneath them.
 *
 * The cost is the usual one for this shape of interface: the result lives in static storage and the
 * next call overwrites it, so a caller holding both a `basename` and a `dirname` result at once
 * must copy one. `sys_unix.c` does not.
 */
#ifndef OOPS_POSIX_LIBGEN_H
#define OOPS_POSIX_LIBGEN_H

#ifdef __cplusplus
extern "C" {
#endif

/* The last path component. "/" for "/", "." for NULL or "". Never modifies `path`. */
char *basename(const char *path);
/* Everything before the last component. "." when there is no separator. Never modifies `path`. */
char *dirname(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_POSIX_LIBGEN_H */
