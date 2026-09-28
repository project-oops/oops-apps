/*
 * Payload entry point.
 *
 * C++ rather than C: LÖVE's `main` is at `src/love.cpp`, a C++ translation unit, and in a
 * freestanding C++ build it is an ordinary, mangled function that a C declaration would miss
 * without the link saying so.
 *
 * The game is the player's own `Balatro.exe`. It is a LÖVE executable with the game appended
 * as a zip archive, and LÖVE's bundled PhysFS mounts it as one - the Windows program at the
 * front is never run, only the archive at the back is read. So argv[1] names it, the same
 * way `love game.love` is run on a desktop.
 */
#include "oops/syscall.h"
#include "oops/system.h"

#include "player_data.h"

extern "C" void oops_crashtrace_install(void);

#include <cstdlib>

int main(int argc, char **argv);

/* app.mk derives the entry symbol from the app name; lld only warns on a mismatch. */
extern "C" int balatro_start(const payload_args_t *args);

__attribute__((visibility("default"))) extern "C" int
balatro_start(const payload_args_t *args) {
    static char arg0[] = "/app0/love";
    static char arg1[] = "/app0/Balatro.exe";
    char *argv[] = {arg0, arg1, nullptr};

    (void)args;

    oops_log_init(OOPS_APP_ID);
    /* The kernel log drops bursts and this start-up is one. */
    (void)oops_log_enable_disk_sink(OOPS_APP_ID, 0);
    oops_log_info("BLTR", "entry");
    oops_crashtrace_install();

    oops_require_player_data(
        "Balatro.exe", "Balatro.exe",
        "Balatro plays from your own copy of the game: Balatro.exe, from the directory it "
        "is installed in. It is read as an archive, not run.");

    /* LÖVE's save directory is resolved through HOME (`love.filesystem`'s appdata). */
    if (setenv("HOME", OOPS_POSIX_HOME, 1) != 0) {
        oops_log_error("BLTR", "HOME could not be set; saves will not persist");
    }

    /* Namespace-scope constructors: .init_array is not walked for this payload. */
    oops_run_init_array();

    return main(2, argv);
}
