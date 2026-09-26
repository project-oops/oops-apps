/*
 * `utils/StringHelper.h`, forwarding to where libultraship keeps it.
 *
 * `src/engine/mods/ModManager.cpp` is the one source that writes the short form; the header is at
 * `libultraship/include/ship/utils/StringHelper.h` and every other source here reaches it as
 * `ship/utils/StringHelper.h`.
 *
 * A forwarding header rather than `-I libultraship/include/ship` on the path: that directory holds
 * `resource/`, `utils/`, `window/` and more at its top level, and this title has its own
 * `src/port/resource/`, so adding it would put two meanings of `resource/...` on the path.
 */
#ifndef OOPS_SPGK_UTILS_STRINGHELPER_H
#define OOPS_SPGK_UTILS_STRINGHELPER_H

#include <ship/utils/StringHelper.h>

#endif /* OOPS_SPGK_UTILS_STRINGHELPER_H */
