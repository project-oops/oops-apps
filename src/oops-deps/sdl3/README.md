# SDL3

SDL 3.4.16, vendored beside `oops-deps/sdl2` rather than replacing it. `make -f oops-sdl3.mk
sdl3-census` compiles the source list; the archive is `build/libSDL3.a`.

## A second SDL

`oops-deps/sdl2` serves the SDL2 titles, and Extreme Tux Racer reaches it through
`sdl12-compat`, which is itself an SDL2 program. Bugdom and Bugdom 2 are SDL3 programs, and SDL3
is not a version bump: it renamed the configuration header, moved the driver interfaces a backend
implements, and removed the 2.x API those ports would otherwise use.

**`sdl2-compat` is not a way round this.** It runs SDL2 programs on top of SDL3, which is the
opposite direction.

## The patch

SDL3 has a supported extension point for an unknown platform. `SDL_PLATFORM_PRIVATE` selects
`include/SDL_build_config_private.h`, and the driver lists in `SDL_video.c:89`,
`SDL_audio.c:29` and `SDL_joystick.c:55` each begin with a `_PRIVATE` entry. There is a
`_PRIVATE` selector for every subsystem a platform has to supply.

The one patch is a three-line arm in `src/thread/SDL_thread_c.h`, the one place that does not
honour it: its chain ends at `#error Need thread implementation for this platform`, so setting
`SDL_THREAD_PRIVATE`, as the generated config invites, stops the thread sources. **A report of it
upstream is written by a person**: `upstream/CLAUDE.md` asks that generative AI not be used in
contributions or bug reports to SDL.

## What `backend/` defines

The list is every symbol the archive references that nothing in it defines, restricted to the
SDL ones. Each maps onto something `oops-sdk` has, in `SDL_prosperothread.c`,
`SDL_prosperotimer.c`, `SDL_prosperofilesystem.c`, `SDL_prosperovideo.c`, `SDL_prosperoaudio.c`
and `SDL_prosperojoystick.c`.

| | |
|---|---|
| **Drivers** | `PRIVATE_bootstrap`, `PRIVATEAUDIO_bootstrap`, `SDL_PRIVATE_JoystickDriver` |
| **Threads** | `SDL_SYS_CreateThread`, `SDL_SYS_SetupThread`, `SDL_SYS_SetThreadPriority`, `SDL_SYS_WaitThread`, `SDL_SYS_DetachThread`, `SDL_GetCurrentThreadID` |
| **Mutex and semaphore** | `SDL_CreateMutex`, `SDL_DestroyMutex`, `SDL_LockMutex`, `SDL_TryLockMutex`, `SDL_UnlockMutex`, `SDL_CreateSemaphore`, `SDL_DestroySemaphore`, `SDL_SignalSemaphore`, `SDL_WaitSemaphoreTimeoutNS` |
| **Time** | `SDL_GetPerformanceCounter`, `SDL_GetPerformanceFrequency`, `SDL_SYS_DelayNS`, `SDL_GetSystemTimeLocalePreferences` |
| **Filesystem** | `SDL_SYS_GetBasePath`, `SDL_SYS_GetPrefPath`, `SDL_SYS_GetUserFolder`, `SDL_SYS_GetCurrentDirectory`, `SDL_SYS_GetExeName` |
| **Path operations** | `SDL_SYS_EnumerateDirectory`, `SDL_SYS_GetPathInfo`, `SDL_SYS_RemovePath`, `SDL_SYS_RenamePath`, `SDL_SYS_CopyFile`, `SDL_SYS_CreateDirectory` |

Unlike SDL2, the mutex and semaphore are the platform's here: SDL3's generic implementations are
built on primitives a platform provides. `oops/thread.h` has all of them - `oops_mutex_*`
including a recursive variant, `oops_sem_*`, `oops_cond_*` and TLS - so this is a mapping rather
than an implementation.

To re-derive the list after a bump:

```sh
nm --defined-only  build/libSDL3.a | grep -oE ' [TDBRWV] [A-Za-z_][A-Za-z0-9_]*$' | sed 's/^ . //' | sort -u > def
nm --undefined-only build/libSDL3.a | grep -oE ' U [A-Za-z_][A-Za-z0-9_]*$'      | sed 's/^ U //' | sort -u > und
comm -23 und def | grep -E '^(SDL_|PRIVATE)'
```

A compile census says nothing about this: a driver a platform does not supply is a link-time
absence, not a compile error.

## Where the backend answers "no"

Each is argued in the file it lives in; this is the index.

| | |
|---|---|
| `SDL_SYS_SetThreadPriority` | Fails. The SDK sets priority through a thread's creation attributes and has no call to change a running one. SDL treats it as advisory. |
| `SDL_SYS_GetExeName` | Fails. No `/proc/self/exe` here - the same absence `common/posix`'s `readlink` records. |
| `SDL_SYS_GetUserFolder` | Answers the one writable directory for every folder. Documents, Screenshots and Saved Games are the same place. |
| `SDL_SYS_RemovePath` | Removes a file. There is no `rmdir` underneath, so an empty directory fails. |
| `SDL_GetSystemTimeLocalePreferences` | Writes neither format. SDL's contract is that a platform leaves alone what it does not know, and the settings are not exposed to a payload. |
| A second window | Refused, rather than handed the first. |
| A swap interval other than 1 | Refused. The flip is on vsync. |
| `CreateWindowFramebuffer` | Absent, so `SDL_GetWindowSurface` fails rather than returning a buffer nothing presents. |
| Audio recording | Absent. `oops/audio.h` is output only, so SDL offers no recording device. |
| `RumbleTriggers` | Refused. The trigger motors take an effect curve, not an amplitude; a caller falls back to ordinary rumble. |
| `SetSensorsEnabled` | Refused. The pad has an IMU and `oops_pad_state_t` carries it, but `SDL_SENSOR_DISABLED` is set. |
| The Guide button | Not reported. `OOPS_BUTTON_CREATE` and `OOPS_BUTTON_PS` are the same bit, so sending both would fire Guide on every Create press. The SDL2 driver sends both. |

`SDL_WaitSemaphoreTimeoutNS`'s bounded form is polled at 500us, because there is no timed
semaphore wait - latency, not a lost wakeup, since the count is held and the next poll takes it.
`SDL_SYS_GetPathInfo` costs an extra call per path because `oops/fs.h` declares `oops_file_info_t`
and has no `oops_fs_stat` to fill one in.

## Single-file entries in the source list

Both are recorded in `oops-sdl3.mk` beside the list:

- `src/main/SDL_main_callbacks.c` - the rest of `src/main/` is entry-point machinery a payload
  replaces, but `SDL_HasMainCallbacks` and `SDL_IterateMainCallbacks` are called from the event
  loop whatever style a program uses.
- `src/haptic/dummy/` - `SDL_HAPTIC_DISABLED` does not remove `SDL_haptic.c`. The dummy backend
  answers for it, and its own guard is `#if defined(SDL_HAPTIC_DUMMY) || defined(SDL_HAPTIC_DISABLED)`.
