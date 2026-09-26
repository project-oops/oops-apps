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
#include "oops/net.h" /* the socket and resolver calls the BSD-socket shims below map onto */
#include "oops/netctl.h" /* oops_net_ctl_get_info, the only source of this machine's own address */
#include "oops/system.h"
#include "oops/thread.h" /* oops_thread_self, for pthread_getthreadid_np */
#include "oops/time.h"

#include <dirent.h>
#include <dlfcn.h> /* Dl_info, for the dladdr below */
#include <errno.h>
#include <locale.h>
#include <arpa/inet.h>  /* the inet_* conversions this file defines */
#include <fcntl.h>      /* F_GETFL/F_SETFL and O_* , for the fcntl below */
#include <ifaddrs.h>    /* struct ifaddrs, for the getifaddrs below */
#include <langinfo.h>   /* nl_item and CODESET, for the nl_langinfo below */
#include <libgen.h>     /* the basename/dirname declarations this file answers */
#include <net/if.h>     /* if_nametoindex, likewise */
#include <netdb.h>      /* struct hostent and h_errno, which this file defines */
#include <netinet/in.h> /* sockaddr_in, htons/ntohs */
#include <signal.h>     /* sighandler_t and the SIG* numbers, for the signal() below */
#include <stdarg.h>     /* va_list, for the variadic ioctl below */
#include <sys/ioctl.h>  /* FIONBIO and the ioctl declaration this file answers */
#include <sys/param.h>  /* PATH_MAX, which pathconf below reports */
#include <sys/select.h> /* fd_set and the select this file implements by polling */
#include <sys/wait.h>   /* waitpid/wait, likewise */
#include <pthread_np.h> /* the declaration this file's pthread_getthreadid_np answers */
#include <pwd.h>
#include <sched.h> /* the declaration this file's sched_yield answers */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h> /* FILE, for fileno below */
#include <string.h>
#include <strings.h> /* the declarations this file's strcasecmp pair answers */
#include <sys/stat.h>
#include <sys/statvfs.h> /* struct statvfs, for the always-failing statvfs below */
#include <sys/sysctl.h> /* the declaration this file's sysctl answers */
#include <sys/time.h>
/* For `struct timespec` and the `CLOCK_*` ids that `clock_gettime` below answers - declared in
 * the SDK's libc header, implemented here, the same split `gettimeofday` has. */
#include <time.h>
#include <unistd.h>

/*
 * Where this title may write, and where `getpwuid` points a program that asks for a home
 * directory. A title sets it from its Makefile.
 *
 * It is a define rather than a call because a payload has one of these for its whole life, and
 * because the title that owns the directory is the one that should name it.
 *
 * **It defaults to `/app0`, and the default used to be `/data/oops-title`, which cannot work.**
 * `/data` is outside a title's sandbox: with the SDK's `fs` channel turned up, Extreme Tux Racer's
 * first syscall of the run is `open("/data")` and it fails, so nothing under it can be created or
 * written and a title loses every setting without a word. `/app0` is inside the sandbox and is
 * writable - Neverball's shim measured that while looking for somewhere else to put its config.
 *
 * `/app0` is the package, so what a title writes there does not survive a redeploy. That is the
 * right default anyway: a home that is wrong-but-writable loses data on a re-restore, and a home
 * that is unreachable loses it every single launch. A title needing real persistence mounts
 * savedata and points `HOME` at the mount, which is what Neverball does.
 */
#ifndef OOPS_POSIX_HOME
#define OOPS_POSIX_HOME "/app0"
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

/* The same answer, and for the same reason: a payload runs as one identity with nothing to
 * distinguish a real user from an effective one. SQLite's unix layer asks for both when it decides
 * who owns a database file it opened. */
uid_t geteuid(void) { return 0; }

/*
 * `sched_yield` - give up the rest of this slice.
 *
 * Over `oops_thread_yield`, which is the same operation under a different name. It always succeeds,
 * which is also what POSIX says: the only documented failure is a system that does not support
 * scheduling at all.
 */
int sched_yield(void) {
    oops_thread_yield();
    return 0;
}

/*
 * **The home directory this shim invents has to exist**, and creating it is this file's job rather
 * than the title's, because nothing else knows it was invented here.
 *
 * A program that asks for a home directory asks in order to write in it, and they all do the same
 * next thing: `mkdir(<home>/.something)`. `mkdir` creates one level - POSIX says so, `oops_fs_mkdir`
 * agrees - so with `<home>` absent that call fails on the missing parent and the program concludes
 * it has nowhere to write. Extreme Tux Racer's first hardware run said `CSPList::Save - unable to
 * open` for exactly this reason: `/data/extreme-tux-racer` had never been created, so
 * `mkdir("/data/extreme-tux-racer/.etr")` could not succeed and no setting was ever saved. Nothing
 * in the log named the directory, which is what makes this worth doing here - the symptom appears
 * two layers above the cause.
 *
 * Every component is created, not only the last, so a title may name a home more than one level
 * deep. The result is deliberately not reported: a title with nowhere to write still runs, and the
 * write that then fails is the right place to hear about it.
 */
static void ensure_home_exists(void) {
    static int done = 0;
    char path[256];
    size_t n = 0;

    if (done) return;
    done = 1;

    for (const char *s = OOPS_POSIX_HOME; *s && n + 1 < sizeof path; s++) {
        path[n++] = *s;
        /* A separator inside the path ends a component; the leading `/` of an absolute path is
           not one, and a trailing `/` would name the component just created. */
        if (*s == '/' && n > 1 && s[1] != '\0') {
            path[n - 1] = '\0';
            if (!oops_fs_exists(path)) oops_fs_mkdir(path, 0777);
            path[n - 1] = '/';
        }
    }
    path[n] = '\0';
    if (n > 0 && !oops_fs_exists(path)) oops_fs_mkdir(path, 0777);

    /*
     * **It says so either way, and that is the point.** A line only on success is an arm that
     * cannot fail: it cannot tell "the directory is there" from "this code is not in the payload
     * you are running", and on this console those are hard to distinguish by other means - the
     * target's file listing is served over a channel that has been caught returning a stale read.
     * So the outcome is reported, with the path, at info.
     */
    oops_kprintf("posix", "home %s: %s", OOPS_POSIX_HOME,
                 oops_fs_exists(path) ? "ready" : "COULD NOT BE CREATED - nothing will persist");
}

struct passwd *getpwuid(uid_t uid) {
    static struct passwd pw;

