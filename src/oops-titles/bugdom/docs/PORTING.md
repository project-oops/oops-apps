# Porting Bugdom

How this port is put together, and the constraints that decide its shape. Counts and sizes are
not repeated here; `make census` reports them.

## Shape

Bugdom is a 1999 Macintosh game ported to SDL2 and immediate-mode OpenGL by jorio. `OOPS_RENDERER`
is `gl1`, and the port calls GL by symbol: no `SDL_GL_GetProcAddress`, no glad, no GLEW, so a
missing entry point is a link error rather than a null call at run time.

The game data ships in the upstream repository under `Data/`, so the title needs nothing from the
player. `make package` copies it into the package beside the eboot and fails if
`Data/Skeletons/DoodleBug.3dmf` is absent from the result.

## SDL2

`upstream.lock` pins the newest release tag, and that revision asks for SDL2:
`find_package(SDL2 REQUIRED COMPONENTS main)` at `CMakeLists.txt:67`, with `#include <SDL.h>`
through `src/Headers`. Bugdom 2's pinned revision is the same. `oops-deps/sdl3` exists and neither
Bugdom pin uses it.

## Pomme

Pomme is jorio's reimplementation of the classic Mac OS toolbox - Resource Manager, Sound Manager,
QuickDraw 3D - and a Mac game of this age rests on it for most of its platform layer. It is a
submodule, so the lock carries `UPSTREAM_SUBMODULES=1`; its revision lives in the pinned commit's
tree, so one hash pins both.

Bugdom's own sources are wildcarded because `CMakeLists.txt:79` globs them. Pomme's are listed by
name instead: Bugdom switches four of its subsystems off through `POMME_NO_VIDEO`, `POMME_NO_INPUT`,
`POMME_NO_GRAPHICS` and `POMME_NO_MP3`, and each switch both drops sources and defines a macro the
remaining sources compile against, so a glob would compile the four that are off against headers
those macros empty.

## The filesystem

Pomme takes libc++'s `<filesystem>`, not the `ghc::filesystem` it bundles for platforms that lack
one. ghc requires a full POSIX filesystem underneath it - `st_dev`/`st_ino` file identity, hard
links, `utimensat`, the `*at()` family, `std::wstring` - and this platform supplies almost none of
that, so each addition would be a stub holding up a library whose model does not fit. libc++'s
implementation needs eleven declarations, each with a real answer, and they are shared:
`src/oops-deps/libcxx/oops-libcxx.mk` lists them.

`patches/0001-take-the-real-filesystem-on-this-console.patch` lifts the two arms of
`CompilerSupport/filesystem.h`'s `#if` that exclude this build. One excludes FreeBSD on the strength
of a system libc++ that is not the freestanding one built here. The other reads
`!(defined(__GNUC__) && __GNUC__ < 9)` and so excludes every clang at every version on every
platform, because clang defines `__GNUC__` as 4; the comment above it describes a libstdc++
limitation, and `!defined(__clang__)` makes the test mean that.

## C++ constraints

Four things a C++ title on this platform has to get right. Each is invisible to a clean compile.

`OOPS_CXX_EXCEPTIONS` is set above the `oops-libcxx.mk` include. That file reads it with `?=` at
include time to choose between `build-eh/libc++.a` and `build/libc++.a`, and the no-exceptions
archive defines `std::terminate`, `std::bad_alloc` and more itself rather than deferring to
libc++abi, which collides at the link.

`OOPS_CXX_EXTERNAL_NEW_DELETE` is defined, so libc++ owns `operator new` and `operator delete`.
libc++ places its own in an `__lcxx_override` section and each carries a guard asserting
`&operator new` lies inside it; `common/cxxrt.cpp`'s definition wins under
`-Wl,--allow-multiple-definition` from outside that section, and libc++'s nothrow `operator new`
then executes `ud2`. Allocation still reaches the SDK heap, because libc++'s `operator new` calls
`malloc`, which is `oops_malloc`.

`shim/bugdom_start.cpp` calls `oops_run_init_array()`. `.init_array` is not walked for a plain C++
payload, so namespace-scope constructors do not otherwise run; `oops/system.h` describes how that
fails.

The shim is C++ because `main` is mangled. In a freestanding C++ translation unit `main` is an
ordinary function, so `Boot.cpp` exports `_Z4mainiPPc`, and a C shim asking for `main` links -
the payload link ignores unresolved symbols - and faults at the first instruction. `common/app.mk`'s
undefined-symbol guard is what reports it.

## The entry point

`FindGameData` at `src/Boot.cpp:35` derives the data directory from `argv[0]`: on a non-Apple build
its first attempt is `parent_path(argv[0]) / "Data"`, accepted only when
`Data/Skeletons/DoodleBug.3dmf` opens. `shim/bugdom_start.cpp` probes for that same file under
`/data/homebrew/<app id>` and then `/app0`, and sets `argv[0]` to the root that answered. Probing
for `Data/` alone would accept a package whose assets failed to copy.

The shim refuses to start when neither root answers. Upstream's later attempts end in
`throw std::runtime_error("Couldn't find the Data folder.")`, which `main` turns into
`SDL_ShowSimpleMessageBox` - a dialog naming no path, needing a controller to dismiss.

The shim exports `HOME`. Pomme's `FindFolder` at `Files.cpp:186` reads `XDG_CONFIG_HOME` then
`HOME` and returns `fnfErr` when both are unset, which Bugdom answers with another alert; the
environment starts empty on this platform, so the shim sets it and creates
`$OOPS_POSIX_HOME/.config` for it.

## Controller

`oops-deps/sdl2`'s joystick driver sends both Back and Guide from `OOPS_BUTTON_CREATE`, because
`OOPS_BUTTON_CREATE` and `OOPS_BUTTON_PS` are the same bit. Bugdom's menus read Back.
