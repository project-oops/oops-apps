/*
 * Payload entry point: builds a one-element command line and calls upstream's main
 * (upstream/soh/src/code/main.c), which is a C translation unit, so the name is
 * unmangled.
 *
 * The loader runs no main and passes no argv. argv[0] gives upstream something to
 * report and keeps argc > 0 for its option parsing; the ROM and archives are found by
 * path, not from argv.
 */
#include "oops/freestd.h"
#include "oops/fs.h"
#include "oops/syscall.h"
#include "oops/system.h"
#include "oops/time.h"
#include <stdlib.h>

int main(int argc, char **argv);

/* app.mk derives the entry symbol from the app name; lld only warns on a mismatch. */
int ship_of_harkinian_start(const payload_args_t *args);

/*
 * Says which game data is present, before libultraship looks.
 *
 * The build ships soh.o2r, the port's own archive, and carries no game assets. oot.o2r
 * and oot-mq.o2r are generated on device from a ROM the player supplies, so a fresh
 * install has neither. libultraship answers that with SDL_ShowSimpleMessageBox ("Main
 * OTR file not found"), which on a console is a dialog with no pointer to dismiss it,
 * so the same fact goes to the log where it can be read.
 *
 * It reports rather than refuses: generating the archive is a thing the title itself
 * does, so reaching main with no archive and a ROM present is the normal first run.
 */
static void soh_report_game_data(void);

static void soh_report_game_data(void) {
    static const char *const archives[] = {"oot.o2r", "oot-mq.o2r"};
    char path[256];
    size_t i;
    int have_archive = 0;

    for (i = 0; i < sizeof(archives) / sizeof(archives[0]); i++) {
        if (oops_snprintf(path, sizeof(path), "%s/%s", OOPS_POSIX_HOME, archives[i]) >
                0 &&
            oops_fs_exists(path)) {
            oops_log_info("SOH", "game data: %s", path);
            have_archive = 1;
        }
    }
    if (have_archive) {
        return;
    }

    if (oops_snprintf(path, sizeof(path), "%s/%s", OOPS_POSIX_HOME, "soh.o2r") > 0 &&
        !oops_fs_exists(path)) {
        oops_log_error("SOH", "%s is missing: the port archive ships with the build",
                       path);
    }

    oops_log_error("SOH",
                   "oot.o2r missing - put an Ocarina of Time ROM in %s "
                   "and the title will build the archive from it",
                   OOPS_POSIX_HOME);
}

__attribute__((visibility("default"))) int
ship_of_harkinian_start(const payload_args_t *args) {
    static char arg0[] = "/app0/soh";
    char *argv[] = {arg0, 0};

    (void)args;

    oops_log_init(OOPS_APP_ID);

    /* The kernel log drops bursts and this start-up is one. The sink falls back to
       /data/<app id>, which has to exist when no USB stick is present. */
    (void)oops_fs_mkdir("/data/" OOPS_APP_ID, 0755);
    (void)oops_log_enable_disk_sink(OOPS_APP_ID, 0);

    oops_log_info("SOH", "entry");

    /* libultraship resolves its config and save paths through HOME, and the environment
       starts empty on this platform. */
    (void)oops_fs_mkdir(OOPS_POSIX_HOME, 0755);
    if (setenv("HOME", OOPS_POSIX_HOME, 1) != 0) {
        oops_log_error("SOH", "HOME could not be set; settings will not persist");
    }

    /* Namespace-scope constructors: .init_array is not walked for this payload, and
       libc++ and libultraship both build dispatch tables in theirs. Idempotent. */
    oops_run_init_array();

    /* ShipUtils seeds from rand() here, std::random_device needing an entropy source
       this platform does not have. Unseeded that is a fixed sequence, so the clock
       stands in for the device. */
    srand((unsigned)oops_time_get_counter());

    soh_report_game_data();

    return main(1, argv);
}