    (void)uid;
    ensure_home_exists();
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

/* There are no directory descriptors to convert - `dirent.h` says why this one cannot be made to
 * work behind the same signature. */
DIR *fdopendir(int fd) {
    (void)fd;
    errno = EBADF;
    return NULL;
}

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
__attribute__((weak)) char *setlocale(int category, const char *locale) {
    static char c_locale[] = "C";

    (void)category;
    (void)locale;
    return c_locale;
}

__attribute__((weak)) struct lconv *localeconv(void) {
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

__attribute__((weak)) int gettimeofday(struct timeval *tv, void *tz) {
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

/*
 * `getpid`, `fileno` and `fstat` - the three a logging library reaches for.
 *
 * spdlog's `details/os-inl.h` wants all three: the pid for a `%P` in a pattern, and the other two
 * to ask how large the file behind a `FILE *` has grown so a rotating sink knows when to roll.
 *
 * **There is one process here**, so `getpid` answers a constant. A number that never changes is
 * the truth on this platform rather than a stand-in for one, and 1 is what every other
 * single-process environment answers.
 *
 * `fileno` reaches into the SDK's `FILE`, which carries the descriptor as its first member - the
 * same field `fdopen` fills in. `fstat` then answers size from that descriptor, which is the one
 * thing any caller of it here asks for; mode is reported as a regular file because a descriptor
 * that came from `fopen` is one.
 *
 * **`fstat` on a descriptor the SDK cannot size answers -1 rather than zero.** A rotating sink
 * told "zero bytes" would never roll and would grow without bound; told "I do not know" it
 * reports the failure instead, which is the outcome worth having.
 *
 * There is no call that sizes a descriptor, so this seeks to the end and reads the position -
 * **and puts the position back**, because the caller did not ask for its file pointer to move
 * and a logging sink that lost its append position would overwrite what it had written.
 */
int getpid(void) { return 1; }

/*
 * `strcasecmp` and `strncasecmp`.
 *
 * POSIX puts these in `<strings.h>` rather than `<string.h>`, and neither existed here. StormLib
 * asked first, through the `_stricmp` aliases in its `StormPort.h`.
 *
 * **ASCII case folding only, which is what callers of these actually mean.** A locale-aware
 * `tolower` would fold differently in a Turkish locale - dotted and dotless i - and every use of
 * this function here is comparing a file name or an extension against a literal, where that
 * would be a bug rather than a feature. The comparison is on `unsigned char`, because `char` is
 * signed on this target and a byte above 127 would otherwise compare as negative.
 */
static int oops_ascii_lower(unsigned char c) {
    return (c >= 'A' && c <= 'Z') ? (c - 'A' + 'a') : c;
}

__attribute__((weak)) int strcasecmp(const char *a, const char *b) {
    if (!a || !b) return a == b ? 0 : (a ? 1 : -1);
    for (;;) {
        const int ca = oops_ascii_lower((unsigned char)*a);
        const int cb = oops_ascii_lower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        if (ca == 0) return 0;
        a++;
        b++;
    }
}

__attribute__((weak)) int strncasecmp(const char *a, const char *b, size_t n) {
    if (!a || !b) return a == b ? 0 : (a ? 1 : -1);
    while (n--) {
        const int ca = oops_ascii_lower((unsigned char)*a);
        const int cb = oops_ascii_lower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        if (ca == 0) return 0;
        a++;
        b++;
    }
    return 0;
}

/*
 * `read`, `write` and `close` on a descriptor.
 *
 * The SDK's `oops_fs_*` are these under other names, so this is a rename and a return type -
 * `ssize_t` where the SDK answers `int64_t`, which is the same width here.
 *
 * Added for libgfxd, whose display-list decoder reads and writes through descriptors the caller
 * hands it. They are general enough that anything doing file I/O the POSIX way will want them,
 * which is the argument for this shim over a per-title one.
 *
 * **`errno` is set from the sign, not from a code.** `oops_fs_read` answers a count or a negative
 * number and does not say which failure it was, so guessing between `EIO`, `EBADF` and `ENOSPC`
 * would be inventing detail. `EIO` is the honest catch-all for "the platform refused and did not
 * say why".
 */
ssize_t read(int fd, void *buf, size_t count) {
    int64_t n;

    if (fd < 0 || (!buf && count)) {
        errno = EINVAL;
        return -1;
    }
    n = oops_fs_read(fd, buf, count);
    if (n < 0) {
        errno = EIO;
        return -1;
    }
    return (ssize_t)n;
}

ssize_t write(int fd, const void *buf, size_t count) {
    int64_t n;

    if (fd < 0 || (!buf && count)) {
        errno = EINVAL;
        return -1;
    }
    n = oops_fs_write(fd, buf, count);
    if (n < 0) {
        errno = EIO;
        return -1;
    }
    return (ssize_t)n;
}

int close(int fd) {
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    return oops_fs_close(fd) == 0 ? 0 : -1;
}

off_t lseek(int fd, off_t offset, int whence) {
    int64_t pos;

    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    /* `SEEK_SET`/`CUR`/`END` are 0/1/2 and so are `OOPS_SEEK_*`, but the mapping is written out
     * rather than relied on: two enumerations agreeing today is not the same as one of them
     * being defined in terms of the other. */
    switch (whence) {
        case SEEK_SET: pos = oops_fs_seek(fd, offset, OOPS_SEEK_SET); break;
        case SEEK_CUR: pos = oops_fs_seek(fd, offset, OOPS_SEEK_CUR); break;
        case SEEK_END: pos = oops_fs_seek(fd, offset, OOPS_SEEK_END); break;
        default:
            errno = EINVAL;
            return -1;
    }
    if (pos < 0) {
        errno = EIO;
        return -1;
    }
    return (off_t)pos;
}

/*
 * **`ftruncate` fails, and that is the honest answer.** The SDK's filesystem has no call that
 * shortens or extends a file, so there is nothing to call. Returning 0 would be the flattering
 * version: a caller that asked to truncate and was told it succeeded then writes on the
 * assumption the file is the length it asked for.
 *
 * Nothing here needs it. StormLib names it only on the archive-*writing* path, and this port
 * reads.
 */
int ftruncate(int fd, off_t length) {
    (void)length;
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    errno = ENOSYS;
    return -1;
}

/*
 * `open`, over `oops_fs_open`.
 *
 * **`fcntl.h` deliberately declared this without defining it**, so that a title reaching for it
 * got an undefined symbol naming the function rather than a silent fault - and for a long time
 * nothing here called it. `openat` below now does, which makes the shim itself the caller and the
 * definition mandatory: without it the link fails on `open`, which is how this was found.
 *
 * The flag values in `fcntl.h` are FreeBSD's and `oops/fs.h` takes the same numbers, so the flags
 * pass straight through. `mode` is read only when `O_CREAT` is set, as POSIX says.
 */
int open(const char *path, int flags, ...) {
    int mode = 0;

    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
    }
    {
        const int fd = oops_fs_open(path, flags, mode);
        if (fd < 0) {
            errno = EIO;
            return -1;
        }
        return fd;
    }
}

/*
 * `openat`, which is `open` for the one anchor this platform has and a refusal for the rest.
 *
 * `fcntl.h` argues it: there are no directory descriptors here, so `AT_FDCWD` - "relative to the
 * working directory" - is the only meaningful `dirfd`, and the working directory is `/`. For that
 * case this is not an approximation of `openat`, it is `openat`. Any other descriptor fails with
 * `EBADF` rather than being quietly treated as the root, which would open the wrong file and look
 * like the caller's bug.
 */
int openat(int dirfd, const char *path, int flags, ...) {
    int mode = 0;

    if (dirfd != AT_FDCWD) {
        errno = EBADF;
        return -1;
    }
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
    }
    return open(path, flags, mode);
}

/* Refused, which is what a kernel without the call answers - and libc++ falls back to a portable
 * copy when it does. `unistd.h` says why that is better than patching its platform detection. */
ssize_t copy_file_range(int infd, off_t *inoffp, int outfd, off_t *outoffp,
                        size_t len, unsigned int flags) {
    (void)inoffp;
    (void)outoffp;
    (void)len;
    (void)flags;
    if (infd < 0 || outfd < 0) {
        errno = EBADF;
        return -1;
    }
    errno = ENOSYS;
    return -1;
}

/* By path, and the same answer: the filesystem cannot resize a file. */
int truncate(const char *path, off_t length) {
    (void)length;
    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }
    errno = ENOSYS;
    return -1;
}

/*
 * `realpath`, and on this platform it is **complete rather than approximate**.
 *
 * POSIX's `realpath` does two things: it resolves symbolic links, and it collapses `.`, `..` and
 * repeated separators against an absolute path. There are no symbolic links on this filesystem -
 * the same finding `readlink`, `symlink` and `S_ISLNK` record from three other directions - so
 * the lexical half *is* the whole job, and the answer this returns is the answer a real one
 * would.
 *
 * Two places where it follows the specification rather than being convenient:
 *
 *   - **The path must exist**, and `ENOENT` if it does not. POSIX requires every component to
 *     resolve, and a caller using this to canonicalise a name it is about to create wants to be
 *     told that rather than handed a tidy string.
 *   - **`resolved` must be at least `PATH_MAX`** when it is not NULL, which is the interface's
 *     long-standing trap and the reason passing NULL is preferred. NULL allocates, and the
 *     caller frees.
 *
 * The working directory is `/` and cannot be changed - `getcwd` and `chdir` above say why - so a
 * relative path is resolved against the root.
 */
char *realpath(const char *path, char *resolved) {
    char work[PATH_MAX];
    char *out;
    size_t w = 0;
    const char *p;

    if (path == NULL || path[0] == '\0') {
        errno = EINVAL;
        return NULL;
    }

    /* Absolute from the start: the working directory is `/`, so a relative path needs only the
     * leading separator rather than a `getcwd` that would answer the same thing. */
    work[w++] = '/';
    p = (path[0] == '/') ? path + 1 : path;

    while (*p != '\0') {
        const char *seg = p;
        size_t len;

        while (*p != '\0' && *p != '/') {
            p++;
        }
        len = (size_t)(p - seg);
        while (*p == '/') {
            p++; /* collapse repeated separators */
        }

        if (len == 0 || (len == 1 && seg[0] == '.')) {
            continue; /* "" and "." contribute nothing */
        }
        if (len == 2 && seg[0] == '.' && seg[1] == '.') {
            /* Pop the last component. At the root there is nothing above, and POSIX says `/..`
             * is `/` rather than an error. */
            while (w > 1u && work[w - 1] != '/') {
                w--;
            }
            if (w > 1u) {
                w--; /* the separator itself */
            }
            continue;
        }
        if (w > 1u) {
            if (w + 1u >= sizeof(work)) {
                errno = ENAMETOOLONG;
                return NULL;
            }
            work[w++] = '/';
        }
        if (w + len >= sizeof(work)) {
            errno = ENAMETOOLONG;
            return NULL;
        }
        memcpy(work + w, seg, len);
        w += len;
    }
    work[w] = '\0';

    /* Every component has to resolve, and with no links that is one question about the whole
     * path. A directory answers through `opendir`, a file through `oops_fs_exists`. */
    if (!oops_fs_exists(work)) {
        oops_dir_t *dir = oops_fs_opendir(work);
        if (!dir) {
            errno = ENOENT;
            return NULL;
        }
        (void)oops_fs_closedir(dir);
    }

    out = resolved;
    if (out == NULL) {
        out = (char *)malloc(w + 1u);
        if (out == NULL) {
            errno = ENOMEM;
            return NULL;
        }
    }
    memcpy(out, work, w + 1u);
    return out;
}

