# sdl-probe — reference

Design notes for the SDL2 integration probe. The [README](../README.md) is the overview; this is
what it exercises and why the link matters as much as the frames.

## Why the link is half the job

A dependency nothing builds is a dependency nobody has checked. `oops-sdl.mk` can name every
source file correctly and still be wrong, because **a payload link passes
`--unresolved-symbols=ignore-all`**: a call into a function nothing defines links cleanly and
faults on the console. `common/app.mk`'s undefined-symbol check is the only place that is caught
before hardware, and it only runs on a real app — so this is that app.

## What it exercises

All four backends at once, in the order SDL brings them up:

| Call | What it reaches |
|---|---|
| `SDL_Init(VIDEO \| JOYSTICK \| GAMECONTROLLER \| TIMER)` | every backend's `Init` |
| `SDL_CreateWindow(..., SDL_WINDOW_OPENGL)` | `oops_display_open` |
| `SDL_GL_CreateContext`, `SDL_GL_MakeCurrent` | the context, and the attributes written back |
| `SDL_PollEvent` | the pump, once a frame |
| `SDL_NumJoysticks`, `SDL_GameControllerOpen` | the pad, through the **declared** mapping — no mapping database |
| `glClear`, `SDL_GL_SwapWindow` | oops-gl, then `oops_display_flip` |

It draws a colour that changes with the frame counter, so a stopped loop and a running one look
different on a screen, and it **ends by itself** after 600 frames or on Options. A probe that has
to be killed costs a console run to find that out.

## What the symbol check proves

Read directly rather than taken on trust, the payload's undefined symbols are **every one a
`sce*` platform import** (plus `sysctlbyname`) — the names the console resolves when it loads a
module. Nothing from SDL is left hanging. The backends are present and defined:

```
PROSPERO_bootstrap            the video driver
PROSPERO_PumpEvents           the event pump
SDL_PROSPERO_JoystickDriver   the pad
PROSPEROAUDIO_bootstrap       audio
SDL_GetTicks64                timers
sdl_probe_start               the entry point
```

Building with the GL sources held out (`make elf OOPS_GL_SRCS=`) names exactly two symbols —
`glClear` and `glClearColor`, the two whose sources were held out — which is what establishes
that SDL itself contributes nothing unresolved, separately from oops-gl.

## On hardware, 2026-09-21

**It ran on a retail console, first attempt, no fault.** 600 flips - exactly `PROBE_FRAMES` -
then `park] work done`, which is the last line of the loop. That pair covers the whole path:
`SDL_Init`, `SDL_CreateWindow`, `SDL_GL_CreateContext`, `SDL_GL_MakeCurrent`, 600 ×
`SDL_PollEvent`, `glClear`, 600 × `SDL_GL_SwapWindow`, and the teardown. A failure in any of
them returns before the first flip.

```
[SDLP00001:SDLPB] SDL_Init ok
[SDLP00001:SDLPB] prospero
[SDLP00001:SDLPB] no pad
```

`prospero` is `SDL_GetCurrentVideoDriver`, so **SDL chose this backend and not the dummy** - the
one thing the flip counter alone could not prove.

### The pad, with a controller switched on

```
[SDLP00001:SDLPB] a pad is present
[SDLP00001:SDLPB] opened as a game controller
[SDLP00001:SDLPB] axis 4 at rest: 0
[SDLP00001:SDLPB] axis 5 at rest: 0
```

### The trigger bug this found

The first run with a pad in hand logged `axis 4 moved to 16383` and `axis 5 moved to 16383` with
**nothing held** - both triggers reading half scale at rest.

`SDL_gamecontroller.c` forces a trigger's *output* range to `[0, 32767]` whatever the mapping
says, and takes the *input* range from the mapping - where a plain `EMappingKind_Axis` means the
full signed range. So SDL was mapping `[-32768, 32767]` onto `[0, 32767]`, and a released
trigger sending 0 landed on the midpoint. `prospero_trigger_axis` was already producing 0 to
32767 correctly; the declaration was what lied.

Marking both triggers `half_axis_positive` makes the two ends agree, and the rest values above
are that fix confirmed. The stick axes are genuinely full-range on both sides and needed no flag.

