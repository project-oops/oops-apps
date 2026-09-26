# gl2-probe - reference

The check taxonomy and design notes for the OpenGL 2.0 conformance probe. The
[README](../README.md) is the overview; this is what the suite measures.

Run the checks with `make check`, or `./bin/oops-apps check gl2-probe` from the repository root.

## What it measures

A probe compares two implementations of the same specification. `gl1-probe`'s pair is the
software rasteriser and the hardware's fixed-function pipeline; this one's is the software
reference and the hardware GL 2.0 back end - the compiler in `glsl_ps.c` and the draw path that
binds what it emits. The suite in `gl2_probe.c` is shared, with a thin runner at each end:
`gl2_probe_selftest.c` on a build machine, `gl2_probe_main.c` on the hardware, and one table of
checks that neither owns.

| Group | What it holds | Why |
|---|---|---|
| The object model | **version gating**, the shared name space, compile and link status, deferred deletion, uniform and attribute locations, the limits | The API is wired. These answer the same on any implementation - they read variables - so they are the minimum rather than the bulk |
| The vertex stage | a program draws, the `mvp` uniform transforms, attribute arrays and current values, the fixed-function built-ins, a vertex-only program | Each ends in a pixel |
| Varyings | interpolation, **perspective correctness**, several at once | The perspective check is the reason the group exists |
| The fragment stage | `gl_FragCoord`, `discard`, `gl_FragDepth`, `gl_FrontFacing`, a fragment-only program, two texture lookups, derivatives | |
| The language | matrix arithmetic, swizzles, control flow, user functions, the built-in library, `mod`, the relational built-ins, short-circuit evaluation, constructors, and **GLSL 1.20** | A compiler that got one of these wrong would pass every check above it |
| What still applies | the depth test, blending, culling, the scissor and the colour mask, and that a draw is deterministic | A programmable pipeline does not replace the per-fragment operations, and an implementation that ran a shader round them would look right until something was blended |
| The rest of GL 2.0 | separate stencil state, a blend equation per channel group, `glDrawBuffers` | The parts of the version that are not about shaders |

## Checks worth naming

**`varying-perspective`.** A varying is linear in clip space, not in screen space. The quad it
draws has `w = 1` on its left edge and `w = 3` on its right, so the value at the screen midpoint
is 0.25 and not 0.5. An affine interpolator answers 128 where this expects 64, and every other
check in the suite passes either way - a quad drawn flat to the viewer cannot tell them apart.

**`mod-and-int-divide`.** GLSL's `mod` is a floored modulus and C's `fmod` is truncated:
`mod(-1.0, 4.0)` is 3 in a shader and -1 in C. A shader tiling a texture by `mod(uv, 1.0)` wraps
correctly with one and mirrors at the origin with the other.

**`short-circuit`.** `&&` and `||` must not evaluate their right operand when the left decides
the answer, which a shader relies on to guard a divide. The right operand here writes through an
`out` parameter, so an implementation that evaluated it anyway leaves a mark.

**`version-gating`** is the only check that takes something away. A context has the entry points
its version defines and no others, so this narrows the suite's own context to 1.5 mid-run,
requires the whole programmable surface to be refused - `GL_INVALID_OPERATION` from a call,
`GL_INVALID_ENUM` from a query, and the query's destination left alone - and then widens it back,
because a claim is a property of the context rather than a one-way switch. It also requires
GL 1.x to be untouched by the narrowing, which shows the gate is on the right boundary.

**`glsl-120`** checks that the two dialects are two. `2 * 0.25` is 0.5 under `#version 120` and a
compile error under `#version 110`, because 1.20 converts `int` to `float` implicitly and 1.10
converts nothing. It measures the result as a colour, because a front end that accepted the
conversion and an interpreter that truncated the answer to 0 would both pass a check that only
compiled. It also requires the 1.10 half to fail.

**`separate-stencil`** draws the shadow-volume idiom: two quads of opposite winding in one draw,
incrementing where a front face passes and decrementing where a back one does, so the stencil
returns to its clear value everywhere both were drawn. An implementation carrying one stencil
state for both faces increments twice and leaves 2, and the check paints only where the buffer
came back to 0.

**`gl_FrontFacing`** is checked against the triangle's own winding, the same sign culling reads -
not two-sided lighting's `prim_polygon_back`, which is set only while GL_LIGHTING and
GL_LIGHT_MODEL_TWO_SIDE are both on. **`texture-sampler`** checks that the texture bridge receives
the GL target enumerant rather than the front end's own type enumerant, which would match no case
and return opaque black.

## The two runners

`FORMATS` is `eboot title` - the artifacts this collection installs. There is no `elf` among
them: that format stages a bare ELF for a homebrew loader, and these are native titles. The
`.elf` is still built, as the intermediate both of the others are made from.

A host run prints a table when it returns; a hardware run cannot assume it will return, because
a compiled shader is words this repository generated and a wave that does not retire takes the
frame with it. So `gl2_probe_main.c` names each check as it starts and again with its verdict,
and a hang leaves behind the name of the check that hung.

The last line, `gl2-probe: NNN/NNN passed on hardware`, is the machine-readable verdict - the
process cannot exit to hand back a status code, so the log is where the result lives.