/* File times cannot be set, which is the same absence `sys/stat.h` records from the reading
 * side - see `sys/time.h` for why success would be the misleading answer. */
int utimes(const char *path, const struct timeval times[2]) {
    (void)path;
    (void)times;
    errno = ENOSYS;
    return -1;
}

/*
 * `unlinkat`. `AT_FDCWD` is the only anchor here, and `AT_REMOVEDIR` asks for a `rmdir` the SDK
 * does not have - refused rather than unlinking the directory's *name* and leaving its contents
 * unreachable, which is what passing it through to `unlink` would do.
 */
int unlinkat(int dirfd, const char *path, int flags) {
    if (dirfd != AT_FDCWD) {
        errno = EBADF;
        return -1;
    }
    if (flags & AT_REMOVEDIR) {
        errno = ENOSYS;
        return -1;
    }
    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }
    return (oops_fs_unlink(path) == 0) ? 0 : (errno = EIO, -1);
}

/* No links on this filesystem - `unistd.h` says why a copy would be worse than a refusal. */
int link(const char *oldpath, const char *newpath) {
    (void)oldpath;
    (void)newpath;
    errno = ENOSYS;
    return -1;
}

int symlink(const char *target, const char *linkpath) {
    (void)target;
    (void)linkpath;
    errno = ENOSYS;
    return -1;
}

/*
 * `pathconf`: `_PC_PATH_MAX` is answered from `sys/param.h`'s `MAXPATHLEN`, everything else is
 * indeterminate. **-1 without touching `errno`** is how POSIX distinguishes "no limit to report"
 * from "the call failed", so `errno` is explicitly left alone here rather than set to something
 * tidy - see `unistd.h`.
 */
long pathconf(const char *path, int name) {
    (void)path;
    return (name == _PC_PATH_MAX) ? (long)PATH_MAX : -1;
}

long fpathconf(int fd, int name) {
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    return (name == _PC_PATH_MAX) ? (long)PATH_MAX : -1;
}

/*
 * `nl_langinfo`, which answers `CODESET` and nothing else - `langinfo.h` says why the one answer
 * is a fact rather than a convenience.
 *
 * The returned strings are literals, so the "caller may not free it, and the next call may
 * overwrite it" contract costs nothing here: nothing is overwritten.
 */
char *nl_langinfo(nl_item item) {
    if (item == CODESET) {
        /* The kernel takes UTF-8 path bytes and the SDK passes them through unchanged. */
        return (char *)"UTF-8";
    }
    return (char *)"";
}

/*
 * **`statvfs` fails, and `sys/statvfs.h` argues why at length.** The short version: nothing in the
 * SDK reports a filesystem's capacity, and the numbers are the whole point of the call. The one
 * caller here - `ghc::filesystem`'s `space()`, which Bugdom's Pomme bundles - turns the failure
 * into "cannot find out", which is the truth.
 *
 * `*buf` is deliberately left untouched rather than zeroed: a caller that ignores the return and
 * reads the structure gets whatever it had, which is more likely to look wrong than a tidy zero
 * would - and looking wrong is the useful outcome.
 */
int statvfs(const char *path, struct statvfs *buf) {
    (void)path;
    (void)buf;
    errno = ENOSYS;
    return -1;
}

int fstatvfs(int fd, struct statvfs *buf) {
    (void)buf;
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    errno = ENOSYS;
    return -1;
}

/*
 * **`chmod` fails too, for the same reason `ftruncate` does.** This platform has no file
 * permissions - `stat` reports a mode built from "is it a directory", not from anything stored -
 * so there is nothing for a mode to change.
 *
 * Returning 0 would be the tempting answer, since both callers here ignore the result: libzip
 * writes `(void)chmod(...)` when it renames a temporary file into place. But a program that
 * *checks* would be told its file is now private when it is exactly as readable as before, and
 * that is the kind of answer someone eventually relies on.
 */
/* ---------------------------------------------------------------------------
 * `basename` / `dirname`, and the interface list that cannot be listed.
 * See `libgen.h`, `net/if.h` and `ifaddrs.h` for the reasoning behind each.
 * ------------------------------------------------------------------------- */

/* One buffer each, so a caller holding both results at once still has two valid strings. */
static char s_basename_buf[256];
static char s_dirname_buf[256];

