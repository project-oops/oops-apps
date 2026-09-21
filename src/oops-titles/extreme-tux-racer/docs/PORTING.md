# Extreme Tux Racer — porting notes

The mirror choice, what is done, and the one dependency left. The [README](../README.md) is the
overview.

## The mirror, and a plan that changed

Upstream proper is Subversion on SourceForge, and the three git mirrors are unofficial. They were
compared by cloning and reading:

| Mirror | Platform layer | Sources | Last commit |
|---|---|---|---|
| meveric | SDL **1.2** | 45 `.cpp` at the root | 2014-11-19 |
| **RKSimon** | **SDL 2** | 45 `.cpp` at the root | 2014-05-29 |
| lutris | **SFML** | 48 `.cpp` under `src/` | 2016-11-19 |

**RKSimon is the same codebase as meveric with the SDL2 port already done.** `SDL_SetVideoMode`,
`SDL_Flip` and `SDL_WM_SetCaption` are gone; `SDL_CreateWindow`, `SDL_GL_CreateContext` and
`SDL_GL_SwapWindow` are there.

**So this title does not need `oops-deps/sdl12-compat`, which was built for it.** That shim was
written on the strength of "ETR's SDL is 1.2", which was true of the mirror read first and not of
the right one. It keeps its place as the general answer for an SDL 1.2 title, but it is not on
this title's path. lutris is newest and is the wrong tree: it tracks modern ETR, which moved to
SFML — a larger dependency nothing else here would share.

Being three years stale is a real cost, accepted knowingly and recorded in `upstream.lock`.

## What is done

**Every gap except one is closed, and each was measured rather than estimated.**

- **The C++ runtime** — `common/cxxrt.cpp`, shared with every C++ title that follows. `new`,
  `delete` and the sized forms over the SDK's heap; thread-safe static-init guards;
  `__cxa_pure_virtual`; `__dso_handle`. 12 symbols, needing only `oops_malloc` and `oops_free`.
- **The POSIX shim** — `shim/etr_posix.c` and `shim/include/`. Nine functions plus `gettimeofday`
  and an `errno`, scoped by reading ETR rather than by what POSIX has.
- **The SDL2 include prefix** — ETR writes `<SDL2/SDL.h>`; five one-line forwarding headers answer
  that.
- **The entry point** — `shim/etr_start.cpp`, three lines calling upstream's `main`.

**There is no patch to ETR at all, and that is the point.** `bh.h` picks its platform includes
with an `#if defined(OS_LINUX)` chain, so `-DOS_LINUX=1` plus `-Ishim/include` satisfies the whole
platform layer from the shim. A `patches/` that stays empty is a title that will survive a bump.

## What is left: one dependency

Compiling all 45 sources reports exactly one missing header, every time:

```
etr sources: 45  compiled: 0  failed: 45
=== distinct missing headers ===
     45 'map' file not found
```

**A C++ standard library.** `bh.h` includes `<string>`, `<map>`, `<list>` and `<iostream>`, and
the target has none of them — `cxx.mk` passes `-nostdinc++` deliberately rather than letting the
build machine's Linux libstdc++ be found by accident.

The surface that has to work is smaller than four headers suggests:

| | Uses | Note |
|---|---|---|
| `string` | **661** | the real dependency, via `using namespace std;` in 4 files |
| `vector<`, `map<`, `list<` | **0** each | `<map>` and `<list>` are dead includes |
| `ostringstream`, `istringstream` | 5, 14 | in 4 files |
| `ofstream`, `cout`, `endl` | 1, 4, 5 | in the same 4 files |

So: `std::string`, and about 29 uses of iostreams across four files. A libc++ with localisation
disabled gives `string` and not iostreams — whether those 29 uses are worth the rest of libc++ is
the question to answer before pinning anything.

**Counting `std::string` was where this nearly went wrong.** A grep for the qualified name
returns 0; four files say `using namespace std;` and write `string` bare, 661 times. Any grep for
a qualified C++ name has to be checked against the unqualified one.

## After that

`SDL2_image`, `SDL2_mixer` and `freetype` — three more pinned upstreams, named by ETR's own
`Makefile`. They are needed to *run*, not to compile, so they come after the standard library.
