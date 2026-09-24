# Dependencies

Third-party libraries a ported title needs, each **pinned to a commit and never committed here**.
Same shape as a title in `../oops-titles/`: an origin, our patches, our build glue, and nothing
of upstream's tracked.

**We do not write these.** `oops-sdk#D010` settled that for SDL and the reasoning holds for all
of them: a library upstream maintains is pinned and bridged, not reimplemented. Our side of each
is a `.mk` file, sometimes a config header its build system would have generated, and in three
cases a patch.

## What is here

| | Pinned at | Ours | For |
|---|---|---|---|
| [`sdl2`](sdl2/) | `release-2.30.9` | 7 backend files, 181-line patch | everything |
| [`sdl12-compat`](sdl12-compat/) | `release-1.2.76` | 79-line patch | an SDL 1.2 title |
| [`sdl2-image`](sdl2-image/) | `release-2.8.12` | a `.mk` | ETR |
| [`sdl2-mixer`](sdl2-mixer/) | `release-2.8.2` | a `.mk` | ETR |
| [`sdl2-ttf`](sdl2-ttf/) | `release-2.24.0` | a `.mk` | Neverball |
| [`freetype`](freetype/) | `VER-2-13-3` | a `.mk` | ETR, and SDL2_ttf beneath |
| [`libcxx`](libcxx/) | `llvmorg-21.1.8` | `__config_site`, 2 headers | any C++ title |
| [`libpng`](libpng/) | `v1.6.58` | one changed line in upstream's config | Neverball |
| [`zlib`](zlib/) | `v1.3.1` | a `.mk` | libpng |
| [`libvorbis`](libvorbis/) | `v1.3.7` | a `.mk` | Neverball |
| [`libogg`](libogg/) | `v1.3.5` | a generated header | libvorbis |
| [`libjpeg-turbo`](libjpeg-turbo/) | `3.1.4` | 2 config headers | Neverball |

All twelve compile freestanding for `x86_64-unknown-freebsd` against `oops-sdk`'s libc, with
`-nostdlibinc` so the build machine's headers cannot leak in.

## Four things that keep recurring

**Peel the tag.** `git ls-remote refs/tags/X` gives the *tag object* for an annotated tag, and
the commit is behind `^{}`. Seven of these are annotated. Getting it wrong puts a hash in a lock
that checks out a different tree, and it has happened here once already.

**A duplicate is usually the right answer.** The collection carries two PNG decoders and two
Vorbis decoders, and neither is waste: `oops_png_decode` and `stb_vorbis` answer "decode this
file", which is all SDL_mixer and a payload with a texture need; libpng and libvorbisfile answer
to programs written against their APIs, which cannot be served by anything else.

**Trim for surface, not size.** MP3, FLAC, Opus, MIDI and MOD are off in the mixer; the JPEG
encoder and arithmetic coder are off; FreeType builds TrueType and not eight other font formats.
Not to save bytes - a decoder no title has a file for is still a parser of untrusted input, and
still something to carry across a bump.

**Read upstream's own build for the file list.** `lib/*.c` in libvorbis sweeps in two standalone
tuning programs with their own `main()`; `libvorbis_la_SOURCES` in its `Makefile.am` already says
which files are the library.

## What building these changed in oops-sdk

Each of these is a gap a real consumer found, and each went into the SDK rather than being
shimmed per-dependency:

`errno.h` over the platform's `__error` · `setjmp.h` over the platform's · `getenv`/`setenv`/
`unsetenv` · `strtoll`/`strtoull`/`strtold` · `alloca` · the `FP_*` classification macros and
`fpclassify` · `rint`/`nearbyint`/`lrint` and their friends · and **`extern "C"` on every libc
header**, without which a C++ caller mangled every name and linked against nothing.

One gap was **not** filled, deliberately: `gmtime` and the calendar functions stay absent, because
`time()` on this target is not a wall clock and `oops-sdk`'s `<libc/time.h>` would rather a
program failed to link than printed 1970. libpng's tIME chunk support is off for the same reason,
which is agreement rather than a workaround.
