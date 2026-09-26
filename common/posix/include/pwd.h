/*
 * `getpwuid` exists so ETR can find a home directory to put its config in. There are no users
 * here, so it answers with the one writable place a payload has - see `etr_posix.c`.
 */
#ifndef OOPS_ETR_PWD_H
#define OOPS_ETR_PWD_H
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
#ifdef __cplusplus
}
#endif

#endif
