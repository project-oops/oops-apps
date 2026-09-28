// `lib/sha256/cmake_config.h.in`. Its one question is `#cmakedefine HAVE_ENDIAN_H`,
// tested with `#ifdef`, so it lives apart from Luanti's own `cmake_config.h`, which
// defines that name to 0 - still "defined" to an `#ifdef`. There is no `<endian.h>`,
// and the portable byte swaps are used.

#pragma once
