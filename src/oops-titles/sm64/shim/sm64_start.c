/*
 * The payload entry point.
 *
 * The loader runs no main and passes no argv. This calls upstream's main in
 * src/pc/pc_main.c, which is a C translation unit, so the name is unmangled and a C
 * shim reaches it.
 *
 * argv[0] keeps argc > 0 for upstream's option parsing and gives anything reading it an
 * answer rather than NULL. No data path comes from argv: the assets are compiled in as
 * zero arrays, to be filled from a ROM the player supplies.
 *
 * HOME is exported for src/pc/configfile.c, the environment starting empty on this
 * platform.
 */
#include "oops/fs.h"
/* For `payload_args_t`, which the entry signature names. */
#include "oops/syscall.h"
#include "oops/system.h"
/* `setenv`; see `oops-sdk/include/libc/stdlib.h` on why the environment starts empty.
 */
#include <stdlib.h>

int main(int argc, char **argv);

/* Prototype for `-Wmissing-prototypes`. `app.mk` derives the entry symbol from the app
   name and lld only *warns* on a mismatch, so the name is worth reading twice. */
int sm64_start(const payload_args_t *args);

__attribute__((visibility("default"))) int sm64_start(const payload_args_t *args) {
    static char arg0[] = "/app0/sm64";
    char *argv[] = {arg0, 0};

    (void)args;

    oops_log_init(OOPS_APP_ID);

    /*
     * The disk sink, because the kernel log drops bursts and this game's start-up is
     * one.
     * `/data/<app id>` must exist - the sink falls back to it with no USB stick
     * present.
     */
    (void)oops_fs_mkdir("/data/" OOPS_APP_ID, 0755);
    (void)oops_log_enable_disk_sink(OOPS_APP_ID, 0);

    oops_log_info("SM64", "entry");

    /* Writable, for `sm64config.txt` - nothing creates it on a first run. */
    (void)oops_fs_mkdir(OOPS_POSIX_HOME, 0755);
    if (setenv("HOME", OOPS_POSIX_HOME, 1) != 0) {
        /* Not fatal: the game runs and only settings fail to persist. Said out loud
           because the symptom - options that reset every launch - does not name this.
         */
        oops_log_error("SM64", "HOME could not be set; settings will not persist");
    }

    return main(1, argv);
}
