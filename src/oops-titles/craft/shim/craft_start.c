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
    int rc;

    (void)args;

    /* Applies `system=` from `/app0/oops-log`, so a run can be turned up without a
     * rebuild. */
    oops_log_init(OOPS_APP_ID);

    oops_log_info("CRFT", "entry");

    /*
     * There is no disk sink. It writes outside `/app0`, and reaching outside unmounts
     * `/app0` along with every asset this title reads. The SDK refuses that unless a
     * title calls `oops_system_allow_sandbox_escape`; this one keeps its assets, so the
     * kernel log is the only sink.
     */

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
