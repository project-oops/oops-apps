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
    /* Craft reads argv only for an optional server host and port, so this is never opened - but it
     * said `/app0/craft`, and `/app0` is a path this title has now established it cannot use. A
     * plausible-but-wrong path in argv[0] is how the next reader gets misled. */
    static char arg0[] = CRAFT_DATA_DIR "/craft";
    char *argv[2] = {arg0, 0};
    int verbose = 0; /* `verbose=1` in <data dir>/oops-log; see below for why it is not default */
    int rc;

    (void)args;

    /*
     * **The log level the launch asked for, and nothing else was reading it.**
     *
     * `oops_log_init` is what applies `/app0/oops-log`'s `system=` line to the global level.
     * Nothing in any payload calls it - a tree-wide grep finds only `oops-sdk`'s own unit tests -
     * so the global level has always been the compiled-in default and every `oops_log_debug` in
     * the SDK was unreachable from a title. The `gl` channel works because `gl_context.c:909` asks
     * `oops_log_channel_level("gl", ...)` for itself; the subsystems that do not ask - AGC among
     * them - had no way to be turned up at all.
     *
     * That cost this port a run: the AGC display failed, `agc_log` recorded why at debug level,
     * and the reason was discarded before it reached the kernel log.
     *
     * It belongs in the SDK's payload startup so that every title gets it rather than each one
     * remembering; raised on the bus. Here until then.
     *
     * **It reads the title's own directory, not `/app0`.** `oops_log_init` looks in
     * `/app0/oops-log`, and a homebrew title cannot open `/app0` at all - the same finding that
     * moved `CRAFT_DATA_DIR`. So the level is taken from `<data dir>/oops-log` here and applied by
     * hand; the SDK call is left in for the app-id it records.
     */
    oops_log_init(OOPS_APP_ID);
    {
        char v[16];
        verbose = (oops_config_value(CRAFT_DATA_DIR "/oops-log", "verbose", v, sizeof(v)) == 0 &&
                   v[0] == '1');
        if (verbose) {
            oops_log_set_level(OOPS_LOG_DEBUG);
        }
    }

    /*
     * **The kernel log drops bursts, and the display's diagnostics are a burst.**
     *
     * `agc_display_open_adopting` records 33 points in a few microseconds. Three runs of this
     * title logged none of them while `[INPUT]` and `[GL]` lines either side came through, and the
     * emit is in the shipped binary - so they are produced and lost in transport, not filtered.
     *
     * The sink catches them, because it writes by direct syscall rather than through the kernel
     * log's transport. It probes USB first and falls back to `/data/<app_name>`, so that directory
     * has to exist *before* the call - which is why the `mkdir` is here rather than below with the
     * database's.
     *
     * **It is off unless the launch asks, and that is about the level more than the sink.** The
     * sink itself is cheap now: `oops-sdk` batches everything below WARN and flushes a warning
     * immediately. What still costs is *debug level at all* - every emitted line is a `klog` and a
     * write to fd 1 whatever the sink does, and the AGC back end logs a line per flip. Craft's own
     * HUD read `0fps` with debug on. Same shape as the 174ms of a 200ms Neverball frame that turned
     * out to be `gl=debug` printing counters.
     *
     * Turn it on for a run by putting `verbose=1` in `<data dir>/oops-log` and restoring that one
     * file - no rebuild. Leave it off for anything being judged on speed.
     */
    if (verbose) {
        (void)oops_fs_mkdir("/data/" OOPS_APP_ID, 0755);
        (void)oops_log_enable_disk_sink(OOPS_APP_ID, 0);
    }

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
