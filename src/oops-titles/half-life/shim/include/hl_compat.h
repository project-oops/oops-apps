/*
 * Force-included into the engine's sources (`-include`), for the one C library function the
 * engine calls that the SDK leaves out on purpose.
 *
 * `fscanf`: the SDK's `stdio.h` provides `sscanf` and asks callers to read a line with
 * `fgets` and scan that. `engine/client/identification.c` reads a sixteen-digit hex id from
 * `~/.xash_id` with `fscanf`, which is exactly one line, so `shim/hl_compat.c` does what the
 * SDK suggests on its behalf. It is scoped to this title rather than added to the SDK, where
 * a line-at-a-time `fscanf` would surprise a caller whose format spans lines.
 */
#ifndef HL_COMPAT_H
#define HL_COMPAT_H

#include <stdio.h>

int fscanf(FILE *f, const char *fmt, ...);

#endif
