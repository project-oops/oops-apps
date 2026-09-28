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
#include <SDL2/SDL.h>
#include <dirent.h>
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
 * It used to report and carry on, on the grounds that whether the game can start is
 * libultraship's answer to give. It does not give one: `OTRGlobals.cpp:295` passes
 * `allowEmptyPaths = true`, so the resource manager accepts having loaded nothing and
 * says nothing, and the engine goes on building a window and a GUI over resources that
 * are not there. What the player sees for that is not a message, it is a title that
 * disappears.
 *
 * So it answers here instead, where the question is already settled: no ROM archive
 * means no game, and the one useful thing left to do is say so on screen.
 *
 * Returns non-zero when there is game data to start with.
 */
static int soh_report_game_data(void);

/*
 * Whether a ROM the game could convert is sitting in the package directory.
 *
 * Upstream converts one itself - `OTRGlobals.cpp:645` searches for a ROM and hands it
 * to `CallZapd` on a worker thread, counting through `extractCount`/`totalExtract` -
 * and that path has been dead here only because ZAPD was not in the payload. It is now,
 * so a ROM is reason enough to start: the game does the rest.
 */
static int soh_have_rom(void) {
    static const char *const suffixes[] = {".z64", ".n64", ".v64"};
    DIR *dir = opendir(OOPS_POSIX_HOME);
    struct dirent *entry;
    int found = 0;

    if (dir == NULL) {
        oops_log_warn("SOH", "could not list %s to look for a ROM", OOPS_POSIX_HOME);
        return 0;
    }
    while (!found && (entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        size_t len = 0;
        size_t s;

        while (name[len]) {
            len++;
        }
        for (s = 0; s < sizeof(suffixes) / sizeof(suffixes[0]); s++) {
            const char *suf = suffixes[s];
            if (len >= 4u && name[len - 4] == suf[0] && name[len - 3] == suf[1] &&
                name[len - 2] == suf[2] && name[len - 1] == suf[3]) {
                oops_log_info("SOH", "found a ROM to convert: %s/%s", OOPS_POSIX_HOME,
                              name);
                found = 1;
                break;
            }
        }
    }
    closedir(dir);
    return found;
}

static int soh_report_game_data(void) {
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
        return 1;
    }

    /* No archive, but a ROM is enough: upstream's extractor converts it on the way in.
     */
    if (soh_have_rom()) {
        oops_log_info("SOH", "no archive yet - the ROM will be converted on this run");
        return 1;
    }

    if (oops_snprintf(path, sizeof(path), "%s/%s", OOPS_POSIX_HOME, "soh.o2r") > 0 &&
        !oops_fs_exists(path)) {
        oops_log_error("SOH", "%s is missing: the port archive ships with the build",
                       path);
    }

    oops_log_error("SOH",
                   "no ROM and no archive - copy a .z64, .n64 or .v64 to "
                   "/data/homebrew/%s/, beside eboot.bin, and it is converted on the "
                   "next launch",
                   OOPS_APP_ID);
    return 0;
}

/*
 * The same sentence the log carries, on the screen.
 *
 * `SDL_ShowSimpleMessageBox` before `SDL_Init` is deliberate and is the path SDL
 * provides for it: with no video device it asks each bootstrap in turn, which reaches
 * this platform's driver, and that draws the message itself when the system dialog
 * cannot be used - there is no video-out session this early for a common dialog to
 * composite against.
 */
static void soh_say_no_rom(void) {
    char message[512];

    /*
     * The path the player can act on, not the one the process sees.
     *
     * `OOPS_POSIX_HOME` is `/app0`, which is where the package is mounted from inside
     * the sandbox and means nothing to somebody copying a file onto the console. The
     * same directory is `/data/homebrew/<title id>` from outside, and it is the one
     * `eboot.bin` sits in - so say that, and name the file beside it that they can
     * already see.
     */
    if (oops_snprintf(message, sizeof(message),
                      "No Ocarina of Time ROM was found.\n\n"
                      "Copy your own ROM here, beside eboot.bin:\n\n"
                      "    /data/homebrew/%s/\n\n"
                      "A .z64, .n64 or .v64 file - any name will do. It is converted "
                      "on the next launch, once, and takes a few minutes.\n\n"
                      "The ROM is yours to supply; nothing else is missing.",
                      OOPS_APP_ID) <= 0) {
        return;
    }
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Game data not found", message,
                             NULL);
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

    /*
     * No ROM archive, no game. Say so and stop, rather than handing an engine that has
     * loaded nothing to `main` and letting it fault a few frames later with the player
     * looking at a title that vanished.
     */
    if (!soh_report_game_data()) {
        soh_say_no_rom();
        oops_log_info("SOH", "stopping: there is no game data to start with");
        /*
         * `exit`, not `return`. Returning from the entry point jumps to a null return
         * address - the fault after the message was drawn, `rip` of 0 - because the
         * loader calls this and has nowhere to go back to. `exit` parks the process
         * instead, which is the conforming ending for a big app on this platform.
         */
        exit(0);
    }

    return main(1, argv);
}
