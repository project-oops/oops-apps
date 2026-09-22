/*
 * The payload entry point.
 *
 * A payload is called, not spawned: there is no `main` the loader runs and no argv to hand it.
 * Neverball's own `main` is in `ball/main.c`, so this calls it - which is the whole adaptation,
 * and the reason it is a few lines rather than a patch renaming `main`.
 *
 * **The working directory matters more here than it did for the other title.** Neverball finds
 * its data by walking upwards and outwards from argv[0] and from a compiled-in default, and the
 * package mounts at `/app0`. `argv[0]` is set accordingly so the search starts in the right
 * place rather than at a path that does not exist.
 */
#include "oops/syscall.h"
#include "oops/system.h"

#include "nb_diag.h"

int main(int argc, char **argv);

/* Declared before it is defined because this file is held to the repository's own warning set -
   `-Wmissing-prototypes` and the rest - unlike upstream's sources, which build into an archive
   with their own flags. */
int nb_start(const payload_args_t *args);

__attribute__((visibility("default"))) int nb_start(const payload_args_t *args) {
    static char arg0[] = "/app0/neverball";
    char *argv[2] = {arg0, 0};

    (void)args;

    oops_log_info("NVRB", "entry");

    /* **The summary belongs here and not in a patch.** The shim already wraps `main`, so the
       point after it returns is ours to use - and `ball/main.c` stays untouched, which is one
       fewer hunk to rebase onto the next upstream revision. */
    {
        const int rc = main(1, argv);
        nb_diag_report();
        return rc;
    }
}
