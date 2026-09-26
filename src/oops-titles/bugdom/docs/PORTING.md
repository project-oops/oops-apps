# Porting Bugdom

What is measured, what is decided, and what has not happened. Every number here came from a build.

## Where it stands

| | |
|---|---|
| C sources compiling for the target | **78 of 78** — `make census`, and all 78 are in the payload |
| C++ sources compiling | **27 of 27** — 25 Pomme, 1 upstream (`Boot.cpp`), 1 ours (the entry point) |
| SDL | **SDL2**, which this collection already vendors — see below |
| GL entry points | **51**, all covered by oops-gl, and called **by symbol** — no loader |
| Game data | ships with upstream, `Data/`, **207 files / 66 MB** — no player purchase, no archive |
| Linked | **yes.** `build/bugdom.elf`, 4,981,312 bytes, `app.mk`'s undefined-symbol guard clean |
| Packaged | **yes.** `make package` — eboot 4,722,768 bytes, `sce_sys/`, `sce_module/libc.prx`, `Data/` |
| Run on hardware | **no** |

## The pin uses SDL2, and reading the clone said otherwise

The candidate survey recorded that Bugdom needs SDL3. It read `find_package(SDL3 CONFIG REQUIRED)`
out of a clone of **master**. `upstream.lock` pins **1.3.4**, the newest release tag, and that
revision has `find_package(SDL2 REQUIRED COMPONENTS main)` at `CMakeLists.txt:67` with
`#include <SDL.h>` throughout `src/Headers`. Bugdom 2's pinned v4.0.0 is the same — checked by
fetching that one commit's `CMakeLists.txt` rather than assuming the same mistake twice.

So this builds against `oops-deps/sdl2`, which five titles are proven on, and the pin stays where
it is for the reason the lock already gives: a port starts from what upstream calls finished.
`oops-deps/sdl3` exists and is complete; neither Bugdom pin needs it.

**The lesson is cheap and worth keeping: read the revision the lock names, not the checkout on
disk.**

## Pomme, and why its source list is transcribed rather than globbed

Pomme is jorio's reimplementation of the classic Mac OS toolbox — Resource Manager, Sound Manager,
QuickDraw 3D. Bugdom is a 1999 Mac game, so this is not an optional dependency but most of the
platform layer. It is a submodule, which is why the lock carries `UPSTREAM_SUBMODULES=1`.

Bugdom's own sources *are* globbed, because upstream globs them — `CMakeLists.txt:79` is
`file(GLOB_RECURSE GAME_SOURCES ...)`, so the directory is the list. Pomme's are not, because
Bugdom switches four of its subsystems off (`POMME_NO_VIDEO`, `_INPUT`, `_GRAPHICS`, `_MP3`) and
each switch both drops sources *and* defines a macro the remaining sources compile against. A glob
would compile the four that are off, against headers those very macros have emptied.

## What the platform was missing, and what it now says

Each of these is in `oops-apps/common/posix` or `oops-sdk/include/libc` and is shared:

| | |
|---|---|
| `BSD` in `sys/param.h` | **An omission, not a decision.** On a real BSD that header is the only place the manifest constant comes from, which is why portable code includes it purely to test it. Pomme's bundled `ghc::filesystem` says exactly that in a comment and then falls off its OS chain into `#error "Operating system currently not supported!"`. 199506 is FreeBSD's value |
| `<strings.h>` from `<string.h>` | FreeBSD's `<string.h>` includes it under `__BSD_VISIBLE`, which is the default, and so do macOS and glibc — which is why so much code expects `<string.h>` to declare `strcasecmp`. Adding it removed a patch this title briefly carried |
| `sys/statvfs.h` | New. **Always fails**: nothing in the SDK reports a filesystem's capacity, and the numbers are the whole point of the call |
| `langinfo.h` | New. `nl_langinfo(CODESET)` answers `"UTF-8"`, which is a fact — the kernel takes UTF-8 path bytes and the SDK passes them through |
| `st_dev`, `st_ino`, `st_nlink` | Added to `struct stat`, and **all zero**. A program comparing `st_dev`/`st_ino` pairs finds every file identical; `sys/stat.h` says so where somebody will read it |
| `O_EXCL`, `pathconf`, `link`, `symlink` | Defined and refused. There is no exclusive create, no path-configurable limit but `PATH_MAX`, and no links of either kind |
| `realpath`, `utimes`, `truncate`, `fchmod`, `openat`/`fchmodat`/`unlinkat`, `fdopendir`, `copy_file_range` | New, for libc++'s `<filesystem>` — see the next section. `realpath` is **complete**: there are no symbolic links here, so collapsing `.` and `..` is the whole job. The `*at()` family answers for `AT_FDCWD` and refuses any other anchor, there being no directory descriptors. `copy_file_range` failing is what makes libc++ fall back to its portable copy |
| `__divti3`, `__modti3`, `__umodti3` | Signed and unsigned 128-bit division in `oops-sdk/src/system/freestd.c`, beside the `__udivti3` that was already there. `<filesystem>`'s file-clock arithmetic is 128-bit, and a freestanding build links no compiler-rt |

## The eight C++ sources: a small `<filesystem>` beat a bigger libc++

Eight Pomme sources failed, all inside `ghc::filesystem`, the `std::filesystem` stand-in Pomme
bundles for platforms without one. The first symptom was `std::wstring`, which our libc++ does not
have — `_LIBCPP_HAS_WIDE_CHARACTERS 0`.

