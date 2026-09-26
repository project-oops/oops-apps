# Porting SuperTux

Against `v0.6.3`, the revision `upstream.lock` pins.

SuperTux runs on oops-gl's **programmable** path: upstream's ES 2.0 renderer, one GLSL program,
three samplers. Everything else about it is the Extreme Tux Racer shape - upstream fetched and
never edited where a shim will do, the stack pinned under `src/oops-deps/`, the content copied
into the package.

## The renderer

SuperTux chooses its renderer at compile time first and at run time second:

| | backend | program |
|---|---|---|
| `-DUSE_OPENGLES2` | `GL33CoreContext` | `data/shader/shader100.*`, `#version 100` |
| `-DUSE_OPENGLES1` | `GL20Context` | none - fixed-function, with `glOrtho` mapped to `glOrthof` |
| neither | `GL33CoreContext`, then `GL20Context` if that throws | `shader330.*`, `#version 330` |

**This port builds `USE_OPENGLES2`.** oops-gl accepts `#version 100` as GLSL 1.10 and drops its
precision qualifiers, as the ES specification says they may be; the fragment shader samples
`diffuse_texture`, `displacement_texture` and `framebuffer_texture`, inside oops-gl's texture
image units. `make shadercheck` compiles both stages through oops-gl's own front end and code
generator, and the fragment shader generates for gfx1030.

The define does four things, all upstream's:

- `GLProgram` loads `shader100.*` rather than `shader330.*`.
- `gl20_context.cpp` and `gl_pixel_request.cpp` compile to nothing.
- `video/gl.hpp` defines `glGenVertexArrays`, `glDeleteVertexArrays` and `glBindVertexArray` as
  empty inline functions, because ES 2.0 has no vertex array objects.
- `video/gl.hpp` includes `<SDL_opengles2.h>` and nothing else. `shim/include/SDL_opengles2.h`
  answers it with oops-gl's own `GL/gl.h` and `GL/glext.h`, so there is one set of declarations
  and it is the one the defining library ships.

`make check` reads every file in `src/video/gl/` against oops-gl's `GL/gl.h` and reports each
entry point the ES 2.0 build calls. It fails if one goes missing, and it fails if oops-gl
declares one of the three vertex-array names, because a declaration beside upstream's inline stub
is a compile error.

**The default arm is not used.** oops-gl refuses `#version 330`, so `GLVideoSystem(true)` would
throw on the first frame and `VideoSystem::create` would catch it and fall back to the
fixed-function backend - the right renderer by the wrong route, through an exception, every
launch. At this revision `shader330.frag` declares the same three samplers and five uniforms as
`shader100.frag` and computes the same thing in GLSL 3.30 spelling. Its one extra branch, a water
reflection, sits after `else if (true)` and cannot run.

**The fixed-function backend is the fallback** - built with neither define and `video` set to
`opengl20` in the config. It needs a `GL/glew.h` in the shim, which the ES 2.0 build does not.

### What the shader path is for

`GLVideoSystem::apply_config` makes two render targets:

```cpp
m_lightmap.reset(new GLTextureRenderer(*this, m_viewport.get_screen_size(), 5));
if (m_use_opengl33core && g_config->fancy_gfx)
  m_back_renderer.reset(new GLTextureRenderer(*this, m_viewport.get_screen_size(), 1));
```

The **lightmap** is unconditional: every level with lighting renders it into a texture through a
framebuffer object and composites it back, which `gl2-probe`'s `fbo/texture` checks. The
attachment is colour only: oops-gl refuses a depth or stencil attachment with
`GL_FRAMEBUFFER_UNSUPPORTED`, and `GLFramebuffer` asks for neither.

The **back renderer** exists only on the shader path. It is the copy of the frame that
`shader100.frag`'s `backbuffer != 0.0` branch samples as `framebuffer_texture` and offsets by
`displacement_texture` - water surfaces and heat haze, which the fixed-function backend cannot
draw.

## The stack

**C++ with exceptions and RTTI.** The title builds with `OOPS_CXX_EXCEPTIONS = 1` against libc++,
libc++abi and libunwind - the route `src/oops-utilities/cxx-throw` exercises. Extreme Tux
Racer's `-fno-exceptions -fno-rtti` suits a program with neither, and not this one: the compiler
refuses a `throw` at the line that wrote it.

