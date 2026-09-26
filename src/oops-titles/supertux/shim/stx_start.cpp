/*
 * The payload entry point.
 *
 * A payload is called, not spawned, so no loader runs `main` or supplies argv. This
 * builds an argv and calls upstream's `main` (`src/main.cpp`).
 *
 * `main` is declared here because upstream has no header for it. It is a C++ function
 * with a mangled name, so it is not `extern "C"`.
 */
#include "oops/syscall.h"
#include "oops/system.h"

int main(int argc, char **argv);

extern "C" __attribute__((visibility("default"))) int
stx_start(const payload_args_t *args) {
    /*
     * `--datadir` points PhysFS at the package's content directly, so the result does
     * not depend on `main.cpp`'s fallbacks (`SUPERTUX2_DATA_DIR`, `BUILD_DATA_DIR`,
     * `SDL_GetBasePath`).
     */
    static char arg0[] = "/app0/supertux2";
    static char arg1[] = "--datadir";
    static char arg2[] = "/app0/data";
    char *argv[4] = {arg0, arg1, arg2, 0};

    (void)args;

    oops_klog("STUX", "entry");

    /*
     * A payload has no crt, so the title runs `.init_array` before `main`. SuperTux has
     * namespace-scope objects with constructors (`DART_SOUND` in `badguy/dart.cpp`);
     * skipped, those strings are empty. See `oops/system.h`.
     */
    oops_run_init_array();

    return main(3, argv);
}
