# FreeType

Font rasterisation, pinned at `VER-2-13-3`. Extreme Tux Racer wants it directly - 24 of its 45
sources include `ft2build.h` and its data ships six TrueType fonts - and Neverball reaches the
same library through SDL2_ttf, which is why it lives here rather than in either title.

Same arrangement as every dependency here: an origin, our build glue, and nothing of upstream's
committed. **`patches/` is empty**, and that is the whole story of the port - FreeType is plain C
that asks its platform for very little.

## Where it stands

**15 of 15 sources compile** for `x86_64-unknown-freebsd -ffreestanding`, against `oops-sdk`'s
libc and no host headers.

## Fifteen files, not two hundred

FreeType builds every font format it knows: Type 1, CFF, CID, PCF, BDF, PFR, Windows FNT, plus
validators and a glyph cache. `oops-freetype.mk` compiles the core, the `sfnt`/`truetype` driver
pair, both rasterisers and the autohinter - upstream's own minimal TrueType set.

The list is trimmed for **surface** rather than size: a format no title opens is still a parser
reading untrusted bytes, and still something to carry across a bump.

## What it wanted from the SDK, and what that changed

Building it found three genuine gaps in `oops-sdk`'s libc, all now filled there rather than
shimmed here:

| Wanted | Answer |
|---|---|
| `<setjmp.h>` | new SDK header. Declarations over the platform's own - `setjmp` cannot be written in C, and the console's FreeBSD-derived libc already has it. `jmp_buf` is FreeBSD's amd64 layout because the platform writes it. |
| `getenv` | new in the SDK, answering NULL. A payload has no environment block, so every name is unset - which is what NULL means, not a stub. |
| `FP_NAN` and its four siblings | were already needed by libc++; arrived with it |

**The shape of the `setjmp` dependency is worth knowing.** `ftstdlib.h` includes `<setjmp.h>`
unconditionally for the `ft_jmp_buf` *type*, so every translation unit needs the header - while
the *functions* are called only from the gzip and LZW decompressors. A TrueType build compiles
against the header and never references the symbols, which is why this works without
`setjmp`/`longjmp` having been confirmed to bind on hardware. A title that turns on compressed
font support should want that confirmed first, and the SDK header says so.
