/*
 * SQLite's platform layer, over oops-sdk.
 *
 * Built with `-DSQLITE_OS_OTHER=1`, which removes SQLite's unix backend entirely and
 * makes this file the whole of what SQLite knows about the machine: a VFS, a mutex
 * implementation, and the `sqlite3_os_init` hook that registers them.
 *
 * # Why not shim the unix backend instead
 *
 * It was the shorter-looking option and it is the wrong one. SQLite's unix VFS wants
 * `mmap`, `fchown`, `fchmod`, `rmdir` and POSIX record locks, none of which this
 * platform has - but the part that decides it is `struct stat`:
 *
 *   - `sqlite3.c:24366` builds a file's identity from `st_dev` and `st_ino`, and uses
 * it as the key of the table that stops two connections to the same file from
 * corrupting each other. A `stat` that answers zero for both makes *every* file the
 * same file.
 *   - `sqlite3.c:24419` reads `st_nlink == 0` as "this file has already been unlinked".
 * A zero there tells SQLite its database is gone.
 *
 * Both compile. Both are wrong in a way that shows up as lost data much later, and
 * neither would have been caught by anything short of running it. `SQLITE_OS_OTHER` is
 * the supported way to say "this platform is not unix", and SQLite is explicitly
 * designed for it.
 *
 * # What this VFS does not do, stated rather than faked
 *
 *   - **No locking.** `xLock`, `xUnlock` and `xCheckReservedLock` succeed without doing
 * anything. That is correct here and not a stub: locking exists to arbitrate between
 * *processes*, and a payload is one process with one copy of SQLite in it. Concurrency
 * *within* the process is the mutex layer below, which is real.
 *   - **No `xSync`.** There is no `fsync` in the SDK, so a sync returns OK without
 * forcing anything to the device. A database is consistent while the machine is up and
 * may lose the last writes if it loses power. Returning an error instead would make
 * every commit fail.
 *   - **`xTruncate` fails**, with `SQLITE_IOERR_TRUNCATE`, because the SDK's filesystem
 * cannot shorten a file. It is a real error rather than a silent success: a truncate
 * that reports success and does nothing leaves a journal with stale bytes past its
 * header, which is how a rollback reads garbage. Nothing in the intended use reaches it
 * - the default `journal_mode=DELETE` removes journals rather than truncating them, and
 * a database that only grows never shrinks - and `oops_fs_truncate` is the one SDK
 * addition that would close it properly.
 */

#include "oops/fs.h"
#include "oops/heap.h"
#include "oops/thread.h"
#include "oops/time.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "sqlite3.h"

/* ---------------------------------------------------------------------------
 * The file handle
 * ------------------------------------------------------------------------- */

typedef struct oops_sqlite_file {
    sqlite3_io_methods const
        *pMethods; /* must be first: SQLite casts to sqlite3_file */
    int fd;
    int delete_on_close; /* SQLITE_OPEN_DELETEONCLOSE */
    char *path; /* owned, for delete_on_close; NULL for a temporary with no name */
} oops_sqlite_file;

static int oops_vfs_close(sqlite3_file *pFile) {
    oops_sqlite_file *f = (oops_sqlite_file *)pFile;
    if (f->fd >= 0) {
        oops_fs_close(f->fd);
        f->fd = -1;
    }
    if (f->delete_on_close && f->path != NULL) {
        oops_fs_unlink(f->path);
    }
    if (f->path != NULL) {
        oops_free(f->path);
        f->path = NULL;
    }
    return SQLITE_OK;
}

/*
 * **A short read is zero-filled and reported as `SQLITE_IOERR_SHORT_READ`, not as an
 * error.** SQLite relies on that distinction: reading past the end of a growing
 * database is normal, and the pager treats the tail as zeroes. Returning a plain
 * `SQLITE_IOERR` would turn an ordinary read into a corrupt-database report.
 */
