/*
 * `_exit()` for `src/SDL.c`'s `SDL_ExitProcess`, the one POSIX declaration SDL needs
 * that the SDK's libc lacks. Without it the include reaches the build host's header.
 *
 * `oops-sdk`'s `exit` goes straight to `SYS_exit` with no atexit list, which is
 * `_exit`'s contract (oops-sdk#D009).
 */
#ifndef OOPS_SDL_UNISTD_H
#define OOPS_SDL_UNISTD_H

#include <stdlib.h> /* oops-sdk's, via -Iinclude/libc */

static inline void _exit(int status) {
    exit(status);
}

#endif /* OOPS_SDL_UNISTD_H */
