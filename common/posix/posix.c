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
#include "oops/heap.h"
#include "oops/time.h"

#include <dirent.h>
#include <errno.h>
#include <locale.h>
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
 * `opendir`, `readdir`, `closedir` over `oops/fs.h`.
 *
 * **This used to be an existence test and nothing more**, because Extreme Tux Racer only calls
 * `opendir` to find out whether a directory is there. Neverball walks directories for real, so
 * the SDK grew `oops_fs_readdir` and this became the POSIX spelling over it.
 *
 * `readdir` returns a pointer to storage owned by the `DIR`, which is what POSIX says and what
 * lets a caller write the usual `while ((ent = readdir(d)))` loop. It is overwritten by the next
 * call on the same handle, so two interleaved walks of one directory would collide - as they
 * would on any system.
 */
struct OOPS_DIR {
    oops_dir_t *dir;
    struct dirent ent;
};

DIR *opendir(const char *path) {
    struct OOPS_DIR *d;

    if (!path) {
        errno = EINVAL;
        return NULL;
    }

    d = (struct OOPS_DIR *)oops_malloc(sizeof(*d));
    if (!d) {
        errno = ENOMEM;
        return NULL;
    }

    d->dir = oops_fs_opendir(path);
    if (!d->dir) {
        oops_free(d);
        errno = ENOENT;
        return NULL;
    }
    d->ent.d_name[0] = '\0';
    return (DIR *)d;
}

struct dirent *readdir(DIR *dir) {
    struct OOPS_DIR *d = (struct OOPS_DIR *)dir;
    oops_dirent_t e;

    if (!d) {
        errno = EINVAL;
        return NULL;
    }

    /*
     * POSIX cannot tell "end of directory" from "error" by the return value alone - both are
     * NULL - and distinguishes them by whether `errno` changed. `oops_fs_readdir` does not have
     * that problem, returning 1, 0 and -1, so the two cases are separated here properly: the end
     * leaves `errno` alone, a failure sets it.
     */
    int rc = oops_fs_readdir(d->dir, &e);
    if (rc <= 0) {
        if (rc < 0) errno = EINVAL;
        return NULL;
    }

    {
        size_t i = 0;
        while (i + 1u < sizeof(d->ent.d_name) && e.name[i] != '\0') {
            d->ent.d_name[i] = e.name[i];
            i++;
        }
        d->ent.d_name[i] = '\0';
    }
    return &d->ent;
}

int closedir(DIR *dir) {
    struct OOPS_DIR *d = (struct OOPS_DIR *)dir;
    int rc;

    if (!d) {
        errno = EINVAL;
        return -1;
    }
    rc = oops_fs_closedir(d->dir);
    oops_free(d);
    return rc;
}

/*
 * The one locale. See `locale.h` for why `setlocale` reports success for any request rather than
 * refusing one it cannot honour: a program that asks for a locale and is told no tends to stop,
 * and a program told "C" carries on formatting the way it already assumed.
 */
char *setlocale(int category, const char *locale) {
    static char c_locale[] = "C";

    (void)category;
    (void)locale;
    return c_locale;
}

struct lconv *localeconv(void) {
    static char point[] = ".";
    static char empty[] = "";
    static struct lconv lc;

    lc.decimal_point = point;
    lc.thousands_sep = empty;
    lc.grouping = empty;
    lc.int_curr_symbol = empty;
    lc.currency_symbol = empty;
    lc.mon_decimal_point = empty;
    lc.mon_thousands_sep = empty;
    lc.mon_grouping = empty;
    lc.positive_sign = empty;
    lc.negative_sign = empty;
    return &lc;
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
