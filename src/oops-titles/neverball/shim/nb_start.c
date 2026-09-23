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
#include "oops/keyboard.h" /* oops_input_set_log_level */
#include <stdlib.h> /* setenv, for HOME */
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


    /* **Capture one settled frame of the title screen, when `/app0/capture` asks for it.**
       Frame 3 rather than 0: the first frames build the window and upload the level's textures,
       so they are a loader rather than a frame of the game. The swap does the arming and the
       writing, so upstream's own loop is untouched and what lands in the file is the program's
       real behaviour.

       Replay it on a build machine with `oops-gl/gl-replay`, where the rasteriser is the
       reference: the difference between that image and a screenshot of this frame is whatever
       the conformance suite is passing through. That is how the blending fault was cornered,
       and it is the reason this stays in a title that now renders correctly - the next port
       will want it, and a facility nobody can find is a facility nobody uses.

       **Behind a marker file, because it is an instrument and not part of the game.** It ran on
       every launch until 2026-09-23 and logged an error on every launch with it, which is the
       kind of noise that gets a real message overlooked.

       **Where it can be written is found, not assumed.** The obvious choice - the title's own
       directory under `/data/homebrew` - is where the package was installed and is *not*
       writable from inside the sandbox: a first attempt captured 193,746 calls and 9.3 MB and
       then failed to open the file, which cost a hardware run to learn. Nothing else in this
       collection writes a file on the console, so there was no precedent to copy. Rather than
       guess a second time, each candidate is tried and the first that works is used - and the
       log says which, so the next thing that needs to write something already knows. */
    /* **This is kept only to reproduce a finding, and must not be switched on.**
     *
     * It was written to give the title a writable directory, on the belief that it had none:
     * `pick_home_path` (share/base_config.c:51-74) asks `getenv("HOME")`, this SDK's `getenv`
     * answered NULL, and the fallback is the package directory, which was assumed read-only.
     *
     * **Both halves of that were wrong.** `/app0` is writable - `config_paths` has been
     * creating `/app0/.neverball` and `config_save` filling it with `neverballrc`, `Scores`,
     * `Replays` and `Screenshots` all along - and the five candidate paths below refused only
     * because they name the package from *outside* the sandbox, where the process cannot see
     * it. And mounting savedata is actively destructive: its fallback reaches `/data` through
     * `oops_system_escape_sandbox`, and leaving the sandbox takes `/app0` with it. Measured by
     * the probe below, `before=1 after=0`; what the player sees is a black screen, `Failure to
     * open "classic" theme file`, and a thousand draw calls a frame of geometry with no assets
     * on it.
     *
     * The name was never a storage problem. `config_save` is called from one place - upstream's
     * `ball/main.c:602`, after the main loop returns - and a console title is closed or killed
     * rather than quitting, so it never ran. `st_name.c` now writes the config the moment the
     * name is entered, which is the actual fix and needs none of this. */
    static char mount[64];
    GLboolean have_mount = GL_FALSE;
    /* **`oops_savedata_mount` is called whatever `oops_savedata_init` said**, because the mount
     * is the thing that knows how to succeed without it: it initialises the service itself and,
     * when the vendor container is unavailable, falls back to a title-scoped directory under
     * `/data/savedata/<app id>/`. Gating the call on the init code, which an earlier version of
     * this did, skipped the fallback entirely and reported "savedata unavailable" for a slot
     * that mounts perfectly well - `rc=0`, at `/data/savedata/NVRB00001/NVRBSAVE`.
     *
     * **It has to be before `main`**, because upstream's `config_paths` (ball/main.c:491) chooses
     * the write directory before its `SDL_Init` (:497). That ordering is what made this worth
     * fixing properly rather than working around: the fallback reaches `/data` by escaping the
     * sandbox, and doing that before SDL started used to kill the title in `PROSPERO_VideoInit`
     * with `PRX_NOT_RESOLVED_FUNCTION` - `oops_keyboard_init` loading module 0x0106 along a path
     * the process no longer had. `oops_system_escape_sandbox` now makes the SDK's lazily loaded
     * modules resident before it escapes, so the order this title needs is the order it can
     * have. */
    /* **Behind `/app0/savedata`, because the escape costs the title its own package.**
     *
     * Mounting this before `main` works and `HOME` is exported, and then the title boots to a
     * black screen: `Failure to open "classic" theme file`, a window at 800x600 rather than the
     * display's size, and a thousand draw calls a frame of geometry with nothing on it. The
     * package is mounted at `/app0`, which is a path *inside* the sandbox, and
     * `oops_savedata_mount`'s fallback reaches `/data` by leaving it. Savedata is then readable
     * and writable - the config is there, which is why 800x600 came back out of it - and every
     * asset the game ships is not.
     *
     * So the escape is not a thing to do and then carry on from. That is a heavier finding than
     * the module loading `oops_system_escape_sandbox` now guards against, and it is measured
     * rather than reasoned: the probe below reports whether `/app0` survives, before and after,
     * so the next run says so in one line instead of by the absence of a menu.
     *
     * Until it is settled the marker keeps a default launch working. */
    const int sd_fd = oops_fs_open("/app0/savedata", 0 /* O_RDONLY */, 0);
    const int app0_before = oops_fs_exists("/app0/eboot.bin");
    if (sd_fd >= 0) {
        oops_fs_close(sd_fd);
        const int sd_mount = oops_savedata_mount("NVRBSAVE",
                                                 OOPS_SAVEDATA_MODE_CREATE |
                                                 OOPS_SAVEDATA_MODE_READ_WRITE,
                                                 mount, sizeof(mount));
        oops_log_info("NVRB", "savedata mount rc=%d, /app0 before=%d after=%d",
                      sd_mount, app0_before, oops_fs_exists("/app0/eboot.bin"));
        if (sd_mount == 0) {
            have_mount = GL_TRUE;
            oops_log_info("NVRB", "savedata mounted at %s", mount);

            /* **And that is this title's HOME.** Upstream finds its own way from here:
               `config_paths` builds `<HOME>/.neverball`, sets it as the write directory and
               creates it if needed (share/base_config.c:77-110), and `config_save` writes
               `neverballrc` into it - the player's name, the video and audio settings, the key
               bindings. None of that needed changing; it only ever needed somewhere to write.
               Exported rather than patched in so that the next port gets it the same way. */
            if (setenv("HOME", mount, 1) == 0) {
                oops_log_info("NVRB", "HOME=%s - settings and the player name will persist",
                              mount);
            } else {
                oops_log_error("NVRB", "could not export HOME - settings will not persist");
            }
        } else {
            oops_log_info("NVRB", "savedata unavailable - nothing will persist this launch");
        }
    }

    const int cap_fd = oops_fs_open("/app0/capture", 0 /* O_RDONLY */, 0);
    if (cap_fd >= 0) {
        oops_fs_close(cap_fd);
        static char chosen[96];
        const char *picked = (const char *)0;
        if (have_mount) {
            size_t m = 0u;
            while (mount[m] && m < sizeof(chosen) - 20u) { chosen[m] = mount[m]; m++; }
            const char *name = "/frame.oglcap";
            size_t j = 0u;
            while (name[j] && m < sizeof(chosen) - 1u) { chosen[m++] = name[j++]; }
            chosen[m] = '\0';
            picked = chosen;
        }

        static const char *const dirs[] = {
            "/app0",                        /* the package, and writable - see above */
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
