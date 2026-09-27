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

/* `common/crashtrace.c`: the payload addresses on the stack when a fatal signal
 * arrives, which is the only way to see past a fault raised inside a platform library.
 */
void oops_crashtrace_install(void);
#include "oops/time.h"
#include <stdlib.h>

int main(int argc, char **argv);

/* app.mk derives the entry symbol from the app name; lld only warns on a mismatch. */
int ship_of_harkinian_start(const payload_args_t *args);

/*
 * Says which game data is present, before libultraship looks.
 *
 * The build ships soh.o2r, the port's own archive, and carries no game assets. oot.o2r
 * and oot-mq.o2r are converted from a ROM the player supplies, on a desktop - see
 * `shim/Extract.cpp` for why the converter is not in this payload - so a fresh install
 * has neither. libultraship answers a missing one with SDL_ShowSimpleMessageBox ("Main
 * OTR file not found"), and the same fact goes to the log, where it can be read without
 * a screen.
 *
 * It reports rather than refuses: whether the game can start is libultraship's answer
 * to give, and it gives it a few frames later with more of the picture than this has.
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

    oops_log_error(
        "SOH",
        "oot.o2r missing - convert an Ocarina of Time ROM to it on a desktop "
        "and copy the archive to %s",
        OOPS_POSIX_HOME);
}

__attribute__((visibility("default"))) int
ship_of_harkinian_start(const payload_args_t *args) {
    static char arg0[] = "/app0/soh";
    char *argv[] = {arg0, 0};

    (void)args;

    oops_log_init(OOPS_APP_ID);

    /* Before anything that can fault. This title and Spaghetti Kart both end in a page
     * fault at address 0x8 on a thread named `libcxx`, which the system reports against
     * libkernel rather than against the call that reached it; `common/crashtrace.c`
     * prints the payload addresses left on the stack, which the `.map` turns into
     * names. */
    oops_crashtrace_install();

    /* The kernel log drops bursts and this start-up is one. The sink writes to a USB
       stick when one is mounted and reports nothing when none is: `/data` is outside
       the sandbox and `oops_fs_get_storage_dir` refuses it, because escaping unmounts
       /app0 and its assets. */
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
