/*
 * SDL2's build configuration for the console. Ours, reached by the `__PROSPERO__` arm
 * that `patches/0001-*` adds to upstream's `include/SDL_config.h`; `__PROSPERO__`
 * itself comes from `oops-sdl.mk` on the command line, which is why the patch needs no
 * second hunk for it.
 *
 * Modelled on upstream's own `SDL_config_minimal.h`, which is the config for a platform
 * with nothing: it asks for `stdarg.h`, `stddef.h` and `stdint.h` and nothing else, and
 * a freestanding compiler must provide all three. That is the measurement behind
 * oops-sdk#D010 - SDL's core needs nothing from this SDK, and the dependency runs the
 * other way through `backend/`.
 *
 * **What is off here is off honestly.** A subsystem set to its dummy driver is a
 * subsystem that does nothing and says so through SDL's own API, which is not the same
 * as one that is missing. Each is marked below with what would turn it on, so the next
 * piece of work is named rather than discovered.
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
 * x86-64, so little-endian. Setting it here is upstream's own escape hatch -
 * `SDL_endian.h` asks "not defined in SDL_config.h?" before it decides - and
 * `SDL_config_pandora.h` does exactly this. It matters because the compiler still
 * defines `__FreeBSD__` for our triple, which sends `SDL_endian.h` to `<sys/endian.h>`,
 * and this SDK has no such header. Saying the answer is cheaper than providing a header
 * to be asked.
 */
#define SDL_BYTEORDER 1234

/*
 * `HAVE_LIBC` stays undefined, so SDL uses `src/stdlib/` - its own `SDL_string.c`,
 * `SDL_stdlib.c`, `SDL_malloc.c`, `SDL_qsort.c`. On a `-ffreestanding -nostdlib`
 * payload this is the single fact that makes pinning upstream cheaper than writing a
 * facade.
 */

/*
 * Video: ours. `backend/SDL_prosperovideo.c`, registered by the same patch.
 *
 * **The absent ones are absent, not zero.** SDL tests these with `#ifdef`, so
 * `#define SDL_VIDEO_OPENGL_EGL 0` turns EGL *on* and then fails to find `EGL/egl.h`.
 * That is how this was written first, and the compile sweep is what caught it. ES, ES2
 * and EGL are therefore named here only in this comment.
 */
#define SDL_VIDEO_DRIVER_PROSPERO 1
#define SDL_VIDEO_OPENGL 1
#define SDL_VIDEO_RENDER_OGL 1

/* Timers: ours. `backend/SDL_prosperotimer.c`, because upstream's unix timer wants
   `clock_gettime` and `nanosleep` and this SDK exposes neither. */
#define SDL_TIMER_PROSPERO 1

/*
 * Audio: ours. `backend/SDL_prosperoaudio.c` over `oops/audio.h` - 48 kHz, 16-bit
 * signed, stereo, which is what the port is and what `OpenDevice` writes back into the
 * spec.
 *
 * The dummy driver stays compiled beside it. It is `demand_only`, so it is never chosen
 * unless a title asks for it by name, and it is what makes a build with no working
 * audio still run.
 */
#define SDL_AUDIO_DRIVER_PROSPERO 1
#define SDL_AUDIO_DRIVER_DUMMY 1

/*
 * Joystick: ours. `backend/SDL_prosperojoystick.c` - the pad over `oops/input.h`, with
 * the gamepad mapping declared by the driver so `SDL_GameController` needs no mapping
 * database.
 *
 * Haptic stays disabled: SDL's haptic subsystem is the force-feedback API for wheels
 * and joysticks with effect engines, which the pad is not. Rumble and the light bar
 * reach a title through `SDL_JoystickRumble` and `SDL_JoystickSetLED`, which this
 * driver implements.
 */
#define SDL_JOYSTICK_PROSPERO 1
#define SDL_HAPTIC_DISABLED 1
#define SDL_HIDAPI_DISABLED 1
#define SDL_SENSOR_DISABLED 1

/*
 * Threads: ours. `backend/SDL_prosperothread.c` over `oops/thread.h` - threads, mutexes
 * and semaphores. Upstream's `src/thread/generic/` keeps the condition variable and the
 * thread-local storage, both of which it builds out of those three, so two of the five
 * are free.
 */
