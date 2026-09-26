/*
 * The payload entry point.
 *
 * A payload is called, not spawned: the loader runs no `main` and passes no argv. This
 * calls ETR's `main` from upstream `main.cpp`, so no patch renames it.
 *
 * `main` is declared here because ETR has no header for it. It is not `extern "C"`:
 * it is C++ and its name is mangled.
 */
#include "oops/syscall.h"
#include "oops/system.h"

int main(int argc, char **argv);

extern "C" __attribute__((visibility("default"))) int
etr_start(const payload_args_t *args) {
    /*
     * ETR derives its data directory from `argv[0]` (`game_config.cpp:313`). `/app0` is
     * where the package, and the data shipped in it, is mounted.
     */
    static char arg0[] = "/app0/etr";
    char *argv[2] = {arg0, 0};

    (void)args;

    oops_klog("ETXR", "entry");

    /*
     * Before `main`, because a payload has no crt to do it. ETR's global objects
     * (`Course`, `Tex`, `FT`, `Winsys` and more) have constructors in `.init_array`;
     * `CCourse`'s sets `curr_course = -1`. See `oops/system.h`.
     */
    oops_run_init_array();

    return main(1, argv);
}