static int oops_vfs_read(sqlite3_file *pFile, void *buf, int amt,
                         sqlite3_int64 offset) {
    oops_sqlite_file *f = (oops_sqlite_file *)pFile;
    int64_t got;

    if (oops_fs_seek(f->fd, (int64_t)offset, OOPS_SEEK_SET) < 0) {
        return SQLITE_IOERR_READ;
    }
    got = oops_fs_read(f->fd, buf, (size_t)amt);
    if (got < 0) {
        return SQLITE_IOERR_READ;
    }
    if (got < (int64_t)amt) {
        memset((char *)buf + got, 0, (size_t)((int64_t)amt - got));
        return SQLITE_IOERR_SHORT_READ;
    }
    return SQLITE_OK;
}

/* A short write is an error - unlike a short read there is no sense in which it is
 * expected. */
static int oops_vfs_write(sqlite3_file *pFile, const void *buf, int amt,
                          sqlite3_int64 offset) {
    oops_sqlite_file *f = (oops_sqlite_file *)pFile;
    int64_t put;

    if (oops_fs_seek(f->fd, (int64_t)offset, OOPS_SEEK_SET) < 0) {
        return SQLITE_IOERR_WRITE;
    }
    put = oops_fs_write(f->fd, buf, (size_t)amt);
    if (put != (int64_t)amt) {
        return SQLITE_IOERR_WRITE;
    }
    return SQLITE_OK;
}

/* See the header comment: this reports failure rather than pretending. */
static int oops_vfs_truncate(sqlite3_file *pFile, sqlite3_int64 size) {
    (void)pFile;
    (void)size;
    return SQLITE_IOERR_TRUNCATE;
}

/* No `fsync` on this platform - see the header comment for what that costs. */
static int oops_vfs_sync(sqlite3_file *pFile, int flags) {
    (void)pFile;
    (void)flags;
    return SQLITE_OK;
}

static int oops_vfs_file_size(sqlite3_file *pFile, sqlite3_int64 *pSize) {
    oops_sqlite_file *f = (oops_sqlite_file *)pFile;
    const int64_t here = oops_fs_seek(f->fd, 0, OOPS_SEEK_CUR);
    const int64_t end = oops_fs_seek(f->fd, 0, OOPS_SEEK_END);

    if (here < 0 || end < 0) {
        return SQLITE_IOERR_FSTAT;
    }
    /* Put the descriptor back: SQLite does not expect a size query to move it, and
     * every read and write here seeks first - but a future one that did not would
     * corrupt silently. */
    if (oops_fs_seek(f->fd, here, OOPS_SEEK_SET) < 0) {
        return SQLITE_IOERR_FSTAT;
    }
    *pSize = (sqlite3_int64)end;
    return SQLITE_OK;
}

/* One process, so there is nothing to arbitrate. See the header comment. */
static int oops_vfs_lock(sqlite3_file *pFile, int level) {
    (void)pFile;
    (void)level;
    return SQLITE_OK;
}
static int oops_vfs_unlock(sqlite3_file *pFile, int level) {
    (void)pFile;
    (void)level;
    return SQLITE_OK;
}
static int oops_vfs_check_reserved_lock(sqlite3_file *pFile, int *pResOut) {
    (void)pFile;
    *pResOut = 0; /* nobody else can be holding one */
    return SQLITE_OK;
}

/* `SQLITE_NOTFOUND` is the documented answer for an opcode a VFS does not implement,
 * and SQLite treats it as "no opinion" rather than as a failure. */
static int oops_vfs_file_control(sqlite3_file *pFile, int op, void *pArg) {
    (void)pFile;
    (void)op;
    (void)pArg;
    return SQLITE_NOTFOUND;
}

/* 512 is SQLite's own default and the value its unix backend reports unless the
 * filesystem says otherwise. Nothing here can ask, so the default is the honest answer.
 */
static int oops_vfs_sector_size(sqlite3_file *pFile) {
    (void)pFile;
    return 512;
}

/*
 * Zero: no guarantees claimed.
 *
 * The flags here advertise atomicity properties - `SQLITE_IOCAP_ATOMIC4K` and friends -
 * that let SQLite skip journalling. Claiming one this device does not have trades a
 * journal for silent corruption on an interrupted write, so nothing is claimed.
 */
