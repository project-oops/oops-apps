/*
 * SQLite's platform layer, over oops-sdk.
 *
 * Built with -DSQLITE_OS_OTHER=1, so this file is all SQLite knows about the machine: a
 * VFS, a mutex implementation, and the sqlite3_os_init hook that registers them. The
 * unix backend needs a struct stat with real st_dev, st_ino and st_nlink
 * (sqlite3.c:24366, sqlite3.c:24419), which this platform does not provide.
 *
 * Locking succeeds without doing anything: a payload is one process, and concurrency
 * within it is the mutex layer. xSync returns OK because the SDK has no fsync, so the
 * last writes can be lost on power loss. xTruncate fails with SQLITE_IOERR_TRUNCATE
 * because the SDK cannot shorten a file; journal_mode=DELETE never reaches it.
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
 * A short read is zero-filled and reported as SQLITE_IOERR_SHORT_READ, not as an error:
 * the pager reads past the end of a growing database and treats the tail as zeroes.
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

/* A short write is an error. */
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

/* The SDK cannot shorten a file, so truncation reports failure (see the header). */
static int oops_vfs_truncate(sqlite3_file *pFile, sqlite3_int64 size) {
    (void)pFile;
    (void)size;
    return SQLITE_IOERR_TRUNCATE;
}

/* No fsync on this platform (see the header). */
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
    /* Put the descriptor back: SQLite does not expect a size query to move it. */
    if (oops_fs_seek(f->fd, here, OOPS_SEEK_SET) < 0) {
        return SQLITE_IOERR_FSTAT;
    }
    *pSize = (sqlite3_int64)end;
    return SQLITE_OK;
}

/* One process, so there is nothing to arbitrate (see the header). */
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

/* 512 is SQLite's own default; the SDK cannot report the device's sector size. */
static int oops_vfs_sector_size(sqlite3_file *pFile) {
    (void)pFile;
    return 512;
}

/*
 * No atomicity guarantees are claimed. SQLITE_IOCAP_ATOMIC4K and its kin let SQLite
 * skip journalling, which corrupts data on an interrupted write if the claim is false.
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
    /* iVersion is 1, so SQLite never reads past xDeviceCharacteristics. The later
     * fields are written out for -Wmissing-field-initializers. */
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
 * makes names distinct within a run, and the process is alone on its directory.
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
        /* Falls back to read-only here, as the unix backend does, rather than making
         * SQLite retry. */
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
    /* The path is kept only when it is needed at close. */
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
        /* A missing file is not a failure: SQLite deletes journals speculatively. */
        return oops_fs_exists(zName) ? SQLITE_IOERR_DELETE : SQLITE_OK;
    }
    return SQLITE_OK;
}

/*
 * Every existing file answers yes to SQLITE_ACCESS_READWRITE: there are no permissions
 * to consult, so "writable" and "exists" are the same question.
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
 * Loadable extensions are not supported. SQLite checks xDlOpen for NULL and reports
 * "unable to open shared library" to the caller.
 */
#define oops_vfs_dlopen 0
#define oops_vfs_dlerror 0
#define oops_vfs_dlsym 0
#define oops_vfs_dlclose 0

/*
 * Non-cryptographic randomness from the nanosecond clock. SQLite seeds its own PRNG
 * with it, which names temporary files and salts rollback journals.
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
 * 2440587.5 is the Julian day of the Unix epoch. oops-sdk's wall clock is UTC.
 */
static int oops_vfs_current_time(sqlite3_vfs *vfs, double *pNow) {
    (void)vfs;
    *pNow = 2440587.5 + (double)oops_time_get_ms() / 86400000.0;
    return SQLITE_OK;
}

/*
 * The reason for the last failure. The SDK's filesystem answers a sign rather than a
 * reason, so this writes an empty string, as SQLite documents for a VFS with no detail.
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
    /* iVersion is 1, so SQLite never reads past xGetLastError. The later fields are
     * written out for -Wmissing-field-initializers. */
    0, /* xCurrentTimeInt64 (v2) */
    0, /* xSetSystemCall   (v3) */
    0, /* xGetSystemCall   (v3) */
    0  /* xNextSystemCall  (v3) */
};

/* ---------------------------------------------------------------------------
 * Mutexes
 *
 * Real mutexes: Craft drives one connection from two threads (db.c runs a writer in
 * db_worker_run while the main thread reads), so SQLITE_THREADSAFE=0 would race.
 * ------------------------------------------------------------------------- */

struct sqlite3_mutex {
    oops_mutex_t m;
    int valid;
};

/*
 * SQLite's static mutexes, which it asks for by number rather than allocating.
 *
 * Indexed by type up to SQLITE_MUTEX_STATIC_LRU2; the first two slots, the dynamic
 * types FAST and RECURSIVE, are unused.
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
    /* Both dynamic kinds are recursive: SQLITE_MUTEX_FAST only promises SQLite will
     * not re-enter it, so a recursive mutex satisfies both. */
    if (oops_mutex_init_recursive(&p->m, "sqlite") != 0) {
        oops_free(p);
        return 0;
    }
    p->valid = 1;
    return p;
}

static void oops_mutex_free(sqlite3_mutex *p) {
    /* A static mutex is never freed, or the next xMutexAlloc would hand out a
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
 * Called only from assert(). Without ownership tracking both answer "yes", so SQLite's
 * locking assertions are vacuous in a SQLITE_DEBUG build; NDEBUG removes the calls.
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
 * SQLite's hook, called from sqlite3_initialize. It only registers the VFS:
 * sqlite3_config refuses once initialisation has started, so oops_sqlite_init installs
 * the mutex methods.
 */
int sqlite3_os_init(void) {
    return sqlite3_vfs_register(&oops_vfs, 1 /* make it the default */);
}

int sqlite3_os_end(void) {
    return SQLITE_OK;
}

/*
 * Called before the first SQLite call of any kind: sqlite3_config is legal only before
 * sqlite3_initialize, which almost every public SQLite function runs on the way in.
 * A late or second call returns SQLITE_MISUSE, which means no-op mutexes.
 */
int oops_sqlite_init(void) {
    int rc = sqlite3_config(SQLITE_CONFIG_MUTEX, &oops_mutex_methods);
    if (rc != SQLITE_OK) {
        return rc;
    }
    return sqlite3_initialize();
}
