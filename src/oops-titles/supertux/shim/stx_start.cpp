/*
 * The payload entry point.
 *
 * A payload is called, not spawned: there is no `main` the loader runs and no argv to
 * hand it. SuperTux's `main` is `src/main.cpp` upstream, three lines that build a
 * `Main` and run it, so this calls it - the same arrangement as Extreme Tux Racer's
 * `etr_start.cpp`.
 *
 * `main` is declared here rather than included, because upstream does not export it in
 * a header. It is C++ in a C++ translation unit and its name is mangled accordingly;
 * `extern "C"` would be wrong.
 */
#include "oops/syscall.h"
#include "oops/system.h"

int main(int argc, char **argv);

extern "C" __attribute__((visibility("default"))) int
stx_start(const payload_args_t *args) {
    /*
     * **`--datadir` is the argument that matters.** It points PhysFS at the content the
     * package carries. Without it `main.cpp` falls through to `SUPERTUX2_DATA_DIR`,
     * then to probing `BUILD_DATA_DIR` for `credits.stxt`, then to `SDL_GetBasePath` -
     * `config.h` names the same place for the second of those, but saying it here means
     * the answer does not depend on which fallback runs first.
     */
    static char arg0[] = "/app0/supertux2";
    static char arg1[] = "--datadir";
    static char arg2[] = "/app0/data";
    char *argv[4] = {arg0, arg1, arg2, 0};

    (void)args;

    oops_klog("STUX", "entry");

    /*
     * **Before `main`, because a payload has no crt to do it.** SuperTux has about
     * fifty namespace-scope objects with constructors - `static const std::string
     * DART_SOUND = "sounds/flame.wav"` in `badguy/dart.cpp` is the shape of most of
     * them - and they run from
     * `.init_array`, which nothing walks unless the title asks. Skipped, every one of
     * those strings is empty and each sound it names silently fails to load.
     * `oops/system.h` has the account; Extreme Tux Racer is where it was found.
     */
    oops_run_init_array();

    return main(3, argv);
}