static int oops_vfs_device_characteristics(sqlite3_file *pFile) {
    (void)pFile;
    return 0;
}

static sqlite3_io_methods const oops_io_methods = {
    1, /* iVersion */
    oops_vfs_close, oops_vfs_read, oops_vfs_write, oops_vfs_truncate, oops_vfs_sync,
    oops_vfs_file_size, oops_vfs_lock, oops_vfs_unlock, oops_vfs_check_reserved_lock,
    oops_vfs_file_control, oops_vfs_sector_size, oops_vfs_device_characteristics,
    0, /* xShmMap    - WAL only, and this build omits WAL */
    0, /* xShmLock   */
    0, /* xShmBarrier */
    0, /* xShmUnmap  */
    /* `iVersion` is 1, so SQLite never reads past `xDeviceCharacteristics`. The later
     * fields are still written out: leaving them off is a
     * `-Wmissing-field-initializers` error under this tree's warning set, and naming
     * them says they were considered rather than forgotten. */
    0, /* xFetch     - the memory-mapped read path, which needs mmap */
    0  /* xUnfetch   */
};

/* ---------------------------------------------------------------------------
 * The VFS
 * ------------------------------------------------------------------------- */

static char *oops_strdup_heap(const char *s) {
    const size_t n = strlen(s) + 1u;
    char *p = (char *)oops_malloc(n);
    if (p != NULL) {
        memcpy(p, s, n);
    }
    return p;
}

/*
 * A temporary name, for `xOpen` with a NULL path.
 *
 * SQLite asks for one when it needs a scratch file it will delete itself. The counter
 * makes them distinct within a run; the process is alone on its directory, so that is
 * enough.
 */
static unsigned oops_temp_counter = 0;

static void oops_temp_name(char *out, int n) {
    const unsigned id = ++oops_temp_counter;
    sqlite3_snprintf(n, out, "%s/etilqs_%08x_%08x", OOPS_SQLITE_TEMP_DIR,
                     (unsigned)(oops_time_get_ns() & 0xffffffffu), id);
}

static int oops_vfs_open(sqlite3_vfs *vfs, const char *zName, sqlite3_file *pFile,
                         int flags, int *pOutFlags) {
    oops_sqlite_file *f = (oops_sqlite_file *)pFile;
    char tmp[256];
    const char *path = zName;
    int oflags = 0;
    int fd;

    memset(f, 0, sizeof(*f));
    f->fd = -1;

    if (path == NULL) {
        oops_temp_name(tmp, (int)sizeof(tmp));
        path = tmp;
    }

    if (flags & SQLITE_OPEN_READWRITE) {
        oflags |= OOPS_O_RDWR;
    } else {
        oflags |= OOPS_O_RDONLY;
    }
    if (flags & SQLITE_OPEN_CREATE) {
        oflags |= OOPS_O_CREAT;
    }

    fd = oops_fs_open(path, oflags, 0644);
    if (fd < 0 && (flags & SQLITE_OPEN_READWRITE) && !(flags & SQLITE_OPEN_CREATE)) {
        /* SQLite opens read-write and falls back to read-only itself when it is told it
         * cannot; telling it early saves a round trip and is what the unix backend
         * does. */
        fd = oops_fs_open(path, OOPS_O_RDONLY, 0644);
        if (fd >= 0 && pOutFlags != NULL) {
            flags = (flags & ~SQLITE_OPEN_READWRITE) | SQLITE_OPEN_READONLY;
        }
    }
    if (fd < 0) {
        return SQLITE_CANTOPEN;
    }

    f->pMethods = &oops_io_methods;
    f->fd = fd;
    f->delete_on_close = (flags & SQLITE_OPEN_DELETEONCLOSE) ? 1 : 0;
    /* The path is kept only when it will be needed at close; copying every path would
     * allocate on every open for nothing. */
    f->path = f->delete_on_close ? oops_strdup_heap(path) : NULL;
    if (f->delete_on_close && f->path == NULL) {
        oops_fs_close(fd);
        f->fd = -1;
        return SQLITE_NOMEM;
    }

    if (pOutFlags != NULL) {
        *pOutFlags = flags;
    }
    return SQLITE_OK;
}