**Turning wide characters on would have been the wrong fix.** `oops-sdk/include/libc/wchar.h` sets
the bar: those functions are absent deliberately, and a port that "genuinely needs `wcslen` and
friends" is the trigger for adding them. A type named in two `path` methods nobody calls is not
that, and the flag is global — every C++ title would rebuild against it.

And `wstring` was not the end of it. With the POSIX gaps above filled, one Pomme source still wanted
`S_ISCHR`, `AT_FDCWD`, `AT_SYMLINK_NOFOLLOW`, `utimensat`, `truncate` and `fchmod`. ghc wants a full
POSIX filesystem — device nodes, timestamps, hard links, `*at()` — and this platform has almost none
of it. Each one added is another honest failure propping up a library whose model does not fit.

**So the second option was taken: build libc++'s own `<filesystem>` and let Pomme use it.** It cost
eleven declarations in the port layer, every one of which had a real answer, against ghc's demand for
file identity and hard links, which do not exist here. It also takes 5,000 lines of somebody else's
portability layer out of the build. `src/oops-deps/libcxx/oops-libcxx.mk` carries the reasoning and
the source list.

**It needs one patch**, `patches/0001-take-the-real-filesystem-on-this-console.patch`.
`CompilerSupport/filesystem.h:8-12` takes `<filesystem>` when `__has_include` finds one, and two arms
of its condition exclude this build:

- `!__FreeBSD__`, on a 2021 note about FreeBSD 13.0-BETA1's libc++ being "problematic". That is an
  observation about one system's implementation; ours is a freestanding libc++ built from source in
  this tree, five years newer. Lifted for `__PROSPERO__` only, so a real FreeBSD build is unchanged.
- `!(defined(__GNUC__) && __GNUC__ < 9)` **excludes clang, which is not what it says.** Clang defines
  `__GNUC__` as **4** and has for its whole life, so this arm rejects every clang at every version on
  every platform. The comment above it is about libstdc++ needing an extra library. Adding
  `!defined(__clang__)` makes the test mean what the comment says — and that arm is not specific to
  this console: any clang build of Pomme silently takes ghc today, including on Linux.

## The link, and three things that only a link finds

A clean census said nothing about any of these.

**The no-exceptions libc++ collides with libc++abi.** `oops-libcxx.mk:66` reads
`OOPS_CXX_EXCEPTIONS` to choose between `build-eh/libc++.a` and `build/libc++.a`, and it reads it
with `?=` **at include time**. Setting the flag lower down, with the rest of the C++ settings, took
the no-exceptions archive — which defines `std::terminate`, `std::bad_alloc` and eighteen more itself
instead of deferring to libc++abi. The failure was twenty duplicate symbols naming neither the
archive nor the ordering. The flag now sits above the include, which is where `supertux/Makefile` has
always had it.

**`main` is mangled here.** In a freestanding C++ translation unit `main` is an ordinary function, so
`Boot.cpp` exports it as `_Z4mainiPPc`. The entry-point shim was C and asked for `main`; the payload
link ignores unresolved symbols, so it linked, and the only thing between that and a fault at the
first instruction on the console was `app.mk`'s guard, which printed one line: `main`. The shim is
C++ now — which is what `extreme-tux-racer/shim/etr_start.cpp` already said, in a comment.

**A C++ shim had no `OOPS_APP_ID`.** `app.mk` puts the three identity defines in `TARGET_CFLAGS`, and
`common/cxx.mk` did not have them. Fixed in `cxx.mk`, so every C++ title gets them.

## The entry point

`shim/bugdom_start.cpp`. `FindGameData` (`src/Boot.cpp:35`) derives the data directory from
`argv[0]` — on a non-Apple build its first attempt is `parent_path(argv[0]) / "Data"` — and accepts
the result only when `Data/Skeletons/DoodleBug.3dmf` opens. So the shim probes for **that one file**
under `/data/homebrew/BUGD00001` and then `/app0`, and sets `argv[0]` to the root that answered.
Probing for `Data/` alone would pass on a package whose assets failed to copy.

It refuses to start when neither root answers, rather than letting `FindGameData` fall through to its
later attempts: those end in `throw std::runtime_error("Couldn't find the Data folder.")`, which
`main` turns into `SDL_ShowSimpleMessageBox` — a dialog that names no path and needs a controller to
dismiss.

It also exports **`HOME`**. Pomme's `FindFolder` (`Files.cpp:186`) reads `XDG_CONFIG_HOME` then
`HOME` and returns `fnfErr` when both are unset, which Bugdom answers with another alert box. The
environment starts empty on this platform, so the one name a payload genuinely knows is set here, and
`OOPS_POSIX_HOME/.config` is created for it.

## What has not been measured

- **Nothing has run.** No frame, no sound, no input, no controller mapping. The 51 GL entry points
  are covered by name and by link, not by a draw.
- The eboot has not been on the console. `pros restore` has not run for this title.
- `oops-deps/sdl2`'s joystick driver sends both Back and Guide from `OOPS_BUTTON_CREATE`
  (`OOPS_BUTTON_CREATE` and `OOPS_BUTTON_PS` are the same bit). Bugdom's menus read Back; whether
  that matters here is unknown until it runs.
