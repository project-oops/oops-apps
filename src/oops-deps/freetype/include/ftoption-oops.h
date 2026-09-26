/*
 * FreeType's build options as upstream sets them, with one turned off. Selected by
 * `-DFT_CONFIG_OPTIONS_H=<ftoption-oops.h>` (`include/freetype/config/ftheader.h`).
 * Including upstream's header and overriding carries every other option across a bump.
 *
 * `FT_CONFIG_OPTION_USE_ZLIB` is off: it brings in `src/gzip/ftgzip.c`, which serves
 * WOFF fonts and gzip-compressed SVG glyphs that no title here ships. The option also
 * removes the direct `FT_Gzip_Uncompress` callers in `sfnt/sfwoff.c` and
 * `sfnt/ttsvg.c`.
 */
#ifndef OOPS_FTOPTION_H
#define OOPS_FTOPTION_H

#include <freetype/config/ftoption.h>

#undef FT_CONFIG_OPTION_USE_ZLIB

#endif /* OOPS_FTOPTION_H */
