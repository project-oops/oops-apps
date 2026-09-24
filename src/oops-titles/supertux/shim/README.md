# Shim

**Empty, with one line of it already known.** This is where the port answers what upstream
expects and the target does not have, without editing upstream: `shim/include/` goes on the
include path ahead of upstream's own headers, and a shim translation unit supplies a function.
Craft's `shim/` is the worked example - twenty-three GLFW entry points and four headers, over
oops-sdk's display, input and clock, with GLFW itself never compiled.

## The one line that is not optional

**Something here must call `glContextSetVersion(2, 0)` before SuperTux asks for the GL version**,
and it belongs wherever this port creates its GL context.

`gl_video_system.cpp` chooses its backend by parsing `glGetString(GL_VERSION)` with `sscanf`
`"%d"`: 3 or more takes `GL33CoreContext`, exactly 2 takes `GL20Context`, and **anything else
throws** - `"OpenGL 2.0 or higher is unsupported"` - so SDL's software renderer is used instead.

oops-gl's default badge is `"1.1 oops-gl fixed-function subset"`. That parses to 1. So a port
that does nothing here gets a correct, silent, software-rendered SuperTux and no error anywhere
saying why. At 2.0 the badge reads `"2.0 oops-gl programmable subset"`, the parse yields 2, and
the fixed-function backend is chosen - which, as `../docs/PORTING.md` sets out, is the one this
port wants.

That the version is the caller's to state is oops-gl's own design: `gl_internal.h` says the badge
is "about the program's expectations, not this library's opinion of itself", and 2.0 is opt-in
precisely so a GL 1.x program cannot reach the programmable path by accident.

## What else is expected to land here

Nothing is written yet, because the C++ stack is ahead of all of it. In rough order of certainty:

- **an SDL2 GL context shim** - or rather, the small part of one `src/oops-deps/sdl2` does not
  already cover. That is where the call above goes.
- **`<signal.h>`, `<netdb.h>` and friends** - the same one-line headers Craft's notes list, if
  PhysFS or Squirrel reach for them.
- **a PhysFS platform layer**, unless upstream's own POSIX backend is satisfied by
  `common/posix.mk`. That is measured work nobody has done.

`../patches/README.md` has the other side of this: when a shim will not do and upstream's own
code has to behave differently.
