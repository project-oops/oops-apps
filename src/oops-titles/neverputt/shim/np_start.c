/*
 * The payload entry point.
 *
 * A payload is called, not spawned: there is no `main` the loader runs and no argv to hand it.
 * Neverputt's own `main` is in `putt/main.c`, so this calls it - which is the whole adaptation,
 * and the reason it is a few lines rather than a patch renaming `main`.
 *
 * `argv[0]` is set because the game finds its data by walking outwards from it and from a
 * compiled-in default, and the package mounts at `/app0`.
 *
 * # What this file deliberately does not do
 *
 * Neverball's shim carries the long version of this note; the short version is that logging,
 * frame capture and storage are the SDK's and a per-title copy of any of them is what makes a
 * second port cost as much as the first.
 *
 *   - `/app0/oops-log` holds `channel=level` lines - `gl=debug` turns on oops-gl's per-flip
 *     accounting with no code here. Leave it at `warn` for a run being judged on speed: the
 *     census is about a hundred kernel-log syscalls a flip, which was 174ms of a 200ms frame
 *     when Neverball was being measured.
 *   - `/app0/oops-capture` takes `frame=N` and an optional `path`.
 *   - Storage needs nothing: `/app0` is writable and `config_paths` creates `/app0/.neverball`
 *     under it. Do not mount savedata to get a writable directory - its fallback reaches `/data`
 *     through `oops_system_escape_sandbox`, and leaving the sandbox takes `/app0` and every
 *     shipped asset with it.
 *
 * **Persistence has a known gap here**, carried over from Neverball and not yet patched:
 * upstream calls `config_save` once, at `putt/main.c:386`, after the main loop returns. A console
 * title is closed or killed rather than quitted, so that line does not run and the settings
 * changed in a session are lost. Neverball's patch 0002 writes the config at the moment the
 * player's name is entered; Neverputt has no name entry, so the equivalent fix wants a different
 * trigger and is deliberately not guessed at before the title has been played.
 */
#include "oops/syscall.h"
#include "oops/system.h"

int main(int argc, char **argv);

/* Declared before it is defined because this file is held to the repository's own warning set -
   `-Wmissing-prototypes` and the rest - unlike upstream's sources, which build into an archive
   with their own flags. */
int np_start(const payload_args_t *args);

__attribute__((visibility("default"))) int np_start(const payload_args_t *args) {
    static char arg0[] = "/app0/neverputt";
    char *argv[2] = {arg0, 0};

    (void)args;

    oops_log_info("NVPT", "entry");

    return main(1, argv);
}
