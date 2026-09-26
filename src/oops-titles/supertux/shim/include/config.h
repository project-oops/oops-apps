/*
 * SuperTux's `config.h`, which upstream's CMake generates from `config.h.cmake`.
 *
 * This port does not run CMake. Non-obvious settings are commented; the rest are the
 * values any 64-bit little-endian build gets.
 */
#ifndef CONFIG_H
#define CONFIG_H

#define PACKAGE_NAME "supertux2"

#define INSTALL_SUBDIR_BIN "."
#define INSTALL_SUBDIR_SHARE "data"

/* Squirrel reads `_SQ64` to size its integers and pointers. The Makefile passes it too,
 * because Squirrel's own sources do not include this file. */
#define SIZEOF_VOID_P 8
#ifndef _SQ64
#define _SQ64
#endif

/* tinygettext's iconv wrapper. SDL's `SDL_iconv` is `const`-correct, so the `const`
 * form. */
#define HAVE_ICONV_CONST
#define ICONV_CONST const

/* Compiles the GL renderers; the Makefile's `-DUSE_OPENGLES2` picks one. Without it
 * only the SDL software renderer exists. */
#define HAVE_OPENGL

/* Unset: upstream does not test it at this revision. `src/addon/downloader.cpp`
 * includes `<curl/curl.h>` unconditionally, and the shim's `curl/curl.h` answers it. */
/* #undef HAVE_LIBCURL */

/* The data directory inside the package. `main.cpp` passes it through
 * `boost::filesystem::canonical` and mounts it with PhysFS. */
#define BUILD_DATA_DIR "/app0/data"
#define BUILD_CONFIG_DATA_DIR "/app0/data"

/* Off: rich presence has no service to talk to here, and `src/sdk/discord.cpp` compiles
 * to nothing without it. */
/* #undef ENABLE_DISCORD */

/* On: a console title is closed through the system, and returning from `main` hands
 * control back to a loader with nothing to do. */
#define REMOVE_QUIT_BUTTON

/* #undef ENABLE_SQDBG */
/* #undef ENABLE_BINRELOC */
/* #undef WORDS_BIGENDIAN */
/* #undef UBUNTU_TOUCH */
/* #undef ENABLE_TOUCHSCREEN_SUPPORT */

#endif /* CONFIG_H */
