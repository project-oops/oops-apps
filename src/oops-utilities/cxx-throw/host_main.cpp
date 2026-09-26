/*
 * The host runner for the probe. It runs the checks against the build machine's C++
 * runtime, a known-good unwinder: a failure here means `probe.cpp` is wrong, and a
 * console failure with this passing means the unwinder is.
 */
#include <cstdio>

#include "cxx_throw.h"

/* `oops_klog` on stdout, so a host run prints the same lines as a console run. */
extern "C" void oops_klog(const char *tag, const char *msg) {
    std::printf("[%s] %s\n", tag, msg);
}

int main() {
    const int total = cxx_throw_total();
    const int passed = cxx_throw_run();

    std::printf("cxx-throw host probe: passed=%d total=%d\n", passed, total);

    if (passed != total) {
        std::printf(
            "cxx-throw host probe: FAIL - the checks do not pass against a known-good "
            "C++ runtime, so the probe itself is wrong\n");
        return 1;
    }

    std::printf(
        "cxx-throw host probe: ok - the checks are sound; what they say on the console "
        "is about the console\n");
    return 0;
}
