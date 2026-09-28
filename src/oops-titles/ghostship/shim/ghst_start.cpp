/*
 * Payload entry point.
 *
 * C++ rather than C: upstream's `main` is at `src/port/Game.cpp`, a C++ translation
 * unit, and a C shim declaring it would reference an unmangled `main` that the payload
 * link does not report as missing. Declaring it from C++ matches what Game.cpp emits.
 *
 * The loader runs no main and passes no argv. argv[0] gives upstream something to
 * report and keeps argc > 0 for its option parsing; the archives are found by path.
 *
 * `sm64.o2r` is the game's assets, converted from a Super Mario 64 ROM the player
 * supplies, on the console, by the Torch converter upstream links for its own first-run
 * extraction. Upstream's first run takes any `.z64` in its directory whose SHA-1 is the
 * US or Japanese release, so the player's part is the ROM file alone, under any name.
 */
#include "oops/freestd.h"
#include "oops/fs.h"
#include "oops/syscall.h"
#include "oops/system.h"

#include "player_data.h"
#include "tar_unpack.h"

#include <cstdlib>
#include <cstring>

extern "C" void oops_crashtrace_install(void);

/* The first start unpacks 842 files; the log says how far it has got. */
static void ghst_unpack_progress(size_t files, void *user) {
    (void)user;
    if (files % 200u == 0u) {
        oops_log_info("GHST", "unpacked %zu asset descriptions", files);
    }
}

int main(int argc, char **argv);

/* app.mk derives the entry symbol from the app name; lld only warns on a mismatch. */
extern "C" int ghostship_start(const payload_args_t *args);

/* Whether the title directory holds a file ending `.z64`: the candidates upstream's
 * first-run scan (`GameExtractor.cpp`) hashes. Whether it is the right ROM is
 * upstream's question, and it answers it on screen. */
static bool ghst_has_rom(void) {
    oops_dir_t *dir = oops_fs_opendir(OOPS_POSIX_HOME);
    oops_dirent_t e;
    bool found = false;

    if (!dir) {
        return false;
    }
    while (!found && oops_fs_readdir(dir, &e) > 0) {
        const size_t n = strlen(e.name);
        found = !e.is_directory && n > 4 && strcmp(e.name + n - 4, ".z64") == 0;
    }
    oops_fs_closedir(dir);
    return found;
}

__attribute__((visibility("default"))) extern "C" int
ghostship_start(const payload_args_t *args) {
    static char arg0[] = "/app0/ghostship";
    char *argv[] = {arg0, nullptr};
    char path[256];

    (void)args;

    oops_log_init(OOPS_APP_ID);
    /* The kernel log drops bursts and this start-up is one. */
    (void)oops_log_enable_disk_sink(OOPS_APP_ID, 0);
    oops_log_info("GHST", "entry");
    oops_crashtrace_install();

    /* Torch's extraction descriptions (`assets/`), which the package carries as one tar
       (see the Makefile's `package`). Upstream's first run checks for the directory. */
    if (oops_tar_unpack_once(OOPS_POSIX_HOME "/ghostship-assets.tar", OOPS_POSIX_HOME,
                             OOPS_POSIX_HOME "/.ghostship-assets-unpacked",
                             ghst_unpack_progress, nullptr) != 0) {
        oops_log_error("GHST",
                       "the asset descriptions could not be unpacked; see above");
        exit(1);
    }

    /* An archive already converted is enough; otherwise a ROM to convert it from.
       Without either, say where the ROM goes and stop, rather than leaving upstream's
       first run looking for one it will not find. */
    if ((oops_snprintf(path, sizeof(path), "%s/%s", OOPS_POSIX_HOME, "sm64.o2r") <= 0 ||
         !oops_fs_exists(path)) &&
        !ghst_has_rom()) {
        oops_require_player_data("sm64.o2r", "your Super Mario 64 ROM (a .z64 file)",
                                 "Ghostship plays from your own Super Mario 64 ROM (US "
                                 "or JP), converted on the console the first time it "
                                 "starts.");
    }

    /* libultraship resolves its config and save paths through HOME, and the environment
       starts empty on this platform. */
    if (setenv("HOME", OOPS_POSIX_HOME, 1) != 0) {
        oops_log_error("GHST", "HOME could not be set; settings will not persist");
    }

    /* Namespace-scope constructors: .init_array is not walked for this payload, and
       libultraship registers its resource factories and console variables from them. */
    oops_run_init_array();

    /* `exit`, not `return`: the loader called this and has nowhere to return to. */
    exit(main(1, argv));
    return 0; /* not reached; oops-sdk's `exit` is not declared noreturn */
}
