# Ship of Harkinian

A native port of *The Legend of Zelda: Ocarina of Time*, built on the `zeldaret/oot`
decompilation — [upstream](https://github.com/HarbourMasters/Shipwright), pinned at `9.2.3`
("Ackbar Delta", 2026-04-14).

**It plays from the player's own copy of the game, which they provide on the console, at run
time.** That is the property that chose this title over every other port surveyed, and it is
explained below because it is a policy question as much as a technical one.

## Why this one

A survey on 2026-09-25 cloned thirteen decompilation and port projects and measured each one's
OpenGL surface against what `oops-gl` defines. The measurement counts *definitions* in
`oops-sdk/src/gl/*.c`, not header declarations, because a declared-but-undefined entry point is
the thing that links clean and faults on the console.

| port | language | GL calls needed | missing from oops-gl |
|---|---|---|---|
| sm64-port | C | 34 | **0** |
| **libultraship** (this, and the rest of its family) | C++20 | 54 | **4** |
| perfect_dark | C + C++ | 61 | 6 (the same 4, plus two debug) |

**For the configuration this port would actually build, it is two, not four.** The first table
counts symbols textually across the whole file, and `gfx_opengl.cpp` compiles differently per
platform:

| symbol | where | compiled here? |
|---|---|---|
| `glGenVertexArrays`, `glBindVertexArray` | :725–726, inside `#if defined(__APPLE__) \|\| defined(USE_OPENGLES)` (:724–727) | **no** |
| `glBlitFramebuffer` | :907, :977, :994, unguarded | yes |
| `glRenderbufferStorageMultisample` | :820, :832, unguarded | yes — but behind a runtime `msaa_level` |

So `oops-gl` needs **`glBlitFramebuffer`**, and `glRenderbufferStorageMultisample` only if
multisampling is left on. It already has the rest of the framebuffer object family
(`glGenFramebuffers`, `glBindFramebuffer`, `glFramebufferTexture2D`). It has no vertex array
objects, and on this path it does not need any — a core profile would require one, and this port
does not ask for a core profile.

This correction is recorded rather than quietly fixed because the first count was produced the
wrong way, and the same mistake in the other direction is what made a texture bug take a day on
2026-09-24: **a symbol grep over a file with platform branches measures a build nobody runs.**

**And `libultraship` already has a low-GL profile.** `gfx_opengl.cpp` selects its shader dialect
at compile time: `#version 410 core` on Apple, `#version 300 es` under `USE_OPENGLES`, and
otherwise `#version 130` with `varying`, `texture2D` and `gl_FragColor` — the GL 2.1-era idioms.
Nothing needs a core profile.

### The shaders work; the version number does not

Measured on 2026-09-25 with `gl2-probe`'s `libultraship-dialect` arm. Every construct the
renderer emits compiles and runs: `attribute`, `varying`, `texture2D`, `gl_FragColor`, two
samplers and an interpolated colour input, drawn and read back as the exact three-factor product.

**`#version 130` itself is refused.** `oops-gl` says so in its own words —
`only GLSL 1.10, 1.20 and ES 1.00 are implemented; this shader asks for another` — and `130` is
the one thing about that branch which is not 1.20.

So the port carries **a one-line patch turning `#version 130` into `#version 120`** in
`libultraship/src/fast/backends/gfx_opengl.cpp`. That is the minimal change: the alternative is
claiming 1.30 in `oops-gl` while implementing none of what 1.30 added (`in`/`out`, `texture()`,
integer operations), which would be a lie the next port discovers.

The patch reaches a submodule, which works because `common/upstream-fetch.sh` checks submodules
out *before* applying patches — an ordering chosen for exactly this and paid off within the hour.

The probe arm asserts both halves, so it is also what retires the patch: if `oops-gl` ever
implements 1.30, the arm's second half fails and whoever sees it should delete the patch rather
than the assertion.

**Worth recording how this was nearly missed.** Reading `glsl_pp.c` showed `do_version` storing
the number without judging it, which says 130 is accepted. It is not — the gate is further in. A
first bisect appeared to confirm the reading because every branch of it returned non-zero, and
non-zero is a pass. One `printf` of the compiler's info log answered in a single run what five
pass/fail runs could not.

## The family, which is the real argument

`libultraship` is not this title's renderer. It is Harbour Masters' shared runtime, and every one
of these is built on it:

| port | game | calls GL directly |
|---|---|---|
| Shipwright (this) | Ocarina of Time | no |
| 2ship2harkinian | Majora's Mask | no |
| Starship | Star Fox 64 | no |
| SpaghettiKart | Mario Kart 64 | no |
| PaperBoat | Paper Mario 64 | no |
| Ghostship | — | no |

Every one measured **zero** direct GL calls: the renderer is entirely inside `libultraship`. So
the platform work done here is done for all of them, and the second title out of this family
should cost roughly what Neverputt cost after Neverball.

## Run time, not build time

This is the distinction that separated the candidates, and it is worth stating precisely because
`../README.md` has carried a rule against user-supplied assets since before any of this existed.

- **sm64-port, perfect_dark, `zeldaret/oot`, `zeldaret/mm`** extract assets during the *build*.
  Without a ROM on the build machine the build fails. Nothing ships.
- **This** builds with no ROM anywhere. The player puts their own copy on the console, the game
  notices it on first run, converts it into an `.o2r`/`.otr` archive and starts.

The second shape is the one the existing rule already makes room for in its emulator exception:
it builds, boots, and asks. `soh/CMakeLists.txt:623` links `ZAPDLib` into the game binary, so the
conversion genuinely runs on the device rather than on a PC beforehand — which is the whole
reason the property holds.

## What this costs, measured rather than guessed

At the pinned revision:

| part | C | C++ | headers |
|---|---|---|---|
| `soh/soh` (the game) | 11 | 401 | 228 |
| `libultraship/src` (the runtime) | 1 | 138 | — |
| `ZAPDTR` (asset conversion, linked in) | 12 | 93 | 103 |
| `OTRExporter` | 0 | 26 | 29 |

**C++20** (`CMAKE_CXX_STANDARD 20`). For scale, Extreme Tux Racer — the largest C++ port here
before this — is 45 sources.

Twelve third-party libraries, of which the collection already pins three:

The order to vendor them in is **measured from what the sources actually include**, not from the
order CMake fetches them — the two disagree sharply, and the CMake order would have started with
the wrong one.

| library | files including it | vendored at | built |
|---|---|---|---|
| **spdlog** | **123** | [`oops-deps/spdlog`](../../oops-deps/spdlog) | header-only |
| nlohmann/json | 42 | [`oops-deps/nlohmann-json`](../../oops-deps/nlohmann-json) | header-only |
| tinyxml2 | 40 | [`oops-deps/tinyxml2`](../../oops-deps/tinyxml2) | `libtinyxml2.a` |
| prism-processor | 3 | [`oops-deps/prism-processor`](../../oops-deps/prism-processor) | `libprism.a`, 6 sources |
| libgfxd | 1 | [`oops-deps/libgfxd`](../../oops-deps/libgfxd) | `libgfxd.a` |
| stb | 1 | [`oops-deps/stb`](../../oops-deps/stb) | `libstb.a` |
| thread-pool | 1 | [`oops-deps/thread-pool`](../../oops-deps/thread-pool) | header-only |
| StormLib | archive layer | [`oops-deps/stormlib`](../../oops-deps/stormlib) | `libstorm.a`, 231 sources |
| libzip | archive layer | [`oops-deps/libzip`](../../oops-deps/libzip) | `libzip.a`, 114 sources |
| zlib | — | [`oops-deps/zlib`](../../oops-deps/zlib) | `libz.a`, already vendored |
| SDL | — | [`oops-deps/sdl2`](../../oops-deps/sdl2) | already vendored |
| single-header-metal-cpp | — | — | Apple only, not needed |

The archive pair is the ROM-reading path, reached through
`libultraship/include/ship/resource/archive/` rather than by direct include — which is why a grep
for their headers in the sources finds nothing. `OtrArchive.h` reads `.otr` through StormLib and
`O2rArchive.h` reads `.o2r` through libzip; neither is optional.

**spdlog first was the uncomfortable answer**, because it is the largest of them and the one that
leans hardest on the C++20 standard library rather than on anything this SDK controls. It was also
unavoidable: 123 of the tree's files include it, so nothing else could be compiled and checked
until it was there. Starting with the small header-only ones would have produced three vendored
directories and no way to tell whether any of them worked.

It paid for itself twice over. Making spdlog compile meant giving libc++ a threading API, and
`oops-deps/libcxx/include/__external_threading` is what every threaded C++ port after this one
stands on — prism-processor's `std::variant` and `std::string` are already using it.

## Submodules

`libultraship`, `ZAPDTR` and `OTRExporter` are submodules of the pinned commit, and the build is
nothing without them. `common/upstream-fetch.sh` gained `UPSTREAM_SUBMODULES=1` for this title:
it checks them out after the revision is verified and before patches apply, then confirms each
declared path is non-empty, because `git submodule update` reports success for a module it
decided to skip.

One hash still pins everything — a submodule's revision lives in the superproject's tree, so
there is no second pin to drift. Note that `torch`, which newer Shipwright HEAD uses, does **not**
exist at `9.2.3`; the asset pipeline here is the older `ZAPDTR` + `OTRExporter` pair.

## State

Every third-party library this title needs is vendored, pinned and building. Nothing of the
title's own code is compiled yet.

The order of work, cheapest useful thing first:

1. ~~`glBlitFramebuffer` and `glRenderbufferStorageMultisample` in `oops-gl`~~ — **done**,
   2026-09-25, with `test_gl2_blit_framebuffer_reads_the_read_binding`. The blit needed the GL 3.0
   read/draw binding split, which `oops-gl` did not have.
2. ~~A shader-dialect probe arm~~ — **done**, `gl2-probe`'s `libultraship-dialect`. It found the
   `#version 130` refusal above, which no amount of reading the front end had.
3. ~~The `#version 120` patch~~ — **done**, `patches/0001-ask-for-the-glsl-this-target-implements.patch`.
   One line, verified end to end: the fetch applies it *inside the `libultraship` submodule*,
   which works because submodules are checked out before patches run.
4. ~~**A threaded libc++.**~~ — **done**. Compiling spdlog's header found the real blocker:
   `_LIBCPP_HAS_THREADS 0`, so `std::mutex`, `std::thread` and `std::condition_variable` did not
   exist, and 11 of the 15 errors were that. `oops-sdk` already had the whole API
   (`oops_thread_*`, `oops_mutex_*`, `oops_sem_*`), and libc++ has
   `_LIBCPP_HAS_THREAD_API_EXTERNAL` for exactly this. It is an `oops-deps/libcxx` job rather than
   a title one, and every threaded C++ port after this one gets it.
5. ~~The rest of the nine, in the measured order~~ — **done**. All ten are in the table above and
   all ten build.
6. ~~The archive pair, `StormLib` and `libzip`~~ — **done**. The ROM-reading path is there:
   `SFileOpenArchive`/`SFileReadFile` and `zip_open`/`zip_fread` are all defined, and each archive
   was checked with `nm --undefined-only` rather than by the build succeeding — which for StormLib
   is how 201 missing sources were caught.
7. **The title's own sources.** 669 C++ files across `soh/soh`, `libultraship/src`, `ZAPDTR` and
   `OTRExporter`, and an entry point. This is where the work is now.
