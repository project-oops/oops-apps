/*
 * Payload entry point.
 *
 * C++ rather than C: upstream's `main` is at `dependencies/RSDKv5/RSDKv5/main.cpp`, a C++
 * translation unit, and in a freestanding C++ build it is an ordinary, mangled function
 * that a C declaration would miss without the link saying so. Declaring it from C++ matches
 * what main.cpp emits. That `main` hands `RSDK_main` the game logic, which is linked in
 * (upstream's `GAME_STATIC`) rather than loaded from a library at run time.
 *
 * The data is the player's own `Data.rsdk`, from their copy of the game, which the engine
 * opens by that name from its working directory (`RSDK/User/Core/UserCore.cpp:313`).
 */
#include "oops/syscall.h"
#include "oops/system.h"

#include "player_data.h"

extern "C" void oops_crashtrace_install(void);

#include <cstdlib>

int main(int argc, char **argv);

/* app.mk derives the entry symbol from the app name; lld only warns on a mismatch. */
extern "C" int sonic_mania_start(const payload_args_t *args);

__attribute__((visibility("default"))) extern "C" int
sonic_mania_start(const payload_args_t *args) {
    static char arg0[] = "/app0/sonic-mania";
    char *argv[] = {arg0, nullptr};

    (void)args;

    oops_log_init(OOPS_APP_ID);
    /* The kernel log drops bursts and this start-up is one. */
    (void)oops_log_enable_disk_sink(OOPS_APP_ID, 0);
    oops_log_info("SMNA", "entry");
    oops_crashtrace_install();

    oops_require_player_data(
        "Data.rsdk", "Data.rsdk",
        "Sonic Mania plays from your own copy of the game: Data.rsdk, from the directory "
        "the game is installed in.");

    /* Settings.ini and the save files are written through HOME. */
    if (setenv("HOME", OOPS_POSIX_HOME, 1) != 0) {
        oops_log_error("SMNA", "HOME could not be set; settings will not persist");
    }

    /* Namespace-scope constructors: .init_array is not walked for this payload. */
    oops_run_init_array();

    return main(1, argv);
}
