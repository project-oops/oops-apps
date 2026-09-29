/*
 * Payload entry point.
 *
 * C++ rather than C: it goes through `common/cxx.mk`, which brings the C++ runtime the
 * game links. Upstream's `main` (`src/main.cpp`) is named below by its symbol.
 *
 * The file manager (`src/io/file_manager.cpp`) is pointed at the package by
 * environment: `SUPERTUXKART_DATADIR` for stk-code's own `data/` (it appends `/data/`),
 * `SUPERTUXKART_ASSETS_DIR` for the karts, tracks, textures and music, and
 * `SUPERTUXKART_SAVEDIR` for config and saves. All of them are under `/app0`, where the
 * package mounts. Both trees ship as one tar (see the Makefile's `package`), unpacked
 * here on the first start, because `pros restore` costs per file.
 */
#include "oops/syscall.h"
#include "oops/system.h"

#include "loading_screen.h"

#include <cstdlib>

extern "C" void oops_crashtrace_install(void);
/* `SDL_bool SDL_SetHint(...)` (`SDL_hints.h`), declared here because the shim does not
 * compile against SDL's headers. */
extern "C" int SDL_SetHint(const char *name, const char *value);
/* oops-mesa's `.init_array` walk, which also runs Mesa's own dynamic initialisers. It
 * is guarded, so Mesa calling it again is harmless; the SDK's `oops_run_init_array` is
 * not guarded against this one and would construct everything twice. */
extern "C" void oops_mesa_run_init_array(void);

/* By its assembler name: `common/cxx.mk` compiles this file freestanding, where `main`
 * is an ordinary function and would be mangled, while upstream's hosted `main.cpp`
 * emits it plain. */
int stkt_upstream_main(int argc, char *argv[]) __asm__("main");

/* app.mk derives the entry symbol from the app name; lld only warns on a mismatch. */
extern "C" int supertuxkart_start(const payload_args_t *args);

__attribute__((visibility("default"))) extern "C" int
supertuxkart_start(const payload_args_t *args) {
    static char arg0[] = "/app0/supertuxkart";
    char *argv[] = {arg0, nullptr};

    (void)args;

    /* Before anything touches Mesa or a C++ global: the engine registers its GUI
       widgets and scripting bindings from namespace-scope constructors. */
    oops_mesa_run_init_array();

    oops_log_init(OOPS_APP_ID);
    /* The kernel log drops bursts and this start-up is one. */
    (void)oops_log_enable_disk_sink(OOPS_APP_ID, 0);
    oops_log_info("STKT", "entry");
    oops_crashtrace_install();

    /* 5,586 files behind a loading screen, on the first start only. */
    if (oops_loading_unpack_once("SuperTuxKart", OOPS_POSIX_HOME "/stk-data.tar",
                                 OOPS_POSIX_HOME,
                                 OOPS_POSIX_HOME "/.stk-data-unpacked") != 0) {
        oops_log_error("STKT", "the game data could not be unpacked; see above");
        exit(1);
    }

    if (setenv("HOME", OOPS_POSIX_HOME, 1) != 0 ||
        setenv("SUPERTUXKART_DATADIR", OOPS_POSIX_HOME, 1) != 0 ||
        setenv("SUPERTUXKART_ASSETS_DIR", OOPS_POSIX_HOME "/assets", 1) != 0 ||
        setenv("SUPERTUXKART_SAVEDIR", OOPS_POSIX_HOME "/.supertuxkart", 1) != 0) {
        oops_log_error("STKT", "the environment could not be set; the data will not be "
                               "found");
    }

    /* The edit box calls `SDL_StartTextInput` on focus; with this the SDL backend
       answers it with the system keyboard (`oops-deps/sdl2`). A hint rather than the
       environment, because this SDL keeps its own. */
    (void)SDL_SetHint("SDL_ENABLE_SCREEN_KEYBOARD", "1");

    /* `exit`, not `return`: the loader called this and has nowhere to return to. */
    exit(stkt_upstream_main(1, argv));
    return 0;
}
