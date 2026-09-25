/*
 * SuperTux's `config.h`, which upstream's CMake generates from `config.h.cmake`.
 *
 * Written by hand because this port does not run CMake, and every line is a decision rather than
 * a transcription - the template has a `#cmakedefine` for each one and no default. The ones that
 * matter are commented; the rest are the values any 64-bit little-endian build would get.
 */
#ifndef CONFIG_H
#define CONFIG_H

#define PACKAGE_NAME "supertux2"

#define INSTALL_SUBDIR_BIN "."
#define INSTALL_SUBDIR_SHARE "data"

/* Squirrel reads `_SQ64` to size its integers and pointers; upstream derives it from the pointer
 * size CMake measured. This target is 64-bit, so it is set rather than measured. */
#define SIZEOF_VOID_P 8
#define _SQ64

/* tinygettext's iconv wrapper. SDL's `SDL_iconv` is `const`-correct, so the `const` form. */
#define HAVE_ICONV_CONST
#define ICONV_CONST const

/* **The renderer is compiled in; which one is `-DUSE_OPENGLES2` in the Makefile.** Without
 * `HAVE_OPENGL` only the SDL software renderer exists, which is the fallback and not the port. */
#define HAVE_OPENGL

/* **Unset, and nothing upstream tests it at this revision.** `src/addon/downloader.cpp` includes
 * `<curl/curl.h>` unconditionally, so the add-on downloader is answered by the shim's
 * `curl/curl.h` rather than by this switch. Left unset so that a later upstream which does test
 * it gets the honest answer. */
/* #undef HAVE_LIBCURL */

/* **Where the data is.** `/app0/data` is inside the package, as it is for Extreme Tux Racer and
 * Neverball, because the package is the only place on this target a title's content can be
 * guaranteed to be. `main.cpp` passes this through `boost::filesystem::canonical` and mounts it
 * with PhysFS. */
#define BUILD_DATA_DIR "/app0/data"
#define BUILD_CONFIG_DATA_DIR "/app0/data"

/* Off: rich presence has no service to talk to here, and `src/sdk/discord.cpp` compiles to
 * nothing without it. */
/* #undef ENABLE_DISCORD */

/* **On.** A console title is left through the system, not through its own menu - and a "Quit"
 * entry that returns from `main` hands control back to a loader that has nothing to do with it.
 * Upstream added this switch for exactly that shape of platform. */
#define REMOVE_QUIT_BUTTON

/* #undef ENABLE_SQDBG */
/* #undef ENABLE_BINRELOC */
/* #undef WORDS_BIGENDIAN */
/* #undef UBUNTU_TOUCH */
/* #undef ENABLE_TOUCHSCREEN_SUPPORT */

#endif /* CONFIG_H */
