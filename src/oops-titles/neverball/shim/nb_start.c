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
#include "oops/fs.h"
#include "oops/savedata.h"
#include <GL/gl.h>

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

    /* **Capture one settled frame of the title screen.** Frame 3 rather than 0: the first
       frames build the window and upload the level's textures, so they are a loader rather
       than a frame of the game. The swap does the arming and the writing, so upstream's own
       loop is untouched and what lands in the file is the program's real behaviour.

       Replay it on a build machine with `oops-gl/gl-replay`, where the rasteriser is the
       reference: the difference between that image and a screenshot of this frame is the bug
       that ninety-three conformance checks are all passing through.

       **Where it can be written is found, not assumed.** The obvious choice - the title's own
       directory under `/data/homebrew` - is where the package was installed and is *not*
       writable from inside the sandbox: a first attempt captured 193,746 calls and 9.3 MB and
       then failed to open the file, which cost a hardware run to learn. Nothing else in this
       collection writes a file on the console, so there was no precedent to copy. Rather than
       guess a second time, each candidate is tried and the first that works is used - and the
       log says which, so the next thing that needs to write something already knows. */
    {
        /* **Savedata is mounted, not found.** The first attempt probed `/savedata0` as a path
           and it refused, because that mount point does not exist until `oops_savedata_mount`
           makes it - and neither does any other writable place, which is what all five
           candidates refusing actually meant. This asks the SDK for a slot first and writes
           into the path it hands back; the plain directories stay behind it as a fallback for
           a console where the savedata service is not available. */
        static char chosen[96];
        const char *picked = (const char *)0;
        static char mount[64];
        if (oops_savedata_init() == 0 &&
            oops_savedata_mount("NVRBCAP0", OOPS_SAVEDATA_MODE_CREATE |
                                            OOPS_SAVEDATA_MODE_READ_WRITE,
                                mount, sizeof(mount)) == 0) {
            oops_log_info("NVRB", "savedata mounted at %s", mount);
            size_t m = 0u;
            while (mount[m] && m < sizeof(chosen) - 20u) { chosen[m] = mount[m]; m++; }
            const char *name = "/frame.oglcap";
            size_t j = 0u;
            while (name[j] && m < sizeof(chosen) - 1u) { chosen[m++] = name[j++]; }
            chosen[m] = '\0';
            picked = chosen;
        } else {
            oops_log_info("NVRB", "savedata unavailable - trying plain directories");
        }

        static const char *const dirs[] = {
            "/data",                        /* the user partition */
            "/data/homebrew/NVRB00001",     /* the install directory - measured to refuse */
            "/download0",
            "/savedata0",
            "/tmp",
        };
        for (unsigned i = 0u; i < sizeof(dirs) / sizeof(dirs[0]) && !picked; i++) {
            /* A real open, not a guess about permissions: the only reliable test of whether a
               path can be written is writing to it. One byte, then removed. */
            char probe[96];
            size_t n = 0u;
            while (dirs[i][n] && n < sizeof(probe) - 20u) { probe[n] = dirs[i][n]; n++; }
            const char *leaf = "/.nvrb-probe";
            size_t k = 0u;
            while (leaf[k] && n < sizeof(probe) - 1u) { probe[n++] = leaf[k++]; }
            probe[n] = '\0';
            if (oops_fs_write_all(probe, "x", 1u) == 0) {
                (void)oops_fs_unlink(probe);
                size_t m = 0u;
                while (dirs[i][m] && m < sizeof(chosen) - 20u) { chosen[m] = dirs[i][m]; m++; }
                const char *name = "/frame.oglcap";
                size_t j = 0u;
                while (name[j] && m < sizeof(chosen) - 1u) { chosen[m++] = name[j++]; }
                chosen[m] = '\0';
                picked = chosen;
            }
            oops_log_info("NVRB", "capture path %s: %s", dirs[i], picked ? "writable" : "no");
        }
        if (picked) {
            oops_log_info("NVRB", "capturing frame 3 to %s", picked);
            oops_gl_capture_frame(3u, picked);
        } else {
            oops_log_error("NVRB", "no writable directory found - frame not captured");
        }
    }

    /* **The summary belongs here and not in a patch.** The shim already wraps `main`, so the
       point after it returns is ours to use - and `ball/main.c` stays untouched, which is one
       fewer hunk to rebase onto the next upstream revision. */
    {
        const int rc = main(1, argv);
        nb_diag_report();
        return rc;
    }
}
