# SDL2

Upstream SDL2, pinned, with a console video driver of ours registered into it. **We do not
implement SDL** - see `oops-sdk#D010`.

Titles want a platform layer, and there is a different one each time: mesa-demos wants GLUT,
Neverball and Armagetron and SuperTux 2 want SDL2, Extreme Tux Racer wants SDL 1.2, Craft wants
GLFW. Writing each one costs a surface that grows per title and goes stale on its own. Pinning
upstream and writing a backend costs a backend.

## Layout

```
oops-deps/sdl2/
  upstream.lock   where SDL comes from and exactly which revision
  upstream/       the fetched tree. Never edited, never committed (.gitignore'd)
  patches/        our changes to it - two hunks, and it should stay that size
  backend/        our video driver. Ordinary .c files, compiled with -I into upstream
  include/        our SDL_config_prospero.h
  oops-sdl.mk     what a title includes, the way oops-sdk.mk works
```

This is the layout `src/oops-titles/README.md` specifies for a title, because a dependency is
the same problem: an origin, our patches, our code and the metadata.

## Why the patch is three one-line hunks and must stay that way

Our backend is **not** delivered by the patch. It is ordinary source in `backend/`, compiled with
`-Iupstream/src -Iupstream/include`, which reaches SDL's internal headers the same way upstream's
own drivers do. Code inside a `.patch` cannot be read, reviewed or compiled on its own, and has
to be rebased every bump.

So the patch does only what can be done nowhere but upstream's own files: **181 lines, 14 hunks
across 11 files, for seven complete subsystems.** Almost all of it is one `#elif` arm naming the
platform beside the ones upstream already keeps for vita, PSP, the N-Gage and the 3DS.

| File | What it adds |
|---|---|
| `include/SDL_config.h` | a `__PROSPERO__` arm in the config chain, ahead of the minimal fallback |
| `src/video/SDL_sysvideo.h`, `SDL_video.c` | the video bootstrap's extern and its table entry |
| `src/joystick/SDL_sysjoystick.h`, `SDL_joystick.c` | the joystick driver's extern and its table entry |
| `src/audio/SDL_sysaudio.h`, `SDL_audio.c` | the audio bootstrap's extern and its table entry |
| `src/thread/SDL_thread_c.h` | an arm pointing at our `SYS_ThreadHandle`, reached through `-Ibackend` |
| `src/dynapi/SDL_dynapi.h` | dynapi off - **upstream requires this to be edited**, in its own words: `#error Nope, you have to edit this file to force this off` |
| `src/joystick/SDL_steam_virtual_gamepad.c` | skips `<sys/stat.h>` and its `stat()` - a file only a desktop Steam client writes |
| `src/file/SDL_rwops.c` | three hunks: no `<errno.h>`, no `fstat`, no `strerror(errno)` in one message |

`__PROSPERO__` itself is defined on the command line by `oops-sdl.mk`, so detecting the platform
needs no hunk in `SDL_platform.h`.

**Only the last of those is us patching around something.** The rest is either registration -
which has to happen in upstream's tables - or upstream asking to be patched. `SDL_rwops.c` is the
one place where the platform shape is genuinely unusual: stdio without the POSIX that normally
comes with it. The alternative was `errno.h` and `sys/stat.h` shims with an `fstat` reporting
every handle a regular file, which would teach the whole build that `fstat` exists.

**A growing `patches/` is a signal.** It means something that could have lived in `backend/` did
not, and the answer is to move it rather than to carry the patch.

## What upstream already solved, so we do not

Measured against `release-2.30.9`, and each of these was checked rather than assumed:

