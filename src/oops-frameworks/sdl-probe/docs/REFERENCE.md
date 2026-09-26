# sdl-probe - reference

Design notes for the SDL2 integration probe. The [README](../README.md) is the overview; this is
what it exercises and what its log says.

## The link

`oops-sdl.mk` can name every source file correctly and still be wrong, because a payload link
passes `--unresolved-symbols=ignore-all`: a call into a function nothing defines links cleanly
and faults on the hardware. `common/app.mk`'s undefined-symbol check catches that before the
hardware, and it only runs on a real app - this one.

## What it exercises

Four subsystems at once, in the order SDL brings them up:

| Call | What it reaches |
|---|---|
| `SDL_Init(VIDEO \| JOYSTICK \| GAMECONTROLLER \| TIMER)` | every backend's `Init` |
| `SDL_CreateWindow(..., SDL_WINDOW_OPENGL)` | `oops_display_open` |
| `SDL_GL_CreateContext`, `SDL_GL_MakeCurrent` | the context, and the attributes written back |
| `SDL_PollEvent` | the pump, once a frame |
| `SDL_NumJoysticks`, `SDL_GameControllerOpen` | the pad, through the declared mapping - no mapping database |
| `glClear`, `SDL_GL_SwapWindow` | oops-gl, then `oops_display_flip` |

It draws a colour that changes with the frame counter, so a stopped loop and a running one look
different, and it ends by itself after `PROBE_FRAMES` (600) flips with `park] work done`, the
last line of the loop. A failure anywhere on the path returns before the first flip.

## The symbol check

The payload's undefined symbols are all `sce*` platform imports (plus `sysctlbyname`) - the names
the platform resolves when it loads a module. The backends are present and defined:

```
PROSPERO_bootstrap            the video driver
PROSPERO_PumpEvents           the event pump
SDL_PROSPERO_JoystickDriver   the pad
PROSPEROAUDIO_bootstrap       audio
SDL_GetTicks64                timers
sdl_probe_start               the entry point
```

Building with the GL sources held out (`make elf OOPS_GL_SRCS=`) leaves exactly `glClear` and
`glClearColor` undefined, which separates SDL's contribution from oops-gl's.

## The log

```
[SDLP00001:SDLPB] SDL_Init ok
[SDLP00001:SDLPB] prospero
[SDLP00001:SDLPB] no pad
```

The second line is `SDL_GetCurrentVideoDriver`: SDL chose this backend and not the dummy.

With a controller switched on:

```
[SDLP00001:SDLPB] a pad is present
[SDLP00001:SDLPB] opened as a game controller
[SDLP00001:SDLPB] axis 4 at rest: 0
[SDLP00001:SDLPB] axis 5 at rest: 0
```

`SDL_GameControllerOpen` succeeding shows the driver's `GetGamepadMapping` supplied the button and
axis layout, with no entry in the community mapping database and no
`SDL_GameControllerAddMapping` call. The probe reads every axis at rest and logs every event it
receives.

When SDL reports zero joysticks, the probe raises the SDK log level and asks the SDK directly:

```
[SDLP00001:INPUT]  scePadInit returned 0
[SDLP00001:INPUT]  resolved user_id=0x1ea2f4d9
[SDLP00001:INPUT]  scePadOpen(user=0x1ea2f4d9, port=0) returned handle 65996544
[SDLP00001:SDLPB]  joystick subsystem: up
[SDLP00001:SDLPB]  oops_input_poll rc=0 connected=0 buttons=0x0
```

`oops_input_poll` returns `-1` when the read fails, so `rc=0` is a read that worked, and
`connected` comes straight off the platform's own record (`oops-sdk/src/input/input.c:89`).
`scePadSetProcessPrivilege(1)` gives `0x80920005` and `scePadGetHandle` gives `0x80920008` on the
way; the SDK tries `GetHandle` first and falls back to `Open` by design.

The axis figures in the event log are not peaks. Logging fires on the first sample past a
deadzone, so `axis 4 moved to 24543` is direction, not range. Rest values are exact.

## The pad mapping

Confirmed at two layers: the SDK's telemetry reports the platform's button word, and the probe
reports what SDL delivered.

| Pressed | SDK `btn=` | SDL button | Declared |
|---|---|---|---|
| cross | `0x4000` (`OOPS_BUTTON_CROSS`) | 0 | 0 |
| circle | `0x2000` (`CIRCLE`) | 1 | 1 |
| square | `0x8000` (`SQUARE`) | 2 | 2 |
| triangle | `0x1000` (`TRIANGLE`) | 3 | 3 |
| L1 | `0x0400` (`L1`) | 9 | 9 |
| R1 | - | 10 | 10 |
| D-pad up/down/left/right | - | 11/12/13/14 | 11/12/13/14 |
| L3 / R3 | - | 7 / 8 | 7 / 8 |

Both triggers are declared `half_axis_positive`. `SDL_gamecontroller.c` forces a trigger's output
range to `[0, 32767]` and takes the input range from the mapping, where a plain
`EMappingKind_Axis` means the full signed range; `prospero_trigger_axis` produces 0 to 32767. The
stick axes are full-range and carry no flag.

Options and PS never arrive. `scePadSetProcessPrivilege(1)` returns `0x80920005`: the payload does
not hold pad privilege, which is the privilege that surfaces system-reserved buttons, and
`oops/input.h` says the same of `OOPS_BUTTON_PS`. Buttons 5 (guide) and 6 (start) are withheld
by the system, so a run ends at its frame count.

## The flip cost

The CPU tiling of the linear framebuffer into the display's tiled layout dominates the flip by
orders of magnitude; the submit is microseconds. That is `oops_display_try_gpu_tiler` territory,
not the SDL backend's. A renderer that draws straight into the display's tiled layout removes it.
