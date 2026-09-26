# Porting Bugdom

What is measured, what is decided, and what has not happened. Every number here came from a build.

## Where it stands

| | |
|---|---|
| C sources compiling for the target | **78 of 78** — `make census` |
| C++ sources compiling | **18 of 26** — the eight are all Pomme's filesystem layer |
| SDL | **SDL2**, which this collection already vendors — see below |
| GL entry points | **51**, all covered by oops-gl, and called **by symbol** |
| Game data | ships with upstream, `Data/`, **207 files** — no player purchase, no archive needed |
| Linked | **no.** `PAYLOAD_SRCS` is not armed |
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

Bugdom's own sources *are* globbed, because upstream globs them —
`CMakeLists.txt:79` is `file(GLOB_RECURSE GAME_SOURCES ...)`, so the directory is the list. Pomme's
are not, because Bugdom switches four of its subsystems off (`POMME_NO_VIDEO`, `_INPUT`,
`_GRAPHICS`, `_MP3`) and each switch both drops sources *and* defines a macro the remaining sources
compile against. A glob would compile the four that are off, against headers those very macros
have emptied.

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

## The eight C++ sources, and why the answer is not a bigger libc++

All eight fail inside `ghc::filesystem`, the `std::filesystem` stand-in Pomme bundles. The first
symptom was `std::wstring`, which our libc++ does not have — `_LIBCPP_HAS_WIDE_CHARACTERS 0` — and
which ghc names in two `path` methods nobody calls.

**Turning wide characters on would be the wrong fix**, and `oops-sdk/include/libc/wchar.h` already
sets the bar: the functions are absent deliberately, and a port that "genuinely needs `wcslen` and
friends" is the trigger for adding them. A type mentioned in two unused method signatures is not
that. The flag is also global — every C++ title would rebuild against it.

And `wstring` is not the end of it. With the POSIX gaps above filled, one Pomme source still wants
`S_ISCHR`, `AT_FDCWD`, `AT_SYMLINK_NOFOLLOW`, `utimensat`, `truncate` and `fchmod`. ghc wants a
full POSIX filesystem — device nodes, timestamps, hard links, `*at()` — and this platform has
almost none of it. Each one added is another honest failure propping up a library whose model does
not fit.

**So: a small `<filesystem>` of our own, in the shared C++ layer, over `oops/fs.h`.** What Pomme
actually uses is tiny, and every call maps onto something the SDK already has:

```
fs::path ×23     fs::exists ×6          fs::is_regular_file ×3   fs::is_directory ×3
fs::remove       fs::directory_iterator fs::current_path         fs::create_directory
fs::create_directories                  fs::filesystem_error
.c_str() .lexically_normal() .u8string() .filename() .extension() .parent_path()
```

That is less code than making ghc compile, it needs no wide characters and no `st_dev`, and it
takes 5,000 lines of somebody else's portability layer out of the build.

**It needs one patch.** `CompilerSupport/filesystem.h:8-12` takes `<filesystem>` when
`__has_include` finds one — but excludes `__FreeBSD__` outright, on a 2021 note about FreeBSD
13.0-BETA1's libc++ being "problematic". That observation is about the system's implementation, not
this freestanding one, so the exclusion has to be lifted for this platform.

## What has not been measured

- **Nothing has linked.** `PAYLOAD_SRCS` is unarmed, so there is no ELF and no undefined-symbol
  check. The 51 GL entry points are "covered" by a name census, not by a link.
- **Nothing has run.** No frame, no sound, no input.
- The entry point, the icon and the package do not exist yet.
