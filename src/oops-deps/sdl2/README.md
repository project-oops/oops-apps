# SDL2

Upstream SDL2, pinned, with a video driver of ours registered into it. SDL is not implemented
here - see `oops-sdk#D010`.

Titles want a platform layer, and there is a different one each time: mesa-demos wants GLUT,
Neverball and SuperTux want SDL2, Extreme Tux Racer wants SDL 1.2, Craft wants GLFW. Writing each
one costs a surface that grows per title. Pinning upstream and writing a backend costs a backend.

## Layout

```
oops-deps/sdl2/
  upstream.lock   where SDL comes from and exactly which revision
  upstream/       the fetched tree. Never edited, never committed (.gitignore'd)
  patches/        our changes to it: registration and hunks upstream asks for
  backend/        our video driver. Ordinary .c files, compiled with -I into upstream
  include/        our SDL_config_prospero.h
  oops-sdl.mk     what a title includes, the way oops-sdk.mk works
```

This is the layout a title uses, because a dependency is the same problem: an origin, our
patches, our code and the metadata.

## The patch

The backend is not delivered by the patch. It is ordinary source in `backend/`, compiled with
`-Iupstream/src -Iupstream/include`, which reaches SDL's internal headers the same way upstream's
own drivers do. Code inside a `.patch` cannot be read, reviewed or compiled on its own, and has
to be rebased every bump.

The patch does only what can be done nowhere but upstream's own files. Most of it is one `#elif`
arm naming the platform beside the ones upstream already keeps for vita, PSP, the N-Gage and the
3DS.

| File | What it adds |
|---|---|
| `include/SDL_config.h` | a `__PROSPERO__` arm in the config chain, ahead of the minimal fallback |
| `src/video/SDL_sysvideo.h`, `SDL_video.c` | the video bootstrap's extern and its table entry |
| `src/joystick/SDL_sysjoystick.h`, `SDL_joystick.c` | the joystick driver's extern and its table entry |
| `src/audio/SDL_sysaudio.h`, `SDL_audio.c` | the audio bootstrap's extern and its table entry |
| `src/thread/SDL_thread_c.h` | an arm pointing at our `SYS_ThreadHandle`, reached through `-Ibackend` |
| `src/dynapi/SDL_dynapi.h` | dynapi off - upstream requires this to be edited, in its own words: `#error Nope, you have to edit this file to force this off` |
| `src/joystick/SDL_steam_virtual_gamepad.c` | skips `<sys/stat.h>` and its `stat()` - a file only a desktop Steam client writes |
| `src/file/SDL_rwops.c` | three hunks: no `<errno.h>`, no `fstat`, no `strerror(errno)` in one message |

`__PROSPERO__` itself is defined on the command line by `oops-sdl.mk`, so detecting the platform
needs no hunk in `SDL_platform.h`.

Only `SDL_rwops.c` patches around something. The rest is registration, which has to happen in
upstream's tables, or upstream asking to be patched. `SDL_rwops.c` is where the platform shape is
unusual: stdio without the POSIX that normally comes with it. The alternative, `errno.h` and
`sys/stat.h` shims with an `fstat` reporting every handle a regular file, would teach the whole
build that `fstat` exists.

**A growing `patches/` is a signal.** Something that could live in `backend/` does not, and the
answer is to move it rather than carry the patch.

## What upstream provides

Against `release-2.30.9`:

