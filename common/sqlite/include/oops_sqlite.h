/*
 * The one call a title makes into `common/sqlite`.
 *
 * The rest of common/sqlite is SQLite's own interface - a VFS and a mutex
 * implementation found through sqlite3_os_init. This call has to happen before SQLite
 * initialises itself, and SQLite has no hook that early.
 */
#ifndef OOPS_SQLITE_H
#define OOPS_SQLITE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Installs the mutex implementation and initialises SQLite. Returns an `SQLITE_*` code.
 *
 * Call it before the first SQLite call of any kind, not merely before `sqlite3_open`:
 * nearly every public SQLite function calls `sqlite3_initialize`, after which this
 * returns `SQLITE_MISUSE` and SQLite runs with no-op mutexes.
 */
int oops_sqlite_init(void);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_SQLITE_H */
