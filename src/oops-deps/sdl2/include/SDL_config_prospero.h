/*
 * SDL2's build configuration for the console, reached by the `__PROSPERO__` arm that
 * `patches/0001-*` adds to upstream's `include/SDL_config.h`. `oops-sdl.mk` defines
 * `__PROSPERO__` on the command line.
 *
 * Modelled on upstream's `SDL_config_minimal.h`: SDL's core needs only `stdarg.h`,
 * `stddef.h` and `stdint.h` (oops-sdk#D010). Disabled subsystems use SDL's dummy
 * drivers.
 */
#ifndef SDL_config_prospero_h_
#define SDL_config_prospero_h_
#define SDL_config_h_

#include "SDL_platform.h"

#define HAVE_STDARG_H 1
#define HAVE_STDDEF_H 1
#define HAVE_STDINT_H 1

#ifdef __GNUC__
#define HAVE_GCC_SYNC_LOCK_TEST_AND_SET 1
#endif

/*
 * x86-64 is little-endian. Defined here because the compiler defines `__FreeBSD__` for
 * this triple, which sends `SDL_endian.h` to `<sys/endian.h>`, and the SDK has none.
 */
#define SDL_BYTEORDER 1234

/*
 * `HAVE_LIBC` stays undefined, so SDL uses its own `src/stdlib/` string, qsort and
 * iconv implementations.
 */

/*
 * Video: `backend/SDL_prosperovideo.c`. ES, ES2 and EGL are left undefined rather than
 * 0, because SDL tests them with `#ifdef`.
 */
#define SDL_VIDEO_DRIVER_PROSPERO 1
#define SDL_VIDEO_OPENGL 1
#define SDL_VIDEO_RENDER_OGL 1

/* Timers: `backend/SDL_prosperotimer.c`; the SDK has no `clock_gettime` or
   `nanosleep`. */
#define SDL_TIMER_PROSPERO 1

/*
 * Audio: `backend/SDL_prosperoaudio.c` over `oops/audio.h`, 48 kHz 16-bit stereo. The
 * dummy driver is `demand_only`, chosen only when a title names it.
 */
#define SDL_AUDIO_DRIVER_PROSPERO 1
#define SDL_AUDIO_DRIVER_DUMMY 1

/*
 * Joystick: `backend/SDL_prosperojoystick.c` over `oops/input.h`, with a built-in
 * gamepad mapping. Haptic is off: the pad has no effect engine; rumble and the light
 * bar go through `SDL_JoystickRumble` and `SDL_JoystickSetLED`.
 */
#define SDL_JOYSTICK_PROSPERO 1
#define SDL_HAPTIC_DISABLED 1
#define SDL_HIDAPI_DISABLED 1
#define SDL_SENSOR_DISABLED 1

/*
 * Threads: `backend/SDL_prosperothread.c` over `oops/thread.h` provides threads,
 * mutexes and semaphores; upstream's generic condition variable and TLS build on them.
 */
#define SDL_THREAD_PROSPERO 1

/*
 * The payload is statically linked, so there is nothing to load. `src/loadso/dummy/` is
 * compiled so `SDL_LoadObject` and its siblings resolve and return "not supported".
 */
#define SDL_LOADSO_DISABLED 1

/*
 * `dynapi` is disabled by a hunk in `patches/0001-*`: `src/dynapi/SDL_dynapi.h`
 * rejects a definition from outside the file.
 */

/*
 * The allocator is the SDK's (`oops_malloc`), so `HAVE_MALLOC` keeps SDL's vendored
 * dlmalloc in `src/stdlib/SDL_malloc.c` out of the build.
 */
#define HAVE_MALLOC 1
#define HAVE_CALLOC 1
#define HAVE_REALLOC 1
#define HAVE_FREE 1

/*
 * These headers exist in `oops-sdk/include/libc/`; `SDL_stdinc.h` includes `<stdlib.h>`
 * only when told so, and `SDL_malloc.c` calls `malloc`.
 */
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_MATH_H 1

/*
 * Enables the stdio-backed `SDL_RWops` (`SDL_RWFromFile`, `SDL_LoadFile`). The SDK has
 * no `errno` or `fstat`, which `patches/0001-*` removes from `src/file/SDL_rwops.c`.
 */
#define HAVE_STDIO_H 1

/* Filesystem: `backend/SDL_prosperofilesystem.c` - `/app0/` for the base path and
   `/data/<app>/` for the preference path, created on demand through `oops/fs.h`. */
#define SDL_FILESYSTEM_PROSPERO 1

#endif /* SDL_config_prospero_h_ */
