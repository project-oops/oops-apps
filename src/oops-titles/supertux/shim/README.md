# Shim

This is where the port answers what upstream expects and the target does not have, without
editing upstream: `include/` goes on the include path ahead of upstream's own headers, and a
translation unit here supplies a function. Craft's `shim/` is the worked example - twenty-three
GLFW entry points over oops-sdk, with GLFW itself never compiled.

| | |
|---|---|
| `include/config.h` | upstream's CMake-generated `config.h`, one decision per line |
| `include/version.h` | the version `git describe` would give the pinned tag |
| `include/SDL_opengles2.h` | oops-gl's `GL/gl.h` and `GL/glext.h`, for the ES 2.0 build |
| `include/boost/` | the slice of Boost the game uses - `optional`, `format`, eight `filesystem` operations |
| `include/curl/`, `curl_offline.cpp` | libcurl, answered "no network" - every transfer finishes, failed |
| `include/pthread.h` | real mutexes and `pthread_self` for PhysFS, over `oops/thread.h`; oops-sdk's are stubs |
| `include/stx_physfs_fcntl.h` | forced into the C objects: `fcntl` and `F_GETFL`, which the SDK's `<fcntl.h>` lacks |
| `include/SDL2/SDL.h` | `<SDL2/SDL.h>`, which one upstream header spells with the directory |
| `include/tinygettext_Export.h` | what tinygettext's CMake generates for a static build: empty export macros |
| `stx_start.cpp` | the entry point: `.init_array`, then upstream's `main` with `--datadir` |

## What does not belong here

**A library upstream maintains.** PhysFS, Squirrel, sexp-cpp and tinygettext are upstream's own
submodules and build from the fetched tree; glm and OpenAL Soft are pinned in `src/oops-deps/`.
A shim is for the gap between upstream and the target, not a second implementation of something
with an owner. Boost is the exception and `../docs/PORTING.md` says why: the surface is measured
at about twenty headers' worth, and the pin would be a superproject of over a hundred submodules.

**Something another port would want.** That goes to `common/` or oops-sdk. The POSIX layer
PhysFS stands on is `common/posix.mk` for exactly that reason.

**A change in upstream's behaviour.** That is a patch - `../patches/README.md`.

## The GL version is not the shim's to set

oops-gl reports `"1.1 oops-gl fixed-function subset"` unless a program calls
`glContextSetVersion(2, 0)`, and the report is all that call changes. SuperTux `v0.6.3` never
parses `GL_VERSION` - it prints it in the renderer's name and chooses the backend by compile-time
define - so the shim does not call it.