- **SDL ships its own libc.** `src/stdlib/` has `SDL_string.c`, `SDL_stdlib.c`, `SDL_malloc.c`,
  `SDL_qsort.c`, `SDL_iconv.c`, and `HAVE_LIBC` turns the host's off. On a `-ffreestanding
  -nostdlib` target this is the difference between a port and a rewrite.
- **`SDL_config_minimal.h` needs three headers** - `stdarg.h`, `stddef.h`, `stdint.h` - all three
  of which a freestanding compiler must provide. SDL's core needs nothing from `oops-sdk`; the
  dependency runs the other way, through `backend/`.
- **`src/video/dummy/` is upstream's template for a new platform**, 397 lines including licence
  blocks, and `VideoBootStrap` is a four-field struct. Our driver is that shape with real bodies.
- **`SDL_Scancode` values are USB HID usage codes**, which is what `oops_keyboard_read` already
  returns. The keyboard translation is a range check, not a table.

## Where it stands: it links

**130 of 130 translation units compile, and the result links against `oops-sdk` with zero
undefined symbols.** 130 SDL objects and 39 SDK objects into one 1.3 MB image, for
`x86_64-unknown-freebsd -ffreestanding`, with no host libc, no pthread and no POSIX beyond what
the SDK itself declares.

**The link is the measurement that counts, and the compile is not.** Before linking, SDL needed
62 symbols from outside itself: 44 `oops_*` across display, input, keyboard, mouse, audio, fs,
thread, mutex, semaphore and time, and 18 from the SDK's libc - `malloc`, `free`, `fopen`,
`fread`, `memcpy`, `stderr` and the rest. Every one resolves. Nothing else is asked for.

**And it builds into a payload.** `src/oops-gl/sdl-probe` includes `oops-sdl.mk` and goes through
`common/app.mk` like every other app: `make elf` produces a 2.7 MB ELF whose only undefined
symbols are the 110 `sce*` platform imports the console resolves at load, with all seven
backends' entry points present in it.

That is the check neither a compile sweep nor a hand-rolled link can make, because a payload link
passes `--unresolved-symbols=ignore-all` - so it is the one that had to be made.

This is not a frame on hardware and this section does not claim one.

| Subsystem | Where it comes from |
|---|---|
| Video, GL context, window | `backend/SDL_prosperovideo.c` over `oops/display.h` |
| Events - keyboard, mouse | `backend/SDL_prosperoevents.c` |
| Joystick - the pad, rumble, light bar | `backend/SDL_prosperojoystick.c` over `oops/input.h` |
| Threads, mutexes, semaphores | `backend/SDL_prosperothread.c` over `oops/thread.h` |
| Audio output | `backend/SDL_prosperoaudio.c` over `oops/audio.h` |
| Timers | `backend/SDL_prosperotimer.c` over `oops/time.h` |
| Base and preference paths | `backend/SDL_prosperofilesystem.c` over `oops/fs.h` |
| Condition variables, thread-local storage | **upstream's `src/thread/generic/`**, built on the three above |
| `SDL_RWops`, file and memory | **upstream**, on this SDK's stdio |
| Everything else | **upstream** |

## Four things the build taught, each found by a tool rather than by reading

**`#define SDL_VIDEO_OPENGL_EGL 0` turns EGL on.** SDL tests these with `#ifdef`, so a zero is a
yes. An absent switch has to be absent, not zero.

**Three host headers in a row meant the wrong question.** `src/stdlib/SDL_malloc.c` is a vendored
dlmalloc that wanted `<errno.h>`, then `<sys/param.h>`, then `<pthread.h>` - each answerable with
one more `LACKS_*` key. The right move was `HAVE_MALLOC`, which switches the whole file off and
uses the SDK's allocator: five thousand lines that never compile, and one allocator in the
payload instead of two.

**A compile sweep says nothing about undefined symbols.** `SDL_steam_virtual_gamepad.c` was
dropped from the source list because it wanted `<sys/stat.h>`, and 128 files then compiled
clean - while `SDL_joystick.c` called four of its functions unconditionally. It would have
linked on the day the link was first tried, not before. That is what `common/app.mk`'s check
exists for, and it is why this README quotes a link.

**`_THIS` is per subsystem.** `SDL_sysaudio.h` defines it, uses it and `#undef`s it again on the
way out, so every audio backend redefines it - upstream's own dummy driver does this on its line
29. The video subsystem leaves its standing. Two files in the same directory, two answers.

## What is deliberately not here

- **Haptic.** SDL's haptic subsystem is force feedback for wheels and effect engines. The pad's
  rumble and light reach a title through `SDL_JoystickRumble` and `SDL_JoystickSetLED`.
- **Pad motion sensors.** `oops_pad_state_t` carries the accelerometer, gyroscope and
  orientation, and SDL wants them through `SDL_PrivateJoystickSensor` in its own units with a
  timestamp. `SDL_GameControllerHasSensor` answers no, which is true today.
- **Trigger haptics.** `oops_input_set_trigger_effect` is capture-gated and refuses rather than
  guessing the platform's parameter block, so `SDL_JoystickRumbleTriggers` reports unsupported.
- **`SDL_SetThreadPriority`.** The SDK takes a priority at thread creation and has no call to
  change one after, so this refuses instead of quietly doing nothing.
- **Audio capture.** There is no capture path in `oops/audio.h`.

Each of those refuses rather than returning success, which is `oops-sdk#D009` one layer up: a
caller that believes the call worked must not draw a different picture from one that knows it
did not.

## SDL 1.2

Not a second backend. [`../sdl12-compat`](../sdl12-compat/) is upstream's SDL 1.2 API over SDL2,
pinned beside this one, and it links: **zero duplicate symbols, zero undefined.** Extreme Tux
Racer's SDL is 1.2 and this is what it will stand on.

It needs one thing from the SDL2 side that a title does not: **its symbols renamed.** Linked
statically, sdl12-compat's 233 SDL 1.2 entry points collide with SDL2's, starting at `SDL_Init`.
`oops-sdl12.mk` hands back a renamed copy of the archive built here - the reasoning is in that
directory's README, and none of it changes anything for a title using SDL2 directly.

It also found a real gap here: `SDL_LoadObject` and its two siblings are declared in SDL's public
header whatever `SDL_LOADSO_DISABLED` says, so leaving `src/loadso/dummy/` out of the file list
left three declared-but-undefined names. They are in the list now.

## Bumping

Change `UPSTREAM_REV`, build, and fix what breaks. Two things break in practice:

- **A patch stops applying.** Upstream moved under it. Rebase the hunk, or move what it did into
  `backend/`.
- **The file list moves.** SDL builds with CMake and we compile a list in `oops-sdl.mk`, so a
  file added upstream is a file we do not compile. It surfaces as an undefined symbol at link,
  which `common/app.mk` already checks for - that check is the reason this is a caught failure
  and not a console fault.
