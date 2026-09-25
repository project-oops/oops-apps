/*
 * `ship/install_config.h` - ours, standing in for the one CMake generates from
 * `libultraship/src/ship/install_config.h.in`.
 *
 * The template is two lines, and both are questions about how the program was *installed*:
 *
 *   #define CMAKE_INSTALL_PREFIX "@CMAKE_INSTALL_PREFIX@"
 *   #cmakedefine NON_PORTABLE
 *
 * # `NON_PORTABLE` is not defined, and that is the whole answer
 *
 * libultraship uses it to choose where its assets and its settings file live. Defined, it means
 * the program was installed into a system prefix and should look in `CMAKE_INSTALL_PREFIX` for
 * one and in the user's preferences directory (`SDL_GetPrefPath`) for the other - the shape a
 * Linux distribution package wants. Undefined, everything lives beside the executable.
 *
 * A payload is the second case and cannot be the first. It is mounted at `/app0`, there is no
 * system prefix to have been installed into, and there is no per-user preferences directory
 * because there is no multi-user filesystem - `SDL_GetPrefPath` on this target has nothing
 * truthful to return. So this is not a preference between two working options; one of them does
 * not exist here.
 *
 * `CMAKE_INSTALL_PREFIX` is still defined, because `Context.cpp:408` names it inside the
 * `NON_PORTABLE` branch and a macro that is only referenced in dead code still has to exist for
 * the file to preprocess. `/app0` is the honest value: if that branch ever were taken, it is
 * where this program is.
 */
#ifndef OOPS_SOH_INSTALL_CONFIG_H
#define OOPS_SOH_INSTALL_CONFIG_H

#define CMAKE_INSTALL_PREFIX "/app0"

/* #undef NON_PORTABLE - see above. */

#endif /* OOPS_SOH_INSTALL_CONFIG_H */