static void oops_copy_bounded(char *dst, size_t cap, const char *src, size_t n) {
    if (n >= cap) {
        n = cap - 1u;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

char *basename(const char *path) {
    const char *last;

    if (path == NULL || path[0] == '\0') {
        s_basename_buf[0] = '.';
        s_basename_buf[1] = '\0';
        return s_basename_buf;
    }
    /* Trailing slashes are not part of the name: "/usr/lib/" basenames to "lib". */
    {
        size_t end = strlen(path);
        while (end > 1u && path[end - 1u] == '/') {
            end--;
        }
        if (end == 1u && path[0] == '/') {
            s_basename_buf[0] = '/';
            s_basename_buf[1] = '\0';
            return s_basename_buf;
        }
        last = path;
        for (size_t i = 0; i < end; i++) {
            if (path[i] == '/') {
                last = &path[i + 1u];
            }
        }
        oops_copy_bounded(s_basename_buf, sizeof(s_basename_buf), last,
                          (size_t)(&path[end] - last));
    }
    return s_basename_buf;
}

char *dirname(const char *path) {
    size_t end;
    size_t cut;

    if (path == NULL || path[0] == '\0') {
        s_dirname_buf[0] = '.';
        s_dirname_buf[1] = '\0';
        return s_dirname_buf;
    }
    end = strlen(path);
    while (end > 1u && path[end - 1u] == '/') {
        end--;
    }
    /* Find the separator before the last component. */
    cut = end;
    while (cut > 0u && path[cut - 1u] != '/') {
        cut--;
    }
    if (cut == 0u) {
        /* No separator at all: the directory is the current one, per POSIX. */
        s_dirname_buf[0] = '.';
        s_dirname_buf[1] = '\0';
        return s_dirname_buf;
    }
    /* Drop the separator itself, and any run of them, but keep a lone leading "/". */
    while (cut > 1u && path[cut - 1u] == '/') {
        cut--;
    }
    oops_copy_bounded(s_dirname_buf, sizeof(s_dirname_buf), path, cut);
    return s_dirname_buf;
}

/*
 * `ioctl`, and only `FIONBIO` - see `sys/ioctl.h` for why the rest fail rather than pretend.
 *
 * The variadic third argument is `int *` for this request, which is what every caller passes.
 */
int ioctl(int fd, unsigned long request, ...) {
    va_list ap;
    int rc;

    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    if (request != FIONBIO) {
        errno = EINVAL;
        return -1;
    }
    va_start(ap, request);
    {
        const int *on = va_arg(ap, int *);
        rc = oops_set_nonblocking(fd, (on != NULL && *on != 0) ? 1 : 0);
    }
    va_end(ap);
    if (rc != 0) {
        errno = EIO;
        return -1;
    }
    return 0;
}

/*
 * `fcntl`, and only the non-blocking flag.
 *
 * `F_SETFL` with `O_NONBLOCK` is the one request anything here makes - ioquake3's `sys_unix.c:331`
 * is the only call site in this tree - and it maps onto `oops_set_nonblocking`, so it is real.
 *
 * `F_GETFL` answers `O_RDWR` and nothing else. That is a partial truth rather than a guess: this
 * shim does not track a descriptor's access mode, and the usual reason to ask is to add a flag and
 * set it back, which `F_SETFL` below handles on its own.
 *
 * It also happens to satisfy the other caller in this tree. PhysFS's flush does
 * `if ((fcntl(fd, F_GETFL) & O_ACCMODE) != O_RDONLY) fsync(fd)`, and `O_RDWR` reads as "not
 * read-only", so it calls `fsync` - which is correct for any descriptor. An earlier version of this
 * function failed with `ENOSYS` for that caller's sake; a failure reads the same way to PhysFS, but
 * it silently leaves a socket blocking for ioquake3, which asks for `O_NONBLOCK` and ignores the
 * return. Answering both properly costs nothing.
 *
 * Everything else fails with `EINVAL`. Descriptor duplication, locking and close-on-exec have no
 * meaning here, and `fcntl` is too open-ended an interface to answer generally - the same
 * reasoning as `ioctl` above.
 */
int fcntl(int fd, int cmd, ...) {
    va_list ap;
    int rc = 0;

    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    switch (cmd) {
        case F_GETFL:
            return O_RDWR; /* see the note above */
        case F_SETFL:
            va_start(ap, cmd);
            {
                const int flags = va_arg(ap, int);
                rc = oops_set_nonblocking(fd, (flags & O_NONBLOCK) ? 1 : 0);
            }
            va_end(ap);
            if (rc != 0) {
                errno = EIO;
                return -1;
            }
            return 0;
        default:
            errno = EINVAL;
            return -1;
    }
}

/* No FIFOs on this filesystem - see `sys/stat.h`, and `S_ISFIFO`, which is never true. */
int mkfifo(const char *path, mode_t mode) {
    (void)mode;
    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }
    errno = ENOSYS;
    return -1;
}

/* No second process exists here, so there is never a child to reap. */
pid_t waitpid(pid_t pid, int *status, int options) {
    (void)pid;
    (void)options;
    if (status != NULL) {
        *status = 0;
    }
    errno = ECHILD;
    return -1;
}

pid_t wait(int *status) {
    return waitpid(-1, status, 0);
}

/* 0 is the documented "no such interface", and here it means "no mapping to consult". */
unsigned int if_nametoindex(const char *ifname) {
    (void)ifname;
    return 0u;
}

/* Fails, deliberately - `ifaddrs.h` argues why this is better than an empty list, and names the
 * `oops_net_ctl_get_info` upgrade path. `*ifap` is cleared so a caller that ignores the return does
 * not walk an uninitialised pointer. */
int getifaddrs(struct ifaddrs **ifap) {
    if (ifap != NULL) {
        *ifap = NULL;
    }
    errno = ENOSYS;
    return -1;
}

void freeifaddrs(struct ifaddrs *ifa) {
    (void)ifa; /* nothing was allocated */
}

/* ---------------------------------------------------------------------------
 * `signal`, over `oops_thread_install_exception_handler`.
 *
 * **The handler signatures differ, which is why there is a trampoline.** POSIX hands the handler
 * only the signal number; the SDK's is `(int signum, void *arg1, void *arg2)`. Calling a
 * one-argument function through the three-argument pointer type happens to work on this ABI, and
 * "happens to work" is not a thing to build a crash handler on - so the caller's handler goes in a
 * table and one trampoline of the SDK's own shape calls it.
 *
 * Which signals are real is argued in `signal.h`. The short version: faults are, because the SDK
 * can install them; the rest are not, because nothing here can deliver them.
 * ------------------------------------------------------------------------- */

/* Indexed by signal number. Small and fixed: the highest signal this shim names is SIGTERM (15). */
#define OOPS_POSIX_NSIG 32
static sighandler_t s_sig_handlers[OOPS_POSIX_NSIG];

/* Is this a fault the SDK's exception handler can actually deliver? */
static int oops_signal_is_deliverable(int sig) {
    switch (sig) {
        case SIGSEGV:
        case SIGILL:
        case SIGFPE:
        case SIGBUS:
        case SIGABRT:
            return 1;
        default:
            return 0;
    }
}

static void oops_signal_trampoline(int signum, void *arg1, void *arg2) {
    (void)arg1;
    (void)arg2;
    if (signum > 0 && signum < OOPS_POSIX_NSIG) {
        const sighandler_t h = s_sig_handlers[signum];
        /* SIG_IGN is recorded and does nothing, which is what ignoring means. SIG_DFL never gets
         * here - installing it removes the SDK handler below. */
        if (h != NULL && h != SIG_IGN && h != SIG_ERR) {
            h(signum);
        }
    }
}

sighandler_t signal(int sig, sighandler_t handler) {
    sighandler_t previous;

    if (sig <= 0 || sig >= OOPS_POSIX_NSIG) {
        errno = EINVAL;
        return SIG_ERR;
    }
    if (!oops_signal_is_deliverable(sig)) {
        /* Not an error in the caller's code - a property of the platform. See `signal.h`. */
        errno = EINVAL;
        return SIG_ERR;
    }

    previous = s_sig_handlers[sig];
    s_sig_handlers[sig] = handler;

    if (handler == SIG_DFL) {
        (void)oops_thread_remove_exception_handler(sig);
        return previous;
    }
    if (oops_thread_install_exception_handler(sig, oops_signal_trampoline) != 0) {
        /* The install failed, so the table entry would be a promise nothing keeps. */
        s_sig_handlers[sig] = previous;
        return SIG_ERR;
    }
    return previous;
}

/* ---------------------------------------------------------------------------
 * Sockets, over `oops/net.h`. See `sys/socket.h` and `netdb.h` for what is and is not here.
 * ------------------------------------------------------------------------- */

int h_errno = 0;

/*
 * The `inet_*` conversions, over the SDK's own pair. `oops_net_inet_pton` builds and
 * `oops_net_inet_ntop` reads **network byte order** - `oops-sdk/src/net/net.c:71-72` is where that
 * is established - so nothing here swaps, and `s_addr` passes straight through.
 */
in_addr_t inet_addr(const char *cp) {
    uint32_t packed = 0;
    if (cp == NULL || oops_net_inet_pton(cp, &packed) != 0) {
        return INADDR_NONE;
    }
    return (in_addr_t)packed;
}

/* Static storage, overwritten by the next call - see `arpa/inet.h`. 16 is the most an IPv4
 * dotted-quad plus its terminator can need, which is also `oops_net_inet_ntop`'s minimum. */
char *inet_ntoa(struct in_addr in) {
    static char buf[16];
    if (oops_net_inet_ntop(in.s_addr, buf, sizeof(buf)) != 0) {
        buf[0] = '\0';
    }
    return buf;
}

int inet_pton(int af, const char *src, void *dst) {
    uint32_t packed = 0;
    if (af != AF_INET) {
        errno = EAFNOSUPPORT;
        return -1;
    }
    if (src == NULL || dst == NULL || oops_net_inet_pton(src, &packed) != 0) {
        return 0; /* malformed, which POSIX distinguishes from an unsupported family */
    }
    memcpy(dst, &packed, sizeof(packed));
    return 1;
}

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size) {
    uint32_t packed;
    if (af != AF_INET) {
        errno = EAFNOSUPPORT;
        return NULL;
    }
    if (src == NULL || dst == NULL || size < 16u) {
        errno = ENOSPC;
        return NULL;
    }
    memcpy(&packed, src, sizeof(packed));
    if (oops_net_inet_ntop(packed, dst, (size_t)size) != 0) {
        errno = ENOSPC;
        return NULL;
    }
    return dst;
}

/*
 * Weak fallbacks for every `oops_net_*` call this file makes, so that a title which does not link
 * the SDK's `src/net/net.c` gets a refusal rather than a jump into nothing.
 *
 * **That last part is why the list has to be complete.** A payload link does not report an
 * unresolved symbol on this target - the call site is left pointing at address zero and the fault
 * happens the first time the port tries to open a socket, a long way from the missing source file.
 * So a `oops_net_*` name used below with no weak twin here is a latent crash, and the rule is: if
 * this file calls it, it is in this list.
 */
__attribute__((weak)) int oops_socket(int d, int t, int p) { (void)d; (void)t; (void)p; errno = ENOSYS; return -1; }
__attribute__((weak)) int oops_connect(int s, const char *ip, uint16_t port) { (void)s; (void)ip; (void)port; errno = ENOSYS; return -1; }
__attribute__((weak)) int oops_bind(int s, const char *ip, uint16_t port) { (void)s; (void)ip; (void)port; errno = ENOSYS; return -1; }
__attribute__((weak)) long oops_send(int s, const void *b, size_t l, int f) { (void)s; (void)b; (void)l; (void)f; errno = ENOSYS; return -1; }
__attribute__((weak)) long oops_recv(int s, void *b, size_t l, int f) { (void)s; (void)b; (void)l; (void)f; errno = ENOSYS; return -1; }
__attribute__((weak)) long oops_sendto(int s, const void *b, size_t l, int f, const char *ip, uint16_t port) { (void)s; (void)b; (void)l; (void)f; (void)ip; (void)port; errno = ENOSYS; return -1; }
__attribute__((weak)) long oops_recvfrom(int s, void *b, size_t l, int f, char *ip, size_t ipl, uint16_t *port) { (void)s; (void)b; (void)l; (void)f; (void)ip; (void)ipl; (void)port; errno = ENOSYS; return -1; }
__attribute__((weak)) int oops_setsockopt(int s, int lvl, int opt, const void *v, size_t vl) { (void)s; (void)lvl; (void)opt; (void)v; (void)vl; errno = ENOSYS; return -1; }
__attribute__((weak)) int oops_set_nonblocking(int s, int nb) { (void)s; (void)nb; errno = ENOSYS; return -1; }
/* 0 - "not a would-block". With no network linked every call already failed for a permanent
 * reason, so reporting "try again" would turn `select` below into an infinite wait. */
