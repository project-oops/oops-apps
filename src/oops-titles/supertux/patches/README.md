# Patches

**Empty, and that is the goal rather than a stage.** Craft's `patches/` is empty too, and its
`upstream.lock` says why that is worth protecting: everything upstream needs is answered from
outside its tree, so a version bump is a hash change rather than a rebase.

`common/upstream-fetch.sh` applies every `*.patch` here, in sorted order, after checking out
`UPSTREAM_REV` - and the set of them is part of the stamp, so adding or removing one re-fetches.
Neverball has three and each fixes a behaviour rather than a build; that is the bar.

Two things belong in `../shim/` instead, and reaching for a patch when one of them would do is
the mistake this note exists to prevent:

- **a missing header** - `shim/include/` goes on the include path before upstream's own, so
  `#include <somelib.h>` can be answered without touching a line of upstream.
- **a missing function** - a shim translation unit defines it. That is how Craft answers
  twenty-three GLFW entry points without compiling GLFW.

A patch is for when upstream's own code has to *behave* differently here.

One candidate is already known and is written down in `../docs/PORTING.md` under *The one real
gap*: `GLFramebuffer` and `GLTextureRenderer` render the lightmap through framebuffer objects,
which oops-gl does not have, and the cheaper of the two answers is a patch that uses
`glCopyTexSubImage2D` instead. It is not written yet because the C++ stack is ahead of it in the
queue, and a patch against a title that does not compile is a patch nobody can test.
