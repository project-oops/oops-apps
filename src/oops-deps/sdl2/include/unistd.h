/*
 * The one POSIX declaration upstream SDL asks for that this SDK's libc does not have.
 *
 * `src/SDL.c` includes `<unistd.h>` for `_exit()` and calls it from `SDL_ExitProcess`.
 * Without this header the include falls through to the build host's
 * `/usr/include/unistd.h`, which is a Linux glibc header being read into a
 * FreeBSD-target freestanding payload - it fails, and it would be wrong if it did not.
 *
 * **`_exit` is `exit` here, and that is not a convenience.** `oops-sdk`'s `exit` goes
 * straight to the platform's `SYS_exit`: no atexit list, no return, nothing between the
 * call and the process ending. That is `_exit`'s contract exactly, so this is the same
 * function under its other name rather than an approximation of it - which is the
 * distinction oops-sdk#D009 draws between a bridge and a stub.
 *
 * A second declaration landing here is a signal, not a routine addition: it means SDL
 * wants more POSIX than the SDK has, and the answer is usually for the SDK to grow it
 * rather than for this file to.
 */
#ifndef OOPS_SDL_UNISTD_H
#define OOPS_SDL_UNISTD_H

#include <stdlib.h> /* oops-sdk's, via -Iinclude/libc */

static inline void _exit(int status) {
    exit(status);
}

#endif /* OOPS_SDL_UNISTD_H */
