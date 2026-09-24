# Patches

**Empty, and empty is the goal.** `common/upstream-fetch.sh` applies every `*.patch` here in
sorted order after checking out `UPSTREAM_REV`, and the set of them is part of the fetch stamp,
so adding or removing one re-fetches. Craft carries none - everything upstream needs is answered
from outside its tree - and that is the bar. Neverball carries three, and each fixes a behaviour
rather than a build.

Two things belong in `../shim/` instead, and reaching for a patch when one of them would do is
the mistake this note exists to prevent:

- **a missing header** - `shim/include/` goes on the include path ahead of upstream's own, so
  `#include <somelib.h>` is answered without touching a line of upstream.
- **a missing function** - a shim translation unit defines it. That is how Craft answers
  twenty-three GLFW entry points without compiling GLFW.

A patch is for when upstream's own code has to *behave* differently here.
