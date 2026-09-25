# Patches

**Empty, and that is the goal rather than a stage.** Craft's `patches/` is empty too: everything
upstream needs is answered from outside its tree, so a version bump is a hash change rather than
a rebase.

`common/upstream-fetch.sh` applies every `*.patch` here, in sorted order, after checking out
`UPSTREAM_REV` - and the set of them is part of the stamp, so adding or removing one re-fetches.
Extreme Tux Racer has five and each changes a behaviour rather than a build; that is the bar.

Two things belong in `../shim/` instead, and reaching for a patch when one of them would do is
the mistake this note exists to prevent:

- **a missing header** - `shim/include/` goes on the include path before upstream's own, so
  `#include <somelib.h>` can be answered without touching a line of upstream. `config.h`,
  `SDL_opengles2.h` and the Boost slice are all this.
- **a missing function** - a shim translation unit defines it.

A patch is for when upstream's own code has to *behave* differently here - a menu the pad
cannot reach, a loading screen that reads as a hang. Extreme Tux Racer's `0002-pad-in-menus` is
the likeliest precedent: SuperTux's input is already SDL game-controller aware, which is a
reason to measure before writing one.
