/*
 * The payload entry point.
 *
 * A payload is called, not spawned: the loader runs no `main` and passes no argv. This builds a
 * one-element command line and calls Bugdom 2's `main` in `Source/Boot.cpp`, so no patch renames it.
 *
 * **C++, and it has to be.** `main` is declared here because Bugdom 2 has no header for it, and it is
 * *not* `extern "C"`: in a freestanding C++ translation unit `main` is an ordinary function, so
 * `Boot.cpp` exports it mangled as `_Z4mainiPPc`. A C shim asking for the unmangled name links -
 * `--unresolved-symbols=ignore-all` - and faults at the first instruction on the console. Bugdom's
 * shim was written in C first and `app.mk`'s guard is the only reason that did not reach hardware.
 *
 * **`argv[0]` is the whole interface.** `FindGameData` (`Source/Boot.cpp:30`) derives the data
 * directory from it - on a non-Apple build its first attempt is `parent_path(argv[0]) / "Data"` - and
 * accepts the result only when `Data/Skeletons/Grasshopper.bg3d` opens. So this probes for that one
 * file under each candidate root and hands back the root that answered, rather than hard-coding
 * `/app0`: a homebrew title's payload runs from `/data/homebrew/<id>`, and the package mount is the
 * fallback, not the rule.
 *
 * Its later attempts are `"Data"` relative to the working directory and then a `runtime_error`, and
 * neither is worth reaching: there is no working directory here worth relying on, and the exception
 * surfaces as a message box on a console with nobody to dismiss it.
 *
 * **`HOME`, because that is where preferences go.** Pomme's `FindFolder` reads `XDG_CONFIG_HOME` then
 * `HOME` and returns `fnfErr` when both are unset, which the game answers with an alert. The
 * environment starts empty on this platform, so the one name a payload genuinely knows is exported
 * here.
 *
 * The probe file differs from Bugdom's - `Grasshopper.bg3d` rather than `DoodleBug.3dmf`, `.bg3d`
 * rather than QuickDraw 3D - because Bugdom 2 brought its own model format with its own `Source/3D`.
 */
#include "oops/fs.h"
/* For `payload_args_t`, which the entry signature names. */
#include "oops/syscall.h"
#include "oops/system.h"
/* `setenv`; see `oops-sdk/include/libc/stdlib.h` on why the environment starts empty. */
#include <stdlib.h>

/* Mangled, deliberately - see the header comment. */
int main(int argc, char **argv);

/* Enough for "<root>/Data/Skeletons/Grasshopper.bg3d"; the roots below are short and fixed. */
static const size_t BUG2_PATH_MAX = 256;

/* Appends to `buf` from `off`, NUL-terminating. Returns the new length, or 0 if it would not
   fit - 0 is unambiguous because every caller passes a non-empty prefix or a non-empty `s`. */
static size_t bug2_append(char *buf, size_t cap, size_t off, const char *s) {
    size_t i = 0;
    while (s[i] != '\0') {
        if (off + i + 1u >= cap) {
            return 0;
        }
        buf[off + i] = s[i];
        i++;
    }
    buf[off + i] = '\0';
    return off + i;
}

/*
 * Whether `root` holds the `Data` directory Bugdom 2 would accept.
 *
 * **The same file `FindGameData` checks**, deliberately: a probe for `Data/` alone would pass on a
 * package whose assets failed to copy, and the game would then take its second attempt and throw.
 */
static int bug2_root_has_data(const char *root) {
    char path[BUG2_PATH_MAX];

    const size_t base = bug2_append(path, sizeof(path), 0u, root);
    if (base == 0u) {
        return 0;
    }
    if (bug2_append(path, sizeof(path), base, "/Data/Skeletons/Grasshopper.bg3d") == 0u) {
        return 0;
    }
    if (!oops_fs_exists(path)) {
        return 0;
    }
    oops_log_info("BUG2", "data found: %s", path);
    return 1;
}

extern "C" __attribute__((visibility("default"))) int
bugdom2_start(const payload_args_t *args) {
    /* A homebrew title's own directory first, then the package mount. */
    static const char *const roots[] = {"/data/homebrew/" OOPS_APP_ID, "/app0"};
    static char arg0[BUG2_PATH_MAX];
    const char *root = nullptr;

    char *argv[] = {arg0, nullptr};

    (void)args;

    oops_log_init(OOPS_APP_ID);

    /*
     * The disk sink, because the kernel log drops bursts and this game's start-up is one: Pomme reads
     * every model and every sound resource before the first frame. `/data/<app id>` must exist - the
     * sink falls back to it with no USB stick present.
     */
    (void)oops_fs_mkdir("/data/" OOPS_APP_ID, 0755);
    (void)oops_log_enable_disk_sink(OOPS_APP_ID, 0);

    oops_log_info("BUG2", "entry");

    /* Writable, for high scores and preferences; `.config` is the directory Pomme appends to
       `HOME`, and `FindFolder` is called with `kDontCreateFolder`. */
    (void)oops_fs_mkdir(OOPS_POSIX_HOME, 0755);
    (void)oops_fs_mkdir(OOPS_POSIX_HOME "/.config", 0755);
    if (setenv("HOME", OOPS_POSIX_HOME, 1) != 0) {
        /* Not fatal: the game runs, and only saving fails. Said out loud because the symptom -
           scores that vanish between launches - does not name this. */
        oops_log_error("BUG2", "HOME could not be set; preferences will not persist");
    }

    for (size_t i = 0; i < sizeof(roots) / sizeof(roots[0]); i++) {
        if (bug2_root_has_data(roots[i])) {
            root = roots[i];
            break;
        }
    }
    if (root == nullptr) {
        /*
         * Refused rather than started. `FindGameData` would fall through to `"Data"`, fail that too,
         * and throw `Couldn't find the Data folder.` into a message box - which says nothing about
         * *where* it looked, and needs a controller to acknowledge.
         */
        oops_log_error("BUG2",
                       "no Data/Skeletons/Grasshopper.bg3d under %s or %s - "
                       "the assets are not deployed",
                       roots[0], roots[1]);
        return 1;
    }

    /* `FindGameData` takes `parent_path`, so the trailing name only has to be a name. */
    {
        const size_t n = bug2_append(arg0, sizeof(arg0), 0u, root);
        if (n == 0u || bug2_append(arg0, sizeof(arg0), n, "/Bugdom2") == 0u) {
            oops_log_error("BUG2", "root path too long: %s", root);
            return 1;
        }
    }

    oops_log_info("BUG2", "argv[0]=%s HOME=%s", arg0, OOPS_POSIX_HOME);

    return main(1, argv);
}
