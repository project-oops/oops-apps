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

| library | status |
|---|---|
| SDL | **already vendored** (`src/oops-deps/sdl2`) |
| libzip → zlib | zlib **already vendored**; libzip is new |
| stb | header-only |
| nlohmann/json, thread-pool | header-only C++ |
| tinyxml2 | small, C++ |
| spdlog | C++, wants `<format>`-era library support |
| StormLib | MPQ archives — this is what reads `.otr` |
| libgfxd | F3D display-list decoder, C |
| prism-processor | shader template processor |
| single-header-metal-cpp | Apple only, not needed |

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

Fetch and pin verified. Nothing is built yet.

The order of work, cheapest useful thing first:

1. ~~`glBlitFramebuffer` and `glRenderbufferStorageMultisample` in `oops-gl`~~ — **done**,
   2026-09-25, with `test_gl2_blit_framebuffer_reads_the_read_binding`. The blit needed the GL 3.0
   read/draw binding split, which `oops-gl` did not have.
2. ~~A shader-dialect probe arm~~ — **done**, `gl2-probe`'s `libultraship-dialect`. It found the
   `#version 130` refusal above, which no amount of reading the front end had.
3. **The `#version 120` patch**, as `patches/0001-*`. One line, and the arm that justifies it
   already exists.
4. **Vendor the dependencies** in `src/oops-deps/`, in the order the build needs them — nine to
   go, `make survey` counts them.
5. **Then** the title's own sources.
