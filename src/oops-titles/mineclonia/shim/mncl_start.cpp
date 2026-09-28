/*
 * Payload entry point.
 *
 * C++ rather than C: Luanti's `main` is at `src/main.cpp`, a C++ translation unit, and
 * in a freestanding C++ build it is an ordinary, mangled function that a C declaration
 * would miss without the link saying so.
 *
 * The engine is built `RUN_IN_PLACE`, so its share and user paths are both the
 * directory argv[0] names - `/app0`, where the package mounts. The game is packaged at
 * `games/mineclonia`, and worlds, settings and the mod cache are written beside it. The
 * engine opens on its own main menu, where the game is already the only one installed.
 *
 * That tree - the engine's Lua, shaders, fonts and textures, and the game - is some
 * 10,000 files, so the package carries it as one tar and it is unpacked here the first
 * time the title starts (`common/tar_unpack.c`, and the Makefile's `package` target).
 */
#include "oops/syscall.h"
#include "oops/system.h"

#include "tar_unpack.h"

extern "C" void oops_crashtrace_install(void);

/* The first start takes a while; the log says how far it has got. */
static void mncl_unpack_progress(size_t files, void *user) {
    (void)user;
    if (files % 1000u == 0u) {
        oops_log_info("MNCL", "unpacked %zu files", files);
    }
}

#include <cstdlib>

int main(int argc, char *argv[]);

/* app.mk derives the entry symbol from the app name; lld only warns on a mismatch. */
extern "C" int mineclonia_start(const payload_args_t *args);

__attribute__((visibility("default"))) extern "C" int
mineclonia_start(const payload_args_t *args) {
    static char arg0[] = "/app0/luanti";
    char *argv[] = {arg0, nullptr};

    (void)args;

    oops_log_init(OOPS_APP_ID);
    /* The kernel log drops bursts and this start-up is one. */
    (void)oops_log_enable_disk_sink(OOPS_APP_ID, 0);
    oops_log_info("MNCL", "entry");
    oops_crashtrace_install();

    if (oops_tar_unpack_once(OOPS_POSIX_HOME "/luanti-data.tar", OOPS_POSIX_HOME,
                             OOPS_POSIX_HOME "/.luanti-data-unpacked",
                             mncl_unpack_progress, nullptr) != 0) {
        oops_log_error("MNCL",
                       "the game data could not be unpacked; see the lines above");
        /* `exit`, not `return`: the loader called this and has nowhere to return to. */
        exit(1);
    }

    if (setenv("HOME", OOPS_POSIX_HOME, 1) != 0) {
        oops_log_error("MNCL", "HOME could not be set");
    }

    /* Namespace-scope constructors: .init_array is not walked for this payload, and the
       engine registers its settings and script APIs from them. */
    oops_run_init_array();

    return main(1, argv);
}