__attribute__((weak)) int oops_net_would_block(long rc) { (void)rc; return 0; }
__attribute__((weak)) void oops_close(int s) { (void)s; }
__attribute__((weak)) int oops_net_resolve(const char *n, char *ip, size_t sz) { (void)n; (void)ip; (void)sz; return -1; }
__attribute__((weak)) int oops_net_inet_ntop(uint32_t a, char *d, size_t s) { (void)a; (void)d; (void)s; return -1; }
__attribute__((weak)) int oops_net_inet_pton(const char *s, uint32_t *d) { (void)s; (void)d; return -1; }
__attribute__((weak)) int oops_net_ctl_get_info(oops_net_info_t *out) { (void)out; return -1; }

/*
 * **Which bare BSD names this file has taken, so the SDK does not call back into them.**
 *
 * `oops-sdk/src/net/net.c` reaches the platform's sockets through weak references to the exported
 * names, and the plain spellings are the same ones a POSIX shim has to define - a port calls `bind`,
 * and this file answers. A weak reference is satisfied by any strong definition in the link, so
 * without this the chain is:
 *
 *     port's bind()  ->  bind() below  ->  oops_bind()  ->  net.c's p_bind()  ->  bind() below
 *
 * an unbounded recursion that arrives as a stack overflow on the first packet. `close` is the same
 * shape and quieter: `oops_close(sock)` would reach the descriptor `close` in this file, which
 * routes to `oops_fs_close`, and the socket is never actually closed.
 *
 * So the names are declared rather than discovered. **Anything added to this file that shadows a
 * platform export has to be added here too** - the list is the contract, and a missing bit is the
 * recursion coming back.
 *
 * `listen` and `accept` are absent from both the list and this file, which is why the SDK asks per
 * name instead of once: those two still reach the platform.
 */
unsigned oops_net_bare_names_are_shimmed(void) {
    return OOPS_NET_SHIMMED_BIND | OOPS_NET_SHIMMED_CONNECT |
           OOPS_NET_SHIMMED_RECV | OOPS_NET_SHIMMED_RECVFROM |
           OOPS_NET_SHIMMED_SENDTO | OOPS_NET_SHIMMED_SETSOCKOPT |
           OOPS_NET_SHIMMED_CLOSE;
}

/* `::` and `::1`. Real objects rather than macros because callers take their address - see
 * `netinet/in.h`. Zero-initialised static storage is `::` exactly; the loopback needs its last
 * byte set, which a designated initialiser does through the union member. */
const struct in6_addr in6addr_any;
const struct in6_addr in6addr_loopback = {
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}}};

int socket(int domain, int type, int protocol) {
    if (domain != AF_INET) {
        /* Refused rather than quietly treated as IPv4 - a caller asking for AF_INET6 and getting
         * an IPv4 socket would fail later, somewhere else. */
        errno = EAFNOSUPPORT;
        return -1;
    }
    return oops_socket(domain, type, protocol);
}

/*
 * `oops_connect` names the peer as dotted-quad text, so the 32-bit address is formatted back into
 * a string here. `oops_net_inet_ntop` takes **network byte order** - `oops-sdk/src/net/net.c:71`
 * says so where `pton` builds it - which is what `sin_addr.s_addr` already holds, so it passes
 * straight through with no swap. Getting that backwards would connect to a reversed address,
 * which looks like a routing problem rather than a bug.
 */
int connect(int sock, const struct sockaddr *addr, socklen_t addrlen) {
    const struct sockaddr_in *in = (const struct sockaddr_in *)(const void *)addr;
    char ip[16];

    if (addr == NULL || addrlen < (socklen_t)sizeof(*in) || in->sin_family != AF_INET) {
        errno = EINVAL;
        return -1;
    }
    if (oops_net_inet_ntop(in->sin_addr.s_addr, ip, sizeof(ip)) != 0) {
        errno = EINVAL;
        return -1;
    }
    return oops_connect(sock, ip, ntohs(in->sin_port));
}

ssize_t send(int sock, const void *buf, size_t len, int flags) {
    return (ssize_t)oops_send(sock, buf, len, flags);
}

ssize_t recv(int sock, void *buf, size_t len, int flags) {
    return (ssize_t)oops_recv(sock, buf, len, flags);
}

/*
 * **`shutdown` closes the socket outright, which is more than it was asked to do.**
 *
 * POSIX's version half-closes: `SHUT_WR` sends a FIN and leaves the read side open, which is how a
 * protocol says "I am done sending, tell me what you have". The SDK has no such call. Doing
 * nothing and reporting success would leave a caller waiting for an end-of-stream that never
 * comes, which is a hang; closing is at least an end of stream, and the difference is named here
 * so a protocol that relies on half-close knows where to look.
 *
 * Nothing in this tree calls it. It exists because `sys/socket.h` declares it, and a declaration
 * with no definition is a fault rather than a link error on this target.
 */
int shutdown(int sock, int how) {
    (void)how;
    if (sock < 0) {
        errno = EBADF;
        return -1;
    }
    oops_close(sock);
    return 0;
}

/*
 * One static `hostent` with one address, which is what the interface promises and no more - see
 * `netdb.h` for why that is not thread-safe and why there is no second address to fall back to.
 */
struct hostent *gethostbyname(const char *name) {
    static struct hostent ent;
    static struct in_addr addr;
    static char *addr_list[2];
    static char namebuf[256];
    char ip[16];

    if (name == NULL) {
        h_errno = HOST_NOT_FOUND;
        return NULL;
    }
    if (oops_net_resolve(name, ip, sizeof(ip)) != 0) {
        h_errno = HOST_NOT_FOUND;
        return NULL;
    }
    {
        uint32_t packed = 0;
        if (oops_net_inet_pton(ip, &packed) != 0) {
            /* Resolution succeeded and the result did not parse, which is the SDK contradicting
             * itself rather than the host being unknown. */
            h_errno = NO_RECOVERY;
            return NULL;
        }
        addr.s_addr = packed; /* network byte order, as `pton` builds it */
    }

    /* Bounded, and always terminated - `strncpy` alone does not terminate when the source fills
     * the buffer, and `h_name` is handed to callers that will print it. */
    {
        size_t n = strlen(name);
        if (n >= sizeof(namebuf)) {
            n = sizeof(namebuf) - 1u;
        }
        memcpy(namebuf, name, n);
        namebuf[n] = '\0';
    }
    addr_list[0] = (char *)&addr;
    addr_list[1] = NULL;

    ent.h_name = namebuf;
    ent.h_aliases = NULL;
    ent.h_addrtype = AF_INET;
    ent.h_length = (int)sizeof(struct in_addr);
    ent.h_addr_list = addr_list;
    h_errno = 0;
    return &ent;
}

/* ---------------------------------------------------------------------------
 * Datagrams: `bind`, `sendto`, `recvfrom`, `setsockopt`
 *
 * Every one of these crosses the same seam. POSIX names an address as a `sockaddr_in` holding a
 * 32-bit number; `oops/net.h` names it as dotted-quad text. So each shim formats one into the other,
 * which is a round trip through a string for something that was already a number - `sys/socket.h`
 * says why that is the SDK's interface rather than an accident here.
 * ------------------------------------------------------------------------- */

/*
 * `sockaddr_in` -> (text, host-order port). 0 on success, -1 with `errno`.
 *
 * `AF_INET` only, refused rather than coerced. `sin_addr.s_addr` is already network byte order and
 * `oops_net_inet_ntop` wants network byte order, so it passes straight through; the *port* does
 * need `ntohs`, because the SDK's calls take a host-order port and swap it themselves.
 */
static int sa_to_text(const struct sockaddr *addr, socklen_t addrlen, char *ip,
                      size_t ip_len, uint16_t *port) {
    const struct sockaddr_in *in = (const struct sockaddr_in *)(const void *)addr;

    if (addr == NULL || addrlen < (socklen_t)sizeof(*in) || ip_len < 16u) {
        errno = EINVAL;
        return -1;
    }
    if (in->sin_family != AF_INET) {
        /* `netinet/in.h` explains why AF_INET6 gets here at all and why it must fail. */
        errno = EAFNOSUPPORT;
        return -1;
    }
    if (oops_net_inet_ntop(in->sin_addr.s_addr, ip, ip_len) != 0) {
        errno = EINVAL;
        return -1;
    }
    *port = ntohs(in->sin_port);
    return 0;
}

