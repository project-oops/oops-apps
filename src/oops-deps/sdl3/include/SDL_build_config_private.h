/*
 * SDL3's build configuration for this console.
 *
 * # This file is reached without a patch, and that is SDL3's doing
 *
 * `include/build_config/SDL_build_config.h:34` starts its platform chain with
 *
 *     #if defined(SDL_PLATFORM_PRIVATE)
 *     #include "SDL_build_config_private.h"
 *
 * so defining `SDL_PLATFORM_PRIVATE` and putting this file on the include path is the whole of the
 * registration. **SDL2 needed a patch for the same thing** - `patches/0001-*` in `oops-deps/sdl2`
 * adds an arm to `SDL_platform.h`'s chain and registers each backend by hand. SDL3 has a supported
 * extension point for a platform it has never heard of, and it goes further than the config header:
 * `SDL_video.c:89`, `SDL_audio.c:29` and `SDL_joystick.c:55` each begin their driver list with a
 * `_PRIVATE` entry, and there is a `_PRIVATE` selector for every subsystem a console has to supply.
 *
 * So `oops-deps/sdl3/patches/` is empty, and should stay that way. A patch here would be a sign
 * that something is being done against the grain of an interface SDL provides.
 *
 * # What is on, and why
 *
 * The rule is the narrowest thing that works: `_PRIVATE` only where a console genuinely has to
 * supply the implementation, `DUMMY` or `DISABLED` everywhere else. A dummy that SDL ships is
 * better tested than a private one we would write, and each `_PRIVATE` is a file in `backend/`
 * that has to be kept correct across bumps.
 */
#ifndef SDL_build_config_private_h_
#define SDL_build_config_private_h_

/* Both, the way every one of SDL's own platform configs does it: this file *is* the build config,
 * and saying so stops the chain from reaching a second one. */
#define SDL_build_config_h_

#include <SDL3/SDL_platform_defines.h>

/* What `SDL_GetPlatform()` answers, and what `SDL.c:764` returns for a private platform. */
#define SDL_PLATFORM_PRIVATE_NAME "Prospero"

/* ---- the C library we stand on -------------------------------------------
 *
 * `oops-sdk`'s freestanding libc plus `oops-apps/common/posix`. Each of these says "the header
 * exists and the function in it is real"; SDL falls back to its own `src/stdlib/` for anything not
 * claimed here, which is why the list is short rather than aspirational. */
#define HAVE_STDARG_H 1
#define HAVE_STDDEF_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STDIO_H 1
#define HAVE_STRING_H 1
#define HAVE_MATH_H 1
#define HAVE_LIMITS_H 1
#define HAVE_MALLOC 1
#define HAVE_CALLOC 1
#define HAVE_REALLOC 1
#define HAVE_FREE 1

/* clang has the atomic builtins; SDL uses this to avoid its own spinlock fallback. */
#ifdef __GNUC__
#define HAVE_GCC_SYNC_LOCK_TEST_AND_SET 1
#endif

/* Little-endian x86-64. SDL works this out for itself on platforms it knows; it does not know
 * this one. */
#define SDL_BYTEORDER 1234

/* ---- what this console supplies -----------------------------------------
 *
 * Each of these names a `backend/` source. `SDL_VIDEO_OPENGL` is what makes SDL's GL paths
 * available at all, and `SDL_VIDEO_RENDER_OGL` is the 2D renderer built on them - a port that only
 * ever calls GL itself still wants the first. */
#define SDL_VIDEO_DRIVER_PRIVATE 1
#define SDL_VIDEO_OPENGL 1
#define SDL_VIDEO_RENDER_OGL 1

#define SDL_AUDIO_DRIVER_PRIVATE 1
/* Beside the private one, not instead of it: SDL falls through the driver list in order, so a
 * machine with no audio device still opens. */
#define SDL_AUDIO_DRIVER_DUMMY 1

#define SDL_JOYSTICK_PRIVATE 1
/*
 * **Empty, and honest.** `SDL_gamepad_db.h:33` expands this into its mapping table, so a private
 * platform can ship the button layout of its own pad and have SDL report a *gamepad* rather than a
 * bare joystick. Filling it in means committing to the button and axis indices the joystick backend
 * reports, and there is no backend yet - a mapping written ahead of one would be a table that looks
 * authoritative and names the wrong buttons. It goes in when `backend/`'s joystick driver does.
 */
#define SDL_PRIVATE_GAMEPAD_DEFINITIONS

#define SDL_THREAD_PRIVATE 1
#define SDL_TIMER_PRIVATE 1
#define SDL_TIME_PRIVATE 1

#define SDL_FILESYSTEM_PRIVATE 1
#define SDL_FSOPS_PRIVATE 1

/* ---- what it does not ----------------------------------------------------
 *
 * `DISABLED` where SDL will compile the subsystem out entirely, `DUMMY` where it wants an
 * implementation and ships one that answers "nothing here". Neither is a gap to be filled later
 * unless a title asks: a pad is a joystick, not a haptic device with force-feedback axes, and
 * `oops_input_*`'s rumble reaches titles through `common/haptics.h` instead. */
#define SDL_HAPTIC_DISABLED 1
#define SDL_HIDAPI_DISABLED 1
#define SDL_SENSOR_DISABLED 1
#define SDL_POWER_DISABLED 1
#define SDL_LOADSO_DUMMY 1
#define SDL_CAMERA_DRIVER_DUMMY 1
#define SDL_DIALOG_DUMMY 1
#define SDL_TRAY_DUMMY 1
#define SDL_PROCESS_DUMMY 1

#endif /* SDL_build_config_private_h_ */
