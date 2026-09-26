/*
 * The payload entry point, which calls Craft's own `main` from `upstream/src/main.c`.
 *
 * Before `main` it installs SQLite's mutexes, which must come before any other SQLite
 * call, and creates the writable `/data/craft` directory the database lives in. Craft
 * gets a single argument, which means single-player: `auth.c` is not compiled and
 * `client.c` stays inert.
 */
#include "oops/fs.h"
/* For `payload_args_t`, which the entry signature names. */
#include "oops/syscall.h"
#include "oops/system.h"

#include "oops_sqlite.h"
#include "sqlite3.h"

int main(int argc, char **argv);

/* Declared because shim sources are held to `-Wmissing-prototypes`. */
int craft_start(const payload_args_t *args);

__attribute__((visibility("default"))) int craft_start(const payload_args_t *args) {
    /* Craft reads argv only for an optional server host and port; argv[0] names the
     * title's real directory. */
    static char arg0[] = CRAFT_DATA_DIR "/craft";
    char *argv[2] = {arg0, 0};
    int verbose =
        0; /* `verbose=1` in <data dir>/oops-log; off by default because of its cost */
    int rc;

    (void)args;

    /*
     * `oops_log_init` records the app id, but it reads `/app0/oops-log`, which a
     * homebrew title cannot open. So the debug level is taken from `<data
     * dir>/oops-log` and applied by hand.
     */
    oops_log_init(OOPS_APP_ID);
    {
        char v[16];
        verbose = (oops_config_value(CRAFT_DATA_DIR "/oops-log", "verbose", v,
                                     sizeof(v)) == 0 &&
                   v[0] == '1');
        if (verbose) {
            oops_log_set_level(OOPS_LOG_DEBUG);
        }
    }

    /*
     * The kernel log drops bursts such as the display's open diagnostics; the disk
     * sink writes by direct syscall and keeps them. It falls back to
     * `/data/<app_name>`, so that directory must exist before the call. Verbose is off
     * by default because debug level logs a line per flip and costs the frame rate.
     */
    if (verbose) {
        (void)oops_fs_mkdir("/data/" OOPS_APP_ID, 0755);
        (void)oops_log_enable_disk_sink(OOPS_APP_ID, 0);
    }

    oops_log_info("CRFT", "entry");

    /* The database's directory. `/app0` is read-only, so this is where Craft's world
     * goes; it matches `OOPS_SQLITE_TEMP_DIR` and the `DB_PATH` the Makefile sets. */
    (void)oops_fs_mkdir("/data/craft", 0755);

    /*
     * Fatal: a failure means SQLite was already initialised, and it would run with
     * no-op mutexes on a connection two threads share.
     */
    rc = oops_sqlite_init();
    if (rc != SQLITE_OK) {
        oops_log_error(
            "CRFT", "oops_sqlite_init failed (%d); refusing to run with unsafe mutexes",
            rc);
        return rc;
    }

    return main(1, argv);
}
