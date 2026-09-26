/*
 * Payload entry point.
 *
 * C++ rather than C: upstream's `main` is at `src/port/Game.cpp`, a C++ translation unit, and a C
 * shim declaring it would reference an unmangled `main` that the payload link does not report as
 * missing - it ignores unresolved symbols and the fault arrives on the console instead. Declaring it
 * from C++ matches whatever Game.cpp emitted.
 *
 * The loader runs no main and passes no argv. argv[0] gives upstream something to report and keeps
 * argc > 0 for its option parsing; the archives are found by path, not from argv.
 */
#include "oops/freestd.h"
#include "oops/fs.h"
#include "oops/syscall.h"
#include "oops/system.h"
#include "oops/time.h"

#include <cstdlib>

int main(int argc, char **argv);

/* app.mk derives the entry symbol from the app name; lld only warns on a mismatch. */
extern "C" int spaghetti_kart_start(const payload_args_t *args);

/*
 * Says which game data is present, before libultraship looks.
 *
 * `spaghetti.o2r` is the port's own assets and ships with the build. `mk64.o2r` is converted from a
 * Mario Kart 64 ROM the player supplies, on a desktop, so a fresh install has only the first. The
 * port answers a missing `mk64.o2r` with a dialog (see `shim/GameExtractor.cpp`); this puts the same
 * fact in the log, where it can be read without a screen.
 */
static void spgk_report_game_data(void) {
    char path[256];
    bool have_game = false;

    if (oops_snprintf(path, sizeof(path), "%s/%s", OOPS_POSIX_HOME, "mk64.o2r") > 0 &&
        oops_fs_exists(path)) {
        oops_log_info("SPGK", "game data: %s", path);
        have_game = true;
    }

    if (oops_snprintf(path, sizeof(path), "%s/%s", OOPS_POSIX_HOME, "spaghetti.o2r") > 0 &&
        !oops_fs_exists(path)) {
        oops_log_error("SPGK", "%s is missing: the port archive ships with the build", path);
    }

    if (!have_game) {
        oops_log_error("SPGK",
                       "mk64.o2r missing - convert a Mario Kart 64 ROM to it on a desktop "
                       "and copy the archive to %s",
                       OOPS_POSIX_HOME);
    }
}

__attribute__((visibility("default"))) extern "C" int
spaghetti_kart_start(const payload_args_t *args) {
    static char arg0[] = "/app0/spaghetti-kart";
    char *argv[] = {arg0, nullptr};

    (void)args;

    oops_log_init(OOPS_APP_ID);

    /* The kernel log drops bursts and this start-up is one. The sink falls back to /data/<app id>,
       which has to exist when no USB stick is present. */
    (void)oops_fs_mkdir("/data/" OOPS_APP_ID, 0755);
    (void)oops_log_enable_disk_sink(OOPS_APP_ID, 0);

    oops_log_info("SPGK", "entry");

    /* libultraship resolves its config and save paths through HOME, and the environment starts
       empty on this platform. */
    (void)oops_fs_mkdir(OOPS_POSIX_HOME, 0755);
    if (setenv("HOME", OOPS_POSIX_HOME, 1) != 0) {
        oops_log_error("SPGK", "HOME could not be set; settings will not persist");
    }

    /* Namespace-scope constructors: .init_array is not walked for this payload, and libc++ and
       libultraship both build dispatch tables in theirs. Idempotent. */
    oops_run_init_array();

    /* libultraship seeds from rand(), std::random_device needing an entropy source this platform
       does not have. Unseeded that is a fixed sequence, so the clock stands in for the device. */
    srand((unsigned)oops_time_get_counter());

    spgk_report_game_data();

    return main(1, argv);
}
