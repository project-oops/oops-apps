/*
 * The payload entry point.
 *
 * A payload is called, not spawned: there is no `main` the loader runs and no argv to hand it.
 * ETR's `main` lives in `main.cpp` upstream, so this calls it - which is the whole of the
 * adaptation, and the reason it is three lines rather than a patch renaming `main`.
 *
 * `main` is declared here rather than included, because ETR does not export it in a header.
 * Declaring it `extern "C"` would be wrong - it is C++ in a C++ translation unit and its name is
 * mangled accordingly.
 */
#include "oops/syscall.h"
#include "oops/system.h"

int main(int argc, char **argv);

extern "C" __attribute__((visibility("default"))) int etr_start(const payload_args_t *args)
{
    /*
     * ETR reads `argv[0]` for its own path handling, so it gets one rather than a null it would
     * have to guard. `/app0` is where the package is mounted and where the data it ships beside
     * this binary lives.
     */
    static char arg0[] = "/app0/etr";
    char *argv[2] = { arg0, 0 };

    (void)args;

    oops_klog("ETXR", "entry");

    /*
     * **Before `main`, because a payload has no crt to do it.** ETR is built of global objects -
     * `Course`, `Tex`, `FT`, `Winsys` and a dozen more - and their constructors run from
     * `.init_array`, which nothing walks unless the title asks. The failure is invisible until a
     * constructor stores something that is not zero: `CCourse`'s sets `curr_course = -1`, and with
     * it left at 0 the game loaded no course at all and faulted on a NaN four layers later.
     * `oops/system.h` has the whole account.
     */
    oops_run_init_array();

    return main(1, argv);
}
