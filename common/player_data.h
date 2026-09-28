/*
 * The data a title does not ship - a ROM, a game's own data files - and the one message
 * a player needs when it is not there.
 *
 * A build here carries no proprietary data: it is published without it, and the player
 * copies their own in beside `eboot.bin`. When it is missing, the useful thing is to
 * say exactly where it goes, on the screen, and stop - not to hand an engine an empty
 * data directory and let it fault later with the player looking at a title that
 * vanished.
 *
 * `/app0` is where the package is mounted from inside the sandbox and means nothing to
 * someone copying files onto the console, so the message names the same directory as
 * the player sees it: `/data/homebrew/<title id>/`, the one `eboot.bin` sits in.
 *
 * Needs SDL2 and `OOPS_POSIX_HOME`: the message goes through
 * `SDL_ShowSimpleMessageBox`, which works before `SDL_Init` and draws the message
 * itself when no video-out session is open yet. The title's `EXTRA_TARGET_CFLAGS`
 * carries `$(OOPS_SDL_INCLUDE)` and
 * `-DOOPS_POSIX_HOME=\"/app0\"`.
 */
#ifndef OOPS_APPS_PLAYER_DATA_H
#define OOPS_APPS_PLAYER_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Returns when `check` exists under `/app0`. Otherwise shows the message and does not
 * return: the process is parked with `exit`, the conforming ending for a big app.
 *
 *   check  the path tested, relative to the title directory - a file, since a file is
 *          what proves a copy finished: `valve/liblist.gam`, not `valve`
 *   place  what the player copies, as they will see it named: `sm64.z64`, `valve/`
 *   what   one sentence saying what it is and where it comes from
 */
void oops_require_player_data(const char *check, const char *place, const char *what);

#ifdef __cplusplus
}
#endif

#endif
