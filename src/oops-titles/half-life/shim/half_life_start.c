/*
 * Payload entry point.
 *
 * The engine is built with `XASH_STATIC_LIBS`, upstream's own mode for platforms that
 * cannot load a library at run time (its Vita and Switch builds): there is no launcher,
 * and `Host_Main` (`engine/common/host.c:1174`) is called directly, as the launcher in
 * `game_launch/game.cpp` would call it through `dlsym`. The game code -
 * hlsdk-portable's client and server, and the menu - is linked in, and the engine finds
 * each library in the table `platform/misc/lib_static.c` reads rather than on disk.
 *
 * The data is the player's own `valve/` directory, from their copy of the game, read at
 * run time from beside `eboot.bin`. `XASH3D_BASEDIR` (`filesystem_engine.c:266`) points
 * the engine there.
 */
#include "oops/syscall.h"
#include "oops/system.h"

#include "player_data.h"

#include <stdlib.h>

void oops_crashtrace_install(void);

typedef void (*pfnChangeGame)(const char *progname);
int Host_Main(int argc, char **argv, const char *progname, int bChangeGame,
              pfnChangeGame pChangeGame);

/* app.mk derives the entry symbol from the app name; lld only warns on a mismatch. */
int half_life_start(const payload_args_t *args);

__attribute__((visibility("default"))) int half_life_start(const payload_args_t *args) {
    static char arg0[] = "/app0/xash3d";
    char *argv[] = {arg0, 0};

    (void)args;

    oops_log_init(OOPS_APP_ID);
    /* The kernel log drops bursts and this start-up is one. */
    (void)oops_log_enable_disk_sink(OOPS_APP_ID, 0);
    oops_log_info("HALF", "entry");
    oops_crashtrace_install();

    /* halflife.wad rather than the directory: a file proves the copy got that far. */
    oops_require_player_data("valve/halflife.wad", "valve/",
                             "Half-Life plays from your own copy of the game: the "
                             "whole valve folder from its "
                             "install directory.");

    if (setenv("XASH3D_BASEDIR", OOPS_POSIX_HOME, 1) != 0 ||
        setenv("HOME", OOPS_POSIX_HOME, 1) != 0) {
        oops_log_error("HALF", "the environment could not be set; the engine will not "
                               "find valve/");
    }

    /* Namespace-scope constructors in the C++ game code: .init_array is not walked for
       this payload. */
    oops_run_init_array();

    /* No change-game callback: the launcher's only answers by restarting the process.
     */
    return Host_Main(1, argv, "valve", 0, NULL);
}
