# SDL3

SDL 3.4.16, vendored for the console beside `oops-deps/sdl2` rather than replacing it.

## Where it stands

| | |
|---|---|
| Sources compiling for the target | **159 of 159** — `make -f oops-sdl3.mk sdl3-census` |
| Archive | `build/libSDL3.a` builds |
| Patches | **one**, three lines — see below |
| Backend | **not written.** 33 symbols outstanding, listed below |

Nothing here has run. The archive links against itself; no title has been built on it.

## Why a second SDL

`oops-deps/sdl2` stays where it is. Neverball, Neverputt, SuperTux and q3rally are built against
it, and Extreme Tux Racer reaches it through `sdl12-compat`, which is itself an SDL2 program.
Bugdom and Bugdom 2 are SDL3 programs, and SDL3 is not a version bump: it renamed the configuration
header, moved the driver interfaces a backend implements, and removed the 2.x API those ports would
otherwise have used.

**`sdl2-compat` is not a way round this**, and the name invites the mistake: it runs SDL2 programs
on top of SDL3, which is the opposite direction.

## One patch, and SDL3 earned that

SDL2 needed a patch that added an arm to the platform chain in `SDL_platform.h` and registered every
backend by hand. **SDL3 has a supported extension point for a platform it has never heard of.**
`SDL_PLATFORM_PRIVATE` selects `include/SDL_build_config_private.h`, and the driver lists in
`SDL_video.c:89`, `SDL_audio.c:29` and `SDL_joystick.c:55` each begin with a `_PRIVATE` entry. There
is a `_PRIVATE` selector for every subsystem a console has to supply.

The single patch is a three-line arm in `src/thread/SDL_thread_c.h`, which is the one place that
does not honour it: its chain ends at `#error Need thread implementation for this platform`, so
setting `SDL_THREAD_PRIVATE` — as the generated config invites — stopped six sources dead. It reads
like a gap rather than a design choice. **If it is reported upstream, it has to be written by a
person**: `upstream/CLAUDE.md` asks that generative AI not be used in contributions or bug reports
to SDL, and that request is theirs to make.

A second patch appearing in this directory would be worth questioning.

## What `backend/` has to define

Measured, not guessed: build the archive, take every symbol it references that nothing in it
defines, and keep the SDL ones. 33 of them, and every one maps onto something `oops-sdk` already
has.

| | |
|---|---|
| **Drivers** (3) | `PRIVATE_bootstrap`, `PRIVATEAUDIO_bootstrap`, `SDL_PRIVATE_JoystickDriver` |
| **Threads** (6) | `SDL_SYS_CreateThread`, `SDL_SYS_SetupThread`, `SDL_SYS_SetThreadPriority`, `SDL_SYS_WaitThread`, `SDL_SYS_DetachThread`, `SDL_GetCurrentThreadID` |
| **Mutex and semaphore** (9) | `SDL_CreateMutex`, `SDL_DestroyMutex`, `SDL_LockMutex`, `SDL_TryLockMutex`, `SDL_UnlockMutex`, `SDL_CreateSemaphore`, `SDL_DestroySemaphore`, `SDL_SignalSemaphore`, `SDL_WaitSemaphoreTimeoutNS` |
| **Time** (4) | `SDL_GetPerformanceCounter`, `SDL_GetPerformanceFrequency`, `SDL_SYS_DelayNS`, `SDL_GetSystemTimeLocalePreferences` |
| **Filesystem** (5) | `SDL_SYS_GetBasePath`, `SDL_SYS_GetPrefPath`, `SDL_SYS_GetUserFolder`, `SDL_SYS_GetCurrentDirectory`, `SDL_SYS_GetExeName` |
| **Path operations** (6) | `SDL_SYS_EnumerateDirectory`, `SDL_SYS_GetPathInfo`, `SDL_SYS_RemovePath`, `SDL_SYS_RenamePath`, `SDL_SYS_CopyFile`, `SDL_SYS_CreateDirectory` |

Unlike SDL2, **the mutex and semaphore are the platform's here**: SDL3's generic implementations are
built on primitives a platform provides rather than the other way round. `oops/thread.h` has all of
them — `oops_mutex_*` including a recursive variant, `oops_sem_*`, `oops_cond_*` and TLS — so this
is a mapping rather than an implementation.

To re-derive the list after a bump:

```sh
nm --defined-only  build/libSDL3.a | grep -oE ' [TDBRWV] [A-Za-z_][A-Za-z0-9_]*$' | sed 's/^ . //' | sort -u > def
nm --undefined-only build/libSDL3.a | grep -oE ' U [A-Za-z_][A-Za-z0-9_]*$'      | sed 's/^ U //' | sort -u > und
comm -23 und def | grep -E '^(SDL_|PRIVATE)'
```

**A census says nothing about this**, which is why the number above comes from the archive. All 159
sources compiled while 57 symbols were missing, because a driver a platform does not supply is a
link-time absence and not a compile error. The same reasoning that made `make glsurface` necessary
for q3rally.

## Two entries in the source list are a single file

Both found by the link rather than by reading, and both recorded in `oops-sdl3.mk` beside the list:

- `src/main/SDL_main_callbacks.c` — the rest of `src/main/` is entry-point machinery a payload
  replaces, but `SDL_HasMainCallbacks` and `SDL_IterateMainCallbacks` are called from the event loop
  whatever style a program uses.
- `src/haptic/dummy/` — `SDL_HAPTIC_DISABLED` does not remove `SDL_haptic.c`. The dummy backend is
  what answers for it, and its own guard is `#if defined(SDL_HAPTIC_DUMMY) || defined(SDL_HAPTIC_DISABLED)`.
  Twenty-one `SDL_SYS_Haptic*` symbols said so.
