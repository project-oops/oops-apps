# Porting SuperTux

Measured against `v0.6.3` (`c1ddb4f2`), the revision `upstream.lock` pins.

## Where it stands

**The graphics are nearly free and the C++ stack is the whole job.** That is the opposite of the
expectation this title was queued under, so the reading is set out below rather than asserted.

| | |
|---|---|
| GL entry points the renderer needs | **29 of 33 are in oops-gl** - `make check` |
| What is missing | four, and all four are the same feature: framebuffer objects |
| Shaders | **none on this path.** The four files in `data/shader/` are not loaded |
| Upstream's own sources | 476 in `src/`; the selection has not been done |
| Submodules | six, and the shared fetcher does not fetch them |

## The renderer

SuperTux ships two OpenGL backends and picks between them at run time, in
`src/video/gl/gl_video_system.cpp`. It reads the major number out of `glGetString(GL_VERSION)`
with `sscanf("%d")`: 3 or more takes `GL33CoreContext`, exactly 2 takes `GL20Context`, and
anything else throws so that SDL's software renderer is used instead.

**`GL20Context` uses no shaders at all.** The whole of it is fixed-function GL 1.x:

| | |
|---|---|
| transform | `glMatrixMode`, `glLoadIdentity`, `glOrtho`, `glTranslatef` - including `GL_TEXTURE` matrix mode for scrolling textures |
| geometry | `glEnableClientState`, `glVertexPointer`, `glTexCoordPointer`, `glColorPointer`, `glDrawArrays` |
| state | `glEnable(GL_TEXTURE_2D)`, `glBindTexture`, `glColor4f`, `glBlendFunc` |

No `glCreateShader`, no `glUseProgram`, no `glUniform*`. It binds **one** texture - `bind_texture`
takes a `displacement_texture` argument and ignores it - and `set_blur` is an empty function.

So the `#version 100` and `#version 330` shaders in `data/shader/` belong to the GLES2 and
GL33Core paths, and this port never loads them. **Every GLSL question is therefore moot on this
path**: the unguarded `precision mediump float;` in `shader100.frag`, the `layout`/`in`/`out`/
`texture()` of `shader330.frag`, and the three samplers both of them declare against oops-gl's
two. None of it is reached.

That inverts the obvious guess. SuperTux looks like this collection's second OpenGL 2.0 title and
on this path it is a GL 1.x one - closer to Neverball, which already runs end to end, than to
Craft.

**Worth knowing if the GL33Core path is ever wanted:** `shader330.frag` would not compile here
even with GL 3 syntax support, and for a reason that is upstream's rather than ours. It tests
`blur != 0.0` where `blur` is a `uniform int`, and loops `for (float y = -blur; y <= blur; ++y)`
on a uniform bound. oops-gl refuses the first (no implicit int/float conversion) and cannot count
the trips of the second. Desktop drivers are lenient about both.

## What `make check` measures, and what it does not

`tools/glcheck.c` reads `upstream/src/video/gl/*.cpp` and oops-sdk's own `GL/gl.h` at run time
and reports every GL entry point the renderer calls that oops-gl does not declare. It is this
title's `shadercheck`: Craft's rendering question lived in its shaders because oops-gl compiles a
fragment shader or refuses it by name, and SuperTux's lives in the entry points because a GL
function that is not there is a **link error**. That is also why GLEW gets shimmed away rather
than used - a missing function should be a named link failure, not a null pointer in frame one.

**It measures surface, not behaviour.** A declared entry point can still be wrong; that is
gl1-probe's job. What this catches is the class of gap that stops a link dead.

**Passing means "unchanged", not "no gaps".** The four absent names are recorded in the tool's
`KNOWN_GAPS`, and it fails if the set moves in *either* direction - a new gap, or one closed
without this document being updated.

Three columns, because "absent" on its own would overstate the work by half:

- **absent on the GL 2.0 path** - the real list, below.
- **absent but GL33Core-only** - `glGenVertexArrays`, `glBindVertexArray`,
  `glDeleteVertexArrays`. This port takes the other backend.
- **absent but only in a file this port does not compile** - `glFenceSync`,
  `glClientWaitSync`. See below; they are upstream's dead code.

## The one real gap: framebuffer objects

Four entry points, one feature:

```
glGenFramebuffers  glBindFramebuffer  glFramebufferTexture2D  glDeleteFramebuffers
```

`GLFramebuffer` wraps them and `GLTextureRenderer` renders the **lightmap** through one.
**It is not a GL33Core luxury.** `GLVideoSystem::apply_config` creates the lightmap outside any
test of which backend is in use:

