/*
 * The host runner for the probe.
 *
 * It runs the same five checks against the build machine's own C++ runtime, which unwinds
 * correctly. So this is not a test of the platform - it is a test of the *probe*, and it is
 * here because obSCEne's fifth rule for adding a check is that a check which has never passed
 * a known-good implementation is not evidence. That rule has caught a probe whose two
 * "must differ" inputs did not differ; it is cheap insurance against shipping a check that
 * cannot pass anywhere.
 *
 * A failure here means `probe.cpp` is wrong. A failure on the console, with this passing, means
 * the unwinder is.
 */
#include <cstdio>

#include "cxx_throw.h"

int main()
{
    const int total  = cxx_throw_total();
    const int passed = cxx_throw_run();

    std::printf("cxx-throw host probe: passed=%d total=%d\n", passed, total);

    if (passed != total) {
        std::printf("cxx-throw host probe: FAIL - the checks do not pass against a known-good "
                    "C++ runtime, so the probe itself is wrong\n");
        return 1;
    }

    std::printf("cxx-throw host probe: ok - the checks are sound; what they say on the console "
                "is about the console\n");
    return 0;
}