/* `dirSync` is ignored for the same reason `xSync` is: there is nothing to force. */
static int oops_vfs_delete(sqlite3_vfs *vfs, const char *zName, int dirSync) {
    (void)vfs;
    (void)dirSync;
    if (oops_fs_unlink(zName) != 0) {
        /* A file that was not there is not a failure - SQLite deletes journals
         * speculatively. */
        return oops_fs_exists(zName) ? SQLITE_IOERR_DELETE : SQLITE_OK;
    }
    return SQLITE_OK;
}

/*
 * **Every existing file answers yes to `SQLITE_ACCESS_READWRITE`.** There are no
 * permissions on this platform to consult, so "can I write it" and "is it there" are
 * the same question. The alternative - answering no - would make SQLite open every
 * database read-only.
 */
static int oops_vfs_access(sqlite3_vfs *vfs, const char *zName, int flags,
                           int *pResOut) {
    (void)vfs;
    (void)flags;
    *pResOut = oops_fs_exists(zName) ? 1 : 0;
    return SQLITE_OK;
}

/*
 * There are no symlinks, no `..` and no working directory to resolve against here, so a
 * path is already full if it starts at the root. A relative one is joined to the
 * title's directory rather than rejected, because SQLite passes through whatever the
 * caller opened with.
 */
static int oops_vfs_full_pathname(sqlite3_vfs *vfs, const char *zPath, int nOut,
                                  char *zOut) {
    (void)vfs;
    if (zPath[0] == '/') {
        sqlite3_snprintf(nOut, zOut, "%s", zPath);
    } else {
        sqlite3_snprintf(nOut, zOut, "%s/%s", OOPS_SQLITE_TEMP_DIR, zPath);
    }
    return SQLITE_OK;
}

/*
 * Loadable extensions are not supported, and these four are the documented way to say
 * so: SQLite checks `xDlOpen` for NULL and reports "unable to open shared library" to
 * the caller. Supplying stubs that fail would be the same answer with more code.
 */
#define oops_vfs_dlopen 0
#define oops_vfs_dlerror 0
#define oops_vfs_dlsym 0
#define oops_vfs_dlclose 0

/*
 * **Randomness, and it is not cryptographic.** SQLite uses this to seed its own PRNG,
 * which names temporary files and salts rollback journals - it is not used for anything
 * an attacker sees. The nanosecond clock is what the SDK offers; a caller needing real
 * entropy should not be using a VFS to get it.
 */
static int oops_vfs_randomness(sqlite3_vfs *vfs, int nByte, char *zOut) {
    int i;
    uint64_t x = oops_time_get_ns() ^ (uint64_t)(uintptr_t)zOut;

    (void)vfs;
    for (i = 0; i < nByte; i++) {
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        x *= 2685821657736338717ull;
        zOut[i] = (char)(x >> 33);
    }
    return nByte;
}

static int oops_vfs_sleep(sqlite3_vfs *vfs, int microseconds) {
    (void)vfs;
    if (microseconds > 0) {
        oops_time_sleep_us((uint32_t)microseconds);
    }
    return microseconds;
}

/*
 * The Julian day number, which is what `xCurrentTime` is defined to return.
 *
 * 2440587.5 is the Julian day of the Unix epoch. The clock underneath is the platform's
 * wall clock; `oops-sdk`'s time is UTC, which is what this wants.
 */
static int oops_vfs_current_time(sqlite3_vfs *vfs, double *pNow) {
    (void)vfs;
    *pNow = 2440587.5 + (double)oops_time_get_ms() / 86400000.0;
    return SQLITE_OK;
}

/*
 * SQLite calls this to put a human-readable reason for the last failure into a buffer.
 * The SDK's filesystem answers a sign rather than a reason, so there is nothing to
 * report and this writes an empty string - which is what the documentation says a VFS
 * with no detail should do.
 */