```cpp
m_lightmap.reset(new GLTextureRenderer(*this, m_viewport.get_screen_size(), 5));
if (m_use_opengl33core && g_config->fancy_gfx)
  m_back_renderer.reset(new GLTextureRenderer(*this, m_viewport.get_screen_size(), 1));
```

The second one - the displacement and blur pass - *is* gated, so that effect is simply off on
this path. The first is not.

**There are two ways to close it and the cheaper one needs no new GL.** oops-gl already has
`glCopyTexImage2D` and `glCopyTexSubImage2D`, and gl1-probe's `copy-tex` check - "Render-to-
texture, which is what `glCopyTexSubImage2D` is for" - passes on hardware. Rendering the lightmap
to the back buffer and copying it into the texture is the pre-FBO way of doing exactly this, and
it is a patch to two upstream files rather than a framebuffer-object subsystem in the GL layer.

The trade is real and is not being hidden: copy-to-texture costs a copy per lightmap frame that
an FBO does not, and it serialises where an FBO need not. Whether that matters here is a
measurement nobody has taken, on a title that does not yet run. **Implementing FBOs properly in
oops-gl is the better long-term answer and benefits every later title**; the patch is the answer
that gets SuperTux drawing first. This document does not pick one.

## The two that are not a gap

`gl_pixel_request.cpp` calls `glFenceSync` and `glClientWaitSync` - GL 3.2 sync objects, on a
path that claims GL 2.0. They are **dead code upstream**. Its only use, in `gl_painter.cpp`, is
inside an `#if 0`:

> FIXME: glFenceSync() causes crashes on Intel I965, so disable GLPixelRequest for now, it's not
> yet properly used anyway.

The file still compiles there, so the symbols are live link references to code that never runs.
Leaving it out of this port's source list costs nothing upstream has not already given up.
`tools/glcheck.c` marks the file `STX_DROPPED` so the two names are reported in their own column
rather than counted against the port - the decision is recorded where it is enforced.

`glGenerateMipmap` is the same shape and does not even reach the report: `gl_texture.cpp` wraps
it in `#if 0` under "Disable the use of mipmaps for the texture", and `glcheck` blanks `#if 0`
regions before scanning for exactly this reason. Without that it would have named an entry point
the game never calls, and the first person to read the report would have gone and implemented it.

## The submodules

**This is the first origin in the collection that needs them, and
`common/upstream-fetch.sh` does not recurse.** At this revision `.gitmodules` names six:

| | |
|---|---|
| `external/physfs` | the filesystem layer; SuperTux does not start without it |
| `external/squirrel` | the scripting language every level's logic is written in |
| `external/sexp-cpp` | the parser for upstream's own data format |
| `external/tinygettext` | translations |
| `external/SDL_ttf` | `src/oops-deps/sdl2-ttf` already pins this separately |
| `external/discord-sdk` | Discord rich presence; off, and should stay off |

Four are non-optional. Two options, and this is a decision for the collection rather than
something to settle inside one title:

1. **Teach `upstream-fetch.sh` submodules.** One flag and a stamp component, benefiting every
   later origin. The cost is that a submodule's revision is then pinned by upstream's tree rather
   than by a lock file here, which is exactly the property `upstream.lock`'s header says a pin
   exists to avoid.
2. **Pin each as its own `src/oops-deps/` entry**, as SDL2, FreeType, libpng and the rest already
   are. That keeps every revision under a lock file this repository owns and matches how the
   dependency stack is already built - at the cost of four more entries and of tracking upstream
   bumps by hand.

Option 2 is the shape the collection already uses; option 1 is less work once. Neither has been
chosen.

## What has not been measured

Stated so that nobody reads the green `make check` above as further than it goes:

- **The source selection.** 476 files in `src/`, and upstream's CMake chooses between renderers,
  scripting backends and optional features. A glob sweeps in arms this port does not take.
  Nobody has read it.
- **The C++ runtime.** Exceptions, RTTI and the static-initialisation guards. The collection's
  plan is that Extreme Tux Racer pays this once and the rest follow cheaply; SuperTux should not
  be the title that pays it, and `../README.md` in `src/oops-titles` says why.
- **Audio.** SuperTux wants OpenAL, and `src/oops-deps/` has `sdl2-mixer`, `libogg` and
  `libvorbis` but no OpenAL. That is a gap nobody has costed.
- **Squirrel and PhysFS on this target.** Both are C/C++ that has never been compiled here.
- **The data.** `data/` is most of the 418 MB the fetch brings down, and shipping it is the same
  problem Neverball's `make package` solves by copying `upstream/data` into the title directory.
