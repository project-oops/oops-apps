/*
 * Payload entry point: builds a one-element command line and calls upstream's main
 * (upstream/soh/src/code/main.c), which is a C translation unit, so the name is unmangled.
 *
 * The loader runs no main and passes no argv. argv[0] gives upstream something to report and
 * keeps argc > 0 for its option parsing; the ROM and archives are found by path, not from argv.
 */
#include "oops/fs.h"
#include "oops/syscall.h"
#include "oops/system.h"
#include <stdlib.h>

int main(int argc, char **argv);

/* app.mk derives the entry symbol from the app name; lld only warns on a mismatch. */
int ship_of_harkinian_start(const payload_args_t *args);

__attribute__((visibility("default"))) int
ship_of_harkinian_start(const payload_args_t *args) {
    static char arg0[] = "/app0/soh";
    char *argv[] = {arg0, 0};

    (void)args;

    oops_log_init(OOPS_APP_ID);

    /* The kernel log drops bursts and this start-up is one. The sink falls back to
       /data/<app id>, which has to exist when no USB stick is present. */
    (void)oops_fs_mkdir("/data/" OOPS_APP_ID, 0755);
    (void)oops_log_enable_disk_sink(OOPS_APP_ID, 0);

    oops_log_info("SOH", "entry");

    /* libultraship resolves its config and save paths through HOME, and the environment
       starts empty on this platform. */
    (void)oops_fs_mkdir(OOPS_POSIX_HOME, 0755);
    if (setenv("HOME", OOPS_POSIX_HOME, 1) != 0) {
        oops_log_error("SOH", "HOME could not be set; settings will not persist");
    }

    /* Namespace-scope constructors: .init_array is not walked for this payload, and libc++ and
       libultraship both build dispatch tables in theirs. Idempotent. */
    oops_run_init_array();

    return main(1, argv);
}