static int oops_vfs_get_last_error(sqlite3_vfs *vfs, int nBuf, char *zBuf) {
    (void)vfs;
    if (nBuf > 0 && zBuf != NULL) {
        zBuf[0] = '\0';
    }
    return 0;
}

static sqlite3_vfs oops_vfs = {
    1,                             /* iVersion */
    (int)sizeof(oops_sqlite_file), /* szOsFile */
    512,                           /* mxPathname */
    0,                             /* pNext */
    "oops",                        /* zName */
    0,                             /* pAppData */
    oops_vfs_open, oops_vfs_delete, oops_vfs_access, oops_vfs_full_pathname,
    oops_vfs_dlopen, oops_vfs_dlerror, oops_vfs_dlsym, oops_vfs_dlclose,
    oops_vfs_randomness, oops_vfs_sleep, oops_vfs_current_time, oops_vfs_get_last_error,
    /* `iVersion` is 1, so SQLite never reads past `xGetLastError`. Written out for the
     * reason the io-methods table gives: the warning set here treats a missing
     * initialiser as an error, and naming them says they were considered. */
    0, /* xCurrentTimeInt64 (v2) */
    0, /* xSetSystemCall   (v3) */
    0, /* xGetSystemCall   (v3) */
    0  /* xNextSystemCall  (v3) */
};

/* ---------------------------------------------------------------------------
 * Mutexes
 *
 * **These are real, and they have to be.** Craft drives one SQLite connection from two
 * threads - `db.c` runs a writer on a worker (`db_worker_run`) while the main thread
 * reads - and its own locking covers 8 of the 21 functions that touch SQLite.
 * `SQLITE_THREADSAFE=0` would therefore be a data race rather than an optimisation.
 * ------------------------------------------------------------------------- */

struct sqlite3_mutex {
    oops_mutex_t m;
    int valid;
};

/*
 * SQLite's static mutexes, which it asks for by number rather than allocating.
 *
 * The count is `SQLITE_MUTEX_STATIC_LRU2 + 1`, which is the highest this version names,
 * with the two dynamic types (`FAST` and `RECURSIVE`, 0 and 1) occupying the first two
 * slots unused. Sizing it from the macro rather than a literal means a SQLite bump that
 * adds one is a compile error here instead of an out-of-bounds write.
 */
#define OOPS_SQLITE_STATIC_COUNT (SQLITE_MUTEX_STATIC_LRU2 + 1)
static struct sqlite3_mutex oops_static_mutexes[OOPS_SQLITE_STATIC_COUNT];

static int oops_mutex_init_all(void) {
    int i;
    for (i = 0; i < OOPS_SQLITE_STATIC_COUNT; i++) {
        if (!oops_static_mutexes[i].valid) {
            /* Static mutexes must be recursive: SQLite enters some of them
             * re-entrantly. */
            if (oops_mutex_init_recursive(&oops_static_mutexes[i].m, "sqlite") != 0) {
                return SQLITE_ERROR;
            }
            oops_static_mutexes[i].valid = 1;
        }
    }
    return SQLITE_OK;
}

static int oops_mutex_end_all(void) {
    int i;
    for (i = 0; i < OOPS_SQLITE_STATIC_COUNT; i++) {
        if (oops_static_mutexes[i].valid) {
            oops_mutex_destroy(&oops_static_mutexes[i].m);
            oops_static_mutexes[i].valid = 0;
        }
    }
    return SQLITE_OK;
}

