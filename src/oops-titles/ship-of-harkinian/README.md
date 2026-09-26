# Ship of Harkinian

A native port of *The Legend of Zelda: Ocarina of Time*, built on the `zeldaret/oot`
decompilation - [upstream](https://github.com/HarbourMasters/Shipwright), pinned at `9.2.3`.

**It plays from the player's own copy of the game, which they provide on the hardware, at run
time.** That property is explained below, because it is a policy question as much as a technical
one.

`make compile-survey` compiles the tree with the build's own flags. It uses `-fsyntax-only` and
does not link, and a payload link does not report an unresolved symbol, so a clean survey is not
a working port.

## The GL surface

`gfx_opengl.cpp` compiles differently per platform, so the surface is counted for the
configuration this port builds, against the definitions in `oops-sdk/src/gl/*.c`:

| symbol | where | compiled here? |
|---|---|---|
| `glGenVertexArrays`, `glBindVertexArray` | :725-726, inside `#if defined(__APPLE__) \|\| defined(USE_OPENGLES)` (:724-727) | not from here - but see below |
| `glBlitFramebuffer` | :907, :977, :994, unguarded | yes |
| `glRenderbufferStorageMultisample` | :820, :832, unguarded | yes - but behind a runtime `msaa_level` |

**A symbol grep over a file with platform branches measures a build nobody runs.**

ImGui's GL3 backend, which libultraship links, uses vertex array objects unconditionally on
desktop GL. `imgui_impl_opengl3.cpp` needs `glGenVertexArrays`, `glBindVertexArray`,
`glDeleteVertexArrays`, `glGetStringi`, and the enums `GL_MAJOR_VERSION`, `GL_MINOR_VERSION`,
`GL_NUM_EXTENSIONS`, `GL_VERTEX_ARRAY_BINDING`, `GL_PIXEL_UNPACK_BUFFER` and
`GL_PIXEL_UNPACK_BUFFER_BINDING`. Grepping the port measures the port, and the port is not the
whole link. A VAO captures attribute state rather than wrapping a handle: the array state and the
element-array-buffer binding belong to the object, and the current attribute values do not.

## The shader dialect

`gfx_opengl.cpp` selects its shader dialect at compile time: `#version 410 core` on Apple,
`#version 300 es` under `USE_OPENGLES`, and otherwise `#version 130` with `varying`, `texture2D`
and `gl_FragColor` - the GL 2.1-era idioms. Nothing needs a core profile.

`gl2-probe`'s `libultraship-dialect` arm checks every construct the renderer emits: `attribute`,
`varying`, `texture2D`, `gl_FragColor`, two samplers and an interpolated colour input, drawn and
read back as the exact three-factor product. oops-gl refuses `#version 130` -
`only GLSL 1.10, 1.20 and ES 1.00 are implemented; this shader asks for another` - so
`patches/0001-ask-for-the-glsl-this-target-implements.patch` turns it into `#version 120` in
`libultraship/src/fast/backends/gfx_opengl.cpp`. Claiming 1.30 in oops-gl while implementing none
of what 1.30 added (`in`/`out`, `texture()`, integer operations) would be a false claim.

The probe arm asserts both halves, so it also retires the patch: if oops-gl implements 1.30, the
arm's second half fails, and the patch is deleted rather than the assertion.

## The family

`libultraship` is Harbour Masters' shared runtime, not this title's renderer, and every one of
these is built on it:

| port | game | calls GL directly |
|---|---|---|
| Shipwright (this) | Ocarina of Time | no |
| 2ship2harkinian | Majora's Mask | no |
| Starship | Star Fox 64 | no |
| SpaghettiKart | Mario Kart 64 | no |
| PaperBoat | Paper Mario 64 | no |
| Ghostship | - | no |

The renderer is entirely inside `libultraship`, so the platform work here is shared by all of
them.

## Run time, not build time

- **sm64-port, perfect_dark, `zeldaret/oot`, `zeldaret/mm`** extract assets during the build.
  Without a ROM on the build machine the build fails, and nothing ships.
- **This** builds with no ROM. The player puts their own copy on the hardware, the game notices it
  on first run, converts it into an `.o2r`/`.otr` archive and starts.

The second shape is the one the titles rule makes room for in its emulator exception: it builds,
boots, and asks. `soh/CMakeLists.txt:623` links `ZAPDLib` into the game binary, so the conversion
runs on the device rather than on a PC beforehand.

## Dependencies

The game (`soh/soh`), the runtime (`libultraship/src`), and the linked-in asset conversion
(`ZAPDTR`, `OTRExporter`) are C++20 (`CMAKE_CXX_STANDARD 20`) with some C.

| library | vendored at | built |
|---|---|---|
| spdlog | [`oops-deps/spdlog`](../../oops-deps/spdlog) | header-only |
| nlohmann/json | [`oops-deps/nlohmann-json`](../../oops-deps/nlohmann-json) | header-only |
| tinyxml2 | [`oops-deps/tinyxml2`](../../oops-deps/tinyxml2) | `libtinyxml2.a` |
| prism-processor | [`oops-deps/prism-processor`](../../oops-deps/prism-processor) | `libprism.a` |
| libgfxd | [`oops-deps/libgfxd`](../../oops-deps/libgfxd) | `libgfxd.a` |
| stb | [`oops-deps/stb`](../../oops-deps/stb) | `libstb.a` |
| thread-pool | [`oops-deps/thread-pool`](../../oops-deps/thread-pool) | header-only |
| StormLib | [`oops-deps/stormlib`](../../oops-deps/stormlib) | `libstorm.a` |
| libzip | [`oops-deps/libzip`](../../oops-deps/libzip) | `libzip.a` |
| zlib | [`oops-deps/zlib`](../../oops-deps/zlib) | `libz.a` |
| SDL | [`oops-deps/sdl2`](../../oops-deps/sdl2) | see its README |
| ImGui | [`oops-deps/imgui`](../../oops-deps/imgui) | see above |
| single-header-metal-cpp | - | Apple only, not needed |

The archive pair is the ROM-reading path, reached through
`libultraship/include/ship/resource/archive/` rather than by direct include, so a grep for their
headers in the sources finds nothing. `OtrArchive.h` reads `.otr` through StormLib and
`O2rArchive.h` reads `.o2r` through libzip; neither is optional. Each archive is checked with
`nm --undefined-only` rather than by the build succeeding.

spdlog's threading needs libc++'s external threading API, in
`oops-deps/libcxx/include/__external_threading`, which every threaded C++ port shares.

`patches/0002` covers the two arms of libultraship that decide how GL arrives.

## Submodules

`libultraship`, `ZAPDTR` and `OTRExporter` are submodules of the pinned commit. With
`UPSTREAM_SUBMODULES=1`, `common/upstream-fetch.sh` checks them out after the revision is verified
and before patches apply - so a patch can reach into a submodule - then confirms each declared
path is non-empty, because `git submodule update` reports success for a module it skipped.

One hash pins everything: a submodule's revision lives in the superproject's tree, so there is no
second pin to drift. `torch`, which newer Shipwright uses, does not exist at `9.2.3`; the asset
pipeline here is the `ZAPDTR` + `OTRExporter` pair.