#define SDL_THREAD_PROSPERO 1

/*
 * No dynamic loading: the payload is statically linked, so there is nothing to load and
 * `GL_GetProcAddress` returning NULL is the true answer rather than a gap.
 *
 * **`src/loadso/dummy/` is still compiled**, and that is the point of the switch rather
 * than an accident. `SDL_LoadObject` is declared in SDL's public header whatever this
 * is set to, so leaving it *undefined* means a caller links cleanly against nothing and
 * faults - the trap `common/app.mk`'s check exists for. Upstream's dummy defines the
 * three names and returns "not supported", which is the same answer said where a caller
 * can hear it. sdl12-compat is the caller that found this.
 */
#define SDL_LOADSO_DISABLED 1

/*
 * `dynapi` is turned off, but **not here** - `src/dynapi/SDL_dynapi.h` refuses a
 * definition from outside with `#error Nope, you have to edit this file to force this
 * off`, so the switch is a hunk in `patches/0001-*` following that file's own idiom,
 * beside the arms upstream already keeps for vita, PSP, the N-Gage and the 3DS.
 * Upstream asking to be patched is the one case where a patch is not a sign the shim
 * missed something.
 *
 * Left on, `SDL_dynapi.c` reaches for `<dlfcn.h>`: a dispatch table for keeping a
 * *shared library's* ABI stable, in a statically linked payload that has no such
 * problem.
 */

/*
 * **The allocator is the SDK's, so SDL's is not compiled at all.** `HAVE_MALLOC` gates
 * the whole of `src/stdlib/SDL_malloc.c` - a vendored dlmalloc, lines 33 to 5193 - and
 * `oops-sdk`'s libc already declares the full family over `oops_malloc`. One allocator
 * in the payload instead of two, and five thousand lines that never compile.
 *
 * This was arrived at from the other end, which is worth recording. Left off, dlmalloc
 * wants
 * `<errno.h>`, then `<sys/param.h>`, then `<pthread.h>` - three host headers in a row,
 * each answered by one more `LACKS_*` switch. Three in a row was the signal that the
 * question was wrong: the right move was not to keep silencing a second allocator but
 * to stop building it.
 */
#define HAVE_MALLOC 1
#define HAVE_CALLOC 1
#define HAVE_REALLOC 1
#define HAVE_FREE 1

/*
 * `HAVE_MALLOC` alone is not enough: `SDL_malloc.c` then *calls* `malloc` without
 * anything having declared it, because `SDL_stdinc.h` only includes `<stdlib.h>` when
 * told the header exists. It does exist - `oops-sdk/include/libc/stdlib.h`, on the
 * include path - so saying so is a statement of fact and not a concession.
 *
 * `HAVE_LIBC` stays undefined regardless. These four name individual headers this SDK
 * really has; `HAVE_LIBC` would claim a whole hosted C library, and the rest of
 * `src/stdlib/` - the string, qsort and iconv implementations - stays SDL's own.
 */
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_MATH_H 1

/*
 * `HAVE_STDIO_H` turns on the stdio-backed `SDL_RWops`, so `SDL_RWFromFile`,
 * `SDL_LoadFile` and `SDL_LoadBMP` work. This SDK's libc has the whole of what that
 * path calls -
 * `fopen`/`fread`/`fwrite`/`fseek`/`ftell`/`fclose`/`feof`/`ferror`/`fflush`.
 *
 * What it does *not* have is the POSIX around stdio: `errno` and `fstat`. That is three
 * hunks of `patches/0001-*` in `src/file/SDL_rwops.c` and it is the one part of the
 * patch set that is not upstream asking to be patched. The alternative was
 * `include/errno.h` and `include/sys/stat.h` shims with an `fstat` that reports every
 * handle a regular file - which would be inventing a syscall result and teaching the
 * whole build that `fstat` exists. A patch that names the platform is the smaller lie,
 * and it is no lie at all.
 */
#define HAVE_STDIO_H 1

/* Filesystem: ours. `backend/SDL_prosperofilesystem.c` - `/app0/` for the base path and
   `/data/<app>/` for the preference path, created on demand through `oops/fs.h`. */
#define SDL_FILESYSTEM_PROSPERO 1

#endif /* SDL_config_prospero_h_ */
