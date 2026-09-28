/*
 * `player_data.h`. The shape Ship of Harkinian proved on the hardware: check before the
 * engine starts, say where the file goes, stop.
 */
#include "player_data.h"

#include "oops/freestd.h"
#include "oops/fs.h"
#include "oops/system.h"

#include <SDL2/SDL.h>
#include <stdlib.h>

void oops_require_player_data(const char *check, const char *place, const char *what) {
    char path[256];
    char message[768];

    if (oops_snprintf(path, sizeof(path), "%s/%s", OOPS_POSIX_HOME, check) > 0 &&
        oops_fs_exists(path)) {
        oops_log_info("DATA", "game data: %s", path);
        return;
    }

    oops_log_error("DATA",
                   "%s missing - %s Copy it to /data/homebrew/%s/%s, beside eboot.bin",
                   check, what, OOPS_APP_ID, place);

    if (oops_snprintf(message, sizeof(message),
                      "%s was not found.\n\n"
                      "%s\n\n"
                      "Copy it here, beside eboot.bin:\n\n"
                      "    /data/homebrew/%s/%s\n\n"
                      "It is yours to supply; nothing else is missing.",
                      place, what, OOPS_APP_ID, place) > 0) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Game data not found", message,
                                 NULL);
    }

    oops_log_info("DATA", "stopping: there is no game data to start with");
    /* `exit`, not `return`: returning from the entry point jumps to a null return
     * address, because the loader calls it and has nowhere to go back to. */
    exit(0);
}
