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
 *
 * # What this file deliberately does not do
 *
 * Three things were here and were removed, each because the SDK does it for every title and a
 * per-title copy is the thing that makes a second port cost as much as the first:
 *
 *   - **Turning the GL log up.** `/app0/oops-log` holds `channel=level` lines that each
 *     subsystem reads for its own channel (`oops_log_channel_level`); `gl=debug` gets oops-gl's
 *     per-flip accounting here and in every title after it, with no code. Leave it at `warn` for
 *     a run that is being judged on speed - the census is about a hundred kernel-log syscalls a
 *     flip, and on 2026-09-24 that was 174ms of a 200ms frame.
 *   - **Arming a frame capture.** `/app0/oops-capture` takes `frame=N` and an optional `path`,
 *     read when the GL context is created, so any title is capturable at whichever frame the
 *     question needs without a rebuild.
 *   - **Mounting savedata for a writable directory.** The title never needed one: `/app0` is
 *     writable, `config_paths` creates `/app0/.neverball`, and `config_save` fills it with
 *     `neverballrc`, `Scores`, `Replays` and `Screenshots`. What was actually broken is that
 *     upstream calls `config_save` from exactly one place - after the main loop returns
 *     (`ball/main.c:602`) - and a console title is closed or killed rather than quitting, so it
 *     never ran. Patch 0002 writes the config when the name is entered, which is the fix.
 *     Mounting savedata is also destructive here: its fallback reaches `/data` through
 *     `oops_system_escape_sandbox`, and leaving the sandbox takes `/app0` and every shipped
 *     asset with it - measured, `before=1 after=0`, a black screen and `Failure to open
 *     "classic" theme file`. Do not reintroduce it to give this title storage.
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
