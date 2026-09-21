/*
 * `getpwuid` exists so ETR can find a home directory to put its config in. There are no users
 * here, so it answers with the one writable place a payload has - see `etr_posix.c`.
 */
#ifndef OOPS_ETR_PWD_H
#define OOPS_ETR_PWD_H
#include <sys/types.h>

struct passwd {
    const char *pw_name;
    const char *pw_dir;
};

#ifdef __cplusplus
extern "C" {
#endif
struct passwd *getpwuid(uid_t uid);
#ifdef __cplusplus
}
#endif

#endif
