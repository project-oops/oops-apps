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
 * `starship.o2r` is the port's own assets and ships with the build. `sf64.o2r` is the
 * game's, converted from a Star Fox 64 ROM the player supplies, on the console, by the
 * Torch converter upstream links from `src/port/extractor` - the same converter
 * `../spaghetti-kart` carries. So the player's part is the ROM file alone (`.z64`,
 * `.n64` or `.v64`, under `baserom.us.rev1.z64` or any supported ROM filename).
 */
#include "oops/freestd.h"
#include "oops/fs.h"
#include "oops/syscall.h"
#include "oops/system.h"

#include "player_data.h"

extern "C" void oops_crashtrace_install(void);

#include <cstdlib>
#include <dirent.h>

int main(int argc, char **argv);

/* app.mk derives the entry symbol from the app name; lld only warns on a mismatch. */
extern "C" int starship_start(const payload_args_t *args);

static bool strf_have_rom(void) {
    static const char *const suffixes[] = {".z64", ".n64", ".v64"};
    DIR *dir = opendir(OOPS_POSIX_HOME);
    if (!dir) {
        return false;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        const char *name = entry->d_name;
        size_t len = 0;
        while (name[len]) {
            len++;
        }
        for (size_t s = 0; s < sizeof(suffixes) / sizeof(suffixes[0]); s++) {
            const char *suf = suffixes[s];
            if (len >= 4 && name[len - 4] == suf[0] && name[len - 3] == suf[1] &&
                name[len - 2] == suf[2] && name[len - 1] == suf[3]) {
                oops_log_info("STRF", "found ROM to convert: %s/%s", OOPS_POSIX_HOME,
                              name);
                closedir(dir);
                return true;
            }
        }
    }
    closedir(dir);
    return false;
}

__attribute__((visibility("default"))) extern "C" int
starship_start(const payload_args_t *args) {
    static char arg0[] = "/app0/starship";
    char *argv[] = {arg0, nullptr};
    char path[256];

    (void)args;

    oops_log_init(OOPS_APP_ID);
    /* The kernel log drops bursts and this start-up is one. */
    (void)oops_log_enable_disk_sink(OOPS_APP_ID, 0);
    oops_log_info("STRF", "entry");
    oops_crashtrace_install();

    /* An archive already converted is enough; otherwise a ROM to convert from. */
    if (oops_snprintf(path, sizeof(path), "%s/%s", OOPS_POSIX_HOME, "sf64.o2r") <= 0 ||
        !oops_fs_exists(path)) {
        if (!strf_have_rom()) {
            oops_require_player_data(
                "baserom.us.rev1.z64", "baserom.us.rev1.z64",
                "Starship plays from your own Star Fox 64 ROM (.z64, "
                ".n64 or .v64), converted on the console the first "
                "time it starts.");
        }
    }

    /* libultraship resolves its config and save paths through HOME, and the environment
       starts empty on this platform. */
    (void)oops_fs_mkdir(OOPS_POSIX_HOME, 0755);
    if (setenv("HOME", OOPS_POSIX_HOME, 1) != 0) {
        oops_log_error("STRF", "HOME could not be set; settings will not persist");
    }

    /* Namespace-scope constructors: .init_array is not walked for this payload. */
    oops_run_init_array();

    /* `exit`, not `return`: the loader called this and has nowhere to return to. */
    exit(main(1, argv));
    return 0; /* not reached; oops-sdk's `exit` is not declared noreturn */
}
