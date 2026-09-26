/*
 * The payload entry point.
 *
 * A payload is called, not spawned: the loader runs no `main` and passes no argv. This
 * calls Neverball's `main` in `ball/main.c`, with `argv[0]` under `/app0`, where the
 * package mounts, so the data search starts there.
 *
 * The write directory is `/app0/.neverball`, created by `config_paths`. Upstream
 * saves only after the main loop (`ball/main.c:602`), which a closed title never
 * reaches, so patch 0002 saves when the name is entered. Savedata is not mounted: its
 * fallback leaves the sandbox, which unmaps `/app0` and every shipped asset.
 */
#include "oops/syscall.h"
#include "oops/system.h"

#include "haptics.h"
#include "nb_diag.h"

int main(int argc, char **argv);

/* Prototype for `-Wmissing-prototypes`, which this file is built under. */
int nb_start(const payload_args_t *args);

__attribute__((visibility("default"))) int nb_start(const payload_args_t *args) {
    static char arg0[] = "/app0/neverball";
    char *argv[2] = {arg0, 0};

    (void)args;

    oops_log_info("NVRB", "entry");

    /* Rumble starts and stops around `main`, so only the bump itself is a patch (see
     * `common/haptics.h`). A failed start makes every haptics call a no-op. */
    oops_haptics_init();

    {
        const int rc = main(1, argv);
        /* Stop the pad before the report is written. */
        oops_haptics_quit();
        nb_diag_report();
        return rc;
    }
}
