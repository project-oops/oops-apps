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
 * `ghostship.o2r` is the port's own assets and ships with the build. `sm64.o2r` is the
 * game's, converted from a Super Mario 64 ROM the player supplies, on the console, by
 * the Torch converter upstream links for its own first-run extraction - the same
 * converter `../spaghetti-kart` carries. So the player's part is the ROM file alone.
 */
#include "oops/freestd.h"
#include "oops/fs.h"
#include "oops/syscall.h"
#include "oops/system.h"
#include "oops/time.h"

#include "player_data.h"

extern "C" void oops_crashtrace_install(void);

#include <cstdlib>

int main(int argc, char **argv);

/* app.mk derives the entry symbol from the app name; lld only warns on a mismatch. */
extern "C" int ghostship_start(const payload_args_t *args);

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

    /* An archive already converted is enough; otherwise the ROM it is converted from. */
    if (oops_snprintf(path, sizeof(path), "%s/%s", OOPS_POSIX_HOME, "sm64.o2r") <= 0 ||
        !oops_fs_exists(path)) {
        oops_require_player_data(
            "sm64.z64", "sm64.z64",
            "Ghostship plays from your own Super Mario 64 ROM (US or JP, .z64), converted on "
            "the console the first time it starts.");
    }

    /* libultraship resolves its config and save paths through HOME, and the environment
       starts empty on this platform. */
    (void)oops_fs_mkdir(OOPS_POSIX_HOME, 0755);
    if (setenv("HOME", OOPS_POSIX_HOME, 1) != 0) {
        oops_log_error("GHST", "HOME could not be set; settings will not persist");
    }

    /* Namespace-scope constructors: .init_array is not walked for this payload. */
    oops_run_init_array();

    /* std::random_device needs an entropy source this platform does not have. */
    srand((unsigned)oops_time_get_counter());

    return main(1, argv);
}
