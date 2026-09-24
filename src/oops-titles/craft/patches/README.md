# Patches

**Empty, and that is the goal rather than a stage.** `upstream.lock` puts it plainly: everything
upstream needs is answered from outside its tree, so a version bump is a hash change rather than
a rebase. Craft is the collection's proof that a port can be all shim and no patch.

`common/upstream-fetch.sh` applies every `*.patch` here, in sorted order, after checking out
`UPSTREAM_REV` - and the set of them is part of the stamp, so adding or removing one re-fetches.
Neverball has three and each fixes a behaviour rather than a build; that is the bar.

Two things belong in `../shim/` instead, and reaching for a patch when one of them would do is
the mistake this note exists to prevent:

- **a missing header** - `shim/include/` goes on the include path before upstream's own, which is
  how `<GLFW/glfw3.h>`, `<GL/glew.h>`, `<curl/curl.h>` and `<pthread.h>` are already answered
  without a line of upstream changing.
- **a missing function** - a shim translation unit defines it. `shim/glfw_shim.c` answers
  twenty-three GLFW entry points over oops-sdk's display, input and clock, and GLFW is never
  compiled.

The three headers `make census` still reports missing - `netdb.h`, `fcntl.h`, `signal.h` - are
all the first kind, and none of them is a reason to open this directory.

A patch is for when upstream's own code has to *behave* differently here. Craft has not needed
one, and `docs/PORTING.md` does not expect it to: the SQLite work is a VFS registered from
outside SQLite, which is what SQLite is designed for, and `SQLITE_OMIT_DATETIME_FUNCS` is a
switch upstream already provides.
