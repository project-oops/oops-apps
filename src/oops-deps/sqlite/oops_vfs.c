/*
 * SQLite's operating-system layer for this platform: a VFS and a mutex implementation.
 *
 * The amalgamation is built with `SQLITE_OS_OTHER=1`, SQLite's own switch for a
 * platform it has no layer for. That leaves three things to the embedder, all here:
 *
 * - **`sqlite3_os_init`/`sqlite3_os_end`**, which register the VFS below as the
 * default.
 * - **The VFS**: files over the SDK's descriptors (`oops_fs_open` and friends, which
 * are the kernel's own), with `ftruncate` and `fsync` from the shared POSIX layer.
 * SQLite's Unix VFS would have wanted `fcntl` byte-range locks, `mmap`, `fchown` and
 * static `pthread` initialisers; none of it is needed by a single process.
 * - **Mutexes** (`SQLITE_MUTEX_APPDEF`), over `oops_mutex_*`.
 *
 * # Locking is a no-op, and that is correct here
 *
 * SQLite's file locks keep *processes* from corrupting a database they share. A payload
 * is one process, and threads inside it are serialised by the mutexes, which are real.
 * So every lock level is granted and `xCheckReservedLock` reports none held - the same
 * answer SQLite's own `unix-none` VFS gives, and for the same reason.
 *
 * # What is not here
 *
 * No shared-memory methods, so WAL mode is refused (`SQLITE_OMIT_WAL`); the rollback
 * journal is used, which is SQLite's default. No `xDlOpen`: loadable extensions are
 * compiled out
 * (`SQLITE_OMIT_LOAD_EXTENSION`).
 */
#include "sqlite3.h"

#include "oops/fs.h"
#include "oops/thread.h"
#include "oops/time.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

/* ---- files ---------------------------------------------------------------------- */

typedef struct {
    sqlite3_file base;
    int fd;
    int delete_on_close;
    char path[512];
} oops_sqlite_file;

static int ovfs_close(sqlite3_file *f) {
    oops_sqlite_file *p = (oops_sqlite_file *)f;
    int rc = oops_fs_close(p->fd);

    if (p->delete_on_close) {
        (void)oops_fs_unlink(p->path);
    }
    return rc == 0 ? SQLITE_OK : SQLITE_IOERR_CLOSE;
}

static int ovfs_read(sqlite3_file *f, void *buf, int amt, sqlite3_int64 off) {
    oops_sqlite_file *p = (oops_sqlite_file *)f;
    int64_t got;

    if (oops_fs_seek(p->fd, off, SEEK_SET) != off) {
        return SQLITE_IOERR_READ;
    }
    got = oops_fs_read(p->fd, buf, (size_t)amt);
    if (got < 0) {
        return SQLITE_IOERR_READ;
    }
    if (got < amt) {
        /* SQLite requires the unread tail zeroed on a short read. */
        memset((char *)buf + got, 0, (size_t)(amt - got));
        return SQLITE_IOERR_SHORT_READ;
    }
    return SQLITE_OK;
}

static int ovfs_write(sqlite3_file *f, const void *buf, int amt, sqlite3_int64 off) {
    oops_sqlite_file *p = (oops_sqlite_file *)f;

    if (oops_fs_seek(p->fd, off, SEEK_SET) != off) {
        return SQLITE_IOERR_WRITE;
    }
    return oops_fs_write(p->fd, buf, (size_t)amt) == amt ? SQLITE_OK
                                                         : SQLITE_IOERR_WRITE;
}

static int ovfs_truncate(sqlite3_file *f, sqlite3_int64 size) {
    return ftruncate(((oops_sqlite_file *)f)->fd, (off_t)size) == 0
               ? SQLITE_OK
               : SQLITE_IOERR_TRUNCATE;
}

static int ovfs_sync(sqlite3_file *f, int flags) {
    (void)flags;
    return fsync(((oops_sqlite_file *)f)->fd) == 0 ? SQLITE_OK : SQLITE_IOERR_FSYNC;
}

static int ovfs_file_size(sqlite3_file *f, sqlite3_int64 *size) {
    oops_sqlite_file *p = (oops_sqlite_file *)f;
    int64_t here = oops_fs_tell(p->fd);
    int64_t end = oops_fs_seek(p->fd, 0, SEEK_END);

    if (end < 0) {
        return SQLITE_IOERR_FSTAT;
    }
    (void)oops_fs_seek(p->fd, here, SEEK_SET);
    *size = end;
    return SQLITE_OK;
}

