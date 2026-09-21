/*
 * The POSIX calls a ported title makes, over oops-sdk. Shared by every title that needs them.
 *
 * **This was Extreme Tux Racer's private shim until Neverball wanted the same thing.** That is
 * the pattern this collection keeps meeting: one title shims a standard header, a second title
 * needs it, and the answer is to move it down rather than write it twice. `errno` made the same
 * trip into `oops-sdk` after three consumers, and this made a shorter one into `common/` after
 * two.
 *
 * **The measurement that prompted it is worth keeping.** Neverball compiled 8 of its 86 sources
 * before this was on its include path and 57 after - one header, `sys/stat.h`, was blocking 65
 * files, all of them reaching it through a single `share/dir.h`. A shim's value is rarely
 * proportional to its size.
 *
 * # Why this is here and not in oops-sdk
 *
 * These are POSIX, not C. `oops-sdk`'s libc is the C standard library that any payload can
 * expect; `opendir`, `getpwuid` and `stat` are an operating system's interface, and this console
 * is not that operating system. What is here answers them the way a payload honestly can - and
 * some answers, like `chdir`, are *not* what POSIX says, which is precisely why they belong at
 * the port layer where a reader is looking for compromises.
 *
 * # Scope
 *
 * Exactly what the titles call, and nothing added for completeness:
 *
 *     stat  getcwd  chdir  mkdir  access  opendir  closedir  getpwuid  getuid  gettimeofday
 *
 * A tenth function added because POSIX has one is a function nothing calls, which is what
 * `oops-sdk#D009` argues against from the other side.
 */
#include "oops/fs.h"
#include "oops/time.h"

#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

/*
 * Where this title may write, and where `getpwuid` points a program that asks for a home
 * directory. A title sets it from its Makefile; `/data` is the writable area on this console, so
 * the default is a directory under it named for the payload.
 *
 * It is a define rather than a call because a payload has one of these for its whole life, and
 * because the title that owns the directory is the one that should name it.
 */
#ifndef OOPS_POSIX_HOME
#define OOPS_POSIX_HOME "/data/oops-title"
#endif

int stat(const char *path, struct stat *out) {
    int64_t size;

    if (!path || !out) {
        errno = EINVAL;
        return -1;
    }
    if (!oops_fs_exists(path)) {
        errno = ENOENT;
        return -1;
    }

    /*
     * `oops_fs_file_size` answers for a file. A directory has no size to report and this SDK has
     * no call that distinguishes the two, so a negative size is read as "exists but is not a
     * readable file" - which for everything the titles do with this is a directory.
     */
    size = oops_fs_file_size(path);
    if (size < 0) {
        out->st_mode = S_IFDIR;
        out->st_size = 0;
    } else {
        out->st_mode = S_IFREG;
        out->st_size = size;
    }
    return 0;
}

int mkdir(const char *path, mode_t mode) {
    (void)mode; /* no permission bits here; the directory is the payload's either way */

    if (!path) {
        errno = EINVAL;
        return -1;
    }
    if (oops_fs_mkdir(path, 0777) != 0) {
        /* An existing directory is success, which is what every caller of mkdir-then-write
           wants. */
        return oops_fs_exists(path) ? 0 : -1;
    }
    return 0;
}

int access(const char *path, int mode) {
    (void)mode;
    /*
     * Existence only. This SDK has no permission model to consult, and a payload that can see a
     * file can read it - so answering anything else would be inventing a result.
     */
    if (!path || !oops_fs_exists(path)) {
        errno = ENOENT;
        return -1;
    }
    return 0;
}

char *getcwd(char *buf, size_t size) {
    static const char cwd[] = "/app0";

    if (!buf || size < sizeof(cwd)) {
        errno = EINVAL;
        return NULL;
    }
    /*
     * A payload does not have a working directory it can move; it has the place its package is
     * mounted. `/app0` is that place, and it is what `SDL_GetBasePath` reports for the same
     * reason.
     */
    memcpy(buf, cwd, sizeof(cwd));
    return buf;
}

int chdir(const char *path) {
    /*
     * **Answers, and changes nothing.** Both titles use `chdir` as a directory test - enter it,
     * then enter the old one again - so the return value is the whole of what the caller reads.
     * Reporting success for a path that is not there would make that test say yes to everything;
     * reporting failure for one that is would make it say no to everything.
     *
     * This is the function in this file that is furthest from what POSIX promises, and the
     * reason the file is at the port layer rather than in the SDK's libc.
     */
    if (!path || !oops_fs_exists(path)) {
        errno = ENOENT;
        return -1;
    }
    return 0;
}

uid_t getuid(void) { return 0; }

struct passwd *getpwuid(uid_t uid) {
    static struct passwd pw;

    (void)uid;
    pw.pw_name = "player";
    pw.pw_dir = OOPS_POSIX_HOME;
    return &pw;
}

/*
 * `opendir` and `closedir`, for the one use the titles have: does this directory exist. There is
 * no enumeration here and `dirent.h` deliberately does not declare `readdir` - see the comment
 * there for why declaring it would be worse than not having it.
 *
 * The handle is the address of a file-scope object rather than an allocation: there is nothing
 * to keep in it, and a `closedir` that frees nothing cannot leak or double-free.
 */
static int posix_dir_token;

DIR *opendir(const char *path) {
    if (!path || !oops_fs_exists(path)) {
        errno = ENOENT;
        return NULL;
    }
    return (DIR *)&posix_dir_token;
}

int closedir(DIR *dir) {
    if (dir != (DIR *)&posix_dir_token) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

int gettimeofday(struct timeval *tv, void *tz) {
    uint64_t us;

    (void)tz;
    if (!tv) {
        errno = EINVAL;
        return -1;
    }
    us = oops_time_get_us();
    tv->tv_sec = (long)(us / 1000000u);
    tv->tv_usec = (long)(us % 1000000u);
    return 0;
}
