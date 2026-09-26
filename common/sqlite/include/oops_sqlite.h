/*
 * The one call a title makes into `common/sqlite`.
 *
 * Everything else in that file is SQLite's own interface - a VFS and a mutex
 * implementation that SQLite finds through `sqlite3_os_init`. This is the exception,
 * because it has to happen *before* SQLite initialises itself and SQLite has no hook
 * that early.
 */
#ifndef OOPS_SQLITE_H
#define OOPS_SQLITE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Installs the mutex implementation and initialises SQLite. Returns an `SQLITE_*` code.
 *
 * **Before the first SQLite call of any kind**, not merely before `sqlite3_open`:
 * nearly every public SQLite function calls `sqlite3_initialize` on the way in, and
 * once that has run `sqlite3_config` refuses and this returns `SQLITE_MISUSE`. A caller
 * that ignores the return value gets a working database with no-op mutexes, which is a
 * data race rather than a failure.
 */
int oops_sqlite_init(void);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_SQLITE_H */