/* One process: see the header. */
static int ovfs_lock(sqlite3_file *f, int level) {
    (void)f;
    (void)level;
    return SQLITE_OK;
}

static int ovfs_check_reserved(sqlite3_file *f, int *out) {
    (void)f;
    *out = 0;
    return SQLITE_OK;
}

static int ovfs_file_control(sqlite3_file *f, int op, void *arg) {
    (void)f;
    (void)op;
    (void)arg;
    return SQLITE_NOTFOUND;
}

static int ovfs_sector_size(sqlite3_file *f) {
    (void)f;
    return 4096;
}

static int ovfs_device_characteristics(sqlite3_file *f) {
    (void)f;
    return 0;
}

static const sqlite3_io_methods ovfs_io = {
    1,
    ovfs_close,
    ovfs_read,
    ovfs_write,
    ovfs_truncate,
    ovfs_sync,
    ovfs_file_size,
    ovfs_lock,
    ovfs_lock, /* xUnlock: nothing was taken */
    ovfs_check_reserved,
    ovfs_file_control,
    ovfs_sector_size,
    ovfs_device_characteristics,
    0,
    0,
    0,
    0, /* no shared memory: WAL is compiled out */
    0,
    0, /* no memory mapping */
};

/* ---- the VFS ----------------------------------------------------------------------
 */

static int ovfs_open(sqlite3_vfs *vfs, sqlite3_filename name, sqlite3_file *f,
                     int flags, int *out_flags) {
    static unsigned temp_serial;
    oops_sqlite_file *p = (oops_sqlite_file *)f;
    int oflags = (flags & SQLITE_OPEN_READWRITE) ? O_RDWR : O_RDONLY;

    (void)vfs;
    memset(p, 0, sizeof(*p));
    if (flags & SQLITE_OPEN_CREATE) {
        oflags |= O_CREAT;
    }
    if (flags & SQLITE_OPEN_EXCLUSIVE) {
        oflags |= O_EXCL;
    }
    if (name) {
        sqlite3_snprintf((int)sizeof(p->path), p->path, "%s", name);
    } else {
        /* A temporary file SQLite names nothing: one of ours, removed on close. */
        sqlite3_snprintf((int)sizeof(p->path), p->path, "/app0/.sqlite-temp-%u",
                         ++temp_serial);
        oflags |= O_CREAT | O_RDWR;
        flags |= SQLITE_OPEN_DELETEONCLOSE;
    }
    p->delete_on_close = (flags & SQLITE_OPEN_DELETEONCLOSE) != 0;
    p->fd = oops_fs_open(p->path, oflags, 0644);
    if (p->fd < 0) {
        return SQLITE_CANTOPEN;
    }
    p->base.pMethods = &ovfs_io;
    if (out_flags) {
        *out_flags = flags;
    }
    return SQLITE_OK;
}

static int ovfs_delete(sqlite3_vfs *vfs, const char *path, int sync_dir) {
    (void)vfs;
    (void)sync_dir;
    if (oops_fs_unlink(path) == 0 || !oops_fs_exists(path)) {
        return SQLITE_OK;
    }
    return SQLITE_IOERR_DELETE;
}

static int ovfs_access(sqlite3_vfs *vfs, const char *path, int what, int *out) {
    (void)vfs;
    (void)what;
    /* Existence answers all three questions: nothing here is read-only to its owner. */
    *out = oops_fs_exists(path) ? 1 : 0;
    return SQLITE_OK;
}

static int ovfs_full_path(sqlite3_vfs *vfs, const char *path, int size, char *out) {
    (void)vfs;
    /* The SDK resolves a relative path against /app0 itself (`oops_fs_resolve_path`).
     */
    sqlite3_snprintf(size, out, "%s", path);
    return SQLITE_OK;
}

static int ovfs_randomness(sqlite3_vfs *vfs, int n, char *out) {
    uint64_t x = oops_time_get_counter() | 1u;
    int i;

    (void)vfs;
    for (i = 0; i < n; i++) {
        /* xorshift over the clock: seeds SQLite's own PRNG, which is all this is for.
         */
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        out[i] = (char)(x & 0xff);
    }
    return n;
}

static int ovfs_sleep(sqlite3_vfs *vfs, int us) {
    (void)vfs;
    oops_time_sleep_us((uint32_t)us);
    return us;
}

