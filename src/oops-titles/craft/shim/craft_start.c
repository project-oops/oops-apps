/*
 * The payload entry point.
 *
 * A payload is called rather than spawned: there is no `main` the loader runs and no argv to hand
 * it. Craft's own `main` is in `upstream/src/main.c`, so this calls it - which is the whole
 * adaptation, and the reason this is a short file rather than a patch renaming `main`.
 *
 * # What has to happen before `main`
 *
 * **SQLite's mutex implementation, and it has to be first.** `oops_sqlite_init` installs it
 * through `sqlite3_config`, which SQLite refuses once it has initialised itself - and nearly every
 * public SQLite function initialises it on the way in. Craft's `db_init` calls `sqlite3_open`, so
 * by then it is already too late. Getting this wrong does not fail: SQLite carries on with the
 * no-op mutexes the build links, and Craft drives one connection from two threads.
 *
 * **The writable directory has to exist.** `/app0` is the read-only package; the database goes to
 * `/data/craft`, which nothing creates on a first run. `oops_fs_mkdir` on an existing directory is
 * not an error worth reporting, so the return is logged rather than acted on.
 *
 * # Craft's argv
 *
 * `main(argc, argv)` reads an optional server host and port from it. One argument means
 * single-player, which is the only mode this port has - `auth.c` is not compiled and `client.c`
 * stays inert. `argv[0]` is a real path rather than a bare name because Craft passes it to nothing
 * that would care, and a plausible one costs nothing if that ever changes.
 */
#include "oops/fs.h"
/* For `payload_args_t`, which the entry signature names - the same include `neverball`'s entry
 * point uses for it. */
#include "oops/syscall.h"
#include "oops/system.h"

#include "oops_sqlite.h"
#include "sqlite3.h"

int main(int argc, char **argv);

/* Declared before it is defined because this file is ours and is held to the repository's warning
   set - `-Wmissing-prototypes` among them - unlike upstream's sources. */
int craft_start(const payload_args_t *args);

__attribute__((visibility("default"))) int craft_start(const payload_args_t *args) {
    static char arg0[] = "/app0/craft";
    char *argv[2] = {arg0, 0};
    int rc;

    (void)args;

    oops_log_info("CRFT", "entry");

    /* The database's directory. `/app0` is read-only, so this is where Craft's world goes; it
     * matches `OOPS_SQLITE_TEMP_DIR` and the `DB_PATH` the Makefile sets. */
    (void)oops_fs_mkdir("/data/craft", 0755);

    /*
     * **A failure here is fatal and is reported, not swallowed.** The only thing that can go wrong
     * is that something already initialised SQLite, and the consequence - real-looking mutexes
     * that do nothing, on a database two threads share - is exactly the kind of fault that would
     * be blamed on the port months later. Better to refuse to start.
     */
    rc = oops_sqlite_init();
    if (rc != SQLITE_OK) {
        oops_log_error("CRFT", "oops_sqlite_init failed (%d); refusing to run with unsafe mutexes",
                       rc);
        return rc;
    }

    return main(1, argv);
}
