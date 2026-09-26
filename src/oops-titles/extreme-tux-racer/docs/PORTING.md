# Extreme Tux Racer - porting notes

The mirror choice, the platform layer, and where the data lives. The [README](../README.md) is
the overview.

## The mirror

Upstream proper is Subversion on SourceForge, and the git mirrors are unofficial:

| Mirror | Platform layer | Sources |
|---|---|---|
| meveric | SDL 1.2 | `.cpp` at the root |
| **RKSimon** | **SDL 2** | `.cpp` at the root |
| lutris | SFML | `.cpp` under `src/` |

RKSimon is the same codebase as meveric with the SDL2 port done: `SDL_SetVideoMode`, `SDL_Flip`
and `SDL_WM_SetCaption` are gone; `SDL_CreateWindow`, `SDL_GL_CreateContext` and
`SDL_GL_SwapWindow` are there. So this title does not use `oops-deps/sdl12-compat`. lutris tracks
modern ETR, which moved to SFML - a larger dependency nothing else here would share. The
mirror's age is recorded in `upstream.lock`.

## The platform layer

- **The C++ runtime** - `common/cxxrt.cpp`, shared with every C++ title. `new`, `delete` and the
  sized forms over the SDK's heap; thread-safe static-init guards; `__cxa_pure_virtual`;
  `__dso_handle`. It needs only `oops_malloc` and `oops_free`.
- **The POSIX shim** - `shim/etr_posix.c` and `shim/include/`. The functions ETR calls, plus
  `gettimeofday` and an `errno`, scoped by reading ETR rather than by what POSIX has.
- **The SDL2 include prefix** - ETR writes `<SDL2/SDL.h>`; one-line forwarding headers answer
  that.
- **The entry point** - `shim/etr_start.cpp`, three lines calling upstream's `main`.

`bh.h` picks its platform includes with an `#if defined(OS_LINUX)` chain, so `-DOS_LINUX=1` plus
`-Ishim/include` satisfies the whole platform layer from the shim, with no patch to ETR's build.

## The C++ standard library

`bh.h` includes `<string>`, `<map>`, `<list>` and `<iostream>`. `cxx.mk` passes `-nostdinc++` so
the build machine's libstdc++ cannot be found, and `src/oops-deps/libcxx` answers them. The
surface ETR uses:

| | Use |
|---|---|
| `string` | the real dependency, via `using namespace std;` |
| `vector<`, `map<`, `list<` | none; `<map>` and `<list>` are dead includes |
| `ostringstream`, `istringstream` | a few files |
| `ofstream`, `cout`, `endl` | the same files |

A grep for `std::string` finds nothing, because those files write `string` bare. A count of a
qualified C++ name is checked against the unqualified one.

`SDL2_image`, `SDL2_mixer` and `freetype` - named by ETR's own `Makefile`, needed to run rather
than to compile - are pinned in `src/oops-deps/`. `make package` is the runnable result.

## Where the data lives

`game_config.cpp:313` sets `data_dir = prog_dir + "/data"`, and `prog_dir` is `argv[0]` with its
last three characters cut off - `InitConfig` assumes a path ending in `etr`. So
`shim/etr_start.cpp` passes `/app0/etr`, the data directory is `/app0/data` inside the title
package, and `make package` copies `upstream/data` into the package.

The config directory is separate: `getpwuid(getuid())->pw_dir` is `OOPS_POSIX_HOME`,
`/data/extreme-tux-racer`, which survives a re-deploy of the title and is writable. ETR `mkdir`s
it on first run.

## SDL's renderer and the GL context

`winsys.cpp` creates a window, then an `SDL_Renderer` (`:148`) to clear the screen black, and
then a GL context (`:277`). `SDL_VIDEO_RENDER_OGL` is on in `SDL_config_prospero.h`, so that
renderer is SDL's OpenGL renderer sharing oops-gl with everything ETR draws afterwards - two
clients of the same context in one program.
