# SuperTux shim

What each file answers. The shim and patch policy is in
[`../../README.md`](../../README.md#shims-and-patches).

| | |
|---|---|
| `include/config.h` | upstream's CMake-generated `config.h`, one decision per line |
| `include/version.h` | the version `git describe` gives the pinned tag |
| `include/SDL_opengles2.h` | oops-gl's `GL/gl.h` and `GL/glext.h`, for the ES 2.0 build |
| `include/boost/` | the slice of Boost the game uses: `optional`, `format`, eight `filesystem` operations |
| `include/curl/`, `curl_offline.cpp` | libcurl, answered "no network": every transfer finishes, failed |
| `include/pthread.h` | mutexes and `pthread_self` for PhysFS, over `oops/thread.h` |
| `include/stx_physfs_fcntl.h` | forced into the C objects: `fcntl` and `F_GETFL`, which the SDK's `<fcntl.h>` lacks |
| `include/SDL2/SDL.h` | `<SDL2/SDL.h>`, which one upstream header spells with the directory |
| `include/tinygettext_Export.h` | what tinygettext's CMake generates for a static build: empty export macros |
| `stx_start.cpp` | the entry point: `.init_array`, then upstream's `main` with `--datadir` |

PhysFS, Squirrel, sexp-cpp and tinygettext are upstream's own submodules and build from the
fetched tree; glm and OpenAL Soft are pinned in `src/oops-deps/`. Boost is sliced here rather
than pinned because the game uses about twenty headers of it (`../docs/PORTING.md`).

oops-gl reports `"1.1 oops-gl fixed-function subset"` unless a program calls
`glContextSetVersion(2, 0)`. SuperTux `v0.6.3` chooses its backend by compile-time define and
never parses `GL_VERSION`, so the shim does not call it.