/* Julian day in milliseconds; 210866760000000 is the Unix epoch on that scale. */
static int ovfs_current_time_int64(sqlite3_vfs *vfs, sqlite3_int64 *out) {
    (void)vfs;
    *out = (sqlite3_int64)210866760000000LL +
           (sqlite3_int64)oops_time_get_epoch_seconds() * 1000;
    return SQLITE_OK;
}

static int ovfs_current_time(sqlite3_vfs *vfs, double *out) {
    sqlite3_int64 ms;

    (void)ovfs_current_time_int64(vfs, &ms);
    *out = (double)ms / 86400000.0;
    return SQLITE_OK;
}

static int ovfs_last_error(sqlite3_vfs *vfs, int n, char *buf) {
    (void)vfs;
    if (n > 0) {
        buf[0] = '\0';
    }
    return errno;
}

static sqlite3_vfs ovfs = {
    2,
    (int)sizeof(oops_sqlite_file),
    512,
    0,
    "oops",
    0,
    ovfs_open,
    ovfs_delete,
    ovfs_access,
    ovfs_full_path,
    0,
    0,
    0,
    0, /* no loadable extensions */
    ovfs_randomness,
    ovfs_sleep,
    ovfs_current_time,
    ovfs_last_error,
    ovfs_current_time_int64,
    0,
    0,
    0,
};

int sqlite3_os_init(void) {
    return sqlite3_vfs_register(&ovfs, 1);
}

int sqlite3_os_end(void) {
    return SQLITE_OK;
}

/* ---- mutexes ----------------------------------------------------------------------
 */

typedef struct sqlite3_mutex {
    oops_mutex_t m;
    int id;
} oops_sqlite_mutex;

/* SQLite's static mutexes, SQLITE_MUTEX_STATIC_MAIN through SQLITE_MUTEX_STATIC_VFS3.
 */
#define OOPS_SQLITE_STATIC 12
static oops_sqlite_mutex s_static[OOPS_SQLITE_STATIC];

/* Called once, single-threaded, before any other mutex method. */
static int omx_init(void) {
    int i;

    for (i = 0; i < OOPS_SQLITE_STATIC; i++) {
        if (oops_mutex_init(&s_static[i].m, "sqlite") != 0) {
            return SQLITE_NOMEM;
        }
        s_static[i].id = i + 2;
    }
    return SQLITE_OK;
}

static int omx_end(void) {
    int i;

    for (i = 0; i < OOPS_SQLITE_STATIC; i++) {
        (void)oops_mutex_destroy(&s_static[i].m);
    }
    return SQLITE_OK;
}

static sqlite3_mutex *omx_alloc(int id) {
    oops_sqlite_mutex *p;

    if (id == SQLITE_MUTEX_FAST || id == SQLITE_MUTEX_RECURSIVE) {
        p = sqlite3_malloc((int)sizeof(*p));
        if (!p) {
            return 0;
        }
        if ((id == SQLITE_MUTEX_RECURSIVE ? oops_mutex_init_recursive(&p->m, "sqlite")
                                          : oops_mutex_init(&p->m, "sqlite")) != 0) {
            sqlite3_free(p);
            return 0;
        }
        p->id = id;
        return p;
    }
    if (id - 2 >= 0 && id - 2 < OOPS_SQLITE_STATIC) {
        return &s_static[id - 2];
    }
    return 0;
}

static void omx_free(sqlite3_mutex *p) {
    if (p && (p->id == SQLITE_MUTEX_FAST || p->id == SQLITE_MUTEX_RECURSIVE)) {
        (void)oops_mutex_destroy(&p->m);
        sqlite3_free(p);
    }
}

static void omx_enter(sqlite3_mutex *p) {
    (void)oops_mutex_lock(&p->m);
}

static int omx_try(sqlite3_mutex *p) {
    return oops_mutex_trylock(&p->m) == 0 ? SQLITE_OK : SQLITE_BUSY;
}

static void omx_leave(sqlite3_mutex *p) {
    (void)oops_mutex_unlock(&p->m);
}

/* The mutex implementation SQLite asks for under `SQLITE_MUTEX_APPDEF`. */
sqlite3_mutex_methods const *sqlite3DefaultMutex(void);

sqlite3_mutex_methods const *sqlite3DefaultMutex(void) {
    static const sqlite3_mutex_methods methods = {
        omx_init, omx_end, omx_alloc, omx_free, omx_enter, omx_try, omx_leave, 0, 0,
    };
    return &methods;
}
