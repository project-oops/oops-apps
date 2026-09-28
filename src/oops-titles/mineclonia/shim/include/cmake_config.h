// `src/cmake_config.h.in`, filled in as Luanti's CMake would for this build: 5.17.0,
// run in place from the package, sound on, and every optional backend off - no cURL
// (nothing is downloaded), no gettext, no LevelDB, PostgreSQL, Redis, Prometheus or
// SpatialIndex (a single-player world lives in SQLite), no curses console, no OpenSSL.

#pragma once

#define PROJECT_NAME "luanti"
#define PROJECT_NAME_C "Luanti"
#define VERSION_MAJOR 5
#define VERSION_MINOR 17
#define VERSION_PATCH 0
#define VERSION_EXTRA ""
#define VERSION_STRING "5.17.0"
#define PRODUCT_VERSION_STRING "5.17"
#define STATIC_SHAREDIR "."
#define STATIC_LOCALEDIR "locale"
#define BUILD_TYPE "Release"
#define ICON_DIR ""
#define RUN_IN_PLACE 1
#define DEVELOPMENT_BUILD 0
#define ENABLE_UPDATE_CHECKER 0
#define USE_GETTEXT 0
#define USE_CURL 0
#define USE_SOUND 1
#define USE_CURSES 0
#define USE_LEVELDB 0
#define USE_LUAJIT 0
#define USE_POSTGRESQL 0
#define USE_PROMETHEUS 0
#define USE_SPATIAL 0
#define USE_SYSTEM_GMP 0
#define USE_SYSTEM_JSONCPP 0
#define USE_REDIS 0
#define USE_OPENSSL 0
#define HAVE_ENDIAN_H 0
#define HAVE_STRLCPY 0
#define HAVE_MALLOC_TRIM 0
#define CURSES_HAVE_CURSES_H 0
#define CURSES_HAVE_NCURSES_H 0
#define CURSES_HAVE_NCURSES_NCURSES_H 0
#define CURSES_HAVE_NCURSES_CURSES_H 0
#define CURSES_HAVE_NCURSESW_NCURSES_H 0
#define CURSES_HAVE_NCURSESW_CURSES_H 0
#define BUILD_UNITTESTS 0
#define BUILD_BENCHMARKS 0
#define BUILD_WITH_TRACY 0
