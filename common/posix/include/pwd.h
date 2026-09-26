/*
 * `getpwuid` exists so ETR can find a home directory to put its config in. There are no users
 * here, so it answers with the one writable place a payload has - see `etr_posix.c`.
 */
#ifndef OOPS_ETR_PWD_H
#define OOPS_ETR_PWD_H
#include <stddef.h>
#include <sys/types.h>

/*
 * **`char *`, not `const char *`, because that is what the platform declares.** FreeBSD's
 * `struct passwd` has non-const members, and real code relies on it: ioquake3's `sys_unix.c:240`
 * does `return p->pw_name;` from a function returning `char *`, which is an error against a const
 * member and fine against the real one. The strings these point at are still owned by `getpwuid`
 * and must not be written - that is the same contract the platform has, and the same one every
 * caller already honours.
 */
struct passwd {
    char *pw_name;
    char *pw_dir;
};

#ifdef __cplusplus
extern "C" {
#endif
struct passwd *getpwuid(uid_t uid);

/* The reentrant form, which takes the caller's buffer. There is one user and the strings are
 * static, so this fills `pwd` from the same place `getpwuid` does and copies nothing into `buf`.
 * Returns 0 and sets `*result`, as POSIX says, or ERANGE if `buf` is too small to have been
 * plausible. */
int getpwuid_r(uid_t uid, struct passwd *pwd, char *buf, size_t buflen, struct passwd **result);
#ifdef __cplusplus
}
#endif

#endif
