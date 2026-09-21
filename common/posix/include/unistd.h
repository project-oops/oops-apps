/*
 * The four POSIX calls ETR makes from <unistd.h>, over `oops/fs.h`.
 *
 * `getcwd`/`chdir` are a pair used by `DirExistsWin`, which tests a directory by trying to enter
 * it. A payload has one working directory and no way to change it, so `chdir` answers whether
 * the target exists and changes nothing - which is the only thing the caller does with it.
 */
#ifndef OOPS_ETR_UNISTD_H
#define OOPS_ETR_UNISTD_H
#include <stddef.h>
#include <sys/types.h>

#define F_OK 0
#define R_OK 4

#ifdef __cplusplus
extern "C" {
#endif
char *getcwd(char *buf, size_t size);
int chdir(const char *path);
int access(const char *path, int mode);
uid_t getuid(void);
#ifdef __cplusplus
}
#endif

#endif
