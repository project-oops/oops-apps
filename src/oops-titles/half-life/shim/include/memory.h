/*
 * `memory.h` - the pre-standard name for the `mem*` functions, which now live in
 * `string.h`. hlsdk's `cl_dll/entity.cpp` still includes it.
 *
 * Here rather than in `common/posix/include`, where it first went: a shared include path
 * is searched by every title, and a game with a `memory.h` of its own - Spaghetti Kart's
 * `src/racing/memory.h` - found this one instead and lost the declarations in its own.
 */
#ifndef OOPS_HL_MEMORY_H
#define OOPS_HL_MEMORY_H

#include <string.h>

#endif
