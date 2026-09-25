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
#include "oops/system.h"
#include "oops/thread.h" /* oops_thread_self, for pthread_getthreadid_np */
#include "oops/time.h"

#include <dirent.h>
#include <dlfcn.h> /* Dl_info, for the dladdr below */
#include <errno.h>
#include <locale.h>
#include <pthread_np.h> /* the declaration this file's pthread_getthreadid_np answers */
#include <pwd.h>
#include <sched.h> /* the declaration this file's sched_yield answers */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h> /* FILE, for fileno below */
#include <string.h>
#include <strings.h> /* the declarations this file's strcasecmp pair answers */
#include <sys/stat.h>
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

int strcasecmp(const char *a, const char *b) {
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

int strncasecmp(const char *a, const char *b, size_t n) {
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
 * **`chmod` fails too, for the same reason `ftruncate` does.** This platform has no file
 * permissions - `stat` reports a mode built from "is it a directory", not from anything stored -
 * so there is nothing for a mode to change.
 *
 * Returning 0 would be the tempting answer, since both callers here ignore the result: libzip
 * writes `(void)chmod(...)` when it renames a temporary file into place. But a program that
 * *checks* would be told its file is now private when it is exactly as readable as before, and
 * that is the kind of answer someone eventually relies on.
 */
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
int clock_gettime(int clk_id, struct timespec *ts) {
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
