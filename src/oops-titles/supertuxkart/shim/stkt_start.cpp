/*
 * Payload entry point.
 *
 * C++ rather than C: SuperTuxKart's `main` is at `src/main.cpp`, a C++ translation unit,
 * and in a freestanding C++ build it is an ordinary, mangled function that a C declaration
 * would miss without the link saying so.
 *
 * The file manager (`src/io/file_manager.cpp`) is pointed at the package by environment:
 * `SUPERTUXKART_DATADIR` for stk-code's own `data/`, `SUPERTUXKART_ASSETS_DIR` for the
 * karts, tracks, textures and music, and `SUPERTUXKART_SAVEDIR` for config and saves. All
 * of them are under `/app0`, where the package mounts.
 */
#include "oops/syscall.h"
#include "oops/system.h"

extern "C" void oops_crashtrace_install(void);

#include <cstdlib>

int main(int argc, char *argv[]);

/* app.mk derives the entry symbol from the app name; lld only warns on a mismatch. */
extern "C" int supertuxkart_start(const payload_args_t *args);

__attribute__((visibility("default"))) extern "C" int
supertuxkart_start(const payload_args_t *args) {
    static char arg0[] = "/app0/supertuxkart";
    char *argv[] = {arg0, nullptr};

    (void)args;

    oops_log_init(OOPS_APP_ID);
    /* The kernel log drops bursts and this start-up is one. */
    (void)oops_log_enable_disk_sink(OOPS_APP_ID, 0);
    oops_log_info("STKT", "entry");
    oops_crashtrace_install();

    if (setenv("HOME", OOPS_POSIX_HOME, 1) != 0 ||
        setenv("SUPERTUXKART_DATADIR", OOPS_POSIX_HOME, 1) != 0 ||
        setenv("SUPERTUXKART_ASSETS_DIR", OOPS_POSIX_HOME "/assets", 1) != 0 ||
        setenv("SUPERTUXKART_SAVEDIR", OOPS_POSIX_HOME "/.supertuxkart", 1) != 0) {
        oops_log_error("STKT", "the environment could not be set; the data will not be "
                               "found");
    }

    /* Namespace-scope constructors: .init_array is not walked for this payload, and the
       engine registers its GUI widgets and scripting bindings from them. */
    oops_run_init_array();

    return main(1, argv);
}
