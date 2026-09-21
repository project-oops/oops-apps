/*
 * `stat` for the one thing ETR asks it: how big is this file, and is it a directory.
 * `FileExists` calls it and reads `st_size`; the mode macros are here because the header that
 * declares `stat` is expected to carry them.
 */
#ifndef OOPS_ETR_SYS_STAT_H
#define OOPS_ETR_SYS_STAT_H
#include <sys/types.h>

#define S_IFMT   0170000u
#define S_IFDIR  0040000u
#define S_IFREG  0100000u
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)

struct stat {
    mode_t st_mode;
    off_t  st_size;
};

#ifdef __cplusplus
extern "C" {
#endif
int stat(const char *path, struct stat *out);
int mkdir(const char *path, mode_t mode);
#ifdef __cplusplus
}
#endif

#endif
