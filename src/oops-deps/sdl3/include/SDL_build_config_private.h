/*
 * SDL3's build configuration for this console.
 *
 * `include/build_config/SDL_build_config.h:34` includes this file when
 * `SDL_PLATFORM_PRIVATE` is defined, and `SDL_video.c:89`, `SDL_audio.c:29` and
 * `SDL_joystick.c:55` begin their driver lists with a `_PRIVATE` entry.
 *
 * `_PRIVATE` is used only where the console must supply the implementation, each one a
 * file in `backend/`; everything else is SDL's own `DUMMY` or `DISABLED`.
 */
#ifndef SDL_build_config_private_h_
#define SDL_build_config_private_h_

/* As in SDL's own platform configs: this file is the build config, so the chain
 * reaches no second one. */
#define SDL_build_config_h_

#include <SDL3/SDL_platform_defines.h>

/* What `SDL_GetPlatform()` answers (`SDL.c:764`). */
#define SDL_PLATFORM_PRIVATE_NAME "Prospero"

/* The C library: `oops-sdk`'s libc plus `common/posix`. SDL uses its own `src/stdlib/`
 * for anything not claimed here. */
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

/* Little-endian x86-64; SDL cannot infer it for a private platform. */
#define SDL_BYTEORDER 1234

/* What this console supplies, each a `backend/` source. `SDL_VIDEO_OPENGL` enables
 * SDL's GL paths; `SDL_VIDEO_RENDER_OGL` is the 2D renderer built on them. */
#define SDL_VIDEO_DRIVER_PRIVATE 1
#define SDL_VIDEO_OPENGL 1
#define SDL_VIDEO_RENDER_OGL 1

#define SDL_AUDIO_DRIVER_PRIVATE 1
/* After the private driver in SDL's list, so audio still opens with no device. */
#define SDL_AUDIO_DRIVER_DUMMY 1

#define SDL_JOYSTICK_PRIVATE 1
/*
 * Empty: `SDL_gamepad_db.h:33` expands this into SDL's mapping table, but
 * `backend/SDL_prosperojoystick.c` implements `GetGamepadMapping`, which SDL asks
 * first.
 * The layout lives beside the button numbering it describes.
 */
#define SDL_PRIVATE_GAMEPAD_DEFINITIONS

#define SDL_THREAD_PRIVATE 1
#define SDL_TIMER_PRIVATE 1
#define SDL_TIME_PRIVATE 1

#define SDL_FILESYSTEM_PRIVATE 1
#define SDL_FSOPS_PRIVATE 1

/* What it does not: `DISABLED` compiles a subsystem out, `DUMMY` uses SDL's empty
 * implementation. Rumble reaches titles through `common/haptics.h`, not SDL haptics. */
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
