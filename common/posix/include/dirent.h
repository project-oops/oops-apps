/*
 * **This is not a directory enumerator, and it must not be mistaken for one.**
 *
 * ETR calls `opendir` in exactly one place - `DirExists` in `common.cpp` - and never calls
 * `readdir` anywhere. It opens a directory only to find out whether it is there, then closes it.
 * So this answers that question through `oops_fs_exists` and offers nothing else.
 *
 * `readdir` and `struct dirent` are **deliberately not declared**. This SDK cannot enumerate a
 * directory, and a header that declared `readdir` would let a future title compile and then link
 * against nothing - which `oops-sdk#D009` is about and which a payload link would not catch,
 * because it ignores unresolved symbols. An absent declaration is a compile error at the call.
 */
#ifndef OOPS_ETR_DIRENT_H
#define OOPS_ETR_DIRENT_H

typedef struct OOPS_DIR DIR;

#ifdef __cplusplus
extern "C" {
#endif
DIR *opendir(const char *path);
int closedir(DIR *dir);
#ifdef __cplusplus
}
#endif

#endif