- **SDL ships its own libc.** `src/stdlib/` has `SDL_string.c`, `SDL_stdlib.c`, `SDL_malloc.c`,
  `SDL_qsort.c`, `SDL_iconv.c`, and `HAVE_LIBC` turns the host's off. On a `-ffreestanding
  -nostdlib` target this is the difference between a port and a rewrite.
- **`SDL_config_minimal.h` needs three headers** - `stdarg.h`, `stddef.h`, `stdint.h` - all of
  which a freestanding compiler provides. SDL's core needs nothing from `oops-sdk`; the
  dependency runs the other way, through `backend/`.
- **`src/video/dummy/` is upstream's template for a new platform**, and `VideoBootStrap` is a
  four-field struct. Our driver is that shape with real bodies.
- **`SDL_Scancode` values are USB HID usage codes**, which is what `oops_keyboard_read` already
  returns. The keyboard translation is a range check, not a table.

## Subsystems

The archive links against `oops-sdk` with no undefined symbols, no host libc, no pthread and no
POSIX beyond what the SDK declares. `src/oops-frameworks/sdl-probe` builds it into a payload
through `common/app.mk`.

| Subsystem | Where it comes from |
|---|---|
| Video, GL context, window | `backend/SDL_prosperovideo.c` over `oops/display.h` |
| Events - keyboard, mouse | `backend/SDL_prosperoevents.c` |
| Joystick - the pad, rumble, light bar | `backend/SDL_prosperojoystick.c` over `oops/input.h` |
| Threads, mutexes, semaphores | `backend/SDL_prosperothread.c` over `oops/thread.h` |
| Audio output | `backend/SDL_prosperoaudio.c` over `oops/audio.h` |
| Timers | `backend/SDL_prosperotimer.c` over `oops/time.h` |
| Base and preference paths | `backend/SDL_prosperofilesystem.c` over `oops/fs.h` |
| Condition variables, thread-local storage | upstream's `src/thread/generic/`, built on the three above |
| `SDL_RWops`, file and memory | upstream, on this SDK's stdio |
| Everything else | upstream |

## Configuration facts

**`#define SDL_VIDEO_OPENGL_EGL 0` turns EGL on.** SDL tests these with `#ifdef`, so a zero is a
yes. An absent switch has to be absent, not zero.

**`HAVE_MALLOC` is set.** `src/stdlib/SDL_malloc.c` is a vendored dlmalloc that wants
`<errno.h>`, `<sys/param.h>` and `<pthread.h>`. `HAVE_MALLOC` switches the whole file off and
uses the SDK's allocator: one allocator in the payload instead of two.

**A compile sweep says nothing about undefined symbols.** `SDL_joystick.c` calls functions in
`SDL_steam_virtual_gamepad.c` unconditionally, so that file stays in the list. A payload link
passes `--unresolved-symbols=ignore-all`, which is why `common/app.mk` checks for undefined
symbols.

**`_THIS` is per subsystem.** `SDL_sysaudio.h` defines it, uses it and `#undef`s it again on the
way out, so every audio backend redefines it - upstream's own dummy driver does this on its line
29. The video subsystem leaves its definition standing.

## What is not here

- **Haptic.** SDL's haptic subsystem is force feedback for wheels and effect engines. The pad's
  rumble and light reach a title through `SDL_JoystickRumble` and `SDL_JoystickSetLED`.
- **Pad motion sensors.** `oops_pad_state_t` carries the accelerometer, gyroscope and
  orientation, and SDL wants them through `SDL_PrivateJoystickSensor` in its own units with a
  timestamp. `SDL_GameControllerHasSensor` answers no.
- **Trigger haptics.** `oops_input_set_trigger_effect` is capture-gated and refuses rather than
  guessing the platform's parameter block, so `SDL_JoystickRumbleTriggers` reports unsupported.
- **`SDL_SetThreadPriority`.** The SDK takes a priority at thread creation and has no call to
  change one after, so this refuses instead of doing nothing.
- **Audio capture.** There is no capture path in `oops/audio.h`.

Each of those refuses rather than returning success, which is `oops-sdk#D009` one layer up: a
caller that believes the call worked must not draw a different picture from one that knows it
did not.

## SDL 1.2

Not a second backend. [`../sdl12-compat`](../sdl12-compat/) is upstream's SDL 1.2 API over SDL2,
pinned beside this one. Linked statically, its SDL 1.2 entry points collide with SDL2's,
starting at `SDL_Init`, so `oops-sdl12.mk` hands back a renamed copy of the archive built here.
The reasoning is in that directory's README, and none of it changes anything for a title using
SDL2 directly.

`SDL_LoadObject` and its two siblings are declared in SDL's public header whatever
`SDL_LOADSO_DISABLED` says, so `src/loadso/dummy/` is in the file list.

## Bumping

Change `UPSTREAM_REV`, build, and fix what breaks:

- **A patch stops applying.** Upstream moved under it. Rebase the hunk, or move what it did into
  `backend/`.
- **The file list moves.** SDL builds with CMake and `oops-sdl.mk` compiles a list, so a file
  added upstream is a file not compiled. It surfaces as an undefined symbol at link, which
  `common/app.mk` checks for.