| | where | why there |
|---|---|---|
| libc++ / libc++abi / libunwind | `oops-deps/libcxx` | shared; the exceptions build `cxx-throw` uses |
| SDL2 | `oops-deps/sdl2` | shared |
| SDL2_image, libpng, zlib | `oops-deps/` | every sprite is a PNG |
| SDL2_ttf, FreeType | `oops-deps/sdl2-ttf` | upstream pins a fork; the game calls stock functions and none of the fork's additions |
| libogg, libvorbis | `oops-deps/` | `stream_sound_source.cpp` decodes with vorbisfile |
| **PhysFS** | upstream submodule | every file SuperTux opens goes through it |
| **Squirrel** | upstream submodule | every level's scripts |
| **sexp-cpp** | upstream submodule | the parser for every level, sprite and save |
| **tinygettext** | upstream submodule | translations, over `SDL_iconv` |
| **glm** | `oops-deps/glm` | header-only; the renderer's matrices |
| **OpenAL Soft** | `oops-deps/openal-soft` | all audio; see below |
| Boost | `shim/include/boost/` | see below |
| libcurl | `shim/include/curl/` | the add-on downloader, answered "no network" |

**The four submodules arrive with the tree and build with the title.** `UPSTREAM_SUBMODULES=1`
in the lock checks them out at the revisions the pinned commit records, so one hash pins all of
them. They are not `oops-deps/` entries because no other title here links them. The Makefile
takes from each what upstream's own build does - PhysFS trimmed to the `dir` and `zip` archivers
and its POSIX platform layer, with CD-ROM probing off because it calls `getmntinfo`.

The other two submodules are fetched and not compiled. `external/SDL_ttf` is a fork whose
additions the game does not call, and `external/discord-sdk` is off in `config.h`.

**OpenAL is a pinned OpenAL Soft on its SDL2 backend.** SuperTux calls `al*`/`alc*` functions
across three files - buffers, sources, a streaming queue, one listener. An OpenAL subset over SDL
audio would be a second implementation of a library upstream maintains, which
`oops-deps/README.md` rules out. OpenAL Soft's mixer runs on a thread; libc++ here has
`std::thread` over `scePthread`.

**Boost is shimmed, not pinned, because the surface is small.** Most uses are `boost::optional`,
which is `std::optional` with `get()`, `get_ptr()` and `boost::none`; the rest are
`boost::format`, `boost::ref`, `ends_with`, one `second_clock::local_time().date()` for the Saint
Nicholas Day easter egg, one `boost::locale::generator` whose answer is the classic locale, and
`boost::filesystem` operations over the POSIX shim - libc++ here compiles no `<filesystem>`
operations to put them on. Pinning Boost would fetch a superproject of over a hundred submodules
for a few headers' worth of it.

**libcurl is shimmed as absent.** `src/addon/downloader.cpp` includes `<curl/curl.h>`
unconditionally - `HAVE_LIBCURL` is defined in the template and tested nowhere - and uses it for
the add-on browser. The shim's `curl_easy_perform` and `curl_multi_perform` report a connection
failure, and upstream turns that into its own `download failed` exception - the path an offline
desktop takes.

## The shim

- **`include/config.h` and `include/version.h`** - what upstream's CMake would generate. Each
  line of `config.h` is a decision and says why; `REMOVE_QUIT_BUTTON` is on because a title here
  is left through the system.
- **`include/SDL_opengles2.h`** - oops-gl's headers, above.
- **`stx_start.cpp`** - the entry point. Walks `.init_array` (namespace-scope `std::string`
  constants, each a sound or sprite name) and calls upstream's `main` with
  `--datadir /app0/data`.

`patches/` is empty. A patch is for upstream's code that must behave differently here; a
difference that is a header or a function is answered by the shim.

## The content

`data/` ships inside the package at `/app0/data`, as Extreme Tux Racer's does - `make package`
copies it in. SuperTux reads it through PhysFS, and writes its config and saves through PhysFS's
write directory.

## Upstream's own dead code

`gl_pixel_request.cpp` calls `glFenceSync` and `glClientWaitSync`, GL 3.2, and its only use in
`gl_painter.cpp` is inside an `#if 0` - "glFenceSync() causes crashes on Intel I965". The ES 2.0
build empties the file anyway.

`gl_texture.cpp` wraps its one `glGenerateMipmap` in `#if 0` under "Disable the use of mipmaps
for the texture". `tools/glcheck.c` blanks `#if 0` regions before scanning for this reason;
without that it would name an entry point the game never calls.