int bind(int sock, const struct sockaddr *addr, socklen_t addrlen) {
    const struct sockaddr_in *in = (const struct sockaddr_in *)(const void *)addr;
    char ip[INET_ADDRSTRLEN];
    uint16_t port = 0;

    if (sa_to_text(addr, addrlen, ip, sizeof(ip), &port) != 0) {
        return -1;
    }
    /* `oops_bind` spells "any interface" as an empty string, not as "0.0.0.0" - so the one address
     * that does not round-trip through text is the one almost every server binds. */
    if (in->sin_addr.s_addr == INADDR_ANY) {
        ip[0] = '\0';
    }
    return oops_bind(sock, ip, port);
}

ssize_t sendto(int sock, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen) {
    char ip[INET_ADDRSTRLEN];
    uint16_t port = 0;

    /* POSIX: a NULL destination on a connected socket is a plain `send`. */
    if (dest_addr == NULL) {
        return (ssize_t)oops_send(sock, buf, len, flags);
    }
    if (sa_to_text(dest_addr, addrlen, ip, sizeof(ip), &port) != 0) {
        return -1;
    }
    return (ssize_t)oops_sendto(sock, buf, len, flags, ip, port);
}

/*
 * **The sender's address is the part that can fail on its own**, and it fails loudly.
 *
 * `oops_recvfrom` reports the peer only when the platform binds a `recvfrom` export; when it does
 * not, the data still arrives and the address comes back as the empty string - `oops/net.h` states
 * that contract. Filling a zeroed `sockaddr_in` from that would tell the caller every datagram came
 * from 0.0.0.0, and a game protocol that identifies its peers by address would then treat every
 * client as the same one. So this refuses the call with `EOPNOTSUPP` instead.
 *
 * The cost is stated plainly: the datagram has already been consumed by then and is lost. That is a
 * worse outcome than a working `recvfrom` and a better one than a wrong sender, because a caller
 * seeing an error looks here, and a caller seeing 0.0.0.0 looks at its own protocol code.
 *
 * A caller passing `src_addr == NULL` is not asking who sent it, so there is nothing to fail on.
 */
ssize_t recvfrom(int sock, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen) {
    char ip[INET_ADDRSTRLEN];
    uint16_t port = 0;
    long rc;

    ip[0] = '\0';
    rc = oops_recvfrom(sock, buf, len, flags, ip, sizeof(ip), &port);
    if (rc < 0) {
        return -1;
    }
    if (src_addr == NULL || addrlen == NULL) {
        return (ssize_t)rc;
    }
    if (ip[0] == '\0') {
        errno = EOPNOTSUPP;
        return -1;
    }
    {
        struct sockaddr_in in;
        uint32_t packed = 0;
        socklen_t copy = (socklen_t)sizeof(in);

        if (oops_net_inet_pton(ip, &packed) != 0) {
            /* The receive reported a sender and it did not parse, which is the SDK contradicting
             * itself rather than anything the caller did. */
            errno = EOPNOTSUPP;
            return -1;
        }
        memset(&in, 0, sizeof(in));
        in.sin_len = (uint8_t)sizeof(in);
        in.sin_family = AF_INET;
        in.sin_port = htons(port);
        in.sin_addr.s_addr = packed;

        if (*addrlen < copy) {
            copy = *addrlen; /* truncated, as POSIX allows */
        }
        memcpy(src_addr, &in, copy);
        /* The size it *would* have needed, so a caller can see it was truncated. */
        *addrlen = (socklen_t)sizeof(in);
    }
    return (ssize_t)rc;
}

/*
 * Straight through. The constants in `sys/socket.h` and `netinet/in.h` are the platform's real
 * values for exactly this reason: nothing here translates them, so a wrong number would set a
 * different option rather than fail.
 */
int setsockopt(int sock, int level, int optname, const void *optval,
               socklen_t optlen) {
    return oops_setsockopt(sock, level, optname, optval, (size_t)optlen);
}

/* ---------------------------------------------------------------------------
 * `select`, by polling. `sys/select.h` carries the reasoning; this is the mechanism.
 * ------------------------------------------------------------------------- */

/* One millisecond. See `sys/select.h` for what this buys and what it costs. */
#define OOPS_SELECT_POLL_US 1000u

/*
 * Whether a descriptor has something to read, asked without consuming it.
 *
 * `OOPS_MSG_PEEK | OOPS_MSG_DONTWAIT` is a single non-blocking look at the head of the queue that
 * leaves the datagram in place. A byte is enough: peeking does not consume, so a one-byte peek at a
 * 1400-byte packet neither truncates nor dequeues it.
 *
 * `rc == 0` counts as ready, which it is in both interpretations - a zero-length datagram is a
 * datagram, and end-of-stream on a socket is what `select` reports readable so that the caller's
 * `read` returns 0.
 */
static int fd_can_read(int fd) {
    char probe;
    char ip[INET_ADDRSTRLEN];
    uint16_t port = 0;
    long rc = oops_recvfrom(fd, &probe, 1u, OOPS_MSG_PEEK | OOPS_MSG_DONTWAIT, ip,
                            sizeof(ip), &port);

    if (rc >= 0) {
        return 1;
    }
    if (oops_net_would_block(rc)) {
        return 0;
    }
    /* An error that is not "try again". `select` reports a descriptor with a pending error as
     * readable, so that the caller finds out by reading it - and a descriptor that is not a socket
     * at all lands here too, which `sys/select.h` names as a known divergence. */
    return 1;
}

/* Whether any bit below `nfds` is set. Used to tell "three sets zeroed and one filled", which is
 * ordinary, from a caller genuinely asking about writability, which this cannot answer. */
static int fdset_any(const fd_set *set, int nfds) {
    int fd;
    if (set == NULL) {
        return 0;
    }
    for (fd = 0; fd < nfds; fd++) {
        if (FD_ISSET(fd, set)) {
            return 1;
        }
    }
    return 0;
}

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
           struct timeval *timeout) {
    fd_set want;
    uint64_t remaining_us = 0;
    int infinite = (timeout == NULL);

    if (nfds < 0 || nfds > FD_SETSIZE) {
        errno = EINVAL;
        return -1;
    }
    /* Refused rather than answered with a guess - `sys/select.h` says why. */
    if (fdset_any(writefds, nfds) || fdset_any(exceptfds, nfds)) {
        errno = EINVAL;
        return -1;
    }
    if (!infinite) {
        if (timeout->tv_sec < 0 || timeout->tv_usec < 0) {
            errno = EINVAL;
            return -1;
        }
        remaining_us = (uint64_t)timeout->tv_sec * 1000000u + (uint64_t)timeout->tv_usec;
    }

    /* A sleep, which `select(0, NULL, NULL, NULL, &tv)` has always been. */
    if (readfds == NULL) {
        if (infinite) {
            errno = EINVAL; /* nothing to wait for and no deadline: that is a hang, not a call */
            return -1;
        }
        while (remaining_us > 0u) {
            uint32_t step = (remaining_us < OOPS_SELECT_POLL_US) ? (uint32_t)remaining_us
                                                                 : OOPS_SELECT_POLL_US;
            oops_time_sleep_us(step);
            remaining_us -= step;
        }
        return 0;
    }

    want = *readfds;
    for (;;) {
        int ready = 0;
        int fd;

        FD_ZERO(readfds);
        for (fd = 0; fd < nfds; fd++) {
            if (FD_ISSET(fd, &want) && fd_can_read(fd)) {
                FD_SET(fd, readfds);
                ready++;
            }
        }
        if (ready > 0) {
            return ready;
        }
        /* Checked after the first pass, so a zero timeout is a poll rather than a no-op. */
        if (!infinite && remaining_us == 0u) {
            return 0;
        }
        {
            uint32_t step = OOPS_SELECT_POLL_US;
            if (!infinite && remaining_us < (uint64_t)step) {
                step = (uint32_t)remaining_us;
            }
            oops_time_sleep_us(step);
            if (!infinite) {
                remaining_us -= step;
            }
        }
    }
}

/* ---------------------------------------------------------------------------
 * `getaddrinfo` and friends. `netdb.h` lists the four ways this is narrower than a desktop's.
 * ------------------------------------------------------------------------- */

/* One heap block per entry, so that `freeaddrinfo` is one `free` per entry and the `sockaddr` a
 * caller holds through `ai_addr` cannot outlive or be freed apart from the entry pointing at it. */
struct oops_addrinfo_block {
    struct addrinfo    ai;
    struct sockaddr_in sa;
};