static sqlite3_mutex *oops_mutex_alloc(int type) {
    struct sqlite3_mutex *p;

    if (type != SQLITE_MUTEX_FAST && type != SQLITE_MUTEX_RECURSIVE) {
        if (type < 0 || type >= OOPS_SQLITE_STATIC_COUNT) {
            return 0;
        }
        return &oops_static_mutexes[type];
    }

    p = (struct sqlite3_mutex *)oops_malloc(sizeof(*p));
    if (p == NULL) {
        return 0;
    }
    /*
     * **Both dynamic kinds are recursive, deliberately.** `SQLITE_MUTEX_FAST` only
     * promises that SQLite will not re-enter it, so a recursive mutex satisfies it too;
     * the reverse is not true, and getting the two the wrong way round is a deadlock
     * rather than an error. The cost is a counter.
     */
    if (oops_mutex_init_recursive(&p->m, "sqlite") != 0) {
        oops_free(p);
        return 0;
    }
    p->valid = 1;
    return p;
}

static void oops_mutex_free(sqlite3_mutex *p) {
    /* A static mutex is never freed - SQLite does not free them, and this guards the
     * case anyway because freeing one would leave the next `xMutexAlloc` handing out a
     * destroyed lock. */
    if (p == NULL || (p >= &oops_static_mutexes[0] &&
                      p < &oops_static_mutexes[OOPS_SQLITE_STATIC_COUNT])) {
        return;
    }
    oops_mutex_destroy(&p->m);
    p->valid = 0;
    oops_free(p);
}

static void oops_mutex_enter(sqlite3_mutex *p) {
    if (p != NULL) {
        oops_mutex_lock(&p->m);
    }
}

static int oops_mutex_try(sqlite3_mutex *p) {
    if (p == NULL) {
        return SQLITE_OK;
    }
    return oops_mutex_trylock(&p->m) == 0 ? SQLITE_OK : SQLITE_BUSY;
}

static void oops_mutex_leave(sqlite3_mutex *p) {
    if (p != NULL) {
        oops_mutex_unlock(&p->m);
    }
}

/*
 * **These two are only ever called from inside `assert()`.** SQLite has no way to ask a
 * mutex who owns it here, so neither can answer honestly - and both return "yes, that
 * is fine", which is what every implementation without ownership tracking does. The
 * consequence is precise and worth naming: SQLite's internal locking assertions become
 * vacuous in a `SQLITE_DEBUG` build. They cost nothing in the build that ships, where
 * `NDEBUG` removes the calls entirely.
 */
static int oops_mutex_held(sqlite3_mutex *p) {
    (void)p;
    return 1;
}
static int oops_mutex_notheld(sqlite3_mutex *p) {
    (void)p;
    return 1;
}

static sqlite3_mutex_methods const oops_mutex_methods = {
    oops_mutex_init_all, oops_mutex_end_all, oops_mutex_alloc,
    oops_mutex_free,     oops_mutex_enter,   oops_mutex_try,
    oops_mutex_leave,    oops_mutex_held,    oops_mutex_notheld};

/* ---------------------------------------------------------------------------
 * Initialisation
 * ------------------------------------------------------------------------- */

/*
 * `sqlite3_os_init` is SQLite's hook, called from `sqlite3_initialize`. Registering the
 * VFS is all it does - the mutex methods cannot be installed from here, because
 * `sqlite3_config` refuses to run once initialisation has started. That is what
 * `oops_sqlite_init` below is for.
 */
int sqlite3_os_init(void) {
    return sqlite3_vfs_register(&oops_vfs, 1 /* make it the default */);
}

int sqlite3_os_end(void) {
    return SQLITE_OK;
}

/*
 * **Call this before the first SQLite call of any kind.**
 *
 * `sqlite3_config` is only legal before `sqlite3_initialize`, and almost every public
 * SQLite function calls `sqlite3_initialize` on the way in - so "before `sqlite3_open`"
 * is not good enough if anything touched SQLite first.
 *
 * It is idempotent: a second call finds SQLite already initialised, `sqlite3_config`
 * answers `SQLITE_MISUSE`, and that is reported rather than swallowed, because the
 * difference between "configured" and "too late to configure" is the difference between
 * real mutexes and the no-op ones this build would otherwise link.
 */
int oops_sqlite_init(void) {
    int rc = sqlite3_config(SQLITE_CONFIG_MUTEX, &oops_mutex_methods);
    if (rc != SQLITE_OK) {
        return rc;
    }
    return sqlite3_initialize();
}
