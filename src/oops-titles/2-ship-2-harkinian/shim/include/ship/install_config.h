/*
 * `ship/install_config.h`, standing in for the header CMake generates from
 * `libultraship/src/ship/install_config.h.in`.
 *
 * NON_PORTABLE stays undefined, so libultraship keeps its assets and settings beside
 * the executable. Defined, it would read assets from CMAKE_INSTALL_PREFIX and settings
 * from `SDL_GetPrefPath`; a payload is mounted at `/app0` and has neither a system
 * prefix nor a per-user preferences directory.
 *
 * CMAKE_INSTALL_PREFIX is still defined because `Context.cpp:408` names it inside the
 * NON_PORTABLE branch, and `/app0` is where the program lives.
 */
#ifndef OOPS_SOH_INSTALL_CONFIG_H
#define OOPS_SOH_INSTALL_CONFIG_H

#define CMAKE_INSTALL_PREFIX "/app0"

/* #undef NON_PORTABLE - see above. */

#endif /* OOPS_SOH_INSTALL_CONFIG_H */