/* 0 on success, `EAI_SERVICE` otherwise. NULL is a port of 0, which is what a caller resolving a
 * name without a service means. */
static int service_to_port(const char *service, uint16_t *out) {
    unsigned long v = 0;
    const char *p = service;

    *out = 0;
    if (service == NULL || *service == '\0') {
        return 0;
    }
    for (; *p != '\0'; p++) {
        if (*p < '0' || *p > '9') {
            /* A name, and there is no services database here to look it up in - see `netdb.h`. */
            return EAI_SERVICE;
        }
        v = v * 10u + (unsigned long)(*p - '0');
        if (v > 65535u) {
            return EAI_SERVICE;
        }
    }
    *out = (uint16_t)v;
    return 0;
}

int getaddrinfo(const char *node, const char *service,
                const struct addrinfo *hints, struct addrinfo **res) {
    int family = AF_UNSPEC;
    int socktype = 0;
    int protocol = 0;
    int flags = 0;
    uint16_t port = 0;
    uint32_t packed = 0;
    int rc;
    struct oops_addrinfo_block *block;

    if (res == NULL) {
        return EAI_SYSTEM;
    }
    *res = NULL;
    if (hints != NULL) {
        family = hints->ai_family;
        socktype = hints->ai_socktype;
        protocol = hints->ai_protocol;
        flags = hints->ai_flags;
    }
    if (family != AF_UNSPEC && family != AF_INET) {
        /* Including `AF_INET6`. `netinet/in.h` explains why answering IPv4 here instead would be
         * worse than failing: the caller would hand the result to an IPv6 socket. */
        return EAI_FAMILY;
    }
    rc = service_to_port(service, &port);
    if (rc != 0) {
        return rc;
    }

    if (node == NULL) {
        /* POSIX: `AI_PASSIVE` means an address to bind to - any interface - and its absence means
         * an address to connect to, which for "this machine" is the loopback. */
        packed = (flags & AI_PASSIVE) ? htonl(INADDR_ANY) : htonl(INADDR_LOOPBACK);
    } else if (oops_net_inet_pton(node, &packed) == 0) {
        /* Numeric, so no resolver is involved and this works with no network at all. */
    } else if (flags & AI_NUMERICHOST) {
        return EAI_NONAME; /* the caller said not to resolve, and it was not numeric */
    } else {
        char ip[INET_ADDRSTRLEN];
        if (oops_net_resolve(node, ip, sizeof(ip)) != 0) {
            return EAI_NONAME;
        }
        if (oops_net_inet_pton(ip, &packed) != 0) {
            return EAI_FAIL; /* resolution succeeded and its answer did not parse */
        }
    }

    block = (struct oops_addrinfo_block *)malloc(sizeof(*block));
    if (block == NULL) {
        return EAI_MEMORY;
    }
    memset(block, 0, sizeof(*block));

    block->sa.sin_len = (uint8_t)sizeof(block->sa);
    block->sa.sin_family = AF_INET;
    block->sa.sin_port = htons(port);
    block->sa.sin_addr.s_addr = packed;

    block->ai.ai_flags = flags;
    block->ai.ai_family = AF_INET;
    block->ai.ai_socktype = socktype;
    block->ai.ai_protocol = protocol;
    block->ai.ai_addrlen = (socklen_t)sizeof(block->sa);
    block->ai.ai_canonname = NULL; /* never filled - see `netdb.h` */
    block->ai.ai_addr = (struct sockaddr *)(void *)&block->sa;
    block->ai.ai_next = NULL;      /* one address, always - see `netdb.h` */

    *res = &block->ai;
    return 0;
}

/*
 * Walks and frees. The list this shim produces is one block per entry with the `sockaddr` inside it,
 * so freeing the `addrinfo` frees its address too - which is why `ai_addr` must not be freed
 * separately and why the whole list has to come from `getaddrinfo` above rather than be assembled
 * by a caller.
 */
void freeaddrinfo(struct addrinfo *ai) {
    while (ai != NULL) {
        struct addrinfo *next = ai->ai_next;
        free(ai);
        ai = next;
    }
}

int getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host,
                socklen_t hostlen, char *serv, socklen_t servlen, int flags) {
    const struct sockaddr_in *in = (const struct sockaddr_in *)(const void *)sa;

    if (sa == NULL || salen < (socklen_t)sizeof(*in)) {
        return EAI_FAMILY;
    }
    if (in->sin_family != AF_INET) {
        return EAI_FAMILY;
    }
    /* The caller insists on a name, and there is no reverse resolver here. Answering with the
     * numeric form anyway is what `NI_NAMEREQD` exists to forbid. */
    if (flags & NI_NAMEREQD) {
        return EAI_NONAME;
    }

    if (host != NULL && hostlen > 0) {
        if (hostlen < 16u) {
            return EAI_OVERFLOW;
        }
        if (oops_net_inet_ntop(in->sin_addr.s_addr, host, (size_t)hostlen) != 0) {
            return EAI_FAIL;
        }
    }
    if (serv != NULL && servlen > 0) {
        /* Decimal, always: `NI_NUMERICSERV` or not, there is no services database to name a port
         * from - the same absence `getaddrinfo` reports as `EAI_SERVICE` going the other way. */
        unsigned int port = ntohs(in->sin_port);
        char tmp[6];
        int n = 0;
        int i;

        if (port == 0u) {
            tmp[n++] = '0';
        }
        while (port > 0u && n < (int)sizeof(tmp)) {
            tmp[n++] = (char)('0' + (port % 10u));
            port /= 10u;
        }
        if ((socklen_t)n + 1u > servlen) {
            return EAI_OVERFLOW;
        }
        for (i = 0; i < n; i++) {
            serv[i] = tmp[n - 1 - i];
        }
        serv[n] = '\0';
    }
    return 0;
}

const char *gai_strerror(int ecode) {
    switch (ecode) {
    case 0:            return "no error";
    case EAI_AGAIN:    return "temporary failure in name resolution";
    case EAI_BADFLAGS: return "invalid flags";
    case EAI_FAIL:     return "non-recoverable failure in name resolution";
    case EAI_FAMILY:   return "address family not supported";
    case EAI_MEMORY:   return "memory allocation failure";
    case EAI_NONAME:   return "name or service not known";
    case EAI_SERVICE:  return "service not supported for this socket type";
    case EAI_SOCKTYPE: return "socket type not supported";
    case EAI_SYSTEM:   return "system error";
    case EAI_OVERFLOW: return "argument buffer overflow";
    default:           return "unknown error";
    }
}

/*
 * The console's own address where a hostname is asked for. `unistd.h` argues the case; the short
 * version is that this platform has no name and the one thing a caller does with the answer is
 * resolve it, which an address satisfies exactly.
 */
int gethostname(char *name, size_t len) {
    oops_net_info_t info;

    if (name == NULL || len == 0u) {
        errno = EINVAL;
        return -1;
    }
    memset(&info, 0, sizeof(info));
    if (oops_net_ctl_get_info(&info) != 0 || info.ip_address[0] == '\0') {
        errno = ENOSYS;
        return -1;
    }
    {
        size_t n = strlen(info.ip_address);
        if (n + 1u > len) {
            errno = ENAMETOOLONG;
            return -1;
        }
        memcpy(name, info.ip_address, n + 1u);
    }
    return 0;
}

/*
 * One process, so: alive if and only if it is us. `signal.h` has the reasoning and names the caller
 * that depends on it.
 */
int kill(pid_t pid, int sig) {
    if (sig != 0) {
        errno = ENOSYS;
        return -1;
    }
    if ((int)pid != getpid()) {
        errno = ESRCH;
        return -1;
    }
    return 0;
}

/* Both always fail, which is the truth and is what the callers are written for - see `unistd.h`.
 * `fork` must never return 0, or the caller runs its child branch in the only process there is. */
pid_t fork(void) {
    errno = ENOSYS;
    return (pid_t)-1;
}

int execvp(const char *file, char *const argv[]) {
    (void)file;
    (void)argv;
    errno = ENOSYS;
    return -1;
}

/*
 * `usleep`, which is `oops_time_sleep_us` under its POSIX name.
 *
 * POSIX's version fails with `EINVAL` for a value of a million or more and expects the caller to
 * use `sleep` instead. That rule exists because `useconds_t` was once 32 bits on systems where a
 * second's worth of microseconds was close to the limit; it is not a property of anything here,
 * and a caller asking to sleep two seconds means it. So this sleeps, and the divergence is named
 * rather than left to be discovered - tinycthread's `thrd_sleep` passes whatever it was given.
 */
int usleep(useconds_t microseconds) {
    if (microseconds > 0u) {
        oops_time_sleep_us((uint32_t)microseconds);
    }
    return 0;
}