**A compile, a link and three clean hardware runs all missed this.** It needed a pad in
someone's hands and a probe that logs what arrives rather than consuming it silently - which is
the argument for a probe logging everything it can see, and the reason the axis dump above reads
every axis at rest instead of waiting for one to move.

**`SDL_GameControllerOpen` succeeding is the result worth having**, because it is what proves the
driver's `GetGamepadMapping` did its job. SDL accepted the button and axis layout from the driver
itself, with no entry in the community mapping database and no `SDL_GameControllerAddMapping`
call from the probe - which is exactly the reason that function was written rather than shipping
a GUID-keyed text file and keeping it current.

### The mapping, worked through by hand

With a controller in hand and the probe logging what arrives, every reachable element was pressed
in a known order. The mapping is confirmed **at two layers independently** - the SDK's own
telemetry reports the platform's button word, and the probe reports what SDL delivered:

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

Both layers agreeing matters more than either alone: it rules out a mapping that is
self-consistently wrong.

### Two things the same run showed that are worth keeping

**Options and PS never arrive, and the reason is in every log.**
`scePadSetProcessPrivilege(1)` returns `0x80920005` on every run - the payload does not hold pad
privilege, which is exactly the privilege that surfaces system-reserved buttons.
`oops/input.h` says as much of `OOPS_BUTTON_PS`: *"when pad privilege is enabled"*. So buttons 5
(guide) and 6 (start) are withheld by the system rather than dropped by this driver, and **this
probe's "press Options to stop early" affordance does not work on this console** - a run ends at
its frame count.

**The axis figures in the event log are not peaks.** The logging fires on the first sample past a
deadzone, not the maximum, so `axis 4 moved to 24543` means "it went well past halfway in the
right direction" and not "it reaches full scale". Rest values are exact because they are read
directly; travel is directional evidence only. Verifying full range would need peak tracking,
and this file does not claim it.

### Before that: `no pad` was true, and establishing it took a third run

The first two runs could not tell *no controller is connected* from *the joystick backend never
asked*, because the SDK reports its pad init through `oops_log_debug`, which is level-gated and
was off - while the keyboard's lines come through `oops_kprintf`, which is not. Identical
silence, two very different meanings.

The probe now raises the level itself and asks the SDK directly whenever SDL reports zero
joysticks:

```
[SDLP00001:INPUT]  scePadInit returned 0
[SDLP00001:INPUT]  resolved user_id=0x1ea2f4d9
[SDLP00001:INPUT]  scePadOpen(user=0x1ea2f4d9, port=0) returned handle 65996544
[SDLP00001:SDLPB]  joystick subsystem: up
[SDLP00001:SDLPB]  oops_input_poll rc=0 connected=0 buttons=0x0
```

In order: the subsystem is up, this driver's `Init` ran, the pad opened with a real handle, and
**`rc=0` means `scePadReadState` succeeded** - `oops_input_poll` returns `-1` when the read
fails, so a zero return is a read that worked. `connected` comes straight off the platform's own
record (`oops-sdk/src/input/input.c:89`). The console said no controller is attached; the driver
reported that faithfully.

Two negative codes appear on the way and neither blocks anything:
`scePadSetProcessPrivilege(1)` gives `0x80920005` and `scePadGetHandle` gives `0x80920008`,
after which `scePadOpen` succeeds - the SDK tries `GetHandle` first and falls back to `Open` by
design.

**Still unexercised:** the axes, the buttons, and the declared gamepad mapping. Those need a
controller switched on, not another run.

## The flip cost

**The tiling dominates the flip by three orders of magnitude, and that is the finding - not any
single number.** Across two runs the submit is **10 to 63 microseconds** and the tiling is
**8697 to 12311**, so a flip lands somewhere near 9 to 12 milliseconds and the probe runs at
roughly 80 to 110 frames per second depending on the run.

Quoting one figure would have been tidier and wrong: the first run averaged about 12.3 ms a flip
and the third about 9.7 ms, on the same build. The CPU walking the linear framebuffer into the
GPU's tiled layout is what varies; the submit never approaches it.

That is `oops_display_try_gpu_tiler` territory rather than anything this probe or the SDL backend
controls, and the obvious reading of the number - "SDL is slow" - is the wrong one. It is also
the cost that oops-mesa's `64KB_R_X` swizzle finding is the path to removing: if a renderer can
draw straight into the display's tiled layout, this 12 ms disappears.
