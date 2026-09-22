/*
 * FreeType's build options, as upstream sets them, with one turned off.
 *
 * Selected by `-DFT_CONFIG_OPTIONS_H=<ftoption-oops.h>`, which is FreeType's own documented hook
 * (`include/freetype/config/ftheader.h`).
 *
 * **This includes upstream's header rather than replacing it.** The first version was a `sed`
 * copy of the whole thing with one line changed - a thousand lines of somebody else's file in
 * this tree, which the identity scan stopped on the first try because FreeType's option comments
 * cite contributors by name and address. Including and overriding is smaller, carries every
 * other option forward across a bump without anybody re-copying, and leaves upstream's text
 * where it belongs.
 *
 * # What is turned off, and why
 *
 * `FT_CONFIG_OPTION_USE_ZLIB` brings in `src/gzip/ftgzip.c`, which is the only thing in a font
 * build that calls `setjmp` and `longjmp`. Those were imports from the platform's C library
 * until `oops-sdk` grew its own; even so, the module is 3,000 lines serving WOFF fonts and
 * gzip-compressed SVG glyphs, and no title here ships either.
 *
 * Removing the module from the source list alone was not enough - `sfnt/sfwoff.c` and
 * `sfnt/ttsvg.c` call `FT_Gzip_Uncompress` directly, so that only moved the undefined symbol.
 * This option is the switch that removes the callers too.
 */
#ifndef OOPS_FTOPTION_H
#define OOPS_FTOPTION_H

#include <freetype/config/ftoption.h>

#undef FT_CONFIG_OPTION_USE_ZLIB

#endif /* OOPS_FTOPTION_H */