/*
 * `sleep`, in whole seconds.
 *
 * Returns 0 always. POSIX's return is "seconds left over if a signal cut the sleep short", and
 * nothing on this platform can cut one short, so the sleep completes and there is never a
 * remainder - the same reasoning as `nanosleep`'s `rem` above.
 *
 * Slept in one-second steps rather than as one multiplication, so that a caller asking for an
 * hour cannot overflow the 32-bit microsecond count that `oops_time_sleep_us` takes.
 */
unsigned int sleep(unsigned int seconds) {
    unsigned int i;
    for (i = 0; i < seconds; i++) {
        oops_time_sleep_us(1000000u);
    }
    return 0;
}

/*
 * `nanosleep`, over the SDK's microsecond sleep.
 *
 * **It rounds up, and that direction is the contract.** POSIX says the sleep lasts *at least* the
 * requested interval, so 500ns becomes 1us rather than 0. Rounding down would turn a caller's
 * sub-microsecond delay into a busy loop that never yields - libultraship's frame pacer
 * (`gfx_sdl2.cpp:662`) asks for exactly this kind of short sleep every frame.
 *
 * `oops_time_sleep_us` takes a 32-bit count, which runs out at about 71 minutes, so a longer
 * request is slept in chunks rather than silently truncated to `UINT32_MAX`.
 *
 * `rem` is for resuming a sleep a signal interrupted. Nothing can interrupt one here - there is no
 * signal delivery on this platform - so the sleep always completes and `rem` is zeroed, which is
 * what a caller's `while (nanosleep(&req, &rem)) req = rem;` loop needs to see to stop.
 */
int nanosleep(const struct timespec *req, struct timespec *rem) {
    uint64_t us;

    if (rem != NULL) {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
    }
    if (req == NULL || req->tv_nsec < 0 || req->tv_nsec >= 1000000000L || req->tv_sec < 0) {
        errno = EINVAL;
        return -1;
    }

    /* Rounded up, per the note above. */
    us = (uint64_t)req->tv_sec * 1000000u + ((uint64_t)req->tv_nsec + 999u) / 1000u;
    while (us > 0xffffffffu) {
        oops_time_sleep_us(0xffffffffu);
        us -= 0xffffffffu;
    }
    if (us > 0) {
        oops_time_sleep_us((uint32_t)us);
    }
    return 0;
}

/*
 * **Always 0, and `*info` is zeroed anyway.** There is no run-time symbol table in a payload to
 * look an address up in - see `dlfcn.h` for the whole argument, including why the zeroing matters
 * for the one caller in this tree.
 */
int dladdr(const void *addr, Dl_info *info) {
    (void)addr;
    if (info != NULL) {
        info->dli_fname = NULL;
        info->dli_fbase = NULL;
        info->dli_sname = NULL;
        info->dli_saddr = NULL;
    }
    return 0;
}

/* The `*at()` form. `AT_FDCWD` is the only anchor here - `fcntl.h` and `openat` above say why -
 * and for it this is `chmod`, which refuses. */
int fchmodat(int dirfd, const char *path, mode_t mode, int flags) {
    (void)flags;
    if (dirfd != AT_FDCWD) {
        errno = EBADF;
        return -1;
    }
    return chmod(path, mode);
}

/* By descriptor, and the same answer: there are no permissions here to change. */
int fchmod(int fd, mode_t mode) {
    (void)mode;
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    errno = ENOSYS;
    return -1;
}

int chmod(const char *path, mode_t mode) {
    (void)mode;
    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }
    errno = ENOSYS;
    return -1;
}

/*
 * `isatty` and `fsync`, the other two `os-inl.h` reaches for.
 *
 * **Nothing here is a terminal.** A payload's `stdout` goes to the kernel log, not to a tty, so
 * `isatty` is 0 for every descriptor - which is the answer that makes a logging library skip its
 * colour escapes, and the right one: those escapes would land in the log as literal bytes.
 *
 * `fsync` has nothing to flush to. The SDK's writes are not buffered behind a descriptor the way
 * a hosted libc's are, so success is the truthful answer rather than a stub's optimism - there is
 * no pending state that this failing to act on could lose.
 */
int isatty(int fd) {
    (void)fd;
    return 0;
}

int fsync(int fd) {
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    return 0;
}

/* `fcntl` is defined once, further up, and honours F_SETFL's O_NONBLOCK. */

/* `stat`: there are no symbolic links here - see `<sys/stat.h>`. */
int lstat(const char *path, struct stat *out) { return stat(path, out); }

/* No symbolic links, so nothing is one - see `<unistd.h>`. */
ssize_t readlink(const char *path, char *buf, size_t size) {
    (void)path;
    (void)buf;
    (void)size;
    errno = EINVAL;
    return -1;
}

/*
 * **`sysctl` answers nothing.** The kernel's MIB is not a payload's to query, and the caller so
 * far - OpenAL Soft asking `kern.proc.pathname` for its own executable's path - has a fallback
 * for exactly this failure. `ENOENT` is what FreeBSD gives for a name it does not have.
 */
int sysctl(const int *name, unsigned int namelen, void *oldp, size_t *oldlenp,
           const void *newp, size_t newlen) {
    (void)name;
    (void)namelen;
    (void)oldp;
    (void)oldlenp;
    (void)newp;
    (void)newlen;
    errno = ENOENT;
    return -1;
}

int fileno(FILE *stream) {
    if (!stream) {
        errno = EINVAL;
        return -1;
    }
    return stream->fd;
}

int fstat(int fd, struct stat *out) {
    int64_t here;
    int64_t size;

    if (!out || fd < 0) {
        errno = EINVAL;
        return -1;
    }
    here = oops_fs_tell(fd);
    if (here < 0) {
        errno = EBADF;
        return -1;
    }
    size = oops_fs_seek(fd, 0, OOPS_SEEK_END);
    /* Back to where the caller was, whatever the size call said. */
    (void)oops_fs_seek(fd, here, OOPS_SEEK_SET);
    if (size < 0) {
        errno = EBADF;
        return -1;
    }
    out->st_mode = S_IFREG;
    out->st_size = size;
    return 0;
}

/*
 * `pthread_getthreadid_np`, FreeBSD's small-integer thread id.
 *
 * The target is `x86_64-unknown-freebsd`, so `__FreeBSD__` is defined and a portable program's
 * FreeBSD branch is the one that compiles - spdlog reaches this for the thread id it puts in a
 * log line. The handle `oops_thread_self` returns is what identifies a thread here; its low bits
 * are stable for that thread's life and distinct between live threads, which is all a log line
 * needs. It is not a kernel tid and nothing should treat it as one.
 */
int pthread_getthreadid_np(void) {
    return (int)(uintptr_t)oops_thread_self();
}

/*
 * `clock_gettime`, which is what a monotonic clock looks like to C++.
 *
 * Added for libc++'s `std::chrono::steady_clock`. Enabling `_LIBCPP_HAS_THREADS` obliges
 * `_LIBCPP_HAS_MONOTONIC_CLOCK` - `__config` refuses the other combination, reasonably, since a
 * timed wait needs a clock that cannot go backwards - and `steady_clock::now()` reaches here.
 *
 * **Every clock id answers from the same counter, and that is not a shortcut.** `oops_time_get_ns`
 * is the platform's monotonic counter, so `CLOCK_MONOTONIC` is exactly right. `CLOCK_REALTIME` is
 * the one being approximated: a caller wanting a wall-clock date wants
 * `oops_time_get_epoch_seconds`, and the difference matters to a file timestamp but not to the
 * only consumer here, which is measuring intervals. Rather than silently hand back an uptime for
 * a date, `CLOCK_REALTIME` is offset by the epoch so the two agree to a second.
 *
 * An unknown clock id is `EINVAL` rather than a best guess, because a program asking for
 * `CLOCK_PROCESS_CPUTIME_ID` and getting wall time would draw the wrong conclusion quietly.
 */
__attribute__((weak)) int clock_gettime(int clk_id, struct timespec *ts) {
    uint64_t ns;

    if (!ts) {
        errno = EINVAL;
        return -1;
    }
    ns = oops_time_get_ns();
    switch (clk_id) {
        case CLOCK_REALTIME:
            /* The counter's nanoseconds, carried on top of the wall-clock second. */
            ts->tv_sec = (time_t)oops_time_get_epoch_seconds();
            ts->tv_nsec = (long)(ns % 1000000000u);
            return 0;
        case CLOCK_MONOTONIC:
            ts->tv_sec = (time_t)(ns / 1000000000u);
            ts->tv_nsec = (long)(ns % 1000000000u);
            return 0;
        default:
            errno = EINVAL;
            return -1;
    }
}
