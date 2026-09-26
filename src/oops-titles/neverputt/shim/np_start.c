/*
 * The payload entry point.
 *
 * A payload is called, not spawned: the loader runs no `main` and passes no argv. This
 * calls Neverputt's `main` in `putt/main.c`, with `argv[0]` under `/app0`, where the
 * package mounts, so the data search starts there.
 *
 * The write directory is `/app0/.neverball`, created by `config_paths`. Upstream saves
 * only after the main loop (`putt/main.c:386`), which a closed title never reaches.
 * Savedata is not mounted: its fallback leaves the sandbox, which unmaps `/app0`.
 */
#include "oops/syscall.h"
#include "oops/system.h"

#include "haptics.h"

int main(int argc, char **argv);

/* Prototype for `-Wmissing-prototypes`, which this file is built under. */
int np_start(const payload_args_t *args);

__attribute__((visibility("default"))) int np_start(const payload_args_t *args) {
    static char arg0[] = "/app0/neverputt";
    char *argv[2] = {arg0, 0};

    (void)args;

    oops_log_info("NVPT", "entry");

    /* Rumble starts and stops around `main`, so only the bump itself is a patch (see
     * `common/haptics.h`). A failed start makes every haptics call a no-op. */
    oops_haptics_init();

    {
        const int rc = main(1, argv);
        oops_haptics_quit();
        return rc;
    }
}
